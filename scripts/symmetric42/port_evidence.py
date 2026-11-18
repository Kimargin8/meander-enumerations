"""Verify a saved two-port certificate without C++, using the embedded weighted layer."""
import base64
from functools import lru_cache
from hashlib import sha256
import struct
import zlib

MASK=(1<<64)-1

@lru_cache()
def field_width(n,h):
    maximum=1
    for t in range(n):
        a=n-1-t
        for u in range((t+1)%2,t+2,2):
            for v in range(t%2,t+1,2):
                if abs(u-h)>a or abs(v-h)>a+1:continue
                q=max(0,min((u+h-a)//2,(v+h-a-1)//2))
                maximum=max(maximum,u+v-2*q)
    return maximum+1

def mix(x):
    x=((x^(x>>30))*0xbf58476d1ce4e5b9)&MASK
    x=((x^(x>>27))*0x94d049bb133111eb)&MASK
    return (x^(x>>31))&MASK

def embed(path,max_bytes=4<<20):
    size=path.stat().st_size
    if size>max_bytes:
        d=sha256()
        with path.open("rb") as src:
            for block in iter(lambda:src.read(1<<20),b""):d.update(block)
        return dict(size=size,sha256=d.hexdigest(),embedded=False)
    raw=path.read_bytes()
    return dict(size=size,sha256=sha256(raw).hexdigest(),embedded=True,encoding="zlib+base64",
                data=base64.b64encode(zlib.compress(raw,9)).decode("ascii"))

def verify(row):
    N,H,t=row["source_N"],row["source_H"],row["t"]
    assert N-H==2*row["j"]==row["n"]-row["h"] and row["n"]==t+1
    b=row["layer"];assert b["embedded"] and b["encoding"]=="zlib+base64"
    raw=zlib.decompress(base64.b64decode(b["data"],validate=True))
    assert len(raw)==b["size"] and sha256(raw).hexdigest()==b["sha256"]
    width=field_width(N,H);a=N-1-t
    keys=[2|(1<<width)|((a+1)<<(width+12)),2|(a<<(width+12))]
    found={};p=seen=digest=0;last=-N-1
    while p<len(raw):
        delta=raw[p]-N;cnt=struct.unpack_from("<Q",raw,p+1)[0];p+=9
        assert last<delta<=N and cnt>0
        last=delta
        for _ in range(cnt):
            key=struct.unpack_from("<Q",raw,p)[0];p+=8;w=0
            for shift in range(0,127,7):
                byte=raw[p];p+=1;w|=(byte&127)<<shift
                if byte<128:break
            else:raise ValueError("unterminated weight")
            assert 0<w<1<<128
            digest=(digest+mix(key^mix(w&MASK)^mix(((w>>64)+12345)&MASK)))&MASK
            if key in keys:
                which=keys.index(key)
                assert key not in found and delta==(1,-1)[which]
                found[key]=w
            seen+=1
    assert seen==row["input_states"] and digest==row["layer_digest64"]
    assert (found.get(keys[0],0),found.get(keys[1],0))==(row["left"],row["right"])
    assert row["left"]+row["right"]==row["value"]
    return True
