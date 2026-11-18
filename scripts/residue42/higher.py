"""Explore the next 2-adic bit with an unpinned even offset on the second circle."""
import json
from collections import Counter
from check import matchings,traversal


def census(n):
    m=2*n
    pieces=Counter()
    for a in matchings(n):
        for b in matchings(n):
            rooted=traversal(a,b)
            if rooted is None:continue
            pieces['M']+=1
            for t in range(0,m,2):
                p=rooted[t:]+rooted[:t]
                pieces['total']+=1
                hh=lambda j:(j+n)%m
                neg=lambda j:(-j)%m
                q=tuple(p[hh(i)] for i in range(m))
                c=tuple(hh(q[i]) for i in range(m))
                br=tuple((-p[-i%m])%m for i in range(m))
                pieces['B']+=br==p
                pieces['C']+=c==p
                pieces['BC']+=tuple((-c[-i%m])%m for i in range(m))==p
                pieces['LC']+=tuple(neg(u) for u in c)==p
                pieces['RC']+=tuple(c[-i%m] for i in range(m))==p
                pieces['T']+=all(p[p[i]]==i for i in range(m))
                pieces['CT']+=all(q[q[i]]==i for i in range(m))
                pieces['LT']+=all(p[p[i]]==neg(i) for i in range(m))
                pieces['CLT']+=all(q[q[i]]==neg(i) for i in range(m))
    v=pieces
    assert v['total']==n*v['M']
    assert v['RC']==v['LC']
    assert (v['total']+v['B']+v['C']+v['BC']+2*(v['LC']+v['T']+v['CT']+v['LT']+v['CLT']))%16==0
    return dict(n=n,**v)


if __name__=='__main__':
    print(json.dumps([census(n) for n in (3,5,7)],indent=2))
