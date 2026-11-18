"""Run restartable streaming height sectors with bounded RAM and scratch storage (Linux)."""
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
from campaign import FLAGS, ROOT, atomic, digest


def main():
    ap=ArgumentParser(description=__doc__)
    ap.add_argument("n",type=int)
    ap.add_argument("heights",type=int,nargs="+")
    ap.add_argument("--scratch",type=Path,required=True)
    ap.add_argument("--physical-volume",type=Path,help="check free capacity on a WSL backing volume as well")
    ap.add_argument("--reserve-mib",type=int,default=2048,help="minimum free space kept on both volumes")
    ap.add_argument("--mib",type=int,required=True)
    ap.add_argument("--disk-mib",type=int,required=True)
    ap.add_argument("--seconds",type=int,default=3600)
    ap.add_argument("--results",type=Path,default=ROOT/"experiments/runs/symmetric42/spill")
    ap.add_argument("--stop-layer",type=int,default=100)
    ap.add_argument("--fifo",action="store_true",help="independent transfer grouped by completed left opening count")
    args=ap.parse_args()
    if not 1<=args.n<=61 or any(h<0 or h>args.n or (args.n-h)%2 for h in args.heights):
        ap.error("invalid sector")
    if min(args.mib,args.disk_mib,args.seconds)<=0:ap.error("positive limits required")
    source=Path(__file__).with_name("fifo_external.cpp" if args.fifo else "spill.cpp")
    names=["fold.cpp","compact.cpp","spill.cpp"]
    if args.fifo:
        names.extend(["fifo.cpp","fifo_external.cpp"])
        if args.results==ROOT/"experiments/runs/symmetric42/spill":
            args.results=ROOT/"experiments/runs/symmetric42/fifo_spill"
    sources={name:digest(source.with_name(name)) for name in names}
    signature=sha256(json.dumps(sources,sort_keys=True).encode()).hexdigest()
    args.scratch.mkdir(parents=True,exist_ok=True)
    def limits():
        cap=args.mib<<20
        resource.setrlimit(resource.RLIMIT_AS,(cap,cap));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"spill"
        subprocess.run(["g++",*FLAGS,source,"-o",exe],check=True)
        for h in args.heights:
            rec=args.results/f"n{args.n:02}"/f"h{h:02}.json"
            if rec.exists():
                prev=json.loads(rec.read_bytes())
                if prev["status"]=="exact" and prev["source_sha256"]==signature:continue
            work=args.scratch/signature[:12]/f"n{args.n:02}h{h:02}"
            work.mkdir(parents=True,exist_ok=True)
            # A resume can reuse the previous saved layer and partial output.
            existing=sum(p.stat().st_size for p in work.iterdir() if p.is_file())
            def free_space():
                free=shutil.disk_usage(work).free
                if args.physical_volume:free=min(free,shutil.disk_usage(args.physical_volume).free)
                return free
            if free_space()+existing<(args.disk_mib+args.reserve_mib)<<20:
                raise RuntimeError("scratch limit plus reserve exceeds available filesystem space")
            progress=work/"progress.txt"
            resume=int(progress.read_text().split()[2]) if progress.exists() else 0
            started=time.monotonic()
            row=dict(n=args.n,h=h,source_sha256=signature,sources=sources,binary_sha256=digest(exe),
                     compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
                     compiler_flags=FLAGS,limit_address_mib=args.mib,limit_disk_mib=args.disk_mib,
                     limit_seconds=args.seconds,minimum_free_mib=args.reserve_mib,
                     resumed_from_layer=resume,utc=datetime.now(timezone.utc).isoformat())
            try:
                command=[exe,str(args.n),str(h),str(work),str(args.disk_mib),str(args.stop_layer)]
                with subprocess.Popen(command,preexec_fn=limits,text=True,stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE) as proc:
                    while True:
                        try:
                            output,errors=proc.communicate(timeout=20)
                            break
                        except subprocess.TimeoutExpired:
                            reason=None
                            if time.monotonic()-started>=args.seconds:reason="timeout"
                            if free_space()<args.reserve_mib<<20:reason="free-space reserve reached"
                            if reason:
                                proc.kill();output,errors=proc.communicate();errors+=reason
                                break
                if proc.returncode or not output.strip():
                    row.update(status="incomplete",code=proc.returncode,detail=errors[-300:])
                else:
                    n,h2,value,peak,moves,rss,elapsed,width,traffic,disk=output.split()
                    assert (int(n),int(h2))==(args.n,h)
                    row.update(status="exact",value=int(value),peak_states=int(peak),
                               attempted_moves=int(moves),process_peak_KiB_this_invocation=int(rss),
                               kernel_seconds_this_invocation=float(elapsed),word_field_width=int(width),
                               layer_bytes_read_and_written=int(traffic),max_two_layers_bytes=int(disk))
                    for name in (("sectors","compact","spill","fifo") if args.fifo else ("sectors","compact")):
                        refpath=args.results.parent/name/f"n{args.n:02}"/f"h{h:02}.json"
                        if refpath.exists() and (ref:=json.loads(refpath.read_bytes()))["status"]=="exact":
                            assert row["value"]==ref["value"]
                            if not args.fifo or name=="fifo":
                                assert row["attempted_moves"]==ref["attempted_moves"]
                            row["compared_with_"+name]=True
            except subprocess.TimeoutExpired:
                row.update(status="incomplete",code="timeout")
            row["wall_seconds_this_invocation"]=time.monotonic()-started
            row["completed_layer"]=int(progress.read_text().split()[2]) if progress.exists() else 0
            atomic(rec,row)
            print(args.n,h,row["status"],row.get("value"),row["completed_layer"],flush=True)


if __name__=="__main__":main()
