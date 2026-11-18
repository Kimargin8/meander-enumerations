"""Check the public sequence endpoints and compare the current public repository with the reviewed revision."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from hashlib import sha256
import json
from pathlib import Path
from urllib.request import Request, urlopen

ROOT=Path(__file__).resolve().parents[2]
REVIEWED="0deb2b05a83b84b979262986984c34808e111aca"
REPO="hunterhogan/mapFolding"
SEQUENCES=("A006663","A223096","A005315","A077055","A400429")


def get(url):
    with urlopen(Request(url,headers={"User-Agent":"meander-table-reproducibility/1.0"}),timeout=30) as response:
        return response.read()


def endpoint(item):
    seq,url=item
    raw=get(url)
    rows=[tuple(map(int,s.split())) for s in raw.splitlines() if s.strip() and not s.lstrip().startswith(b"#")]
    if any(b[0]!=a[0]+1 for a,b in zip(rows,rows[1:])):raise ValueError(("public table gap",seq))
    return seq,dict(url=url,first=rows[0][0],last=rows[-1][0],last_value=rows[-1][1],terms=len(rows),sha256=sha256(raw).hexdigest())


def main():
    base="https://api.github.com/repos/"+REPO
    head=json.loads(get(base+"/commits/main"))
    sha=head["sha"]
    changes=json.loads(get(base+"/compare/"+REVIEWED+"..."+sha))
    assert changes["status"] in ("ahead","identical")
    release=json.loads(get("https://pypi.org/pypi/mapFolding/json"))
    wheel=next(f for f in release["urls"] if f["packagetype"]=="bdist_wheel")
    official=[(s,f"https://oeis.org/{s}/b{s[1:]}.txt") for s in SEQUENCES]
    supplement=[(s,f"https://raw.githubusercontent.com/{REPO}/{sha}/mapFolding/oeis/terms/files/{s[:4]}/b{s[1:]}.txt") for s in ("A223096","A077055","A400429")]
    with ThreadPoolExecutor(6) as pool:
        a=dict(pool.map(endpoint,official));b=dict(pool.map(endpoint,supplement))
    record=dict(checked_utc=datetime.now(timezone.utc).isoformat(timespec="seconds"),official=a,repository_supplements=b,
                reviewed_commit=REVIEWED,commit=sha,commit_utc=head["commit"]["committer"]["date"],
                compare_url=f"https://github.com/{REPO}/compare/{REVIEWED}...{sha}",compared_commits=changes["total_commits"],
                changed_files=[f["filename"] for f in changes["files"]],
                pypi=dict(version=release["info"]["version"],wheel_sha256=wheel["digests"]["sha256"],
                          uploaded=wheel["upload_time_iso_8601"],url=release["info"]["package_url"]))
    target=ROOT/"experiments/runs/priority42/final_tables.json"
    tmp=target.with_suffix(".tmp")
    tmp.write_bytes((json.dumps(record,indent=2)+"\n").encode())
    tmp.replace(target)
    print(sha,{s:r["last"] for s,r in a.items()},{s:r["last"] for s,r in b.items()})


if __name__=="__main__":main()
