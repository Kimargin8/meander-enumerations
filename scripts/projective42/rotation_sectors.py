"""Predict residues in every terminal minimum-height class by small cyclic covers."""
from collections import Counter
from itertools import combinations
import json
from math import comb
from pathlib import Path
from run import DATA,ROOT,atomic,digest


def fixed_sectors(n, prime):
    # A fundamental interval for this odd-order rotation has length 2n/p.
    block = 2 * n // prime
    length = 2 * n
    half = (1 << n) - 1
    masks = [((half << i) | ((half << i) >> length)) & ((1 << length)-1)
             for i in range(block)]
    histogram = Counter()
    for plus in combinations(range(block), block // 2):
        opens = set(plus)
        level = bottom = cut = 0
        for i in range(block):
            level += 1 if i in opens else -1
            if level < bottom:
                bottom,cut = level,i+1
        stack,arcs = [],[]
        mates = [0] * length
        for t in range(cut, cut + block):
            if t % block in opens:
                stack.append(t)
                continue
            left = stack.pop()
            for k in range(prime):
                a,b = (left + k*block) % length,(t + k*block) % length
                mates[a],mates[b] = b,a
                arcs.append((1 << a) | (1 << b))
        v = steps = 0
        while True:
            v = (mates[v]+n) % length
            steps += 1
            if not v: break
        if steps != n: continue
        height = min(sum((arc & mask).bit_count() == 1 for arc in arcs) for mask in masks)
        histogram[height] += 1
    return dict(sorted(histogram.items()))


def main():
    prior = ROOT / 'experiments/runs/cyclic_projective42/checked.json'
    rotations = {r['n']:r for r in json.loads(prior.read_bytes())['rows']}
    checks = []
    for n,prime in [(44,11),(45,5),(46,23),(47,47)]:
        counts = fixed_sectors(n,prime)
        known = next(x['fixed'] for x in rotations[n]['rotations'] if x['d']==prime)
        if sum(counts.values()) != known: raise ValueError((n,counts,known))
        checks.append(dict(n=n,prime=prime,fundamental_words=comb(2*n//prime,n//prime),
                           fixed_total=known,fixed_by_minimum=counts,P_mod_2n=rotations[n]['P_mod'],
                           rotation_total_inputs=[{k:v[k] for k in ('d','phi','fixed')}
                                                  for v in rotations[n]['rotations']]))
    result = dict(method='Cyclic subgroup of prime order acting on labelled diagrams within each minimum class',
                  rotation_totals_source=prior.relative_to(ROOT).as_posix(),rotation_totals_sha256=digest(prior),
                  code_sha256=digest(Path(__file__)),rows=checks)
    atomic(DATA/'rotation_sectors.json',result)
    print(checks)


if __name__=='__main__':main()
