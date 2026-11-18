"""Decode or fingerprint the optional system-Zstandard layer transport."""
import argparse
import base64
import ctypes as C
from hashlib import sha256
import os
from pathlib import Path
import struct
import zlib
from port_evidence import embed as embed_plain

MAGIC=b"\xffLRWRZ01"
BLOCK=1<<20


def version():
    library=C.CDLL("libzstd.so.1")
    library.ZSTD_versionString.restype=C.c_char_p
    return library.ZSTD_versionString().decode("ascii")


def blocks(path):
    with path.open("rb") as f:
        if f.read(len(MAGIC))!=MAGIC:
            f.seek(0)
            yield from iter(lambda:f.read(BLOCK),b"")
            return
        lib=C.CDLL("libzstd.so.1")
        decode=lib.ZSTD_decompress
        decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t];decode.restype=C.c_size_t
        output=C.create_string_buffer(BLOCK)
        while header:=f.read(8):
            if len(header)!=8:raise ValueError("truncated packed header")
            raw,length=struct.unpack("<II",header)
            if not 0<raw<=BLOCK or not 0<length<2*BLOCK:raise ValueError("invalid packed block")
            data=f.read(length)
            if len(data)!=length or decode(output,BLOCK,data,length)!=raw:raise ValueError("invalid Zstandard frame")
            yield output.raw[:raw]


def embed(path,max_bytes=4<<20):
    with path.open("rb") as f:
        if f.read(8)!=MAGIC:return embed_plain(path,max_bytes)
    digest=sha256();size=0;data=bytearray()
    for part in blocks(path):
        digest.update(part);size+=len(part)
        if size<=max_bytes:data.extend(part)
        elif data:data.clear()
    result=dict(size=size,sha256=digest.hexdigest(),embedded=size<=max_bytes,storage="blocked-zstd-v1")
    if result["embedded"]:
        result.update(encoding="zlib+base64",data=base64.b64encode(zlib.compress(data,9)).decode("ascii"))
    return result


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("source",type=Path);p.add_argument("target",type=Path)
    a=p.parse_args()
    if a.source.resolve()==a.target.resolve():p.error("select a separate output file")
    a.target.parent.mkdir(parents=True,exist_ok=True)
    tmp=a.target.with_name(a.target.name+".pending")
    h=sha256()
    with tmp.open("wb") as out:
        for part in blocks(a.source):out.write(part);h.update(part)
    os.replace(tmp,a.target)
    print(h.hexdigest(),a.target)
