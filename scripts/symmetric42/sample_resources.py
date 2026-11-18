"""Observe actual concurrent Linux resource use of the marked sector processes."""
from argparse import ArgumentParser
from datetime import datetime,timezone
import json
from pathlib import Path
import shutil
import time
from campaign import atomic


def clocks():
    return datetime.now(timezone.utc).isoformat()


def sample(roots):
    workers=[]
    for p in Path("/proc").glob("[0-9]*"):
        try:
            args=(p/"cmdline").read_bytes().split(b"\0")
            if len(args)!=7 or not args[0].endswith(b"/spill"):continue
            work=Path(args[3].decode()).resolve()
            if not any(work.is_relative_to(r) for r in roots):continue
            stat={x.split(":")[0]:int(x.split()[1]) for x in (p/"status").read_text().splitlines()
                  if x.startswith(("VmRSS:","VmHWM:"))}
            workers.append((int(args[1]),int(args[2]),stat["VmRSS"],stat["VmHWM"],work))
        except (FileNotFoundError,ProcessLookupError,PermissionError,KeyError):continue
    return workers


def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("output",type=Path)
    p.add_argument("--scratch-root",type=Path,action="append",required=True)
    p.add_argument("--backing-volume",type=Path,required=True)
    p.add_argument("--until-file",type=Path,required=True)
    p.add_argument("--seconds",type=int,default=15)
    a=p.parse_args()
    if a.seconds<=0:p.error("positive sample interval required")
    roots=[r.resolve() for r in a.scratch_root]
    previous=json.loads(a.output.read_bytes()) if a.output.exists() else {}
    start=previous.get("start_utc",clocks());peaks=previous.get("max_process_VmHWM_KiB",{})
    max_rss=previous.get("observed_max_combined_resident_KiB",0)
    max_space=previous.get("observed_max_active_layer_bytes",0)
    max_files=previous.get("observed_max_all_retained_layer_bytes",0)
    samples=previous.get("samples",0);min_free=previous.get("observed_minimum_backing_volume_free_bytes")
    while True:
        workers=sample(roots);samples+=1
        for n,h,rss,hwm,_ in workers:
            key=f"n{n}h{h:02}"
            peaks[key]=max(peaks.get(key,0),hwm)
        files={}
        for root in roots:
            for f in root.rglob("*.bin"):
                try:
                    s=f.stat()
                    files[s.st_dev,s.st_ino]=s.st_size
                except FileNotFoundError:pass
        active_size=0
        for work in {r[-1] for r in workers}:
            for f in work.glob("*.bin"):
                try:active_size+=f.stat().st_size
                except FileNotFoundError:pass
        max_space=max(max_space,active_size)
        max_files=max(max_files,sum(files.values()))
        max_rss=max(max_rss,sum(w[2] for w in workers))
        free=shutil.disk_usage(a.backing_volume).free
        min_free=free if min_free is None else min(min_free,free)
        atomic(a.output,dict(start_utc=start,last_utc=clocks(),interval_seconds=a.seconds,samples=samples,
                   observed_max_combined_resident_KiB=max_rss,max_process_VmHWM_KiB=peaks,
                   observed_max_active_layer_bytes=max_space,observed_max_all_retained_layer_bytes=max_files,
                   observed_minimum_backing_volume_free_bytes=min_free))
        if a.until_file.exists():break
        time.sleep(a.seconds)


if __name__=="__main__":main()
