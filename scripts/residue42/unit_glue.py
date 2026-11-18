"""Build every small marked diagram from opened blocks and compare the exact sets."""
import json
from functools import cache
from itertools import combinations,product
from check import matchings,traversal


@cache
def exposed(n,holes):
    diagrams=[]
    for choice in combinations(range(n),holes):
        pieces=[(a+1,b) for a,b in zip((-1,)+choice,choice+(n,))]
        if any((b-a)%2 for a,b in pieces): continue
        for matches in product(*(matchings((b-a)//2) for a,b in pieces)):
            word=[-1]*n
            for (a,b),small in zip(pieces,matches):
                for i,j in enumerate(small): word[a+i]=a+j
            diagrams.append((word,choice))
    return diagrams


@cache
def blocks(n,left,right):
    # Side is 0 or 1, and the chosen lifts name the left and right attachments.
    systems=[]
    holes0=(left==0)+(right==0)
    for p0,p1 in product(exposed(n,holes0),exposed(n,2-holes0)):
        words=[p0[0],p1[0]]
        pending=[0]
        seen={0}
        while pending:
            i=pending.pop()
            for w in words:
                j=w[i]
                if j>=0 and j not in seen: seen.add(j); pending.append(j)
        if len(seen)!=n: continue
        holes=[p0[1],p1[1]]
        ends=(left*n+holes[left][0],right*n+holes[right][-1])
        edges=[(s*n+i,s*n+j) for s,w in enumerate(words) for i,j in enumerate(w) if j>i]
        systems.append((edges,ends))
    return systems


def glue(n,cuts):
    out=set()
    end=cuts[1:]+(cuts[0]+n,)
    sizes=[b-a for a,b in zip(cuts,end)]
    for first in (0,1):
        phase=first
        modes=[]
        for d in sizes:
            next_phase=phase^(d%2)
            modes.append(blocks(d,phase,next_phase))
            phase=next_phase
        assert phase==1-first
        for pieces in product(*modes):
            fst=[]; lst=[]
            diagram=[-1]*(2*n)
            def add(x,y):
                assert x!=y and diagram[x]==diagram[y]==-1
                diagram[x]=y; diagram[y]=x
            for cut,d,(edges,ends) in zip(cuts,sizes,pieces):
                def physical(x): return (cut+x%d+(x//d)*n)%(2*n)
                for x,y in edges: add(physical(x),physical(y))
                fst.append(physical(ends[0])); lst.append(physical(ends[1]))
            for i,x in enumerate(lst): add(x,fst[(i+1)%len(fst)])
            obj=tuple(diagram)
            assert obj not in out
            out.add(obj)
    return out


rows=[]
for n in (3,5,7,9,11):
    fixed=tuple((i+n)%(2*n) for i in range(2*n))
    lookup={}
    for a in matchings(n):
        if traversal(a,fixed) is None: continue
        lookup[a]=frozenset(c for c in range(n) if sum(not c<=a[i]<c+n for i in range(c,c+n))==1)
    subsets=pairs=0
    for r in range(2,n+1):
        for cuts in combinations(range(n),r):
            actual={a for a,marked in lookup.items() if marked.issuperset(cuts)}
            built=glue(n,cuts)
            assert actual==built,(n,cuts)
            subsets+=1; pairs+=len(actual)
    rows.append(dict(n=n,distinct_cut_sets=subsets,marked_diagrams=pairs))
print(json.dumps(rows,indent=2))
