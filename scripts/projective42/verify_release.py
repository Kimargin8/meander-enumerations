"""Validate every assembled disjoint cover directly from its frozen receipts."""
import hashlib
import json
import argparse
from pathlib import Path
from tempfile import NamedTemporaryFile

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/"experiments/runs/projective42"


def checked(relative, expected):
    path=ROOT/relative
    raw=path.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=expected:
        raise ValueError(f"Digest mismatch: {relative}")
    return json.loads(raw)


def local_open_values(local):
    table = local['open']
    if len(table) != local['n'] + 1 or table[-1] != local['count']:
        raise ValueError('local open table length or endpoint')
    if 'proof_inputs' not in local:
        return table
    preceding, endpoint = local['proof_inputs']
    small = local_open_values(checked(preceding['path'],preceding['sha256']))
    plane = checked(endpoint['path'],endpoint['sha256'])
    if local['algorithm'] == 'odd open equals closed':
        if int(plane.get('closed_order',plane.get('n'))) * 2 - 1 != local['n']:
            raise ValueError('ordinary closed order')
        last = int(plane['answer'])
    elif local['algorithm'] == 'local first-crossing open endpoint':
        cert = plane['certificate']
        if (cert['problem'],cert['n'],cert['representation']) != ('open',local['n'],'count') or not cert['root']:
            raise ValueError('ordinary open first-crossing receipt')
        last = int(cert['count'])
    else:
        raise ValueError('unknown local open derivation')
    if [*small,last] != table:
        raise ValueError('ordinary open endpoint disagreement')
    return table


def verify(n, candidate):
    parts=candidate.get("parts")
    if parts is None:
        row=checked(candidate["source"],candidate["source_sha256"])
        if row["status"]!="exact" or row["value"]!=candidate["P"]:
            raise ValueError("disk surgery disagreement")
        return
    minimum=candidate["height_semantics"].startswith("minimum over")
    limit=n//3 if minimum else n
    seen=[]
    value=0
    for x in parts:
        row=checked(x["source"],x["source_sha256"])
        if row["status"]!="exact": raise ValueError("incomplete operand")
        if x["build_manifest"]:
            b=checked(x["build_manifest"],x["build_manifest_sha256"])
            if b["binary_sha256"]!=x["binary_sha256"]: raise ValueError("binary metadata")
        lo=x["first_height"]; hi=x["last_height"]
        if lo!=row.get("first_height",row["h"]) or hi!=min(row["h"],n):
            raise ValueError("bad sector labelling")
        if row["n"]<n or row["n"]!=x["horizon"]:raise ValueError("horizon")
        v=row["count"] if row["n"]==n else row["prefix_counts"][n]
        if v!=x["count"]:raise ValueError("sector value")
        if "formula" in row:
            inp=row["input"]
            raw=(ROOT/inp["path"]).read_bytes()
            if hashlib.sha256(raw).hexdigest()!=inp["sha256"]:raise ValueError("formula input")
            if row["external_tabulated_input"]:
                table={int(s.split()[0]):int(s.split()[1]) for s in raw.decode().splitlines()
                       if s.strip() and not s.startswith("#")}
            else:
                table=local_open_values(json.loads(raw))
            if row["h"]==0:
                z=n*table[row["input_index"]]
            else:
                vals=[0]*(n+1)
                for i in range(1,n+1):vals[i]=i*table[i]-sum(table[j]*vals[i-j] for j in range(1,i))
                z=2*vals[n]-n*table[n]
            if z!=v:raise ValueError("formula value")
        value+=v
        seen.extend(range(lo,hi+1,2))
    if seen!=list(range(n%2,limit+1,2)) or value!=candidate["P"]:
        raise ValueError(f"disjoint cover disagreement at {n}")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--allow-pending',action='store_true',help='also verify completed high orders while the opposite parity is running')
    allow_pending=parser.parse_args().allow_pending
    papers={}
    rotations={r['n']:r for r in json.loads((BASE/'rotation_sectors.json').read_bytes())['rows']}
    for path in sorted((BASE/"assembled").glob("n*.json")):
        raw=path.read_bytes()
        doc=json.loads(raw);n=doc["n"]
        verify(n,doc)
        for other in doc["alternatives"]:
            if other["P"]!=doc["P"]:raise ValueError("alternative total")
            verify(n,other)
        papers[n]={"P":doc["P"],"passport_sha256":hashlib.sha256(raw).hexdigest(),
                   "complete_covers":1+len(doc["alternatives"])}
        if n in rotations:
            rotation=rotations[n]
            residue=-sum(item['phi']*item['fixed'] for item in rotation['rotation_total_inputs'])%(2*n)
            if residue!=rotation['P_mod_2n'] or doc['P']%(2*n)!=residue:
                raise ValueError(f'rotation orbit total at {n}')
            checks=0
            for route in [doc,*doc['alternatives']]:
                if not route.get('height_semantics','').startswith('minimum over'):continue
                for term in route['parts']:
                    h=term['first_height']
                    expected=rotation['fixed_by_minimum'].get(str(h),0)
                    if h!=term['last_height'] or (term['count']-expected)%rotation['prime']:
                        raise ValueError(f'rotation orbit size in P{n} minimum {h}')
                    checks+=1
            papers[n]['rotation_orbit_checks']=dict(total_modulus=2*n,total_residue=residue,
                                           checked_minimum_sectors=checks,sector_modulus=rotation['prime'])
    missing=[n for n in range(31,max(papers)+1) if n not in papers]
    if missing and not allow_pending:raise ValueError("gap in reported orders")
    result={"min_n":31,"max_n":max(papers),"orders":papers,"status":"verified"}
    if missing:result['pending_orders']=missing
    with NamedTemporaryFile(mode="w",dir=BASE,delete=False,encoding="utf-8") as f:
        json.dump(result,f,indent=2);f.write("\n");tmp=Path(f.name)
    tmp.replace(BASE/('certificate_partial.json' if missing else 'certificate.json'))
    if missing:print(f"Verified completed orders to P{max(papers)}; pending {missing}")
    else:print(f"Verified all P31..P{max(papers)} passports")

if __name__=="__main__":
    main()
