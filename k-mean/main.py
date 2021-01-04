#import all the necessary libraries
import sys
import random
import pandas as pd
import numpy as np
from scipy.spatial import distance
import copy
import warnings

def canopy(data,features,clu):
    initial_centroid = []   
    i = 1
    distances = 0
    centroids = []
    initial_centroid.append(np.reshape(random.choice(data),(1,features)).tolist())
    while i < clu:
            
        data1 = copy.deepcopy(data)

        for j in initial_centroid:
            distances += distance.cdist(j,data,metric = "euclidean")
            
        distnce = distances.flatten().tolist()
        max_dist = max(distnce)

        while [data1[distnce.index(max_dist)]] in initial_centroid:
            del data1[distnce.index(max_dist)]
            del distnce[distnce.index(max_dist)]
            max_dist = max(distnce)

        initial_centroid.append((np.reshape(data1[distnce.index(max_dist)],(-1,features))).tolist())

        distances = 0
        i += 1
    centroids = copy.deepcopy(initial_centroid)
    
    initial_centroid = []
    i = 1

    return centroids

def kmeans_assign(centroids,data,dataset,features):
    for cen in range(len(centroids)):
        dataset["{}".format(cen)] = distance.cdist(centroids[cen],data,metric = "euclidean").flatten().tolist()
    
    dataset["clusters"] = dataset.iloc[:,features:].astype(float).idxmin(axis = 1)

    data_dict = {}

    for clustr in dataset['clusters'].unique():        
        data_dict[clustr] = [[dataset[fea][y] for fea in range(0,features)] for y in dataset[dataset['clusters'] == clustr].index]

    final_dataset = copy.deepcopy(dataset)
    dataset.drop(dataset.iloc[:, features:], axis = 1, inplace = True)
    
    return data_dict,dataset,final_dataset

def kmeans_update(data_dict,features,centroids):
    avgDict = {}
    
    for k,v in data_dict.items():
        avgDict[k] = np.mean(v, axis = 0)

    avg_dict = sorted(avgDict.items())

    for ad in range(len(avg_dict)):
        if str(ad) in avg_dict[ad]:
            centroids[ad] = [avg_dict[ad][1]]
 
    return centroids

def silhoutte(data_dict,features):
    ai = []
    max_int = float('inf')
    bi = []
    si = []
    
    for key,value in data_dict.items():
        
            for point in data_dict[key]:
                ai_dist = 0
                bi_dist = 0
                
                for sec_point in data_dict[key]:
                    if point != sec_point:                        
                        ai_dist += distance.cdist([point],[sec_point],metric = "euclidean")
                    else:
                        continue
                    
                if len(data_dict[key]) == 1:
                    mean_ai = [ai_dist]
                else:
                    mean_ai = ai_dist / (len(data_dict[key]) - 1)
                
                ai.append(mean_ai)

                for ky,val in data_dict.items():
                    if ky != key:
                        bi_dist = distance.cdist([point],val,metric = "euclidean")
                    else:
                        continue

                    cluster_bi_mean = np.mean(bi_dist)

                    if cluster_bi_mean < max_int:
                        max_int = cluster_bi_mean
                
                bi.append(max_int)

    for x in range(len(ai)):
        if ai[x] == bi[x] or ai[x] == 0:
            si_value = 0
            si.append(si_value)
        else:
            si_value = (bi[x] - ai[x]) / max(bi[x],ai[x])
            si.append(si_value)
    
    mean_si = np.mean(si)

    return mean_si

def k_means(dataset,features,data,clusters):
    
    initial_centroid = []   
    sc = []
    
    for clu in clusters:
        centroids = canopy(data,features,clu)
        
        prev_centroid = [None] * len(centroids)
        cur_centroid = centroids

        while True:
            data_dict,dataset,final_dataset = kmeans_assign(cur_centroid,data,dataset,features)
            prev_centroid = copy.deepcopy(cur_centroid)
            cur_centroid = kmeans_update(data_dict,features,cur_centroid)
            error = (np.sum(prev_centroid,axis = 0) - np.sum(cur_centroid,axis = 0))

            if error.all() == 0:
                break
            else:
                continue

        data_dict,final_data_pts,final_dataset = kmeans_assign(cur_centroid,data,dataset,features)     
        sil = silhoutte(data_dict,features)

        initial_centroid = []
        i = 1
        if clu == 1:
            sc.append(0)
        else:
            sc.append(sil)

    return data_dict,final_dataset,sc

if __name__ == "__main__":
    warnings.filterwarnings("ignore")

    dataset = pd.read_csv('sample.dat',header=None)
    samples, features = dataset.shape

    data = dataset.values.tolist()
    if dataset.isnull().values.any() or ~dataset.applymap(np.isreal).all().all():
        exit()
    else:
        cluster_range = [2**n for n in range(0,6)]
        
        kmeans_dict,kmeans_df,sc = k_means(dataset,features,data,cluster_range)

        min_sc = 0
        max_sc = float('inf')
        new_k = 0

        for sc_val in range(1,len(sc)-1):
            if sc[sc_val + 1] > sc[sc_val]:
                continue        
            else:
                new_k = sc_val + 1
                break

        new_clu = [clus for clus in range(int(cluster_range[new_k] / (2**2)),cluster_range[new_k]+1)]

        k_means_dict,final_data_pts,sil_coef = k_means(dataset,features,data,new_clu)

        max_sc = max(sil_coef)

        optimum_k = [new_clu[sil_coef.index(max_sc)]]
        final_dict,final_df,sil_coeff = k_means(dataset,features,data,optimum_k)

        anomaly_array = []
        
        threshold = int(0.01 * len(data))

        final_arr = final_df.values

        cluster_len = {}

        for s in range(len(final_arr)):
            if final_arr[s][-1] in cluster_len:
                cluster_len[final_arr[s][-1]] += 1
            else:
                cluster_len[final_arr[s][-1]] = 1

        for clster,len_val in cluster_len.items():
            if len_val == 1:
                anomaly_array.append(final_dict[clster])
            elif len_val < threshold:
                for values in final_dict[clster]:
                    anomaly_array.append(values)

        anamoly_dict = {}
        for clu_key,clu_val in final_dict.items():
            anamoly_dict[clu_key] = [(np.mean(clu_val,axis = 0) + 2*np.std(clu_val,axis = 0)).tolist(),(np.mean(clu_val,axis = 0) - 2*np.std(clu_val,axis = 0)).tolist()]
            
        z = 0
        for key1 in set(anamoly_dict.keys()) & set(final_dict.keys()):
            for value1 in final_dict[key1]:
                if ~(np.less(np.array(value1),np.array(anamoly_dict[key1][z])).any()) and np.greater(np.array(np.array(value1)),anamoly_dict[key1][z+1]).all():
                    anomaly_array.append(value1)
                else:
                    continue

        anomalies_array = [[np.round(float(i), 2) for i in nested] for nested in anomaly_array]
        
        if len(anomalies_array) == 0:
            print("No anomalies")
        else:
            for anomaly in anomalies_array:
                print(anomaly)

