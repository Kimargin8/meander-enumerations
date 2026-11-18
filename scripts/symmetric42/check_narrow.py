"""Cross-check the single-group and three-group solvers at intermediate and final layers."""
from pathlib import Path
import subprocess
import tempfile
from campaign import FLAGS,ROOT,atomic,digest


def main():
    src=Path(__file__).resolve().parent
    rows=[]
    with tempfile.TemporaryDirectory() as s:
        root=Path(s)
        for name in ("spill","narrow"):
            subprocess.run(["g++",*FLAGS,src/(name+".cpp"),"-o",root/name],check=True)
        for n in (12,19,24):
            for h in range(n%2,n+1,2):
                for step in sorted({1,n//2,n-4,n-1}):
                    checkpoints=[]
                    values=[]
                    for name in ("spill","narrow"):
                        work=root/f"{name}{n}h{h}"
                        r=subprocess.run([root/name,str(n),str(h),work,"400",str(step)],
                                         text=True,capture_output=True,check=True)
                        fields=list(map(int,(work/"progress.txt").read_text().split()))
                        checkpoints.append(fields[:7])
                        if step==n-1:values.append(int(r.stdout.split()[2]))
                    assert checkpoints[0]==checkpoints[1] and len(set(values))<=1
                    rows.append(dict(n=n,h=h,t=step,states=checkpoints[0][3],
                                     digest64=checkpoints[0][4],moves=checkpoints[0][6]))
    result=dict(cases=len(rows),records=rows,sources={name:digest(src/name) for name in
                ("fold.cpp","compact.cpp","spill.cpp","narrow.cpp")})
    atomic(ROOT/"experiments/runs/symmetric42/narrow_check.json",result)
    print("matched",len(rows),"checkpoints")


if __name__=="__main__":main()
