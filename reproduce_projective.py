"""Replay an entire projective order with one isolated, resumable job per minimum height."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from time import monotonic

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/"scripts/projective42"))
from verify_release import local_open_values


def load(path): return json.loads(path.read_bytes())


def result_hash(body):
    return sha256(json.dumps({k:v for k,v in body.items() if k!="result_sha256"},sort_keys=True,separators=(",",":")).encode()).hexdigest()


def save(path,body):
    with tempfile.NamedTemporaryFile("w",encoding="utf8",dir=path.parent,delete=False) as tmp:
        json.dump(body,tmp,indent=2);tmp.write("\n");working=Path(tmp.name)
    working.replace(path)


def minimum_boundary(n):
    target=n if n%2 else n-1
    files=sorted((ROOT/"experiments/runs/projective42").glob("open??.json"))
    for f in files:
        rec=load(f)
        if rec["n"]>=target:
            opened=local_open_values(rec)
            break
    else: raise ValueError("The local ordinary open count required for this order is unavailable.")
    if n%2:
        H=[0]*(n+1)
        for i in range(1,n+1):
            H[i]=i*opened[i]-sum(opened[j]*H[i-j] for j in range(1,i))
        return 2*H[n]-n*opened[n],f
    return n*opened[n-1],f


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("directory",type=Path,help="scratch, per-height results and the final summary")
    ap.add_argument("--order",type=int,help="default: last full order in the supplied edition")
    ap.add_argument("--portable",action="store_true",help="use software bit extraction and deposit")
    args=ap.parse_args()
    terms={r["n"]:r for r in load(ROOT/"data/projective.json")}
    n=args.order if args.order is not None else max(terms)
    if n not in terms or n<3:ap.error("select a tabulated order of at least 3")
    folder=args.directory.resolve()
    folder.mkdir(parents=True,exist_ok=True)
    src=ROOT/"scripts/projective42"
    allowed={"minimum","smart","chain","minpack","slices"}
    recorded={}
    row=terms[n]
    if row.get("evidence"):
        mainrec=load(ROOT/row["evidence"])
        route=next((r for r in [mainrec]+mainrec["alternatives"] if "minimum" in r["method"]),None)
        if route:
            recorded={r["first_height"]:r for r in route["parts"]}
    native=subprocess.run(["g++","-march=native","-dM","-E","-x","c++","-"],input=b"",capture_output=True)
    bmi2=not args.portable and native.returncode==0 and b"#define __BMI2__ 1" in native.stdout
    flags=["-O3","-std=c++20"]+(["-march=native"] if bmi2 else ["-I",str(ROOT/"scripts/portable_bits")])
    compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0]
    config=dict(compiler=compiler,portable_requested=args.portable,bmi2=bmi2,arguments=flags,native_macros=sha256(native.stdout).hexdigest() if bmi2 else None)
    source_hash=sha256(json.dumps(config,sort_keys=True).encode())
    for path in sorted([*src.glob("*.cpp"),*src.glob("*.hpp"),src/"verify_release.py",Path(__file__),ROOT/"scripts/portable_bits/immintrin.h"]):
        source_hash.update(path.name.encode()+b"\0"+path.read_bytes())
    stamp=source_hash.hexdigest()
    direct={}
    for line in (ROOT/"experiments/runs/projective42/direct_minimum.jsonl").read_bytes().splitlines():
        v=json.loads(line)
        if v["n"]==n:direct=dict(zip(range(n%2,n+1,2),v["minimum_cut"]))
    compiled={};entries=[]
    with tempfile.TemporaryDirectory(dir=folder) as tmp:
        for h in range(n%2,n//3+1,2):
            ref=recorded.get(h);expected=ref["count"] if ref else direct.get(h)
            method=Path(ref["source"]).parent.name if ref else "smart"
            if h==n%2:method="ordinary boundary formula"
            elif method not in allowed:method="smart"
            f=folder/f"p{n:02}-min{h:02}.json"
            seen=load(f) if f.exists() else {}
            same=(seen.get("sources"),seen.get("method"),seen.get("n"),seen.get("h"))==(stamp,method,n,h)
            if h!=n%2 and same and seen.get("result_sha256")==result_hash(seen) and (expected is None or seen.get("count")==expected):
                entries.append(seen);continue
            start=monotonic();input_digest=None
            if h==n%2:
                value,source=minimum_boundary(n)
                input_digest=sha256(source.read_bytes()).hexdigest()
            else:
                if method not in compiled:
                    exe=Path(tmp)/method
                    subprocess.run(["g++",*flags,src/(method+".cpp"),"-o",exe],check=True)
                    compiled[method]=exe
                raw=json.loads(subprocess.check_output([compiled[method],str(n),str(h)]))
                if (raw["n"],raw["h"])!=(n,h): raise ValueError("sector identity")
                value=raw["count"]
            if expected is not None and value!=expected:raise ValueError(("minimum sector disagreement",n,h,value,expected))
            entry=dict(n=n,h=h,count=value,method=method,sources=stamp,input_sha256=input_digest,seconds=round(monotonic()-start,4))
            entry["result_sha256"]=result_hash(entry)
            save(f,entry);entries.append(entry)
            print(n,h,value,flush=True)
    total=sum(x["count"] for x in entries)
    if total!=terms[n]["P"]:raise ValueError(("full projective replay",n,total,terms[n]["P"]))
    save(folder/f"p{n:02}-total.json",dict(n=n,P=total,build=config,sectors=entries))
    print(f"P({n}) = {total}",flush=True)


if __name__=="__main__":main()
