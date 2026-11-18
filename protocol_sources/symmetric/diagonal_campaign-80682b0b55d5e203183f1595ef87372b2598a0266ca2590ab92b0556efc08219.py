"""Read every smaller same-deficit sector from a single folded computation."""
from argparse import ArgumentParser
from datetime import datetime,timezone
from hashlib import sha256
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile
import time
from campaign import FLAGS,ROOT,atomic,digest
from port_evidence import embed,verify


def acquire(live,work,first,deadline):
    marker=work/"progress.txt"
    if marker.exists():return
    while time.monotonic()<deadline:
        other=live/"progress.txt"
        if other.exists():
            raw=other.read_bytes()
            t=int(raw.split()[2])
            if t>=max(0,first-2):
                link=work/f"layer{t}.bin"
                try:
                    os.link(live/link.name,link)
                except FileNotFoundError:
                    continue
                except FileExistsError:
                    pass
                tmp=work/"seed.tmp";tmp.write_bytes(raw);tmp.replace(marker)
                return
        time.sleep(.15)
    raise TimeoutError("upstream checkpoint not reached")


def main():
    ap=ArgumentParser(description=__doc__)
    ap.add_argument("N",type=int);ap.add_argument("heights",type=int,nargs="+")
    ap.add_argument("--scratch",type=Path,required=True)
    ap.add_argument("--live-root",type=Path,help="fork a recent committed layer by hard link on the same filesystem")
    ap.add_argument("--min-order",type=int,default=1)
    ap.add_argument("--mib",type=int,default=1000);ap.add_argument("--disk-mib",type=int,default=3000)
    ap.add_argument("--seconds",type=int,default=3600)
    ap.add_argument("--backing-volume",type=Path)
    ap.add_argument("--reserve-mib",type=int,default=2048)
    ap.add_argument("--results",type=Path,default=ROOT/"experiments/runs/symmetric42/all_orders")
    args=ap.parse_args();N=args.N
    if N<1 or any(h>N or h<0 or (N-h)%2 for h in args.heights):ap.error("invalid sector")
    scripts=Path(__file__).resolve().parent
    inputs={f:digest(scripts/f) for f in ("fold.cpp","compact.cpp","spill.cpp")}
    base=sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
    inputs["read_ports.cpp"]=digest(scripts/"read_ports.cpp")
    signature=sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
    def limits():
        cap=args.mib<<20;resource.setrlimit(resource.RLIMIT_AS,(cap,cap))
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory() as build:
        solver=Path(build)/"spill";reader=Path(build)/"read_ports"
        for exe,src in [(solver,"spill.cpp"),(reader,"read_ports.cpp")]:
            subprocess.run(["g++",*FLAGS,scripts/src,"-o",exe],check=True)
        compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0]
        binaries={"solver":digest(solver),"reader":digest(reader)}
        for H in args.heights:
            name=f"n{N:02}h{H:02}"; j=(N-H)//2
            work=args.scratch/signature[:12]/name;work.mkdir(parents=True,exist_ok=True)
            first=max(args.min_order,1,2*j)
            deadline=time.monotonic()+args.seconds
            if args.live_root:acquire(args.live_root/base[:12]/name,work,first,deadline)
            for n in range(first,N+1):
                result=args.results/name/f"n{n:02}.json"
                if result.exists():
                    previous=json.loads(result.read_bytes())
                    if previous["source_sha256"]==signature and "layer" in previous:continue
                marker=work/"progress.txt"
                if marker.exists() and int(marker.read_bytes().split()[2])>n-1:continue
                frees=[shutil.disk_usage(work).free]
                if args.backing_volume:frees.append(shutil.disk_usage(args.backing_volume).free)
                if min(frees)<args.reserve_mib<<20:raise RuntimeError("free-space reserve")
                command=[solver,str(N),str(H),work,str(args.disk_mib),str(n-1)]
                resume=int(marker.read_bytes().split()[2]) if marker.exists() else 0
                usage_path=work/"last-usage.txt"
                measured=["/usr/bin/time","-f","%M %e","-o",usage_path,*command]
                with subprocess.Popen(measured,preexec_fn=limits,text=True,stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE,start_new_session=True) as proc:
                    while True:
                        try:
                            out,errors=proc.communicate(timeout=min(20,max(.1,deadline-time.monotonic())))
                            break
                        except subprocess.TimeoutExpired:
                            free=shutil.disk_usage(work).free
                            if args.backing_volume:free=min(free,shutil.disk_usage(args.backing_volume).free)
                            if free<args.reserve_mib<<20 or time.monotonic()>=deadline:
                                import signal
                                os.killpg(proc.pid,signal.SIGKILL);proc.communicate()
                                raise RuntimeError("free-space reserve or wall-time limit")
                if proc.returncode:raise RuntimeError(errors[-500:])
                order,height,t,count,check,peak,attempts,traffic,disk=map(int,marker.read_bytes().split())
                assert (order,height,t)==(N,H,n-1)
                r=subprocess.run([reader,str(N),str(H),str(t),str(count),str(check),work/f"layer{t}.bin"],
                                 preexec_fn=limits,check=True,text=True,capture_output=True,
                                 timeout=max(1,deadline-time.monotonic()))
                actual_n,actual_h,left,right,value=map(int,r.stdout.split())
                assert (actual_n,actual_h)==(n,n-2*j) and value==left+right
                if out.strip():assert int(out.split()[2])==value and n==N
                refs=[]
                for method in ("sectors","compact","spill","fifo","fifo_spill"):
                    ref=args.results.parent/method/f"n{n:02}"/f"h{actual_h:02}.json"
                    if ref.exists() and (record:=json.loads(ref.read_bytes()))["status"]=="exact":
                        assert record["value"]==value;refs.append(method)
                peak_kib,wall=usage_path.read_text().split()
                evidence=embed(work/f"layer{t}.bin")
                record=dict(n=n,h=actual_h,source_N=N,source_H=H,j=j,t=t,left=left,right=right,
                                   value=value,status="exact",input_states=count,layer_digest64=check,
                                   attempted_moves_through_layer=attempts,compared_methods=refs,
                                   peak_states_through_layer=peak,layer_bytes_read_and_written=traffic,
                                   max_two_layers_bytes=disk,process_peak_KiB_this_invocation=int(peak_kib),
                                   wall_seconds_this_invocation=float(wall),resumed_from_layer=resume,
                                   driver_sha256=digest(Path(__file__)),port_verifier_sha256=digest(scripts/"port_evidence.py"),
                                   compiler=compiler,compiler_flags=FLAGS,binary_sha256=binaries,layer=evidence,
                                   source_sha256=signature,source_files=inputs,
                                   utc=datetime.now(timezone.utc).isoformat())
                if evidence["embedded"]:verify(record)
                atomic(result,record)
                print(N,H,n,actual_h,value,flush=True)


if __name__=="__main__":main()
