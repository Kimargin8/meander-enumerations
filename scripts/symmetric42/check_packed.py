"""Test exact byte restoration, corruption rejection and final counts after a packed resume."""
from hashlib import sha256
from pathlib import Path
import shutil
import subprocess
import tempfile
from campaign import FLAGS,ROOT,atomic,digest
from packed_layer import MAGIC,blocks
import json


def main():
    src=Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp)
        plain=root/"plain";packed=root/"packed";solver=root/"native";zsolver=root/"compressed"
        subprocess.run(["g++",*FLAGS,src/"narrow.cpp","-o",solver],check=True)
        subprocess.run(["g++",*FLAGS,src/"narrow.cpp",src/"packed_stream.cpp","-Wl,--wrap=fopen","-ldl","-o",zsolver],check=True)
        n,h=32,8
        def run(exe,folder,t,**kwargs):
            return subprocess.run([exe,str(n),str(h),folder,"850",str(t)],capture_output=True,text=True,**kwargs)
        run(solver,plain,16,check=True)
        shutil.copytree(plain,packed)
        receipts=[]
        for t in (19,20,21):
            run(solver,plain,t,check=True);run(zsolver,packed,t,check=True)
            first=plain/f"layer{t}.bin";second=packed/f"layer{t}.bin"
            logical=sha256()
            for block in blocks(second):logical.update(block)
            assert logical.hexdigest()==digest(first)
            assert (plain/"progress.txt").read_text().split()[:7]==(packed/"progress.txt").read_text().split()[:7]
            with second.open("rb") as f:framed=f.read(8)==MAGIC
            receipts.append(dict(t=t,raw_bytes=first.stat().st_size,stored_bytes=second.stat().st_size,
                                 raw_sha256=digest(first),packed_sha256=digest(second),framed=framed))
        assert receipts[-1]["framed"]
        bad=root/"bad"
        shutil.copytree(packed,bad)
        damaged=bad/"layer21.bin"
        with damaged.open("r+b") as f:f.truncate(damaged.stat().st_size-71)
        assert run(zsolver,bad,22).returncode!=0
        assert (bad/"progress.txt").read_bytes()==(packed/"progress.txt").read_bytes()
        restored=root/"restored";shutil.copytree(packed,restored)
        with (restored/"layer21.bin").open("wb") as raw:
            for part in blocks(packed/"layer21.bin"):raw.write(part)
        outcomes=[int(run(exe,folder,n-1,check=True).stdout.split()[2])
                  for exe,folder in [(solver,plain),(zsolver,packed),(solver,restored)]]
        assert len(set(outcomes))==1
        expected=json.loads((ROOT/"experiments/runs/symmetric42/assembled/n32.json").read_bytes())
        assert outcomes[0]==expected["by_height"][str(h)]["value"]
        # Small full row: all terminal sectors must also agree with the independent previous transfer.
        n=24
        prior={}
        for line in (ROOT/"experiments/runs/projective41/diameter.jsonl").read_text().splitlines():
            row=json.loads(line);prior[row["n"]]=row["by_height"]
        row=[]
        for h in range(0,n+1,2):
            ans=int(run(zsolver,root/f"small{h}",n-1,check=True).stdout.split()[2])
            assert ans==prior[n][h//2];row.append(ans)
        atomic(ROOT/"experiments/runs/symmetric42/packed_check.json",dict(n=32,h=8,large_sector=outcomes[0],
            comparisons=receipts,truncation_rejected=True,complete_small_order=n,complete_small_heights=row,
            complete_small_total=sum(row),sources={f:digest(src/f) for f in
            ("fold.cpp","compact.cpp","spill.cpp","narrow.cpp","packed_stream.cpp","packed_layer.py")}))
        print("matched",outcomes[0],receipts[-1],sum(row),flush=True)


if __name__=="__main__":main()
