"""Leading marked-diagonal coefficients by uniformly rotating adjacent cups and caps."""
from argparse import ArgumentParser
from collections import Counter
from fractions import Fraction
from functools import lru_cache
from math import factorial,lcm
from pathlib import Path
from campaign import atomic,digest
import json


def partners(word,k):
    n=2*k+1
    mates=[-1]*n;stack=[]
    for i in range(1,n):
        if (word>>(i-1))&1:stack.append(i)
        else:
            a=stack.pop();mates[a]=i;mates[i]=a
    assert not stack
    return mates


@lru_cache(maxsize=50000)
def grow(k,word):
    # A marked port anchors the cyclic order. Insert the matched pair in
    # each of the 2k+1 gaps, counting equal outcomes with multiplicity.
    out=Counter()
    for i in range(2*k+1):
        target=(word&((1<<i)-1))|(1<<i)|((word>>i)<<(i+2))
        out[target]+=1
    return tuple(out.items())


@lru_cache(maxsize=50000)
def shrink(k,word):
    n=2*k+1;mate=partners(word,k);out=Counter()
    for a in range(1,n-1):
        b=a+1;p,q=mate[a],mate[b]
        if p==b:continue
        lo,hi=sorted((p,q))
        changed=(word|(1<<(lo-1)))&~(1<<(hi-1))
        target=(changed&((1<<(a-1))-1))|((changed>>(a+1))<<(a-1))
        out[target]+=1
    # Removing a pair incident with the marked port moves the mark.
    for other in (1,n-1):
        root=mate[other];ports=[(root+i)%n for i in range(1,n) if (root+i)%n not in (0,other)]
        pos={x:i for i,x in enumerate(ports)}
        target=sum(1<<i for i,x in enumerate(ports) if pos[mate[x]]>i)
        out[target]+=1
    return tuple(out.items())


def coefficient(j):
    scale=lcm(*range(1,2*j+2,2));cur={(0,0):{0:1}};peak=1
    for _ in range(2*j):
        nxt={}
        for (left,right),weights in cur.items():
            difference=1+2*(right-left)
            length=abs(difference);k=length//2
            for l,r,d2 in ((left+1,right,difference-2),(left,right+1,difference+2)):
                if max(l,r)>j:continue
                dest=nxt.setdefault((l,r),Counter())
                for word,p in weights.items():
                    scaled=p*(scale//length)
                    if abs(d2)==length:
                        assert length==1;dest[0]+=scaled;continue
                    transitions=grow(k,word) if abs(d2)>length else shrink(k,word)
                    for target,mult in transitions:dest[target]+=scaled*mult
        cur=nxt;peak=max(peak,sum(len(v) for v in cur.values()))
    return Fraction(cur[j,j][0],scale**(2*j)*factorial(2*j)),peak


if __name__=="__main__":
    ap=ArgumentParser(description=__doc__)
    ap.add_argument("folder",type=Path);ap.add_argument("max_j",type=int)
    args=ap.parse_args()
    reference=json.loads((args.folder/"fixed_check.json").read_bytes())
    for j in range(args.max_j+1):
        c,peak=coefficient(j)
        if str(j) in reference:assert str(c)==reference[str(j)]["common_leading_coefficient"]
        row=dict(j=j,degree=2*j,leading_coefficient=str(c),peak_marked_matchings_in_one_layer=peak,
                 agrees_with_full_rational_function=str(j) in reference,source_sha256=digest(Path(__file__)))
        atomic(args.folder/"leading"/f"j{j:02}.json",row);print(j,c,peak,flush=True)
