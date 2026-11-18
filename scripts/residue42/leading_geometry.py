"""Leading marked diagonals by literal chord diagrams for every gap residue."""
from argparse import ArgumentParser
from fractions import Fraction
from itertools import combinations, product
from math import factorial, prod
import json


def connected(sides, gaps):
    # The signs record the side on which the next adjacent chord is placed.
    # The exterior contraction is L_i = v_i and R_i = v_{i+1}.
    t = 1
    local = [set(), set()]
    edges = []
    for side, gap in zip(sides, gaps):
        t += gap
        edges.append((t, t+1))
        a = t-side
        local[side].update((a, a+1))
        t += 2
    n = t + gaps[-1]
    left = (a for a in range(n) if a not in local[0])
    right = (a+1 for a in range(n) if a not in local[1])
    edges.extend(zip(left, right))
    graph = [[] for _ in range(n+1)]
    for a,b in edges:
        graph[a].append(b)
        graph[b].append(a)
    seen = {0}
    stack = [0]
    while stack:
        for v in graph[stack.pop()]:
            if v not in seen:
                seen.add(v)
                stack.append(v)
    return len(seen) == n+1


def evaluate(j):
    total = Fraction(0)
    diagrams = 0
    for left in combinations(range(2*j),j):
        sides = [int(i not in left) for i in range(2*j)]
        d = 1
        periods = [d]
        for side in sides:
            d += 2 if side else -2
            periods.append(abs(d))
        width = prod(periods)
        diagrams += width
        accepted = sum(connected(sides, gaps)
                       for gaps in product(*(range(k) for k in periods)))
        total += Fraction(accepted,width)
    return dict(j=j, diagrams=diagrams, leading=str(total/factorial(2*j)))


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("end", type=int)
    args = parser.parse_args()
    for j in range(args.end+1):
        print(json.dumps(evaluate(j)), flush=True)
