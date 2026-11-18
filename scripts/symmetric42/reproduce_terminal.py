"""Recompute a complete symmetric row and its three predecessors from fresh disk layers."""
from argparse import ArgumentParser
import json
from pathlib import Path
import subprocess
import sys
from campaign import atomic,digest
from port_evidence import verify


def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("directory",type=Path,help="working directory on a native Linux filesystem")
    p.add_argument("--order",type=int,default=46)
    p.add_argument("--packed",action="store_true",help="compress wide checkpoints using the system libzstd.so.1")
    p.add_argument("--backing-volume",type=Path,help="filesystem containing the backing disk of a virtual machine")
    p.add_argument("--mib",type=int,default=8192,help="address-space cap for the single solver process")
    p.add_argument("--disk-mib",type=int,default=32768,help="maximum combined size of two consecutive layers")
    p.add_argument("--reserve-mib",type=int,default=15360,help="minimum free space on monitored volumes")
    p.add_argument("--seconds",type=int,default=86400,help="wall time per height per invocation")
    a=p.parse_args()
    if not __debug__:p.error("run this verifier without -O")
    if not 2<=a.order<=46:p.error("complete rows supported by the packed key have orders 2 through 46")
    if min(a.mib,a.disk_mib,a.seconds)<1 or a.reserve_mib<0:p.error("invalid resource limit")
    folder=a.directory.resolve()
    folder.mkdir(parents=True,exist_ok=True)
    scratch=folder/"scratch";receipts=folder/"receipts"
    backing=a.backing_volume
    if backing is None and "microsoft" in Path("/proc/sys/kernel/osrelease").read_text().lower() and Path("/mnt/c").is_dir():
        backing=Path("/mnt/c")
    n=a.order
    start=max(1,n-3)
    command=[sys.executable,Path(__file__).with_name("diagonal_campaign.py"),str(n),
             *map(str,range(n,-1,-2)),"--scratch",scratch,"--results",receipts,"--single-group",
             "--min-order",str(start),"--mib",str(a.mib),"--disk-mib",str(a.disk_mib),
             "--reserve-mib",str(a.reserve_mib),"--seconds",str(a.seconds)]
    if backing is not None:command.extend(["--backing-volume",backing])
    if a.packed:command.append("--packed")
    subprocess.run(command,check=True)
    rows=[]
    for m in range(start,n+1):
        entries=[]
        for h in range(m%2,m+1,2):
            H=n-m+h
            path=receipts/f"n{n:02}h{H:02}"/f"n{m:02}.json"
            r=json.loads(path.read_bytes())
            assert (r["source_N"],r["source_H"],r["n"],r["h"])==(n,H,m,h)
            verify(r)
            entries.append(dict(h=h,D=r["value"],receipt=str(path.relative_to(folder)),sha256=digest(path)))
        total=sum(x["D"] for x in entries)
        assert m==1 or total%2==0
        index=m-1
        count=total//2 if m>1 else 1
        rows.append(dict(n=m,K_index=index,K=count,D=total,heights=entries))
        print(f"D({m}) = {total}, K({index}) = {count}",flush=True)
    atomic(folder/"summary.json",dict(terminal_order=n,rows=rows))


if __name__=="__main__":main()
