"""Compare every weighted key of a lifted layer with a fresh larger-horizon computation."""
from pathlib import Path
import struct
import subprocess
import tempfile
from campaign import FLAGS,ROOT,atomic,digest


def weights(directory,t):
    raw=(directory/f"layer{t}.bin").read_bytes()
    result={};p=0
    while p<len(raw):
        count=struct.unpack_from("<Q",raw,p+1)[0];p+=9
        for _ in range(count):
            key=struct.unpack_from("<Q",raw,p)[0];p+=8;w=0
            for shift in range(0,127,7):
                byte=raw[p];p+=1;w|=(byte&127)<<shift
                if byte<128:break
            assert w>0 and key not in result
            result[key]=w
    return result


def main():
    src=Path(__file__).resolve().parent
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        base=Path(tmp)
        for name in ("spill","horizon"):
            subprocess.run(["g++",*FLAGS,src/(name+".cpp"),"-o",base/name],check=True)
        def run(*args):
            return subprocess.run(list(map(str,args)),check=True,text=True,capture_output=True).stdout
        for n in (12,16,19):
            for delta in (1,2):
                old_n=n-delta
                for h in range(n%2,n+1,2):
                    if h<delta:continue
                    name=f"{n}-{h}-{delta}"
                    old=base/(name+"old");full=base/(name+"full")
                    for t in sorted({n//2-1,old_n-3,old_n-1}):
                        run(base/"spill",old_n,h-delta,old,256,t)
                        run(base/"spill",n,h,full,256,t)
                        prefix=base/f"{name}-{t}"
                        e=prefix.with_suffix(".exception")
                        a=prefix.with_suffix(".memory");b=prefix.with_suffix(".stream")
                        run(base/"horizon",n,h,old_n,t,"--exception-disk",e,256)
                        run(base/"horizon",n,h,old_n,t,"--merge-checkpoint",old,e,b)
                        run(base/"horizon",n,h,old_n,t,"--lift",old,a)
                        expected=weights(full,t)
                        assert expected==weights(a,t)==weights(b,t)
                        ordinary=len(weights(old,t));extra=len(weights(e,t))
                        assert ordinary+extra==len(expected)
                        rows.append(dict(n=n,h=h,shift=delta,t=t,states=len(expected),
                                         inherited=ordinary,exceptional=extra))
    atomic(ROOT/"experiments/runs/symmetric42/horizon_check.json",
           dict(cases=len(rows),weighted_keys=sum(x["states"] for x in rows),rows=rows,
                sources={f:digest(src/f) for f in ("fold.cpp","compact.cpp","spill.cpp","horizon.cpp")}))
    print("exact dictionary comparison",len(rows),"layers,",sum(x["states"] for x in rows),"keys")


if __name__=="__main__":main()
