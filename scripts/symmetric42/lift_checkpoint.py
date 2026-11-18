"""Reconstruct a higher-horizon layer and hash both inputs and the complete output."""
from argparse import ArgumentParser
from datetime import datetime,timezone
from hashlib import sha256
import json
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile
import time
from campaign import FLAGS,ROOT,atomic,digest
from horizon_evidence import signature,verify_seed


def fingerprint(folder):
    marker=folder/"progress.txt"
    meta=list(map(int,marker.read_text().split()))
    raw=folder/f"layer{meta[2]}.bin"
    check=sha256()
    with raw.open("rb") as f:
        for block in iter(lambda:f.read(8<<20),b""):check.update(block)
    return dict(metadata=meta,metadata_sha256=digest(marker),size=raw.stat().st_size,
                layer_sha256=check.hexdigest())


def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("n",type=int);p.add_argument("h",type=int);p.add_argument("old_n",type=int);p.add_argument("t",type=int)
    for field in ("shorter","complement","target","receipt"):p.add_argument(field,type=Path)
    p.add_argument("--mib",type=int,default=2500);p.add_argument("--disk-mib",type=int,default=6500)
    p.add_argument("--backing-volume",type=Path);p.add_argument("--reserve-mib",type=int,default=15360)
    a=p.parse_args();here=Path(__file__).resolve().parent
    files={f:digest(here/f) for f in ("fold.cpp","compact.cpp","spill.cpp","horizon.cpp")}
    def limits():
        limit=a.mib<<20
        resource.setrlimit(resource.RLIMIT_AS,(limit,limit));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory() as build:
        exe=Path(build)/"horizon"
        subprocess.run(["g++",*FLAGS,here/"horizon.cpp","-o",exe],check=True)
        base=[exe,str(a.n),str(a.h),str(a.old_n),str(a.t)]
        subprocess.run([*base,"--exception-disk",a.complement,str(a.disk_mib)],check=True,preexec_fn=limits)
        old,extra=map(fingerprint,(a.shorter,a.complement))
        a.target.mkdir(parents=True,exist_ok=True)
        needed=old["size"]+extra["size"]+(a.reserve_mib<<20)
        for path in (a.target,a.backing_volume):
            if path and shutil.disk_usage(path).free<needed:raise RuntimeError("insufficient room for the merged checkpoint")
        start=time.monotonic()
        if not (a.target/"progress.txt").exists():
            subprocess.run([*base,"--merge-checkpoint",a.shorter,a.complement,a.target],check=True,preexec_fn=limits)
        merged=fingerprint(a.target)
        j=(a.n-a.h)//2
        verification=ROOT/"experiments/runs/symmetric42/horizon_check.json"
        seed=dict(shorter=old,complement=extra,output=merged,deficit=j,
                  shorter_opening_budget=a.old_n-j,left_closing_threshold=a.t-(a.old_n-j),
                  right_closing_threshold=a.t-1-(a.old_n-j),source_files=files,sources_sha256=signature(files),
                  compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
                  compiler_flags=FLAGS,binary_sha256=digest(exe),driver_sha256=digest(Path(__file__)),
                  comparison_sha256=digest(verification),merge_and_output_hash_seconds=time.monotonic()-start,
                  utc=datetime.now(timezone.utc).isoformat())
        assert verify_seed(seed)
        atomic(a.target/"seed.json",seed);atomic(a.receipt,seed)
        print(a.n,a.h,a.t,merged["metadata"][3],merged["layer_sha256"],flush=True)


if __name__=="__main__":main()
