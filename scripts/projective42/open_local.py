"""Certify the standard plane input table locally with a bounded vacuum run."""
import argparse
from datetime import datetime, timezone
import json
import os
import platform
import subprocess
from run import ROOT, DATA, BUILD, atomic, digest


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument("n",type=int)
    cli.add_argument("--cap-mib",type=int,default=2048)
    cli.add_argument("--timeout",type=int,default=7200)
    args=cli.parse_args()
    src=ROOT/"scripts/projective42"
    files=["open.cpp","fold.cpp","word.cpp"]
    flags=["-O3","-std=c++20","-march=native"]
    BUILD.mkdir(exist_ok=True,parents=True)
    exe=BUILD/f"open-{os.getpid()}"
    subprocess.run(["g++",*flags,str(src/"open.cpp"),"-o",str(exe)],check=True)
    sig=digest(exe)
    dest=DATA/f"open{args.n:02}.json"
    try:
        if dest.exists() and dest.stat().st_size and json.loads(dest.read_text()).get("binary_sha256")==sig:
            return
        start=datetime.now(timezone.utc).isoformat()
        result=subprocess.run(["prlimit",f"--as={args.cap_mib*1024**2}",str(exe),str(args.n)],
                              capture_output=True,text=True,timeout=args.timeout)
        if result.returncode:
            atomic(DATA/f"open{args.n:02}.incomplete.json",dict(n=args.n,status="incomplete",
                   error=result.stderr[-400:],returncode=result.returncode,cap_mib=args.cap_mib,start_utc=start))
            return
        row=json.loads(result.stdout)
        if len(row["open"])!=args.n+1 or row["open"][-1]!=row["count"]:raise ValueError("open count")
        row.update(status="exact",algorithm="vacuum planar open transfer",cap_mib=args.cap_mib,
                   timeout_seconds=args.timeout,start_utc=start,binary_sha256=sig,
                   source_sha256={f:digest(src/f) for f in files},flags=flags,
                   compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
                   platform=platform.platform())
        atomic(dest,row)
        print(args.n,row["count"],row["rss_kib"],row["seconds"])
    finally:
        exe.unlink(missing_ok=True)


if __name__=="__main__":
    main()
