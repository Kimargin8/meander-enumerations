"""Compute the additional order-15 rotation moment used at n=45 in both directions."""
from hashlib import sha256
from math import comb
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from time import perf_counter

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/"scripts/modular41"))
from rotation import face_sum

if __name__=="__main__":
    out=ROOT/"experiments/runs/closed_residues42"
    out.mkdir(exist_ok=True,parents=True)
    src=HERE/"rotation_halves.cpp"
    assert 2*16**2*(comb(30,15)//16)**2 < 2**64
    with tempfile.TemporaryDirectory() as temp:
        exe=Path(temp)/"halves"
        subprocess.run(["g++","-O3","-std=c++17",src,"-o",exe],check=True)
        tic=perf_counter()
        half=list(map(int,subprocess.check_output([exe,"15"],text=True).split()))
        sec=perf_counter()-tic
    tic=perf_counter()
    forward=face_sum(15)
    assert half[1:6]==[forward[k] for k in ("connected","sum_even_upper","sum_even_lower","sum_product","fixed")]
    blob=dict(m=15,forward=forward,midpoint=half,midpoint_seconds=sec,forward_seconds=perf_counter()-tic,
              half_source=sha256(src.read_bytes()).hexdigest(),
              forward_source=sha256((ROOT/"scripts/modular41/rotation.py").read_bytes()).hexdigest())
    (out/"moment15.json").write_text(json.dumps(blob,indent=2)+"\n")
    print(half,flush=True)
