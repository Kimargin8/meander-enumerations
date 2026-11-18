"""Verify or restore both weighted inputs to an archived lifted layer."""
from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
import json
import os
from pathlib import Path
import subprocess
import tempfile
from campaign import FLAGS
from horizon_evidence import verify_seed


def scan(path):
    h=sha256();length=0
    with path.open("rb") as f:
        for block in iter(lambda:f.read(1<<20),b""):
            h.update(block);length+=len(block)
    return length,h.hexdigest()


def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("receipt",type=Path);p.add_argument("checkpoint",type=Path)
    p.add_argument("--output",type=Path,help="restore both inputs to files; by default only verify their bytes")
    a=p.parse_args();s=json.loads(a.receipt.read_bytes());assert verify_seed(s)
    n,h,t=s["output"]["metadata"][:3];old_n=s["shorter"]["metadata"][0]
    assert scan(a.checkpoint/f"layer{t}.bin")== (s["output"]["size"],s["output"]["layer_sha256"])
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp);exe=root/"restore"
        subprocess.run(["g++",*FLAGS,Path(__file__).with_name("restore_horizon.cpp"),"-o",exe],check=True)
        paths=[root/name for name in ("shorter","complement")]
        def run():
            subprocess.run([exe,str(n),str(h),str(old_n),str(t),a.checkpoint,*paths],check=True)
        if a.output:
            paths=[a.output/name/f"layer{t}.pending" for name in ("shorter","complement")]
            for f in paths:f.parent.mkdir(parents=True,exist_ok=True)
            run();values=[scan(f) for f in paths]
        else:
            for f in paths:os.mkfifo(f)
            with ThreadPoolExecutor(max_workers=2) as pool:
                keep=[os.open(f,os.O_RDWR|os.O_NONBLOCK) for f in paths]
                pending=[pool.submit(scan,f) for f in paths]
                try:run()
                finally:
                    for fd in keep:os.close(fd)
                values=[x.result() for x in pending]
        for name,value,f in zip(("shorter","complement"),values,paths):
            assert value==(s[name]["size"],s[name]["layer_sha256"])
            if a.output:
                meta=s[name]["metadata"]
                f.replace(f.with_name(f"layer{t}.bin"))
                (f.parent/"progress.txt").write_text(" ".join(map(str,meta))+"\n")
                if name=="complement":(f.parent/"exception.txt").write_text(f"{old_n} {t}\n")
            print(name,*value,flush=True)


if __name__=="__main__":main()
