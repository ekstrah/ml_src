import collections
import functools
import itertools
import graphlib
import queue
import typing

import numpy as np
import pandas as pd
import vose


__all__ = ['BayesNet']


@pd.api.extensions.register_series_accessor('cdt')
class CDTAccessor:

    def __init__(self, series: pd.Series):
        self.series = series
        self.sampler = None

    def sample(self):
        if self.sampler is None:
            self.sampler = vose.Sampler(
                weights=self.series.to_numpy(dtype=float),
                seed=np.random.randint(2 ** 16)
            )
        idx = self.sampler.sample()
        return self.series.index[idx]

    @functools.lru_cache(maxsize=256)
    def __getitem__(self, idx):
        return self.series[idx]

    def sum_out(self, *variables):
        nodes = list(self.series.index.names)
        for var in variables:
            nodes.remove(var)
        return self.series.groupby(nodes).sum()


def pointwise_mul_two(left: pd.Series, right: pd.Series):
    """Pointwise multiplication of two series.

    Examples
    --------

    Example taken from figure 14.10 of Artificial Intelligence: A Modern Approach.

    >>> a = pd.Series({
    ...     ('T', 'T'): .3,
    ...     ('T', 'F'): .7,
    ...     ('F', 'T'): .9,
    ...     ('F', 'F'): .1
    ... })
    >>> a.index.names = ['A', 'B']

    >>> b = pd.Series({
    ...     ('T', 'T'): .2,
    ...     ('T', 'F'): .8,
    ...     ('F', 'T'): .6,
    ...     ('F', 'F'): .4
    ... })
    >>> b.index.names = ['B', 'C']

    >>> pointwise_mul_two(a, b)
    B  A  C
    F  T  T    0.42
            F    0.28
        F  T    0.06
            F    0.04
    T  T  T    0.06
            F    0.24
        F  T    0.18
            F    0.72
    dtype: float64

    This method returns the Cartesion product in case two don't share any part of their index
    in common.

    >>> a = pd.Series({
    ...     ('T', 'T'): .3,
    ...     ('T', 'F'): .7,
    ...     ('F', 'T'): .9,
    ...     ('F', 'F'): .1
    ... })
    >>> a.index.names = ['A', 'B']

    >>> b = pd.Series({
    ...     ('T', 'T'): .2,
    ...     ('T', 'F'): .8,
    ...     ('F', 'T'): .6,
    ...     ('F', 'F'): .4
    ... })
    >>> b.index.names = ['C', 'D']

    >>> pointwise_mul_two(a, b)
    A  B  C  D
    T  T  F  F    0.12
             T    0.18
          T  F    0.24
             T    0.06
       F  F  F    0.28
             T    0.42
          T  F    0.56
             T    0.14
    F  T  F  F    0.36
             T    0.54
          T  F    0.72
             T    0.18
       F  F  F    0.04
             T    0.06
          T  F    0.08
             T    0.02
    dtype: float64

    Here is an example where both series have a one-dimensional index:

    >>> a = pd.Series({
    ...     'T': .3,
    ...     'F': .7
    ... })
    >>> a.index.names = ['A']

    >>> b = pd.Series({
    ...     'T': .2,
    ...     'F': .8
    ... })
    >>> b.index.names = ['B']

    >>> pointwise_mul_two(a, b)
    A  B
    T  T    0.06
       F    0.24
    F  T    0.14
       F    0.56
    dtype: float64

    Finally, here is an example when only one of the series has a MultiIndex.

    >>> a = pd.Series({
    ...     'T': .3,
    ...     'F': .7
    ... })
    >>> a.index.names = ['A']

    >>> b = pd.Series({
    ...     ('T', 'T'): .2,
    ...     ('T', 'F'): .8,
    ...     ('F', 'T'): .6,
    ...     ('F', 'F'): .4
    ... })
    >>> b.index.names = ['B', 'C']

    >>> pointwise_mul_two(a, b)
    A  B  C
    T  F  F    0.12
          T    0.18
       T  F    0.24
          T    0.06
    F  F  F    0.28
          T    0.42
       T  F    0.56
          T    0.14
    dtype: float64

    """

    if not set(left.index.names) & set(right.index.names):
        cart = pd.DataFrame(np.outer(left, right), index=left.index, columns=right.index)
        return cart.stack(list(range(cart.columns.nlevels)))

    index, l_idx, r_idx, = left.index.join(right.index, how='inner', return_indexers=True)
    if l_idx is None:
        l_idx = np.arange(len(left))
    if r_idx is None:
        r_idx = np.arange(len(right))

    return pd.Series(left.iloc[l_idx].values * right.iloc[r_idx].values, index=index)


def pointwise_mul(cdts, keep_zeros=False):
    if not keep_zeros:
        cdts = (cdt[cdt > 0] for cdt in cdts)
    return functools.reduce(pointwise_mul_two, cdts)


class BayesNet:

    def __init__(self, *structure, prior_count: int = None):

        self.prior_count = prior_count

        def coerce_list(obj):
            if isinstance(obj, list):
                return obj
            return [obj]

        edges = (e for e in structure if isinstance(e, tuple))
        nodes = set(e for e in structure if not isinstance(e, tuple))

        self.parents = collections.defaultdict(set)
        self.children = collections.defaultdict(set)

        for parents, children in edges:
            for parent, child in itertools.product(coerce_list(parents), coerce_list(children)):
                self.parents[child].add(parent)
                self.children[parent].add(child)

        self.parents = {node: list(sorted(parents)) for node, parents in self.parents.items()}
        self.children = {node: list(sorted(children)) for node, children in self.children.items()}

        ts = graphlib.TopologicalSorter()
        for node in sorted({*self.parents.keys(), *self.children.keys(), *nodes}):
            ts.add(node, *self.parents.get(node, []))
        self.nodes = list(ts.static_order())

        self.P = {}
        self._P_sizes = {}

    def prepare(self):

        for node, P in self.P.items():
            P.sort_index(inplace=True)
            P.index.rename(
                [*self.parents[node], node] if node in self.parents else node,
                inplace=True
            )
            P.name = (
                f'P({node} | {", ".join(map(str, self.parents[node]))})'
                if node in self.parents else
                f'P({node})'
            )

    def _forward_sample(self, init: dict = None):

        init = init or {}

        while True:

            sample = {}
            likelihood = 1.

            for node in self.nodes:

                P = self.P[node]
                if node in self.parents:
                    condition = tuple(sample[parent] for parent in self.parents[node])
                    P = P.cdt[condition]

                if node in init:
                    node_value = init[node]
                else:
                    node_value = P.cdt.sample()

                sample[node] = node_value
                likelihood *= P.get(node_value, 0)

            yield sample, likelihood

    def _flood_fill_sample(self, init: dict = None):

        init = init or {}

        def walk(node, visited):

            if node in visited:
                return

            yield node, visited
            visited.add(node)

            for parent in self.parents.get(node, []):
                yield from walk(parent, visited)

            for child in self.children.get(node, []):
                yield from walk(child, visited)

        P = {}

        for node, visited in walk(node=self.roots[0], visited=set()):

            p = self.P[node]

            if node in init:
                p = p[p.index.get_level_values(node) == init[node]]

            if conditioning := list(visited.intersection(self.markov_boundary(node))):
                p = pointwise_mul([p, pointwise_mul(self.P[c] for c in conditioning)])
                p = p.groupby([*conditioning, node]).sum()
                p = p.groupby(conditioning).apply(lambda g: g / g.sum())

            P[node] = p

        while True:

            sample = init.copy()

            for node, visited in walk(node=self.roots[0], visited=set()):
                p = P[node]
                if visited:
                    condition = tuple(sample[c] for c in p.index.names[:-1])
                    p = p.cdt[condition]
                sample[node] = p.cdt.sample()

            yield sample

    def sample(self, n=1):

        samples = (sample for sample, _ in self._forward_sample())

        if n > 1:
            return pd.DataFrame(next(samples) for _ in range(n)).sort_index(axis='columns')
        return next(samples)

    def partial_fit(self, X: pd.DataFrame):
        for child, parents in self.parents.items():

            if child in self.P:
                old_counts = self.P[child] * self._P_sizes[child]
                new_counts = X.groupby(parents + [child]).size()
                counts = old_counts.add(new_counts, fill_value=0)
            else:
                counts = X.groupby(parents + [child]).size()
                if self.prior_count:
                    combos = itertools.product(*[X[var].unique() for var in parents + [child]])
                    prior = pd.Series(1, pd.MultiIndex.from_tuples(combos, names=parents + [child]))
                    counts = counts.add(prior, fill_value=0)
            self._P_sizes[child] = counts.groupby(parents).sum()
            self.P[child] = counts / self._P_sizes[child]

        for root in self.roots:

            if root in self.P:
                old_counts = self.P[root] * self._P_sizes[root]
                new_counts = X[root].value_counts()
                counts = old_counts.add(new_counts, fill_value=0)
                self._P_sizes[root] += len(X)
                self.P[root] = counts / self._P_sizes[root]


            else:
                self._P_sizes[root] = len(X)
                self.P[root] = X[root].value_counts(normalize=True)

        self.prepare()

        return self

    def fit(self, X: pd.DataFrame):

        self.P = {}
        self._P_sizes = {}
        return self.partial_fit(X)

    def _rejection_sampling(self, *query, event, n_iterations):


        samples = {var: [] for var in query}

        for _ in range(n_iterations):
            sample = self.sample()


            if any(sample[var] != val for var, val in event.items()):
                continue

            for var in query:
                samples[var].append(sample[var])

        samples = pd.DataFrame(samples)
        return samples.groupby(list(query)).size() / len(samples)

    def _llh_weighting(self, *query, event, n_iterations):

        samples = {var: [None] * n_iterations for var in query}
        likelihoods = [None] * n_iterations

        sampler = self._forward_sample(init=event)

        for i in range(n_iterations):

            sample, likelihood = next(sampler)
            for var in query:
                samples[var][i] = sample[var]
            likelihoods[i] = likelihood
        results = pd.DataFrame({'likelihood': likelihoods, **samples})
        results = results.groupby(list(query))['likelihood'].mean()
        results /= results.sum()

        return results

    def _gibbs_sampling(self, *query, event, n_iterations):

        posteriors = {}
        boundaries = {}
        nonevents = sorted(set(self.nodes) - set(event))

        for node in nonevents:

            post = pointwise_mul(self.P[node] for node in [node, *self.children.get(node, [])])

            if boundary := self.markov_boundary(node):
                post = post.groupby(boundary).apply(lambda g: g / g.sum())
                post = post.reorder_levels([*boundary, node])

            post = post.sort_index()
            posteriors[node] = post
            boundaries[node] = boundary


        state = next(self._forward_sample(init=event))[0]

        samples = {var: [None] * n_iterations for var in query}
        cycle = itertools.cycle(nonevents) 

        for i in range(n_iterations):

            var = next(cycle)

            P = posteriors[var]
            condition = tuple(state[node] for node in boundaries[var])
            if condition:
                P = P.cdt[condition]
            state[var] = P.cdt.sample()

            for var in query:
                samples[var][i] = state[var]

        samples = pd.DataFrame(samples)
        return samples.groupby(list(query)).size() / len(samples)

    def _variable_elimination(self, *query, event):

        relevant = {*query, *event}
        for node in list(relevant):
            relevant |= self.ancestors(node)
        hidden = relevant - {*query, *event}

        factors = []
        for node in relevant:
            factor = self.P[node].copy()

            for var, val in event.items():
                if var in factor.index.names:
                    factor = factor[factor.index.get_level_values(var) == val]

            factors.append(factor)

        for node in hidden:
            prod = pointwise_mul(
                factors.pop(i)
                for i in reversed(range(len(factors)))
                if node in factors[i].index.names
            )
            prod = prod.cdt.sum_out(node)
            factors.append(prod)

        posterior = pointwise_mul(factors)
        posterior = posterior / posterior.sum()
        posterior.index = posterior.index.droplevel(list(set(posterior.index.names) - set(query)))
        return posterior

    def ancestors(self, node):
        parents = self.parents.get(node, ())
        if parents:
            return set(parents) | set.union(*[self.ancestors(p) for p in parents])
        return set()

    @property
    def roots(self):

        return [node for node in self.nodes if node not in self.parents]

    def query(self, *query: typing.Tuple[str], event: dict, algorithm='exact',
              n_iterations=100) -> pd.Series:

        if not query:
            raise ValueError('At least one query variable has to be specified')

        for q in query:
            if q in event:
                raise ValueError('A query variable cannot be part of the event')

        if algorithm == 'exact':
            answer = self._variable_elimination(*query, event=event)

        elif algorithm == 'gibbs':
            answer = self._gibbs_sampling(*query, event=event, n_iterations=n_iterations)

        elif algorithm == 'likelihood':
            answer = self._llh_weighting(*query, event=event, n_iterations=n_iterations)

        elif algorithm == 'rejection':
            answer = self._rejection_sampling(*query, event=event, n_iterations=n_iterations)

        else:
            raise ValueError('Unknown algorithm, must be one of: exact, gibbs, likelihood, ' +
                             'rejection')

        answer = answer.rename(f'P({", ".join(query)})')

        if isinstance(answer.index, pd.MultiIndex):
            answer = answer.reorder_levels(sorted(answer.index.names))

        return answer.sort_index()

    def impute(self, sample: dict, **query_params) -> dict:
        missing = []
        event = sample.copy()
        for k, v in sample.items():
            if v is None:
                missing.append(k)
                del event[k]

        posterior = self.query(*missing, event=event, **query_params)
        for k, v in zip(posterior.index.names, posterior.idxmax()):
            event[k] = v

        return event

    def graphviz(self):

        import graphviz

        G = graphviz.Digraph()

        for node in self.nodes:
            G.node(str(node))

        for node, children in self.children.items():
            for child in children:
                G.edge(str(node), str(child))

        return G

    def _repr_svg_(self):
        return self.graphviz()._repr_svg_()

    def full_joint_dist(self, *select, keep_zeros=False) -> pd.DataFrame:

        fjd = pointwise_mul(self.P.values(), keep_zeros=keep_zeros)
        fjd = fjd.reorder_levels(sorted(fjd.index.names))
        fjd = fjd.sort_index()
        fjd.name = f'P({", ".join(fjd.index.names)})'
        return fjd / fjd.sum()

    def predict_proba(self, X: typing.Union[dict, pd.DataFrame]):


        if isinstance(X, dict):
            return self.predict_proba(pd.DataFrame([X])).iloc[0]

        fjd = self.full_joint_dist().reorder_levels(X.columns)
        return fjd[pd.MultiIndex.from_frame(X)]

    def predict_log_proba(self, X: typing.Union[dict, pd.DataFrame]):

        return np.log(self.predict_proba(X))

    @property
    def is_tree(self):

        return not any(len(parents) > 1 for parents in self.parents.values())

    def markov_boundary(self, node):
        children = self.children.get(node, [])
        return sorted(
            set(self.parents.get(node, [])) |
            set(children) |
            set().union(*[self.parents[child] for child in children]) -
            {node}
        )

    def iter_dfs(self):
        def bfs(node, visited):

            yield node
            visited.add(node)

            for child in self.children.get(node, []):
                if child not in visited:
                    yield from bfs(child, visited)

        visited = set()

        for root in self.roots:
            yield from bfs(root, visited)
