"""Check explicit order-raising constructions for all allowed quarter-turn orders."""
import json
from check import matchings, shift_matching, traversal


def inflate(p, j, direction):
    length = len(p)
    h = length//2
    signs = {}
    for _ in range(4):
        signs[j] = signs[(j+h)%length] = direction
        j = p[j]
        direction *= -1
    assert len(signs) == 8
    blocks, t = [], 0
    for i in range(length):
        size = 3 if i in signs else 1
        blocks.append(list(range(t,t+size)))
        t += size
    lifted = []
    for i in range(length):
        lifted.extend(blocks[p[i]][::signs.get(i, 1)])
    return tuple(lifted)


def validate(p):
    length = len(p)
    n = length//2
    assert p[0] == 0 and sorted(p) == list(range(length))
    assert all(p[p[i]] == -i%length and p[(i+n)%length] == (p[i]+n)%length
               for i in range(length))
    for offset in (0,1):
        mate = [-1]*length
        for i in range(offset,length,2):
            x=p[i]
            y=p[(i+1)%length]
            mate[x],mate[y]=y,x
        stack = []
        for i,j in enumerate(mate):
            if j>i: stack.append(j)
            else: assert stack.pop()==i
        assert not stack


seeds = []
for a in matchings(9):
    p = traversal(a,shift_matching(a,9))
    if p and all(p[p[i]] == -i%18 for i in range(18)): seeds.append(p)
assert len(seeds)==2
total=0
for p in seeds:
    for j in range(1,18):
        if j==9: continue
        for direction in (-1,1):
            validate(inflate(p,j,direction))
            total+=1
p=seeds[0]
orders=[9]
for _ in range(20):
    p=inflate(p,1,1)
    validate(p)
    orders.append(len(p)//2)
print(json.dumps(dict(seed_order=9,seeds=seeds,tested_inflations_from_seed=total,
                      independently_verified_witness_orders=orders)))
