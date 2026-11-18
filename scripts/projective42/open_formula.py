"""Minimum-zero and minimum-one contributions from published plane meanders."""
from hashlib import sha256
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/"experiments/runs/projective42/formula"
OEIS=ROOT/"references/oeis/2026-11-15"


def table(name):
    path=OEIS/f"b{name[1:]}.txt"
    pairs={int(s.split()[0]):int(s.split()[1])
           for s in path.read_text().splitlines() if s.strip() and not s.startswith("#")}
    return pairs,dict(sequence=name,url=f"https://oeis.org/{name}",
                      path=path.relative_to(ROOT).as_posix(),
                      sha256=sha256(path.read_bytes()).hexdigest())


def main():
    closed,closed_src=table("A005315")
    opened,opened_src=table("A005316")
    DEST.mkdir(exist_ok=True)
    H=[0]*(max(opened)+1)
    # An odd open meander with 2k-1 crossings is equivalent to a closed
    # meander of order k. Use the existing local closed solver at odd endpoints.
    for n,closed_path in [(45,DEST.parent/"closed23.txt"),
                          (47,ROOT/"experiments/runs/rescue36/validated24.json")]:
        previous=DEST.parent/f"open{n-1:02}.json"
        target=DEST.parent/f"open{n:02}.json"
        if not previous.is_file() or not closed_path.is_file():
            continue
        lower=json.loads(previous.read_bytes())
        closed_run=json.loads(closed_path.read_bytes())
        last=int(closed_run["answer"])
        if last!=opened[n] or int(closed_run.get("closed_order",closed_run.get("n")))!=((n+1)//2):
            raise ValueError("closed-to-open identity")
        if target.is_file() and json.loads(target.read_bytes()).get("algorithm")!="odd open equals closed":
            continue
        sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=sha256(p.read_bytes()).hexdigest())
                 for p in (previous,closed_path)]
        target.write_text(json.dumps(dict(n=n,count=last,open=[*lower["open"],last],
                              status="exact",algorithm="odd open equals closed",proof_inputs=sources),
                              indent=2)+"\n")
    local=[]
    for p in sorted(DEST.parent.glob("open[0-9][0-9].json")):
        if not p.stat().st_size:
            continue
        data=json.loads(p.read_text(encoding="utf-8-sig"))
        values=data["open"]
        if len(values)!=data["n"]+1 or data["count"]!=values[-1]:
            raise ValueError(p)
        for i,v in enumerate(values):
            if i in opened and opened[i]!=v:
                raise ValueError(f"published open input disagrees at {i}")
        local.append((values,dict(method=data.get("algorithm","local planar two-ended transfer"),
                          path=p.relative_to(ROOT).as_posix(),sha256=sha256(p.read_bytes()).hexdigest(),
                          horizon=data["n"])))
    def input_for(n):
        return next(((values,info) for values,info in local if len(values)>n),(None,None))
    for n in range(1,len(H)):
        H[n]=n*opened[n]-sum(opened[j]*H[n-j] for j in range(1,n))
        if n%2:
            x=2*H[n]-n*opened[n]
            if x<0: raise ArithmeticError(n)
            local_values,source=input_for(n)
            if local_values is not None:
                local_h=[0]*(n+1)
                for i in range(1,n+1):
                    local_h[i]=i*local_values[i]-sum(local_values[j]*local_h[i-j] for j in range(1,i))
                if x!=2*local_h[n]-n*local_values[n]: raise ValueError(n)
            (DEST/f"n{n:02}h01.json").write_text(json.dumps(dict(n=n,h=1,count=x,status="exact",
                formula="2n [z^n] log(1+O(z)) - n o_n",input=source or opened_src,
                last_input=n,external_tabulated_input=source is None),indent=2)+"\n")
    for k,x in sorted(closed.items()):
        n=2*k
        if n:
            local_values,source=input_for(n-1)
            if local_values is not None and local_values[n-1]!=x: raise ValueError(n)
            (DEST/f"n{n:02}h00.json").write_text(json.dumps(dict(n=n,h=0,count=n*x,status="exact",
                formula="n M_(n/2) = n o_(n-1)",input=source or closed_src,
                input_index=n-1 if source else k,last_input=n-1 if source else k,
                external_tabulated_input=source is None),indent=2)+"\n")

if __name__=="__main__":
    main()
