"""ABBA comparison of one and three output groups on the same complete packed input layer."""
from argparse import ArgumentParser
from datetime import datetime,timezone
from hashlib import sha256
from pathlib import Path
import os
import resource
import shutil
import subprocess
import tempfile
from campaign import ROOT,FLAGS,atomic,digest
from packed_layer import blocks,version


def main():
    p=ArgumentParser(description=__doc__);p.add_argument("--n",type=int,default=38)
    p.add_argument("--h",type=int,default=6);p.add_argument("--cut",type=int,default=23)
    a=p.parse_args();src=Path(__file__).resolve().parent
    cpu=min(os.sched_getaffinity(0));args=[str(a.n),str(a.h)]
    def limits():
        resource.setrlimit(resource.RLIMIT_AS,(2900<<20,2900<<20))
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,{cpu})
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp);origin=root/"origin"
        for name,source in (("one","narrow.cpp"),("three","spill.cpp")):
            subprocess.run(["g++",*FLAGS,src/source,src/"packed_stream.cpp","-Wl,--wrap=fopen","-ldl","-o",root/name],check=True)
        subprocess.run([root/"three",*args,origin,"1800",str(a.cut)],check=True,
                       preexec_fn=limits,capture_output=True)
        old=origin/f"layer{a.cut}.bin"
        logical=sha256()
        for block in blocks(old):logical.update(block)
        rows=[]
        for k,name in enumerate(("three","one","one","three")):
            work=root/f"case{k}";work.mkdir()
            os.link(old,work/old.name);shutil.copyfile(origin/"progress.txt",work/"progress.txt")
            measure=work/"usage"
            subprocess.run(["/usr/bin/time","-f","%e %U %S %M","-o",measure,root/name,*args,work,
                            "1800",str(a.cut+1)],check=True,preexec_fn=limits,capture_output=True)
            wall,user,system,rss=measure.read_text().split()
            meta=list(map(int,(work/"progress.txt").read_text().split()))
            matching=meta[:7]
            assert not rows or rows[0]["logical_checkpoint"]==matching
            rows.append(dict(groups=name,wall_seconds=float(wall),user_seconds=float(user),
                             system_seconds=float(system),max_rss_KiB=int(rss),logical_checkpoint=matching,
                             stored_output_bytes=(work/f"layer{a.cut+1}.bin").stat().st_size))
            (work/f"layer{a.cut+1}.bin").unlink()
            print(name,wall,rss,matching[3],flush=True)
        result=dict(n=a.n,h=a.h,input_layer=a.cut,order=["three","one","one","three"],cpu=cpu,
                    input_raw_sha256=logical.hexdigest(),input_stored_sha256=digest(old),zstd=version(),rows=rows,
                    source_files={f:digest(src/f) for f in ("fold.cpp","compact.cpp","spill.cpp","narrow.cpp","packed_stream.cpp")},
                    compiler_flags=FLAGS,utc=datetime.now(timezone.utc).isoformat())
        atomic(ROOT/"experiments/runs/symmetric42/packed_groups_benchmark.json",result)


if __name__=="__main__":main()
