"""Exhaustive rooted-permutation audit of two symmetries of projective meanders."""
from __future__ import annotations

import argparse
import json
from collections import Counter
from functools import cache
from math import comb


@cache
def matchings(n):
    if not n:
        return [()]
    return [(2 * t + 1,) + tuple(x + 1 for x in a) + (0,) +
            tuple(x + 2 * t + 2 for x in b)
            for t in range(n) for a in matchings(t) for b in matchings(n - 1 - t)]


def shift_matching(a, t):
    m = len(a)
    return tuple((a[(i - t) % m] + t) % m for i in range(m))


def reflection(a, t):
    m = len(a)
    return tuple((t - a[(t - i) % m]) % m for i in range(m))


def traversal(a, b):
    word = []
    point = 0
    while not word or point:
        word.append(point)
        point = (a if len(word) % 2 else b)[point]
    return tuple(word) if len(word) == len(a) else None


def check(n):
    n2 = 2 * n
    projective = set()
    sectors = Counter()
    fixed = Counter()
    for a in matchings(n):
        b = shift_matching(a, n)
        p = traversal(a, b)
        if p is None:
            continue
        assert all(p[(i + n) % n2] == (p[i] + n) % n2 for i in range(n2))
        assert p not in projective
        projective.add(p)
        height = sum(v >= n for v in a[:n])
        sectors[height] += 1
        for test, perm in [('halfturn', b), ('halfreverse', reflection(a, n - 1)),
                           ('fullreverse', reflection(a, -1))]:
            if perm == a:
                fixed[test, height] += 1
        if height > 1:
            assert len({a, b, reflection(a, n - 1), reflection(a, -1)}) == 4
    counts = Counter()
    contracted = set()
    for p in projective:
        inverse = tuple(sorted(range(n2), key=p.__getitem__))
        left = tuple((-x) % n2 for x in p)
        right = tuple(p[-i % n2] for i in range(n2))
        both = tuple((-x) % n2 for x in right)
        assert all(x in projective for x in (inverse, left, right, both))
        counts['U'] += both == p
        counts['E'] += inverse == p
        counts['Q'] += left == inverse
        if inverse == p and p[1] == n2-1:
            removed={1,n-1,n+1,n2-1}
            kept=[i for i in range(n2) if i not in removed]
            rank={v:i for i,v in enumerate(kept)}
            word=tuple(rank[p[i]] for i in kept)
            assert word not in contracted
            contracted.add(word)
    small=set()
    for a in matchings(n-2):
        word=traversal(a,shift_matching(a,n-2))
        if word is not None and all(word[word[i]]==i for i in range(len(word))):
            small.add(word)
    assert contracted == small
    k = (n + 1) // 2
    d = sum(traversal(a, reflection(a, 0)) is not None for a in matchings(k))
    assert d == counts['U'] == fixed['fullreverse', 1]
    assert all(x % 4 == 0 for h, x in sectors.items() if h > 1)
    assert (len(projective) + d + 2 * counts['E'] + 2 * counts['Q']) % 8 == 0
    assert n % 4 != 3 or counts['Q'] == 0
    return dict(n=n, catalan=comb(2*n, n)//(n+1), P=len(projective),
                by_height=dict(sorted(sectors.items())), D_lower=d,
                E=counts['E'], Q=counts['Q'], contracted_branch=len(contracted))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('last', type=int)
    args = parser.parse_args()
    for odd in range(3, args.last + 1, 2):
        print(json.dumps(check(odd)), flush=True)
