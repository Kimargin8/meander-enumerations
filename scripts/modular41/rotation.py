"""Connected ordered meanders modulo odd rotational primes from smaller face-weighted counts."""
from functools import lru_cache
from hashlib import sha256
from itertools import combinations
import json
from math import comb
from pathlib import Path
import platform
import sys


@lru_cache(None)
def advance(u, pairing, top_open, bottom_open):
    """Pair the dangling ends of each already traced path; retain stack order."""
    size=len(pairing)
    top=list(range(u))
    bottom=list(range(u,size))
    if (not top_open and not top) or (not bottom_open and not bottom):
        return None
    mates=list(pairing)+[-1,-1]
    if top_open and bottom_open:
        top.append(size)
        bottom.append(size+1)
        mates[size],mates[size+1]=size+1,size
    elif top_open != bottom_open:
        opened,closed=(top,bottom) if top_open else (bottom,top)
        partner=mates[closed.pop()]
        opened.append(size)
        mates[partner],mates[size]=size,partner
    else:
        x,y=top.pop(),bottom.pop()
        a,b=mates[x],mates[y]
        if a==y:
            return "closed" if size==2 else None
        mates[a],mates[b]=b,a
    order=top+bottom
    renumber={old:new for new,old in enumerate(order)}
    return len(top),tuple(renumber[mates[i]] for i in order)


def face_sum(m):
    # Each hemisphere has one outer face of even depth. Opening an arc
    # after an odd number of vertices creates a further even-depth face.
    start=(0,())
    dp={start:(1,1,1,1)}
    done=None
    stage_sizes=[]
    for step in range(2*m):
        rem=2*m-step-1
        target={}
        for (u,pair),(count,A,B,AB) in dp.items():
            for top in (0,1):
                for bot in (0,1):
                    unext=u+2*top-1
                    bnext=len(pair)-u+2*bot-1
                    if min(unext,bnext)<0 or max(unext,bnext)>rem:
                        continue
                    dest=advance(u,pair,top,bot)
                    if dest is None or (dest=="closed" and rem):
                        continue
                    da=top*(step%2)
                    db=bot*(step%2)
                    N,X,Y,XY=target.get(dest,(0,0,0,0))
                    target[dest]=(N+count,X+A+da*count,Y+B+db*count,
                                  XY+AB+da*B+db*A+da*db*count)
        if rem:
            dp=target
            stage_sizes.append(len(dp))
        else:
            done=target["closed"]
    count,A,B,AB=done
    assert A==B
    return dict(m=m,connected=count,sum_even_upper=A,sum_even_lower=B,
                sum_product=AB,fixed=(m+1)*(A+B)-2*AB,peak_states=max(stage_sizes))


def fixed_matchings(m,p):
    """Direct generation on the full circle of 2mp vertices by edge orbits."""
    n=2*m*p
    stride=2*m
    mates=[-1]*n
    existing=[]
    def cross(x,y):
        a,b=x
        c,d=y
        return a<c<b<d or c<a<d<b
    def rec():
        try:
            x=mates.index(-1)
        except ValueError:
            yield tuple(mates)
            return
        for y in range(x+1,n):
            if mates[y]>=0:
                continue
            orbit=[tuple(sorted(((x+j*stride)%n,(y+j*stride)%n))) for j in range(p)]
            ends=[w for edge in orbit for w in edge]
            if len(set(ends))!=2*p or any(mates[w]>=0 for w in ends):
                continue
            if any(cross(a,b) for a,b in combinations(orbit,2)) or any(cross(a,b) for a in orbit for b in existing):
                continue
            for a,b in orbit:
                mates[a],mates[b]=b,a
            existing.extend(orbit)
            yield from rec()
            del existing[-p:]
            for a,b in orbit:
                mates[a]=mates[b]=-1
    return list(rec())


def direct(m,p):
    fixed=fixed_matchings(m,p)
    assert len(fixed)==comb(2*m,m)
    connected=0
    for up in fixed:
        for down in fixed:
            node=up[down[0]]
            pairs=1
            while node:
                node=up[down[node]]
                pairs+=1
            connected+=pairs==m*p
    return dict(m=m,p=p,matchings=len(fixed),ordered_pairs=len(fixed)**2,connected=connected)


def main():
    root=Path(__file__).resolve().parents[2]
    table=root/"research/external/meander-tree-bitmap_c6904a0e2970_12/oeis/b005315.txt"
    known=dict(map(int,line.split()) for line in table.read_text().splitlines() if line.strip() and not line.startswith("#"))
    moments={m:face_sum(m) for m in range(1,14)}
    assert all(x["connected"]==known[m] for m,x in moments.items())
    finite=[direct(m,p) for m in range(1,6) for p in (3,5)]
    finite += [direct(3,13)]
    assert all(r["connected"]==moments[r["m"]]["fixed"] for r in finite)
    congruences=[]
    for n,expected in sorted(known.items()):
        for p in (3,5,7,11,13,17,19,23,29,31,37):
            if n and n%p==0 and n//p in moments:
                r=moments[n//p]["fixed"]%p
                assert expected%p==r
                congruences.append(dict(n=n,p=p,residue=r))
    residues={2:0,3:moments[13]["fixed"]%3,13:moments[3]["fixed"]%13}
    cls=[x for x in range(78) if all(x%p==v for p,v in residues.items())]
    assert len(cls)==1
    doc=dict(object="A005315, ordered pairs, fixed 2n labelled boundary points",
             n39_residues=residues,n39_mod78=cls[0],face_transfer=list(moments.values()),
             direct_permutation_orbits=finite,published_prime_checks=congruences,
             code_sha256=sha256(Path(__file__).read_bytes()).hexdigest(),
             known_table_sha256=sha256(table.read_bytes()).hexdigest(),python=platform.python_version())
    with Path(sys.argv[1]).open("x",encoding="utf8") as f:
        json.dump(doc,f,indent=2)
        f.write("\n")
    print(moments[3],moments[13],cls,len(congruences))


if __name__=="__main__":
    main()
