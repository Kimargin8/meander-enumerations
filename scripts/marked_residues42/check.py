"""Recover the symmetric marked count modulo eight from smaller fixed classes."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts/residue42"))
from check import matchings,traversal


def direct(order):
    m=2*order
    count=[0]*8
    for a in matchings(order):
        b=tuple((-a[-i%m])%m for i in range(m))
        p=traversal(a,b)
        if p is None:continue
        assert all(p[-i%m]==(-v)%m for i,v in enumerate(p)) and p[order]==order
        inv=tuple(p.index(i) for i in range(m))
        for inverse,w in enumerate((p,inv)):
            for turn in range(2):
                q=tuple((w[(i+order)%m]+order)%m for i in range(m)) if turn else w
                for reflection in range(2):
                    fixed=all(((-x)%m if reflection else x)==y for x,y in zip(q,p))
                    count[4*inverse+2*turn+reflection]+=fixed
    return count


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("out",type=Path)
    ap.add_argument("--max",type=int,default=17)
    ap.add_argument("--seconds",type=int,default=30,help="limit each individual class")
    args=ap.parse_args()
    source=Path(__file__).with_name("roots.cpp")
    dest=args.out;dest.mkdir(parents=True,exist_ok=True)
    flags=["-std=c++17","-O3","-Wall","-Wextra"]
    meta=dict(source_sha256=sha256(source.read_bytes()).hexdigest(),driver_sha256=sha256(Path(__file__).read_bytes()).hexdigest(),
              flags=flags,compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0])
    from csv import DictReader
    csv=ROOT/"data/symmetric.csv"
    if not csv.exists():csv=ROOT/"output/meanders42/symmetric.csv"
    with csv.open() as f: ds={int(r["n"]):int(r["D"]) for r in DictReader(f)}
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"signed"
        subprocess.run(["g++",*flags,source,"-o",exe],check=True)
        for n in range(3,args.max+1,2):
            group=direct(n) if n<=11 else None
            row=dict(n=n,D=ds[n],D_lower=ds[(n+1)//2],direct=group,classes={})
            for mode,key,pos in ((0,"SC",3),(1,"T",4),(2,"CT",6),(3,"ST",5),(4,"SCT",7)):
                try:
                    output=subprocess.check_output([exe,str(n),str(mode)],text=True,timeout=args.seconds).split()
                except subprocess.TimeoutExpired:
                    row["classes"][key]=dict(status="time limit",limit_seconds=args.seconds)
                    continue
                a,b,v,nodes=map(int,output[:4])
                assert (a,b)==(n,mode) and (group is None or v==group[pos])
                row["classes"][key]=dict(value=v,nodes=nodes,seconds=float(output[4]))
            if group: assert group[:3]==[ds[n],0,ds[(n+1)//2]]
            if all("value" in r for r in row["classes"].values()):
                residue=-(ds[(n+1)//2]+sum(r["value"] for r in row["classes"].values()))%8
                assert residue==ds[n]%8
                row["D_mod8"]=residue
            (dest/f"n{n:02}.json").write_text(json.dumps(dict(**row,build=meta),indent=2)+"\n")
            print(n,row.get("D_mod8"),{k:r.get("value") for k,r in row["classes"].items()},flush=True)


if __name__=="__main__":main()
