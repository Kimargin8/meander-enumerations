"""Export D(2k,0)=M_k with its input table and proof locator."""
from argparse import ArgumentParser
from pathlib import Path
from campaign import ROOT,atomic,digest

if __name__=="__main__":
    ap=ArgumentParser(description=__doc__)
    ap.add_argument("folder",type=Path);ap.add_argument("orders",type=int,nargs="+")
    args=ap.parse_args()
    path=ROOT/"references/oeis/2026-11-15/b005315.txt"
    values={int(x[0]):int(x[1]) for l in path.read_text().splitlines() if len(x:=l.split())==2 and x[0].isdigit()}
    for n in args.orders:
        if n<2 or n%2 or n//2 not in values:raise ValueError("a known even-order comparison is required")
        atomic(args.folder/"identities"/f"n{n:02}"/"h00.json",
               dict(n=n,h=0,value=values[n//2],ordinary_order=n//2,status="exact",
                    method="cyclic relabelling of the two independent halves: D(2k,0)=M_k",
                    source_file=str(path.relative_to(ROOT)).replace("\\","/"),input_sha256=digest(path),
                    reference_url="https://oeis.org/A005315/b005315.txt",
                    source_sha256=digest(Path(__file__))))
