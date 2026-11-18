"""Exact polynomial certificate for the fixed-winding conjectures in mapFolding 0.40.1."""
import argparse
import ast
from datetime import datetime, timezone
from functools import cache
from hashlib import sha256
import json
from math import gcd
from pathlib import Path
import platform
import subprocess
import tempfile
from zipfile import ZipFile


def multiply(a,b):
    c=[0]*(len(a)+len(b)-1)
    for i,x in enumerate(a):
        for j,y in enumerate(b): c[i+j]+=x*y
    return c


def divide(a,b):
    a=a[:]
    q=[0]*(len(a)-len(b)+1)
    for i in reversed(range(len(q))):
        q[i],rem=divmod(a[i+len(b)-1],b[-1])
        if rem: raise ValueError("nonintegral quotient")
        for k,x in enumerate(b): a[i+k]-=q[i]*x
    if any(a): raise ValueError("nonzero remainder")
    return q


@cache
def phi(s):
    v=[-1]+[0]*(s-1)+[1]
    for d in range(1,s):
        if s%d==0: v=divide(v,phi(d))
    return v


def denominator(j):
    q=[1]
    for m in range(1,2*j+1):
        k=m//gcd(m,2)
        if k>j: continue
        power=(2*j-1) if k==1 else j+1-k
        factor=[1,-1] if m==1 else phi(m)
        for _ in range(power): q=multiply(q,factor)
    return q


def extract(wheel):
    name="mapFolding/oeis/A400429.py"
    with ZipFile(wheel) as z: code=z.read(name)
    candidates={}
    for f in ast.parse(code).body:
        if not isinstance(f,ast.FunctionDef) or not f.name.startswith("A005315of"): continue
        suffix=f.name.removeprefix("A005315of")
        j=int(suffix) if suffix.isdigit() else -1
        if j<3: continue
        assert ast.literal_eval(f.args.defaults[-1])==j
        terms={i.target.id:ast.literal_eval(i.value) for i in f.body if isinstance(i,ast.AnnAssign)
               and isinstance(i.target,ast.Name) and i.target.id in ("numerator","denominator")}
        assert len(terms)==2 and terms["denominator"][0]==1
        result=next(i for i in f.body if isinstance(i,ast.Return))
        assert ast.unparse(result.value).startswith("2 * crunchCoefficients(x - 2 * ")
        assert not any(isinstance(i,ast.If) for i in f.body)
        candidates[j]=terms
    if set(candidates)!=set(range(3,12)): raise ValueError(f"unexpected functions {list(candidates)}")
    return dict(reference="Hunter Hogan, mapFolding 0.40.1, A005315of3 through A005315of11 at default offsets",
                url="https://pypi.org/project/mapFolding/0.40.1/",member=name,
                wheel_sha256=sha256(wheel.read_bytes()).hexdigest(),member_sha256=sha256(code).hexdigest(),
                shift="multiply numerator by 2*z^(2*j)",candidates=candidates)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out",type=Path)
    parser.add_argument("deficit",type=int,nargs="+")
    parser.add_argument("--wheel",type=Path)
    parser.add_argument("--candidates",type=Path)
    parser.add_argument("--extra",type=int,default=5)
    parser.add_argument("--memory-mib",type=int,default=6144)
    parser.add_argument("--arch",action="store_true",help="independent bounded arch transfer")
    args=parser.parse_args()
    args.out.mkdir(exist_ok=True,parents=True)
    cand=extract(args.wheel) if args.wheel else json.loads(args.candidates.read_bytes())
    (args.out/"candidates.json").write_text(json.dumps(cand,indent=2)+"\n")
    src=Path(__file__).with_name("arch.cpp" if args.arch else "diagonal.cpp")
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"diagonal"
        flags=["-std=c++17","-O3","-Wall","-Wextra"]
        subprocess.run(["g++",*flags,str(src),"-o",str(exe)],check=True)
        for j in args.deficit:
            entry=cand["candidates"].get(j,cand["candidates"].get(str(j)))
            q=denominator(j)
            bound=len(q)-1+4*j
            if entry:
                num=[0]*(2*j)+[2*a for a in entry["numerator"]]
                den=list(entry["denominator"])
                factor=divide(q,den)
                candidate_num=multiply(num,factor)
                bound=max(bound,len(candidate_num)-1)
            measure=Path(tmp)/"measure"
            p=subprocess.run(["/usr/bin/time","-f","%e %M","-o",str(measure),
                "prlimit",f"--as={args.memory_mib*2**20}",exe,str(j),str(bound+args.extra)],
                text=True,capture_output=True)
            if p.returncode: raise RuntimeError(f"j={j}: code {p.returncode}: {p.stderr}")
            values=[0]
            for line in p.stdout.splitlines():
                n,value=map(int,line.split())
                if n!=len(values): raise ValueError("missing coefficient")
                values.append(value)
            actual=multiply(values,q)[:len(values)]
            if len(actual)!=bound+args.extra+1: raise ValueError("incomplete run")
            if any(actual[bound+1:]): raise ValueError("coefficient past the proven bound")
            if entry:
                for n,value in enumerate(actual):
                    prediction=candidate_num[n] if n<len(candidate_num) else 0
                    if value!=prediction: raise ValueError(f"discrepancy at j={j}, n={n}")
            reduced_num=actual[:bound+1]
            while reduced_num and not reduced_num[-1]: reduced_num.pop()
            reduced_den=q[:]
            for m in range(1,2*j+1):
                cyclo=[1,-1] if m==1 else phi(m)
                while True:
                    try: quotient=divide(reduced_den,cyclo)
                    except ValueError: break
                    try: reduced=divide(reduced_num,cyclo)
                    except ValueError: break
                    reduced_num,reduced_den=reduced,quotient
            sec,rss=measure.read_text().split()
            row=dict(j=j,numerator_degree_bound=bound,common_denominator_degree=len(q)-1,
                    checked_through=len(values)-1,candidate_denominator_degree=len(den)-1 if entry else None,
                    states=int(p.stderr.split()[-1]),seconds=float(sec),peak_KiB=int(rss),
                    utc=datetime.now(timezone.utc).isoformat(),values=values,
                    common_denominator=q,candidate_multiplier=factor if entry else None,
                    reduced_numerator=reduced_num,reduced_denominator=reduced_den,
                    transfer=src.name,arithmetic_sha256=sha256(src.with_name("positive.hpp").read_bytes()).hexdigest(),
                    source_sha256=sha256(src.read_bytes()).hexdigest(),driver_sha256=sha256(Path(__file__).read_bytes()).hexdigest(),
                    compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],flags=flags,
                    platform=platform.platform(),status="published candidate certified" if entry else "generating function reconstructed")
            (args.out/f"j{j:02}.json").write_text(json.dumps(row,indent=2)+"\n")
            print(j,bound,row["states"],sec,rss,flush=True)


if __name__=="__main__": main()
