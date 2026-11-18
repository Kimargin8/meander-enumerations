"""Bounded local runs of height sectors, with published and prior exact controls."""
import argparse
import concurrent.futures
from datetime import datetime, timezone
from hashlib import sha256
import json
from pathlib import Path
import platform
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name("fold.cpp")
FLAGS = ["-O3", "-march=native", "-std=c++17", "-Wall", "-Wextra"]


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf8", dir=path.parent,
                                     delete=False, suffix=".tmp") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")
    Path(stream.name).replace(path)


def run(n, h, exe, signature, folder, memory, seconds):
    path = folder / f"n{n:02}" / f"h{h:02}.json"
    if path.exists():
        prev = json.loads(path.read_bytes())
        if prev["status"] == "exact" and prev["source_sha256"] == signature:
            return prev
    def limits():
        import resource
        resource.setrlimit(resource.RLIMIT_AS, (memory, memory))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    status = dict(n=n, h=h, source_sha256=signature, limit_address_bytes=memory,
                  limit_seconds=seconds, utc=datetime.now(timezone.utc).isoformat())
    try:
        proc = subprocess.run([exe, str(n), str(n), str(h)], text=True, capture_output=True,
                              preexec_fn=limits, timeout=seconds)
    except subprocess.TimeoutExpired:
        status.update(status="incomplete",code="timeout",detail="process exceeded its wall-time limit")
        atomic(path,status)
        return status
    if proc.returncode:
        status.update(status="incomplete", code=proc.returncode, detail=proc.stderr[-500:])
    else:
        order, height, value, peak, candidates, rss, elapsed, bits = proc.stdout.split()
        assert (int(order),int(height)) == (n,h)
        status.update(status="exact", value=int(value), peak_states=int(peak),
                      attempted_moves=int(candidates), process_peak_KiB=int(rss),
                      kernel_seconds=float(elapsed), word_field_width=int(bits))
    atomic(path, status)
    print(n,h,status["status"],status.get("value"),status.get("kernel_seconds"),flush=True)
    return status


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("folder",type=Path)
    ap.add_argument("orders",nargs="+",type=int)
    ap.add_argument("--workers",type=int,default=1)
    ap.add_argument("--per-process-mib",type=int,default=2048)
    ap.add_argument("--seconds",type=int,default=900)
    ap.add_argument("--min-h",type=int,default=0)
    ap.add_argument("--max-h",type=int,default=100)
    ap.add_argument("--compact",action="store_true",help="use segmented hash tables and 128-bit checked weights")
    ap.add_argument("--fifo",action="store_true",help="use the independent internal-chord/through-endpoint transfer")
    args=ap.parse_args()
    source_hash=digest(SOURCE)
    build_source=SOURCE
    sources={SOURCE.name:source_hash}
    if args.compact or args.fifo:
        build_source=SOURCE.with_name("compact.cpp")
        sources[build_source.name]=digest(build_source)
        source_hash=sha256((SOURCE.read_bytes()+build_source.read_bytes())).hexdigest()
    if args.fifo:
        build_source=SOURCE.with_name("fifo.cpp")
        sources[build_source.name]=digest(build_source)
        source_hash=sha256(json.dumps(sources,sort_keys=True).encode()).hexdigest()
    known = {}
    for path in sorted((ROOT/"experiments/runs/projective41").glob("diameter*.jsonl")):
        for line in path.read_text().splitlines():
            row=json.loads(line)
            known[row["n"]]=row
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"fold"
        subprocess.run(["g++",*FLAGS,build_source,"-o",exe],check=True)
        protocol=dict(source_sha256=source_hash,driver_sha256=digest(Path(__file__)),
                      binary_sha256=digest(exe),source_files=sources,flags=FLAGS,platform=platform.platform(),
                      compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0])
        atomic(args.folder/"protocol.json", protocol)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
            for n in args.orders:
                before=time.monotonic()
                heights=[h for h in range(n%2,n+1,2) if args.min_h<=h<=args.max_h]
                # Cost is concentrated near h=n/5. Start these sectors first.
                heights.sort(key=lambda h:abs(h-n/5))
                futures=[pool.submit(run,n,h,exe,source_hash,args.folder,args.per_process_mib*2**20,
                                     args.seconds) for h in heights]
                for f in concurrent.futures.as_completed(futures): f.result()
                rows=[]
                for h in range(n%2,n+1,2):
                    path=args.folder/f"n{n:02}"/f"h{h:02}.json"
                    if path.exists():
                        rec=json.loads(path.read_bytes())
                        if rec["status"]=="exact" and rec["source_sha256"]==source_hash: rows.append(rec)
                if len(rows)!=n//2+1: continue
                values=[r["value"] for r in rows]
                total=sum(values)
                assert n==1 or total%2==0
                if n in known:
                    assert values==known[n]["by_height"] and total==known[n]["count"]
                result=dict(n=n,D=total,K=total//2 if n>1 else 1,K_index=n-1,
                            by_height=values,largest_sector_states=max(r["peak_states"] for r in rows),
                            attempted_moves=sum(r["attempted_moves"] for r in rows),
                            summed_kernel_seconds=sum(r["kernel_seconds"] for r in rows),
                            max_process_KiB=max(r["process_peak_KiB"] for r in rows),
                            campaign_seconds_this_invocation=time.monotonic()-before,
                            reference_compared_height_by_height=n in known,
                            source_sha256=source_hash)
                atomic(args.folder/f"n{n:02}"/"total.json",result)
                print("COMPLETE",n,total,flush=True)


if __name__=="__main__": main()
