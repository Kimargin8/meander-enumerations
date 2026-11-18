"""Independent all-order rotational stabilizer audit for labelled projective meanders."""
import json
from collections import Counter
from math import gcd
from pathlib import Path
from check import matchings, shift_matching, traversal


def divisors(n):
    return [d for d in range(1,n+1) if n%d==0]


def phi(n):
    return sum(gcd(i,n)==1 for i in range(1,n+1))


def prediction(n,d,p):
    if d%2==0: return 0
    m=n//d
    return p[m]*((m+1) if m%2 else (m//2+1))


root=Path(__file__).resolve().parents[2]
runs=root/"experiments/runs"
p={}
small=[]
for n in range(1,14):
    h=tuple((i+n)%(2*n) for i in range(2*n))
    orders=divisors(2*n)
    counts=Counter()
    for a in matchings(n):
        if traversal(a,h) is None: continue
        counts[1]+=1
        for d in orders[1:]:
            counts[d]+=a==shift_matching(a,2*n//d)
    p[n]=counts[1]
    if n>2:
        for d in orders[1:]:
            assert counts[d]==prediction(n,d,p),(n,d,counts[d],prediction(n,d,p))
    small.append(dict(n=n,fixed={d:counts[d] for d in orders}))

def record(n,count):
    assert p.setdefault(n,count)==count,(n,count,p[n])


for name in ["small","large","n29","n30"]:
    f=runs/"projective41"/(name+".jsonl")
    for line in f.read_text().splitlines():
        entry=json.loads(line)
        record(entry["n"],entry["count"])
heights={}
for folder in ("reference","word","fold","block"):
    for path in (runs/"projective42"/folder).glob("n??h??.json"):
        v=json.loads(path.read_bytes())
        if v.get("status")=="exact":
            assert heights.setdefault((v["n"],v["h"]),v["count"])==v["count"]
for n in range(31,46):
    hs=list(range(n%2,n+1,2))
    if all((n,h) in heights for h in hs):
        record(n,sum(heights[n,h] for h in hs))
large=[]
for n in sorted(p):
    if n<=2:continue
    total=p[n]+sum(phi(d)*prediction(n,d,p) for d in divisors(2*n)[1:])
    assert total%(2*n)==0,(n,total%(2*n))
    large.append(dict(n=n,P=p[n],rotation_orbits=total//(2*n)))
print(json.dumps(dict(direct=small,congruences=large),indent=2))
