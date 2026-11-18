"""Verify the distributed tables offline, then replay selected independent computations."""
import argparse
import csv
from functools import lru_cache
from hashlib import sha256
import json
from math import comb, gcd, lcm
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parent


def require(test,what):
    if not test: raise ValueError(what)


def load(path): return json.loads((ROOT/path).read_bytes())


def records(path):
    return [json.loads(s) for s in (ROOT/path).read_bytes().splitlines() if s.strip()]


def check_oeis(pp,kk,refs):
    entry=load("oeis/index.json")
    def integer_rows(name):
        lines=(ROOT/"oeis"/name).read_text(encoding="ascii").splitlines()
        return [tuple(map(int,line.split())) for line in lines if line and not line.startswith("#")]
    for s,info in entry["bfiles"].items():
        raw=(ROOT/"oeis"/info["filename"]).read_bytes()
        require(re.fullmatch(rb"(?:[0-9]+ -?[0-9]+\n)+",raw) and sha256(raw).hexdigest()==info["sha256"],("b-file format",s))
        rows=[tuple(map(int,line.split())) for line in raw.splitlines()]
        require([i for i,_ in rows]==list(range(info["first"],info["last"]+1)),("b-file gap",s))
        values=dict(rows)
        require(all(values[int(i)]==v for i,v in refs[s]["terms"].items()),("existing b-file",s))
        if s=="A006663": require(all(values[i]==r["P"] for i,r in pp.items()),"projective OEIS totals")
        if s=="A223096": require(all(values[i]==r["K"] for i,r in kk.items()),"symmetric OEIS totals")
        if s=="A077055":
            shapes={r["crossings"]-1:r["shapes"] for r in load("data/open_shapes.json")}
            require(all(shapes[i]==v for i,v in rows),"shape indices")
            require(info["last"]+1 not in shapes,"premature contiguous cutoff")
            require(integer_rows("A077055-crossings.txt")==[(i,i+1,v) for i,v in sorted(shapes.items())],"complete shape supplement")
        if s=="A400429":
            known={int(i):v for i,v in (refs[s]["terms"]|refs[s]["additional_terms"]).items()}
            fractions=load("oeis/A400429-rational-functions.json")["diagonals"]
            cells=[]
            for path in (ROOT/"experiments/runs/rainbow42/arch").glob("j*.json"):
                cert=json.loads(path.read_bytes());j=cert["j"]
                require(fractions[str(j)]==dict(numerator=cert["reduced_numerator"],denominator=cert["reduced_denominator"]),("editor's fraction",j))
                first=2
                for n,v in enumerate(cert["values"]):
                    if n<2:continue
                    width=n//2
                    if width>=j:
                        col=width-j+1
                        index=first+col-1
                        require(known.setdefault(index,v)==v,("winding entry",index))
                        cells.append((index,n,col,j,v))
                    first+=width
            require(integer_rows("A400429-certified-cells.txt")==sorted(cells) and len(cells)==entry["certified_triangle_cells"],"complete winding supplement")
            boundary=integer_rows("A400429-closed-boundary.txt")
            closed={int(i):v for i,v in refs["A005315"]["terms"].items()}
            expected=[];first=2
            for n in range(2,2*max(closed)+1):
                i=(n+1)//2
                expected.append((first,n,1,i,closed[i]))
                require(known.setdefault(first,closed[i])==closed[i],("closed winding boundary",n))
                first+=n//2
            require(boundary==expected and all(v==known[i] for i,v in rows) and info["last"]+1 not in known,"contiguous winding terms and closed boundary")
    residual=load("data/ordinary_residues.json")
    extra=[r["n"] for r in residual if str(r["n"]) not in refs["A005315"]["terms"]]
    require(extra==entry["ordinary_residue_orders"],"ordinary comments use only new orders")
    new=[r for r in residual if r["n"] in extra]
    require(integer_rows("A005315-congruences.txt")==[(r["n"],r["modulus"],r["residue"]) for r in new],"ordinary residue supplement")
    require(integer_rows("A077055-congruences.txt")==[(r["shape_crossings"]-1,r["shape_crossings"],r["shape_modulus"],r["shape_residue"]) for r in new],"shape residue supplement")
    high=load("experiments/runs/cyclic_projective42/minimum_checked.json")["orders"]
    fixed=[(r["n"],t["d"],r["n"]//t["d"],int(h),t["fixed_by_minimum"].get(h,0)) for r in high for t in r["rotations"] for h in r["sectors"]]
    checks=[(r["n"],int(h),r["modulus"],v) for r in high for h,v in r["sectors"].items()]
    require(integer_rows("A006663-rotation-classes.txt")==sorted(fixed) and integer_rows("A006663-sector-congruences.txt")==sorted(checks),"rotation supplements")
    projective={}
    for line in (ROOT/"oeis/A006663-height-sectors.txt").read_text(encoding="ascii").splitlines()[1:]:
        n,kind,lo,hi,value=line.split()
        key=(int(n),kind,int(lo),int(hi))
        require(key not in projective,("repeated projective class",key))
        projective[key]=int(value)
    for row in csv.DictReader((ROOT/"data/projective_sectors_additional.csv").read_text().splitlines()):
        n,lo,hi=map(int,(row["order"],row["first_height"],row["last_height"]))
        r=load(row["source"]);v=r["count"] if n==r["n"] else r["prefix_counts"][n]
        require(r["status"]=="exact" and r.get("first_height",r["h"])==lo and min(n,r["h"])==hi and n<=r["n"],("measured sector coordinates",n,lo,hi))
        require(v==int(row["exact_contribution"])==projective[(n,row["statistic"],lo,hi)],("exported projective class",n,lo,hi))
    for r in pp.values():
        for h,v in r.get("fixed_cut_sectors",{}).items():
            require(projective[r["n"],"fixed_cut",int(h),int(h)]==v,("base projective sectors",r["n"],h))
    marked={(r["k"],r["n"],int(h)):v for r in kk.values() for h,v in r["fixed_cut_sectors"].items()}
    for row in csv.DictReader((ROOT/"data/symmetric_sectors_additional.csv").read_text().splitlines()):
        r=load(row["source"]);key=tuple(int(row[k]) for k in ("shape_index","disk_order","height"))
        require(key==(r["n"]-1,r["n"],r["h"]) and r["status"]=="exact" and r["value"]==int(row["labelled_sector"])==marked.setdefault(key,r["value"]),"measured marked sector")
    require(integer_rows("A223096-height-sectors.txt")==sorted((*key,v) for key,v in marked.items()),"complete marked class table")
    terminal=[]
    for path in (ROOT/"experiments/runs/symmetric42/all_orders").glob("n*h*/n*.json"):
        r=json.loads(path.read_bytes())
        terminal.append(tuple(r[k] for k in ("source_N","source_H","n","h","left","right","value")))
        require(terminal[-1][-1]==sum(terminal[-1][-3:-1]),"terminal pair")
    require(integer_rows("A223096-terminal-weights.txt")==sorted(terminal),"all saved terminal pairs")


@lru_cache(None)
def planar(n):
    if not n: return ((),)
    out=[]
    for k in range(n):
        b=2*k+1
        for i in planar(k):
            for j in planar(n-1-k):
                out.append((b,)+tuple(v+1 for v in i)+(0,)+tuple(v+b+1 for v in j))
    return tuple(out)


def small(n):
    p=k=0; winding=[0]*(n//2+1)
    for a in planar(n):
        def component(j):
            i=0; length=0
            while not length or i:
                i=j[a[i]]; length+=1
            return length==n
        h=tuple((i+n)%(2*n) for i in range(2*n))
        d=tuple(n if i==0 else 0 if i==n else -i%(2*n) for i in range(2*n))
        r=tuple(2*n-1-i for i in range(2*n))
        p+=component(h);k+=component(d)
        if component(r):
            height=sum(v>=n for v in a[:n])
            winding[(n-height)//2]+=1
    return p,k,winding


def check_all():
    require(__debug__,"Use Python without -O so that external sector checks execute.")
    sys.path.insert(0,str(ROOT/"scripts/symmetric42"))
    from port_evidence import verify as verify_port_layer
    from horizon_evidence import signature as seed_signature, verify_seed
    manifest=(ROOT/"SHA256SUMS.txt").read_text(encoding="utf8").splitlines()
    listed=set()
    for line in manifest:
        h,name=line.split("  ",1)
        path=(ROOT/name).resolve()
        require(path.is_relative_to(ROOT) and name not in listed and sha256(path.read_bytes()).hexdigest()==h,name)
        listed.add(name)
    pp={v["n"]:v for v in load("data/projective.json")}
    kk={v["k"]:v for v in load("data/symmetric.json")}
    sys.path.insert(0,str(ROOT/"scripts/cyclic_projective42"))
    from minimum_check import verify as check_rotation_sectors
    from base_face_check import verify as check_base_faces
    check_base_faces(load("experiments/runs/cyclic_projective42/base_faces.json"))
    checked=check_rotation_sectors(load("experiments/runs/cyclic_projective42/minimum_checked.json"),
                                  ROOT/"experiments/runs/projective42/assembled")
    require(set(checked)==set(pp).intersection({44,45,46,47}),"high-order cyclic sector checks")
    refs=load("data/published.json")
    def reference(seq):
        v=refs[seq];return {int(x):y for x,y in (v["terms"]|v.get("additional_terms",{})).items()}
    for i,v in reference("A006663").items():
        if i: require(pp[i]["P"]==v,("public P",i))
    for i,v in reference("A223096").items(): require(kk[i]["K"]==v,("public K",i))
    closed=reference("A005315")
    opened=reference("A005316")
    terminal_rotations={v["n"]:v for v in load("experiments/runs/projective42/rotation_sectors.json")["rows"]}
    log_derivative=[0]*(max(pp)+1)
    for i in range(1,len(log_derivative)):
        log_derivative[i]=i*opened[i]-sum(opened[t]*log_derivative[i-t] for t in range(1,i))
    for path in (ROOT/"experiments/runs/projective42").glob("open??.json"):
        r=json.loads(path.read_bytes())
        require(r["count"]==r["open"][-1] and all(v==opened[i] for i,v in enumerate(r["open"]) if i in opened),"planar open transfer")
        for dep in r.get("proof_inputs",[]): require(sha256((ROOT/dep["path"]).read_bytes()).hexdigest()==dep["sha256"],"open input")
    for folder in ("projective_oracle41/diameters","oracle42/diameters","projective_oracle41/near_rainbow","projective_oracle41/near_rainbow_extra"):
        for path in (ROOT/"experiments/runs"/folder).glob("n??.json"):
            r=json.loads(path.read_bytes());n=r["n"]
            require(r["value"]==(pp[n]["P"] if folder.endswith("/diameters") else kk[n-1]["D"]),("independent disk split",n,folder))
    for name in ("diameter.jsonl","diameter28-29.jsonl","diameter30.jsonl","diameter31.jsonl"):
        for r in records("experiments/runs/projective41/"+name):
            n=r["n"];row=kk[n-1]
            saved=row["fixed_cut_sectors"]
            require(r["count"]==row["D"] and r["by_height"]==[saved[str(h)] for h in range(n%2,n+1,2)],("independent folded scan",n))
    short=load("experiments/runs/symmetric42/triangle20.json")
    seen={}
    for f in (ROOT/"experiments/runs/symmetric42/all_orders").glob("n20h*/n??.json"):
        v=json.loads(f.read_bytes());n=v["n"];h=v["h"]
        require(v["value"]==v["left"]+v["right"]==kk[n-1]["fixed_cut_sectors"][str(h)],("two-state small triangle",n,h))
        seen.setdefault(n,set()).add(sha256(f.read_bytes()).hexdigest())
    require(sum(map(len,seen.values()))==short["checked_sectors"]==120,"triangular replay coverage")
    for n,hashes in seen.items(): require(hashes==set(short["rows"][str(n)]["receipt_hashes"]),("small row",n))
    if 45 in kk:
        reports=ROOT/"experiments/runs/symmetric42"
        terminal_report=load("experiments/runs/symmetric42/terminal_report46.json")
        sectors=terminal_report["sectors"]
        require(terminal_report["D"]==kk[45]["D"]==2*terminal_report["K"] and len(sectors)==23,"full terminal report")
        require(terminal_report["concurrent_sampling"]==load("experiments/runs/symmetric42/terminal_resources.json"),"terminal sampling report")
        ports=0
        for path in (reports/"all_orders").glob("n46h*/n*.json"):
            r=json.loads(path.read_bytes())
            if 43<=r["n"]<=46:
                require(verify_port_layer(r) and kk[r["n"]-1]["fixed_cut_sectors"][str(r["h"])]==r["value"],("terminal layer",r["n"],r["h"]))
                ports+=1
        require(ports==91,"four senior terminal rows")
        for s in sectors:
            v=reports/"all_orders"/f"n46h{s['h']:02}"/"n46.json"
            require(sha256(v.read_bytes()).hexdigest()==s["evidence_sha256"],("resource input",s["h"]))
        require(sum(s["attempted_moves"] for s in sectors)==terminal_report["attempted_moves_across_counted_heights"] and
                max(s["peak_states"] for s in sectors)==terminal_report["largest_frontier"],"terminal resource aggregation")
    require(sorted(pp)==list(range(1,max(pp)+1)) and sorted(kk)==list(range(max(kk)+1)),"table gap")
    for kind,table in (("P",pp),("K",kk)):
        for item in table.values():
            n=item["n"];value=item["P"] if kind=="P" else item["D"]
            if kind=="K": require(n==1 or value==2*item["K"],"orbit multiplicity")
            require(0<value<=comb(2*n,n)//(n+1),("Catalan bound",kind,n))
            if item.get("disk") is not None: require(item["disk"]==value,("disk",n))
            if "fixed_cut_sectors" in item:
                sectors={int(i):v for i,v in item["fixed_cut_sectors"].items()}
                require(sum(sectors.values())==value and set(sectors)==set(range(n%2,n+1,2)),("heights",kind,n))
                for i,v in sectors.items():
                    t=(n-i)//2;b=comb(n,t)-(comb(n,t-1) if t else 0)
                    require(0<=v<=b*b,("height bound",kind,n,i))
                if kind=="P" or n%2==0: require(sectors[n%2]==closed[(n+1)//2],("lowest sector",kind,n))
            if kind=="P" and "evidence" in item:
                a=load(item["evidence"])
                for route in [a]+a["alternatives"]:
                    require(route["P"]==value,("alternative",n))
                    if "parts" not in route: continue
                    parts=route["parts"]
                    require(sum(x["count"] for x in parts)==value,("partial sums",n))
                    full=[h for x in parts for h in range(x["first_height"],x["last_height"]+1,2)]
                    require(len(full)==len(set(full)),("overlap",n))
                    end=n//3 if "minimum" in route["method"] else n
                    require(set(full)==set(range(n%2,end+1,2)) or set(full)==set(range(n%2,n+1,2)),("incomplete cover",n))
                    for x in parts:
                        raw=(ROOT/x["source"]).read_bytes()
                        require(sha256(raw).hexdigest()==x["source_sha256"],x["source"])
                        rec=json.loads(raw)
                        observed=rec["count"] if rec["n"]==n else rec["prefix_counts"][n]
                        require(rec["status"]=="exact" and x["count"]==observed,("measured projective contribution",n,x["source"]))
                        if x.get("build_manifest"):
                            require(sha256((ROOT/x["build_manifest"]).read_bytes()).hexdigest()==x["build_manifest_sha256"],"build manifest")
                        if "minimum" in route["method"] and x["first_height"]==x["last_height"]==n%2:
                            expected=2*log_derivative[n]-n*opened[n] if n%2 else n*closed[n//2]
                            require(x["count"]==expected,("minimum cut formula",n))
                        if n in terminal_rotations and "minimum" in route["method"]:
                            rr=terminal_rotations[n];h=x["first_height"]
                            require(h==x["last_height"] and (observed-rr["fixed_by_minimum"].get(str(h),0))%rr["prime"]==0,("sector rotation",n,h))
            if kind=="K" and "evidence" in item:
                a=load(item["evidence"])
                for h,x in a["by_height"].items():
                    for f,hsh in zip(x["results"],x["file_sha256"],strict=True):
                        src=ROOT/"experiments/runs/symmetric42"/f
                        record=json.loads(src.read_bytes())
                        require(sha256(src.read_bytes()).hexdigest()==hsh and record["value"]==x["value"],f)
                        if record.get("layer",{}).get("embedded"): require(verify_port_layer(record),("two terminal weights",f))
                        if record.get("lifted_seed"):
                            seed=record["lifted_seed"]
                            require(verify_seed(seed) and seed_signature(seed)==record["lifted_seed_sha256"],("checkpoint lift",f))
                for h,files in a.get("independent_FIFO_height_files",{}).items():
                    for f in files:
                        v=load("experiments/runs/symmetric42/"+f)
                        require(v["value"]==a["by_height"][h]["value"],("independent queue",n,h))
            if kind=="P" and n>2:
                cyc=load("data/projective_rotations.json")
                known=next((r for r in cyc["rows"] if r["n"]==n),None)
                if known: require(value%(2*n)==known["P_mod"],("rotation",n))
    verified=load("experiments/runs/residue42/verified.json")
    primary=records("experiments/runs/residue42/e39.jsonl")
    branches={r["branch"]:r["count"] for r in primary if "branch" in r}
    require(len(primary)==39 and set(branches)==set(range(1,39)) and sum(branches.values())==primary[-1]["E"],"38 involution branches")
    for suffix in ("head","middle","tail-independent"):
        for r in records("experiments/runs/residue42/e39-"+suffix+".jsonl"):
            if "branch" in r: require(r["count"]==branches[r["branch"]],("second branch run",r["branch"]))
    for r in verified["rows"]:
        n=r["n"];d=kk[(n+1)//2-1]["D"]
        require(r["P"]==pp[n]["P"] and r["D_lower"]==d and (pp[n]["P"]+d+2*(r["E"]+r["Q"]))%8==0,("self-duality action",n))
        if n%8!=1: require(r["Q"]==0,("quarter-turn restriction",n))
        if n==37: require(r["E"]==branches[20],"central deletion")
        if n==39: require(r["E"]==primary[-1]["E"] and verified["M39_mod312"]==16,"direct mod 312")
    fixed_moments={r["m"]:r["fixed"] for r in load("experiments/runs/modular41/rotation_halves.json")["midpoint"]}
    last=load("experiments/runs/closed_residues42/moment15.json")
    require(last["midpoint"][5]==last["forward"]["fixed"],"two moment transfers")
    fixed_moments[15]=last["midpoint"][5]
    for r in load("data/ordinary_residues.json"):
        n=r["n"];f=r["eight_group_nonidentity"]
        for d,a in r["rotation_fixed"].items(): require(a==fixed_moments[n//int(d)],("rotation moment",n,d))
        require(f["HX"]==pp[n]["P"] and f["LX"]==kk[n-1]["D"],"fixed input")
        require(f["H"]==reference("A000136")[n] and f["RX"]==reference("A060206")[(n-1)//2] and f["R"]==reference("A005316")[n],"published fixed sets")
        a=r["residue"];require(0<=a<8*n and (a+sum(f.values()))%8==0,"eight-element action")
        require((a+sum(sum(gcd(i,int(d))==1 for i in range(1,int(d)+1))*v for d,v in r["rotation_fixed"].items()))%n==0,"cyclic action")
        require(n not in closed or a==closed[n]%(8*n),"known ordinary integer")
        require((a+f["LX"])%4==0 and r["shape_residue"]==((a+f["LX"])//4)%(2*n),"shape congruence")
    for r in load("data/open_shapes.json"):
        b=r["crossings"]; t=b//2
        check=(1,0,1) if b==1 else (closed[t+1],kk[t]["D"],4) if b%2 else (reference("A005316")[b],reference("A005316")[t],2)
        require((r["labelled"],r["correction"],r["divisor"])==check and r["divisor"]*r["shapes"]==r["labelled"]+r["correction"],"shape value")
    sys.path.insert(0,str(ROOT/"scripts/rainbow42"))
    from certify import multiply,denominator,divide,phi
    full_leading=load("experiments/runs/symmetric42/fixed_check.json")
    for j,r in full_leading.items():
        base="experiments/runs/symmetric42/"
        require(sha256((ROOT/base/r["certificate_file"]).read_bytes()).hexdigest()==r["certificate_sha256"],("marked diagonal",j))
        leading=load(base+f"leading/j{int(j):02}.json")
        require(leading["leading_coefficient"]==r["common_leading_coefficient"] and leading["degree"]==r["degree"],("leading coefficient",j))
    candidates=load("experiments/runs/rainbow42/fifo/candidates.json")["candidates"]
    triangle=reference("A400429")
    for path in sorted((ROOT/"experiments/runs/rainbow42/arch").glob("j*.json")):
        a=json.loads(path.read_bytes());j=a["j"];v=a["values"];q=denominator(j);b=a["numerator_degree_bound"]
        require(len(v)>b and a["common_denominator"]==q and len(q)-1+4*j<=b,"degree bound")
        for n in range(2*j,len(v)):
            index=1+((n-1)**2//4)+(n//2-j+1)
            if index not in triangle: break
            require(v[n]==triangle[index],("public winding triangle",j,n))
        require(v[2*j]==closed[j] and v[2*j+1]==closed[j+1],("closed anchors",j))
        prod=multiply(v,a["reduced_denominator"])[:len(v)]
        require(prod==a["reduced_numerator"]+[0]*(len(prod)-len(a["reduced_numerator"])),"reduced fraction")
        den=a["reduced_denominator"][:]
        require(multiply(divide(q,den),den)==q,"divisor of the proved denominator")
        period=1
        for order in range(1,2*j+1):
            factor=phi(order);present=False
            while len(den)>=len(factor):
                try: quotient=divide(den,factor)
                except ValueError:break
                den=quotient;present=True
            if present:
                period=lcm(period,order)
                try: divide(a["reduced_numerator"],factor)
                except ValueError: pass
                else: raise ValueError(("uncancelled cyclotomic factor",j,order))
        require(den in ([1],[-1]),("complete pole factorization",j))
        if j in (12,13): require(period=={12:55440,13:720720}[j],("least eventual period",j))
        if str(j) in candidates:
            ref=candidates[str(j)]; mult=a["candidate_multiplier"]
            require(multiply(mult,ref["denominator"])==q,"exact denominator division")
            rhs=multiply([0]*(2*j)+[2*x for x in ref["numerator"]],mult)
            require(len(rhs)<=b+1 and multiply(v,q)[:len(v)]==rhs+[0]*(len(v)-len(rhs)),"published rational identity")
            require(v==load(f"experiments/runs/rainbow42/fifo/j{j:02}.json")["values"],"independent transfer")
        for n in range(2*j,min(12,len(v))):
            require(small_memo(n)[2][j]==v[n],("direct rainbow",n,j))
    for n in range(1,11):
        p,d,_=small_memo(n);require(p==pp[n]["P"] and d==kk[n-1]["D"],("direct Catalan oracle",n))
    check_oeis(pp,kk,refs)
    return pp,kk


small_memo=lru_cache(None)(small)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--projective",type=int,help="recompute a whole projective order")
    ap.add_argument("--symmetric",type=int,help="recompute a whole symmetric shape index")
    ap.add_argument("--diagonal",type=int,help="recompute the ordinary semi-meander certificate")
    ap.add_argument("--arch",action="store_true",help="choose the bounded arch transfer for a diagonal")
    ap.add_argument("--portable",action="store_true",help="force portable software bit extraction and deposit")
    ap.add_argument("--tables-only",action="store_true",help="skip the default sample C++ replay")
    args=ap.parse_args();pp,kk=check_all()
    default=not args.tables_only and all(i is None for i in (args.projective,args.symmetric,args.diagonal))
    with tempfile.TemporaryDirectory() as tmp:
        binary=Path(tmp)/"count"
        for kind,i,file in (("P",23 if default else args.projective,"projective42/minimum.cpp"),
                            ("K",23 if default else args.symmetric,"symmetric42/compact.cpp")):
            if i is None: continue
            require(i in (pp if kind=="P" else kk),"index outside included table")
            n=i if kind=="P" else i+1
            flags=["-O3","-std=c++20"]
            if kind=="P":
                macros=subprocess.run(["g++","-march=native","-dM","-E","-x","c++","-"],input=b"",capture_output=True)
                if not args.portable and macros.returncode==0 and b"#define __BMI2__ 1" in macros.stdout:
                    flags.append("-march=native")
                else:
                    flags+=["-I",str(ROOT/"scripts/portable_bits")]
            subprocess.run(["g++",*flags,ROOT/"scripts"/file,"-o",binary],check=True)
            if kind=="P":
                if n<=2: value=small_memo(n)[0]
                else: value=sum(json.loads(subprocess.check_output([binary,str(n),str(h)]))["count"]
                                for h in range(n%2,n//3+1,2))
                require(value==pp[i]["P"],"projective replay")
            else:
                value=sum(int(subprocess.check_output([binary,str(n),str(n),str(h)]).split()[2]) for h in range(n%2,n+1,2))
                require(value==kk[i]["D"],"symmetric replay")
            if kind=="K": print(f"K {i} {kk[i]['K']} (labelled D {value})")
            else: print("P",i,value)
        j=5 if default else args.diagonal
        if j is not None:
            use_arch=args.arch or j>11
            entry=load(f"experiments/runs/rainbow42/{'arch' if use_arch else 'fifo'}/j{j:02}.json")
            cmd=[sys.executable,ROOT/"scripts/rainbow42/certify.py",str(Path(tmp)/"replay"),str(j),
                 "--candidates",ROOT/"experiments/runs/rainbow42/fifo/candidates.json"]
            if use_arch: cmd.append("--arch")
            subprocess.run(cmd,check=True)
            replay=json.loads((Path(tmp)/"replay"/f"j{j:02}.json").read_bytes())
            require(entry["values"]==replay["values"],"diagonal replay")
    print(f"Verified P through {max(pp)}, K through {max(kk)}, exact diagonal certificates and ordinary residues.")

if __name__=="__main__": main()
