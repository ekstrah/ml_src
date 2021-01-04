import collections
import itertools

import numpy as np


__all__ = ['chow_liu']


def chow_liu(X, root=None):
    marginals = {v: X[v].value_counts(normalize=True) for v in X.columns}
    edge = collections.namedtuple('edge', ['u', 'v', 'mi'])
    mis = (
        edge(
            u, v, mutual_info(
            puv=X.groupby([u, v]).size() / len(X),
            pu=marginals[u],
            pv=marginals[v]
        ))
        for u, v in itertools.combinations(sorted(X.columns), 2)
    )
    edges = ((e.u, e.v) for e in sorted(mis, key=lambda e: e.mi, reverse=True))

    neighbors = kruskal(vertices=X.columns, edges=edges)

    if root is None:
        root = X.columns[0]

    return list(orient_tree(neighbors, root, visited=set()))


def mutual_info(puv, pu, pv):
    pu = pu.reindex(puv.index.get_level_values(pu.name)).values
    pv = pv.reindex(puv.index.get_level_values(pv.name)).values

    return (puv * np.log(puv / (pv * pu))).sum()


class DisjointSet:

    def __init__(self, *values):
        self.parents = {x: x for x in values}
        self.sizes = {x: 1 for x in values}

    def find(self, x):
        while self.parents[x] != x:
            x, self.parents[x] = self.parents[x], self.parents[self.parents[x]]
        return x

    def union(self, x, y):
        if self.sizes[x] < self.sizes[y]:
            x, y = y, x
        self.parents[y] = x
        self.sizes[x] += self.sizes[y]


def kruskal(vertices, edges):

    ds = DisjointSet(*vertices)
    neighbors = collections.defaultdict(set)

    for u, v in edges:

        if ds.find(u) != ds.find(v):
            neighbors[u].add(v)
            neighbors[v].add(u)
            ds.union(ds.find(u), ds.find(v))

        if len(neighbors) == len(vertices):
            break

    return neighbors


def orient_tree(neighbors, root, visited):

    for neighbor in neighbors[root] - visited:
        yield root, neighbor
        yield from orient_tree(neighbors, root=neighbor, visited={root})
