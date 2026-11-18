"""Recompute the last even ordinary open input with Osipov's published solver."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import tempfile
from time import perf_counter

from run import DATA, ROOT, atomic, digest


def annotate(receipt, prior, previous, destination):
    cert = receipt['certificate']
    n, count = cert['n'], int(cert['count'])
    sequence = ROOT / 'references/oeis/2026-11-15/b005316.txt'
    values = dict(map(int,line.split()) for line in sequence.read_text().splitlines()
                  if line.strip() and not line.startswith('#'))
    if values[n] != count:
        raise ValueError('independent public cross-check')
    revision = receipt['source_commit']
    url = f'https://github.com/zouxuoz/first-crossing-meanders/tree/{revision}'
    receipt.update(author='Vladimir Osipov',source_url=url,
                   license='Apache-2.0',license_url=f'{url.replace("/tree/","/blob/")}/LICENSE',
                   build='cargo build --release --locked --manifest-path rust/Cargo.toml',
                   certificate_scope='Native exact count profile; no separate Lean replay of this run.',
                   cross_check=dict(sequence='A005316',url='https://oeis.org/A005316/b005316.txt',
                       index=n,count=values[n],b_file_sha256=digest(sequence),method='compared after the computation'))
    atomic(destination,receipt)
    evidence = [dict(path=p.relative_to(ROOT).as_posix(),sha256=digest(p)) for p in (previous,destination)]
    result = dict(n=n,count=count,open=[*prior['open'],count],status='exact',
                  algorithm='local first-crossing open endpoint',proof_inputs=evidence)
    atomic(DATA / f'open{n:02}.json',result)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('n', type=int)
    cli.add_argument('--threads', type=int, default=2)
    cli.add_argument('--source-dir',type=Path,default=ROOT / 'research/external/first-crossing-meanders_be0a3f5',
                     help='checkout of the pinned first-crossing-meanders release, with its release executable')
    cli.add_argument('--check-saved', action='store_true',help='validate the existing endpoint receipt and refresh derived inputs')
    args = cli.parse_args()
    n = args.n
    if n < 2 or n % 2:
        raise ValueError('an even positive open order is required')
    previous = DATA / f'open{n-1:02}.json'
    prior = json.loads(previous.read_bytes())
    if prior['n'] != n - 1 or len(prior['open']) != n:
        raise ValueError('incomplete preceding ordinary table')
    source = args.source_dir.resolve()
    exe = source / 'rust/target/release/meanders'
    manifest = source / 'LRWR_SOURCE_MANIFEST.json'
    revision = (json.loads(manifest.read_bytes())['commit'] if manifest.is_file() else
                subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip())
    if revision != 'be0a3f5e5d59d984171a2183dcf30acd1df46885':
        raise ValueError('unexpected first-crossing-meanders revision')
    flags = ['count','--problem','open','--algorithm','first_crossing_optimized',
             '--n', str(n),'--threads',str(args.threads),'--cert','count']
    dest = DATA / f'first_crossing_open{n:02}.json'
    if args.check_saved:
        receipt = json.loads(dest.read_bytes())
        cert = receipt['certificate']
        if (receipt['source_commit'] != revision or receipt['binary_sha256'] != digest(exe) or
            (cert['n'],cert['problem'],cert['representation']) != (n,'open','count') or not cert['root']):
            raise ValueError('saved first-crossing receipt')
        annotate(receipt,prior,previous,dest)
        return
    start = datetime.now(timezone.utc).isoformat()
    with tempfile.TemporaryDirectory() as temp:
        raw = Path(temp) / 'count.json'
        mem = Path(temp) / 'memory'
        cmd = ['/usr/bin/time','-f','%M','-o',str(mem),'prlimit','--as=2415919104',
               str(exe),*flags,'--out',str(raw)]
        timer = perf_counter()
        subprocess.run(cmd,timeout=3600,check=True)
        elapsed = perf_counter() - timer
        certificate = json.loads(raw.read_bytes())
        if certificate.get('n') != n or certificate.get('problem') != 'open' or not certificate.get('root'):
            raise ValueError(certificate)
        count = int(certificate['count'])
        receipt=dict(certificate=certificate,start_utc=start,elapsed_seconds=elapsed,
              rss_kib=int(mem.read_text()),binary_sha256=digest(exe),source_commit=revision,
              flags=flags,cap_mib=2304)
    annotate(receipt,prior,previous,dest)
    print(n,count,elapsed)


if __name__ == '__main__':
    main()
