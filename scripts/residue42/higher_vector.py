"""Three restricted chord-word searches for the conditional mod-16 identity."""
import argparse
import json
from time import perf_counter


def search(n, kind):
    size = 2 * n
    p = [-1] * size
    edges = [[], []]
    calls = 0
    if kind == "Q1":
        p[0] = n
        p[n] = 0

    def visit():
        nonlocal calls
        calls += 1
        missing = [i for i, v in enumerate(p) if v == -1]
        if not missing:
            return 1
        # Give priority to completing chords which can reject a partial word.
        i = max(missing, key=lambda t: (p[t-1] >= 0) + (p[(t+1) % size] >= 0))
        total = 0
        for j in missing:
            if (i - j) % 2 != int(kind != "J0"):
                continue
            if kind == "Q1":
                si, sj = (-i) % size, (-j) % size
                assignments = ((i, j), (j, si), (si, sj), (sj, i))
            elif i == j:
                assignments = ((i, i),)
            else:
                assignments = ((i, j), (j, i))
            ends = set()
            for u, v in assignments:
                p[u] = v
                ends.update((u, (u - 1) % size))
            before = [len(layer) for layer in edges]
            for t in ends:
                u, v = sorted((p[t], p[(t + 1) % size]))
                if u < 0:
                    continue
                layer = edges[t % 2]
                if any((u < a < v) != (u < b < v) for a, b in layer):
                    break
                layer.append((u, v))
            else:
                total += visit()
            for level, length in enumerate(before):
                del edges[level][length:]
            for u, _ in assignments:
                p[u] = -1
        return total

    start = perf_counter()
    count = visit()
    return dict(count=count, nodes=calls, seconds=round(perf_counter() - start, 4))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("n", nargs="+", type=int)
    args = parser.parse_args()
    for order in args.n:
        if order < 3 or order % 2 == 0:
            parser.error("use odd n >= 3")
        print(json.dumps({"n":order, **{k:search(order,k) for k in ("J0","J1","Q1")}}), flush=True)
