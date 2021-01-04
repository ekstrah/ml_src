
import numpy as np
import dgl
import torch
import dgl.nn as dglnn
import torch.nn as nn
from torch.nn.parallel import DistributedDataParallel
import torch.nn.functional as F
import torch.multiprocessing as mp
import sklearn.metrics
import tqdm

import utils

def load_data():
    import pickle

    with open('data.pkl', 'rb') as f:
        data = pickle.load(f)
    graph, node_features, node_labels, train_nids, valid_nids, test_nids = data
    utils.prepare_mp(graph)
    
    num_features = node_features.shape[1]
    num_classes = (node_labels.max() + 1).item()
    
    return graph, node_features, node_labels, train_nids, valid_nids, test_nids, num_features, num_classes

class MultiLayerNeighborSampler(dgl.dataloading.BlockSampler):
    def __init__(self, fanouts):
        super().__init__(len(fanouts), return_eids=False)
        self.fanouts = fanouts
        
    def sample_frontier(self, layer_id, g, seed_nodes):
        fanout = self.fanouts[layer_id]
        return dgl.sampling.sample_neighbors(g, seed_nodes, fanout)

def create_dataloader(rank, world_size, graph, nids):
    partition_size = len(nids) // world_size
    partition_offset = partition_size * rank
    nids = nids[partition_offset:partition_offset+partition_size]
    
    sampler = MultiLayerNeighborSampler([4, 4, 4])
    dataloader = dgl.dataloading.NodeDataLoader(
        graph, nids, sampler,
        batch_size=1024,
        shuffle=True,
        drop_last=False,
        num_workers=0
    )
    
    return dataloader

class SAGE(nn.Module):
    def __init__(self, in_feats, n_hidden, n_classes, n_layers):
        super().__init__()
        self.n_layers = n_layers
        self.n_hidden = n_hidden
        self.n_classes = n_classes
        self.layers = nn.ModuleList()
        self.layers.append(dglnn.SAGEConv(in_feats, n_hidden, 'mean'))
        for i in range(1, n_layers - 1):
            self.layers.append(dglnn.SAGEConv(n_hidden, n_hidden, 'mean'))
        self.layers.append(dglnn.SAGEConv(n_hidden, n_classes, 'mean'))
        
    def forward(self, bipartites, x):
        for l, (layer, bipartite) in enumerate(zip(self.layers, bipartites)):
            x = layer(bipartite, x)
            if l != self.n_layers - 1:
                x = F.relu(x)
        return x

def init_model(rank, in_feats, n_hidden, n_classes, n_layers):
    model = SAGE(in_feats, n_hidden, n_classes, n_layers).to(rank)
    return DistributedDataParallel(model, device_ids=[rank], output_device=rank)

@utils.fix_openmp
def train(rank, world_size, data):
    # data is the output of load_data
    torch.distributed.init_process_group(
        backend='nccl',
        init_method='tcp://127.0.0.1:12345',
        world_size=world_size,
        rank=rank)
    torch.cuda.set_device(rank)
    
    graph, node_features, node_labels, train_nids, valid_nids, test_nids, num_features, num_classes = data
    
    train_dataloader = create_dataloader(rank, world_size, graph, train_nids)
    # We only use one worker for validation
    valid_dataloader = create_dataloader(0, 1, graph, valid_nids)
    
    model = init_model(rank, num_features, 128, num_classes, 3)
    opt = torch.optim.Adam(model.parameters())
    torch.distributed.barrier()
    
    best_accuracy = 0
    best_model_path = 'model.pt'
    for epoch in range(10):
        model.train()

        for step, (input_nodes, output_nodes, bipartites) in enumerate(train_dataloader):
            bipartites = [b.to(rank) for b in bipartites]
            inputs = node_features[input_nodes].cuda()
            labels = node_labels[output_nodes].cuda()
            predictions = model(bipartites, inputs)

            loss = F.cross_entropy(predictions, labels)
            opt.zero_grad()
            loss.backward()
            opt.step()

            accuracy = sklearn.metrics.accuracy_score(labels.cpu().numpy(), predictions.argmax(1).detach().cpu().numpy())

            if rank == 0 and step % 10 == 0:
                print('Epoch {:05d} Step {:05d} Loss {:.04f}'.format(epoch, step, loss.item()))

        torch.distributed.barrier()
        
        if rank == 0:
            model.eval()
            predictions = []
            labels = []
            with torch.no_grad():
                for input_nodes, output_nodes, bipartites in valid_dataloader:
                    bipartites = [b.to(rank) for b in bipartites]
                    inputs = node_features[input_nodes].cuda()
                    labels.append(node_labels[output_nodes].numpy())
                    predictions.append(model.module(bipartites, inputs).argmax(1).cpu().numpy())
                predictions = np.concatenate(predictions)
                labels = np.concatenate(labels)
                accuracy = sklearn.metrics.accuracy_score(labels, predictions)
                print('Epoch {} Validation Accuracy {}'.format(epoch, accuracy))
                if best_accuracy < accuracy:
                    best_accuracy = accuracy
                    torch.save(model.module.state_dict(), best_model_path)
                    
        torch.distributed.barrier()
        


if __name__ == '__main__':
    procs = []
    data = load_data()
    for proc_id in range(4):    # 4 gpus
        p = mp.Process(target=train, args=(proc_id, 4, data))
        p.start()
        procs.append(p)
    for p in procs:
        p.join()