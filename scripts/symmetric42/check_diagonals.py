"""Check exact pole orders and high sectors from the marked-diagonal certificates."""
from argparse import ArgumentParser
from fractions import Fraction
from math import factorial,lcm
from pathlib import Path
import json
from campaign import atomic,digest
from fixed_deficit import coefficients,cyclotomic,multiply

def remainder(poly,divisor):
    v=list(poly)
    for high in range(len(v)-1,len(divisor)-2,-1):
        q=v[high]
        for k,a in enumerate(divisor):
            v[high+1-len(divisor)+k]-=q*a
    v=v[:len(divisor)-1]
    while v and not v[-1]:v.pop()
    return v

def audit(folder,deep):
    rows={}
    for path in sorted(folder.glob("fixed[0-9]*.json")):
        rec=json.loads(path.read_bytes());j=rec["j"]
        num=rec["numerator_lowest_power_first"]
        exponents={1:2*j+1,**{2*k+1:j+1-k for k in range(1,j+1)}}
        q=[1];remainders={};leading_den=factorial(2*j)
        for m,e in exponents.items():
            phi=cyclotomic(m)
            rem=remainder(num,phi)
            assert rem, ("cancelled factor",j,m)
            remainders[m]=rem
            factor=phi if m>1 else [1,-1]
            for _ in range(e):q=multiply(q,factor)
            if m>1:leading_den*=sum(phi)**e
        assert q==rec["denominator_lowest_power_first"] and len(num)-1<=len(q)+4*j
        last=max(61,rec["coefficients_calculated_through_n"])
        values=[]
        for n in range(last+1):
            values.append((num[n] if n<len(num) else 0)
                          -sum(q[k]*values[n-k] for k in range(1,min(n+1,len(q)))))
        if deep:
            actual,peak=coefficients(j,last)
            assert values==actual and peak==rec["peak_states"]
        tests=0
        for method in ("sectors","compact","spill","fifo","fifo_spill"):
            for raw in (folder/method).glob("n*/h*.json"):
                v=json.loads(raw.read_bytes());n=v["n"]
                if v["status"]=="exact" and n-v["h"]==2*j and n<=last:
                    assert values[n]==v["value"]
                    tests+=1
        rows[j]=dict(period=lcm(*exponents),degree=2*j,numerator_degree=len(num)-1,
                     denominator_degree=len(q)-1,numerator_mod_cyclotomic=remainders,
                     common_leading_coefficient=str(Fraction(sum(num),leading_den)),
                     matching_completed_sector_records=tests,recomputed_queue=deep,
                     certificate_file=path.name,certificate_sha256=digest(path))
    return rows

if __name__=="__main__":
    ap=ArgumentParser(description=__doc__)
    ap.add_argument("folder",type=Path);ap.add_argument("output",type=Path)
    ap.add_argument("--deep",action="store_true")
    args=ap.parse_args()
    atomic(args.output,audit(args.folder,args.deep))
