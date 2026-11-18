"""Cyclic odd decreasing blocks inside the meandric involutions."""
from itertools import combinations
from math import comb
import json


def counts(n):
    size = 2*n
    # All two-block decompositions represent one of the n vertex reflections.
    total = n
    fixed = 1
    for blocks in range(4, size+1, 2):
        total += size * comb((size+blocks)//2-1, blocks-1) // blocks
        s = (blocks-2)//2
        fixed += sum(comb(t+s-1, s-1)*(n-s-2*t)
                     for t in range((n-s-1)//2+1))
    return total, fixed


def chord_test(p):
    m = len(p)
    for side in range(2):
        edges = [tuple(sorted((p[i],p[(i+1)%m]))) for i in range(side,m,2)]
        for i,(a,b) in enumerate(edges):
            if any((a<c<b) != (a<d<b) for c,d in edges[:i]):
                return False
    return True


def small(n):
    m = 2*n
    words = set()
    for pieces in range(2,m+1,2):
        for cuts in combinations(range(m),pieces):
            p = [-1]*m
            for left,right in zip(cuts,cuts[1:]+(cuts[0]+m,)):
                if (right-left)%2 == 0:
                    break
                for i in range(left,right):
                    p[i%m] = (left+right-1-i)%m
            else:
                word = tuple(p)
                assert all(p[p[i]] == i and (p[i]-i)%2 == 0 for i in range(m))
                assert chord_test(word)
                words.add(word)
    return len(words),sum(all(p[-i%m] == -p[i]%m for i in range(m)) for p in words)


def exceptional_word():
    n = 39
    p = list(range(2*n))
    for i in (1,9,25):
        p[i-1],p[i+1] = p[i+1],p[i-1]
    assert chord_test(p)
    # Count all affine reindexings for which p f p = f^{-1}.
    stabilizers = []
    for sign in (-1,1):
        for shift in range(2*n):
            if all(p[(sign*p[i]+shift)%(2*n)] ==
                   sign*(i-shift)%(2*n) for i in range(2*n)):
                stabilizers.append((sign,shift))
    assert stabilizers == [(1,0)]
    return stabilizers


if __name__ == "__main__":
    for n in (3,5,7,39):
        total,fixed = counts(n)
        if n<39:
            assert (total,fixed) == small(n)
        print(json.dumps(dict(n=n,total=total,fixed=fixed,remaining=(total-fixed),
                              remaining_mod8=(total-fixed)%8)))
    print(json.dumps(dict(triple_reflection_39_normalizers=exceptional_word())))
