"""Build and run individual sectors locally, leaving atomic restartable receipts."""
import argparse
import concurrent.futures
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "experiments/runs/projective42"
BUILD = Path("/tmp/lrwr_projective42")

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def atomic(path, data):
    tmp = path.with_suffix(f".{os.getpid()}.tmp")
    tmp.write_text(json.dumps(data, indent=2)+"\n")
    os.replace(tmp,path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("orders", nargs="+", type=int)
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--cap-mib", type=int, default=2048)
    parser.add_argument("--heights", type=int, nargs="*")
    parser.add_argument("--timeout", type=int, default=1800)
    args=parser.parse_args()
    src=Path(__file__).with_name(args.name+".cpp")
    flags=["-O3","-std=c++20","-march=native","-Wall","-Wextra"]
    hashes={src.name:digest(src)}
    if args.name in ("fold","block","joint","minimum","minpack","chain","smart","slices"):
        hashes["word.cpp"]=digest(src.with_name("word.cpp"))
    if args.name in ("block","joint","minimum","minpack","chain","smart","slices"):
        hashes["fold.cpp"]=digest(src.with_name("fold.cpp"))
    if args.name in ("minimum","minpack","chain","smart","slices"):
        for dep in ("merged.cpp","fast.hpp"):
            hashes[dep]=digest(src.with_name(dep))
    if args.name in ("minpack","chain","smart","slices"):
        hashes["minpack.hpp"]=digest(src.with_name("minpack.hpp"))
    if args.name=="smart":
        hashes["remaining.hpp"]=digest(src.with_name("remaining.hpp"))
    if args.name=="reference":
        dep=ROOT/"scripts/projective41/seams.cpp"
        hashes["projective41/seams.cpp"]=digest(dep)
    BUILD.mkdir(parents=True,exist_ok=True)
    binpath=BUILD/(args.name+"-"+digest(src)[:12]+"-"+str(os.getpid()))
    subprocess.run(["g++",*flags,str(src),"-o",str(binpath)],check=True)
    dest=DATA/args.name
    dest.mkdir(parents=True,exist_ok=True)
    signature=digest(binpath)
    provenance=dict(compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0], flags=flags,
           platform=platform.platform(),binary_sha256=signature,source_sha256=hashes)
    atomic(dest/("build-"+signature[:16]+".json"),provenance)
    atomic(dest/"provenance.json",provenance)
    def job(n,h):
        path=dest/f"n{n:02}h{h:02}.json"
        if path.exists():
            old=json.loads(path.read_text())
            if old.get("status")=="exact" and old["binary_sha256"]==signature:
                return
        cmd=["prlimit",f"--as={args.cap_mib*1024**2}",str(binpath),str(n),str(h)]
        started=datetime.now(timezone.utc).isoformat()
        try:
            proc=subprocess.run(cmd,text=True,capture_output=True,timeout=args.timeout)
        except subprocess.TimeoutExpired:
            row=dict(n=n,h=h,status="incomplete",error="time cap")
        else:
            if proc.returncode==0:
                row=json.loads(proc.stdout);row["status"]="exact"
            else:
                row=dict(n=n,h=h,status="incomplete",returncode=proc.returncode,error=proc.stderr[-400:])
        row.update(start_utc=started,cap_mib=args.cap_mib,timeout_seconds=args.timeout,binary_sha256=signature)
        atomic(path,row)
        print(n,h,row["status"],row.get("count"),row.get("seconds"),flush=True)
    for n in args.orders:
        hs=args.heights if args.heights is not None else list(range(n%2,n+1,2))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
            list(executor.map(lambda h:job(n,h),hs))
    binpath.unlink(missing_ok=True)

if __name__=="__main__":
    main()
