"""Exact fixed-deficit generating functions for symmetric open meanders."""
from argparse import ArgumentParser
from collections import defaultdict
from functools import cache
from hashlib import sha256
import json
from math import lcm
from pathlib import Path


@cache
def moves(state, j):
    opened_l, opened_r, nl, nr, delta, mates = state
    m = len(mates)
    # The old path endpoints, its distinguished root, then the two new vertices.
    root, lv, rv = m, m + 1, m + 2
    def options(depth, opened):
        if opened < j:
            yield "open"
        yield "close" if depth else "through"
    out = []
    for a in options(nl, opened_l):
        for b in options(nr, opened_r):
            p = list(range(m + 3))
            def find(x):
                while p[x] != x:
                    x = p[x]
                return x
            def link(x, y):
                x, y = find(x), find(y)
                if x == y:
                    return False
                p[x] = y
                return True
            for i, q in enumerate(mates):
                if q < 0:
                    link(i, root)
                elif i < q:
                    link(i, q)
            link(lv, rv)
            stacks = [list(range(nl)), list(range(nl, nl + nr))]
            waiting = list(range(nl + nr, m))
            d = delta
            for side, event, vertex in [(0, a, lv), (1, b, rv)]:
                sign = 1 if side == 0 else -1
                if event == "open":
                    stacks[side].append(vertex)
                elif event == "close":
                    if not link(vertex, stacks[side].pop()):
                        break
                else:
                    if d * sign < 0:
                        if not link(vertex, waiting.pop(0)):
                            break
                    else:
                        waiting.append(vertex)
                    d += sign
            else:
                ports = stacks[0] + stacks[1] + waiting
                groups = defaultdict(list)
                for idx, node in enumerate(ports):
                    groups[find(node)].append(idx)
                word = [-1] * len(ports)
                for component, ids in groups.items():
                    if component == find(root):
                        assert len(ids) == 1
                    else:
                        x, y = ids
                        word[x], word[y] = y, x
                assert word.count(-1) == 1
                out.append((opened_l + (a == "open"), opened_r + (b == "open"),
                            len(stacks[0]), len(stacks[1]), d, tuple(word)))
    return tuple(out)


def coefficients(j, last):
    # L0, the first exceptional vertex, starts the unique marked path.
    weights = {(0, 0, 0, 0, 1, (-1,)): 1}
    if j:
        weights[(1, 0, 1, 0, 0, (-1,))] = 1
    values = [0]
    peak = 0
    for n in range(1, last + 1):
        peak = max(peak, len(weights))
        # The terminal vertex R_(n-1) must exhaust the last path port.
        value = sum(v for (ol, or_, nl, nr, d, ports), v in weights.items()
                    if ol == or_ == j and nl == 0 and len(ports) == 1
                    and (nr == 1 or d == 1))
        values.append(value)
        nxt = defaultdict(int)
        for state, weight in weights.items():
            for target in moves(state, j):
                nxt[target] += weight
        weights = nxt
    return values, peak


def multiply(a, b):
    out = [0] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        if x:
            for k, y in enumerate(b):
                out[i+k] += x*y
    return out


def divide_monic(a, b):
    a = list(a)
    q = [0] * (len(a)-len(b)+1)
    for k in range(len(q)-1,-1,-1):
        q[k]=a[k+len(b)-1]
        for i,x in enumerate(b):
            a[k+i]-=q[k]*x
    assert not any(a)
    return q


@cache
def cyclotomic(m):
    a = [-1] + [0]*(m-1) + [1]
    for d in range(1,m):
        if m%d == 0:
            a=divide_monic(a,cyclotomic(d))
    return a


def rational(j, extra):
    period = lcm(*range(1, 2*j+2, 2))
    den=[1]
    powers={1:2*j+1}
    powers.update({2*k+1:j+1-k for k in range(1,j+1)})
    for order,power in powers.items():
        factor=[1,-1] if order==1 else cyclotomic(order)
        for _ in range(power):
            den=multiply(den,factor)
    max_degree = 4*j+len(den)
    values, peak = coefficients(j, max_degree + extra)
    shifts = [(k,c) for k,c in enumerate(den) if c]
    numerator = [sum(c*values[i-sh] for sh,c in shifts if sh <= i)
                 for i in range(len(values))]
    assert not any(numerator[max_degree+1:])
    while numerator and numerator[-1] == 0:
        numerator.pop()
    result = dict(j=j, period_bound=period, denominator_cyclotomic_powers=powers,
                  denominator_lowest_power_first=den,
                  proven_numerator_degree_bound=max_degree,
                  numerator_lowest_power_first=numerator,
                  coefficients_calculated_through_n=len(values)-1, peak_states=peak,
                  source_sha256=sha256(Path(__file__).read_bytes()).hexdigest())
    try:
        import sympy as sy
    except ImportError:
        return result
    z=sy.Symbol("z")
    num=sy.Poly.from_list(numerator[::-1],z)
    den=sy.Poly.from_list(den[::-1],z)
    common=sy.gcd(num,den)
    num,den=sy.div(num,common)[0],sy.div(den,common)[0]
    scale=den.eval(0)
    result["reduced_numerator_lowest_power_first"]=[int(c/scale) for c in num.all_coeffs()[::-1]]
    result["reduced_denominator_factored"]=str(sy.factor(den.as_expr()/scale))
    return result


if __name__ == "__main__":
    ap = ArgumentParser(description=__doc__)
    ap.add_argument("j", type=int)
    ap.add_argument("output", type=Path)
    ap.add_argument("--extra", type=int, default=15)
    args = ap.parse_args()
    document = rational(args.j, args.extra)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document,indent=2) + "\n",encoding="utf8")
