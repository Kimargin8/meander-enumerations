"""Check the self-contained provenance of a lifted wide checkpoint."""
from hashlib import sha256
import json


def signature(value):
    return sha256(json.dumps(value,sort_keys=True,separators=(",",":")).encode()).hexdigest()


def verify_seed(seed):
    old,extra,new=[seed[x] for x in ("shorter","complement","output")]
    n,h,t=new["metadata"][:3]
    N,H,T=old["metadata"][:3]
    assert N<n and t==T< N and n-h==N-H
    assert extra["metadata"][:3]==[n,h,t] and seed["deficit"]==(n-h)//2
    assert new["metadata"][3]==old["metadata"][3]+extra["metadata"][3]
    assert new["metadata"][6]==old["metadata"][6]+extra["metadata"][6]
    assert seed["shorter_opening_budget"]==N-seed["deficit"]
    assert seed["left_closing_threshold"]==t-seed["shorter_opening_budget"]
    assert seed["right_closing_threshold"]==seed["left_closing_threshold"]-1
    for item in (old,extra,new):
        assert len(item["metadata"])==9 and item["size"]>=0
        assert bool(item["size"])==bool(item["metadata"][3])
        for name in ("layer_sha256","metadata_sha256"):
            assert len(item[name])==64 and int(item[name],16)>=0
        canonical=(" ".join(map(str,item["metadata"]))+"\n").encode()
        assert sha256(canonical).hexdigest()==item["metadata_sha256"]
    assert signature(seed["source_files"])==seed["sources_sha256"]
    return True
