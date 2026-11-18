"""Same-host paired single-thread comparison with Hogan's published axis_transfer.cpp."""
from argparse import ArgumentParser
from hashlib import sha256
import os
from pathlib import Path
import resource
from statistics import median
import subprocess
import tempfile
import time
from campaign import atomic
import json

PINNED="47592b5f797471f1ec8c1ba1d271786998a2cc183700383576b33792b5b8e083"
UPSTREAM=("https://github.com/hunterhogan/mapFolding/blob/1862930c2f0bfe5a38ae8cd299e6b85eaa1dc632/"
          "research/matrixMeanders/axis_transfer.cpp")

def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("published_source",type=Path)
    p.add_argument("output",type=Path)
    p.add_argument("indices",nargs="+",type=int)
    p.add_argument("--mib",type=int,default=220)
    p.add_argument("--seconds",type=int,default=240)
    p.add_argument("--tail",type=int,default=4)
    a=p.parse_args()
    assert sha256(a.published_source.read_bytes()).hexdigest()==PINNED
    ours=Path(__file__).with_name("fold.cpp")
    cap=a.mib<<20
    cpus=os.sched_getaffinity(0)
    cpu=sorted(cpus)[len(cpus)//2]
    def limit():
        os.sched_setaffinity(0,{cpu})
        # The pinned public program installs its own RLIMIT_AS (line 429).
        # Limit anonymous allocations independently for both programs.
        resource.setrlimit(resource.RLIMIT_DATA,(cap,cap))
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    records=[]
    with tempfile.TemporaryDirectory() as tmp:
        h=Path(tmp)/"published";f=Path(tmp)/"fold"
        common=["g++","-O3","-march=native"]
        subprocess.run(common+["-std=c++20","-fopenmp",a.published_source,"-o",h],check=True)
        subprocess.run(common+["-std=c++17",ours,"-o",f],check=True)
        env=dict(os.environ,OMP_NUM_THREADS="1",OMP_DYNAMIC="FALSE")
        env.pop("AXIS_PARTIAL_FILE",None)
        for n in a.indices:
            for mode in ("published","fold","fold","published"):
                command=[h,"pile",str(n),str(a.tail)] if mode=="published" else [f,str(n+1),str(n+1),"-2"]
                start=time.monotonic()
                try:
                    proc=subprocess.run(command,preexec_fn=limit,env=env,timeout=a.seconds,text=True,capture_output=True)
                except subprocess.TimeoutExpired:
                    records.append(dict(p=n,mode=mode,status="timeout"));continue
                wall=time.monotonic()-start
                if proc.returncode:
                    records.append(dict(p=n,mode=mode,status="incomplete",detail=proc.stderr[-500:],wall_seconds=wall))
                    continue
                if mode=="published":
                    r=json.loads(proc.stdout)
                    value,elapsed,rss=r["value"],r["seconds"],r["max_rss_kib"]
                else:
                    rows=[l.split() for l in proc.stdout.splitlines()]
                    total=sum(int(x[2]) for x in rows)
                    assert total%2==0
                    value,elapsed,rss=total//2,sum(float(x[6]) for x in rows),max(int(x[5]) for x in rows)
                records.append(dict(p=n,mode=mode,status="exact",K=value,kernel_seconds=elapsed,
                                    wall_seconds=wall,peak_RSS_KiB=rss))
            values={r["K"] for r in records if r["p"]==n and r["status"]=="exact"}
            assert len(values)==1
            print(n,values.pop(),flush=True)
        summary={}
        for n in a.indices:
            per={m:[r["wall_seconds"] for r in records if (r["p"],r["mode"],r["status"])==(n,m,"exact")]
                 for m in ("published","fold")}
            if all(len(t)==2 for t in per.values()):
                summary[n]=median(per["published"])/median(per["fold"])
        atomic(a.output,dict(source_url=UPSTREAM,published_source_sha256=PINNED,
                             fold_sha256=sha256(ours.read_bytes()).hexdigest(),
                             compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
                             physical_host="shared with other local research runs",cpu_affinity=cpu,
                             threads=1,tail_layers=a.tail,data_limit_mib=a.mib,limit_seconds=a.seconds,
                             run_order="published, fold, fold, published at each p",records=records,
                             median_wall_time_published_over_fold=summary))

if __name__=="__main__":main()
