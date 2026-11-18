"""Compare an entire two-port triangle against the independent diameter transfer."""
import argparse
import json
from pathlib import Path
from campaign import ROOT,atomic,digest
from port_evidence import verify


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("N",type=int)
    p.add_argument("--runs",type=Path,default=ROOT/"experiments/runs/symmetric42")
    args=p.parse_args()
    reference={}
    for f in sorted((ROOT/"experiments/runs/projective41").glob("diameter*.jsonl")):
        for line in f.read_text().splitlines():
            r=json.loads(line)
            if r["n"] in reference:
                assert reference[r["n"]][0]==r["by_height"]
            reference[r["n"]]=(r["by_height"],f)
    checked={}
    for n in range(1,args.N+1):
        values,f=reference[n]
        receipts=[]
        for h in range(n%2,n+1,2):
            H=args.N-n+h
            path=args.runs/"all_orders"/f"n{args.N:02}h{H:02}"/f"n{n:02}.json"
            r=json.loads(path.read_bytes())
            assert (r["source_N"],r["source_H"],r["n"],r["h"],r["t"])==(args.N,H,n,h,n-1)
            assert r["left"]+r["right"]==r["value"]==values[h//2]
            if r.get("layer",{}).get("embedded"):verify(r)
            receipts.append(digest(path))
        checked[n]=dict(D=sum(values),sectors=len(values),receipt_hashes=receipts,
                        reference=f.name,reference_sha256=digest(f))
    atomic(args.runs/f"triangle{args.N}.json",dict(
        terminal_order=args.N,checked_rows=len(checked),
        checked_sectors=sum(r["sectors"] for r in checked.values()),rows=checked))


if __name__=="__main__":main()
