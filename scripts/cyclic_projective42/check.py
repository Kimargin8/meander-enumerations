"""Odd projective fixed points. Compare the quotient formula with enumeration on the covering disk."""
from argparse import ArgumentParser
from datetime import datetime, timezone
from functools import lru_cache
from hashlib import sha256
import json
from math import comb, gcd
from pathlib import Path
import subprocess
import tempfile
from time import perf_counter

HERE = Path(__file__).resolve().parent


@lru_cache(None)
def planar(n):
    if not n: return ((),)
    result=[]
    for inside in range(n):
        b=2*inside+1
        for a in planar(inside):
            for c in planar(n-1-inside):
                result.append((b,)+tuple(x+1 for x in a)+(0,)+tuple(b+1+x for x in c))
    return tuple(result)


def count_rotations(n):
    orders=[d for d in range(2,2*n+1) if 2*n%d==0]
    counts={d:0 for d in orders}
    counted=0
    for a in planar(n):
        x=length=0
        while not length or x:
            x=(a[x]+n)%(2*n)
            length+=1
        if length!=n: continue
        counted+=1
        for d in orders:
            step=2*n//d
            counts[d]+=all(a[(i+step)%(2*n)]==(a[i]+step)%(2*n) for i in range(2*n))
    return counted,counts


def main():
    p = ArgumentParser(description=__doc__)
    p.add_argument("terms", type=Path, help="JSON object with integer indices and exact P values (or a b-file)")
    p.add_argument("out", type=Path)
    args = p.parse_args()
    if args.terms.suffix == ".json":
        data = json.loads(args.terms.read_bytes())
        data = data.get("A006663", data)
        values = {int(i):int(v) for i,v in data.get("terms",data).items()}
    else:
        values = dict(tuple(map(int,s.split())) for s in args.terms.read_text().splitlines() if s.strip() and not s.startswith("#"))
    source = HERE/"fixed.cpp"
    flags = ["-std=c++17","-O3","-Wall","-Wextra"]
    result = dict(utc=datetime.now(timezone.utc).isoformat(),cpp_sha256=sha256(source.read_bytes()).hexdigest(),
                  terms_sha256=sha256(args.terms.read_bytes()).hexdigest(),flags=flags,
                  compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],rows=[])
    with tempfile.TemporaryDirectory() as temp:
        exe = Path(temp)/"fixed"
        subprocess.run(["g++",*flags,str(source),"-o",str(exe)],check=True)
        even_base={}
        for n in range(3,48):
            records=[]
            small=count_rotations(n) if n<=10 else None
            if small:
                assert small[0]==values[n] and all(v==0 for d,v in small[1].items() if d%2==0)
            for d in range(3,n+1,2):
                if n%d: continue
                m=n//d
                if m not in values: raise ValueError(f"missing P({m})")
                if m%2: prediction=(m+1)*values[m]
                else:
                    if m not in even_base:
                        x,y,w,f=map(int,subprocess.check_output([exe,str(3*m),"3"],text=True).split())
                        assert (x,y,w)==(3*m,3,comb(2*m,m))
                        even_base[m]=f
                    prediction=(1+m//2)*values[m]
                    assert even_base[m]==prediction
                row=dict(d=d,quotient=m,phi=sum(gcd(j,d)==1 for j in range(1,d+1)),fixed=prediction)
                if small: assert small[1][d]==prediction
                if m<=13 or m%2==0 and d!=3:
                    start=perf_counter()
                    a,b,words,measured=map(int,subprocess.check_output([exe,str(n),str(d)],text=True).split())
                    assert (a,b,words,measured)==(n,d,comb(2*m,m),prediction)
                    row.update(direct_words=words,direct_fixed=measured,elapsed_seconds=perf_counter()-start)
                records.append(row)
            residue=-sum(r["phi"]*r["fixed"] for r in records)%(2*n)
            if n in values and values[n]%(2*n)!=residue: raise ValueError(f"Burnside failure for {n}")
            result["rows"].append(dict(n=n,modulus=2*n,P_mod=residue,reference=values.get(n),rotations=records,
                                       brute_fixed=small[1] if small else None))
        result["even_quotient_volumes"]=even_base
    args.out.parent.mkdir(exist_ok=True,parents=True)
    args.out.write_text(json.dumps(result,indent=2)+"\n")
    print([row for row in result["rows"] if row["n"]==39][0])


if __name__=="__main__": main()
