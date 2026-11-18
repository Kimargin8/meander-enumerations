"""Rotation-orbit checks of labelled projective minimum sectors, orders 44 to 47."""
import argparse
from collections import defaultdict
from datetime import datetime, timezone
from hashlib import sha256
import json
from math import comb, gcd
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
from time import perf_counter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DATA = ROOT / "experiments/runs/cyclic_projective42"
OUT = DATA / "minimum_checked.json"
SOURCE = HERE / "minimum_fixed.cpp"
ORDERS = (44, 45, 46, 47)


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def odd_orders(n):
    return [d for d in range(3, n + 1, 2) if n % d == 0]


def restore(n, d, rooted_bins):
    block = 2 * n // d
    histogram = defaultdict(int)
    for h, k, count in rooted_bins:
        require(0 <= h <= n // 3 and h % 2 == n % 2, (n, d, h))
        require(1 <= k <= block // 2 and count > 0, (n, d, k, count))
        labelled, rem = divmod(block * count, k)
        require(rem == 0, (n, d, h, k))
        histogram[str(h)] += labelled
    return dict(sorted(histogram.items(), key=lambda item: int(item[0])))


def prediction(n, rotations):
    require([r["d"] for r in rotations] == odd_orders(n), ("rotation orders", n))
    totals = defaultdict(int)
    for rot in rotations:
        d = rot["d"]
        base = n // d
        require(rot["dyck_words"] == comb(2 * base, base) // (base + 1), (n, d, "Catalan count"))
        distribution = restore(n, d, rot["rooted_bins"])
        require(distribution == rot["fixed_by_minimum"], (n, d, "fixed bins"))
        phi = sum(gcd(d, g) == 1 for g in range(1, d + 1))
        for h, fixed in distribution.items():
            totals[int(h)] -= phi * fixed
    return {str(h): totals[h] % (2 * n) for h in range(n % 2, n // 3 + 1, 2)}


def compute():
    flags = ["-std=c++17", "-O3", "-Wall", "-Wextra", "-pedantic"]
    rows = []
    with TemporaryDirectory() as temp:
        exe = Path(temp) / "minimum-fixed"
        subprocess.run(["g++", *flags, SOURCE, "-o", exe], check=True)
        for n in ORDERS:
            rotations = []
            for d in odd_orders(n):
                start = perf_counter()
                lines = subprocess.check_output([exe, str(n), str(d)], text=True).splitlines()
                label, count = lines[0].split()
                require(label == "dyck_words", (n, d, lines[0]))
                cells = [list(map(int, line.split())) for line in lines[1:]]
                rotations.append(dict(d=d, dyck_words=int(count), rooted_bins=cells,
                                      fixed_by_minimum=restore(n, d, cells),
                                      elapsed_seconds=round(perf_counter() - start, 6)))
            rows.append(dict(n=n, modulus=2 * n, rotations=rotations,
                             sectors=prediction(n, rotations)))
    return dict(orders=rows, cpp_sha256=digest(SOURCE), python_sha256=digest(Path(__file__)),
                compiler=subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
                flags=flags, utc=datetime.now(timezone.utc).isoformat())


def verify(data, assembled):
    require(data["cpp_sha256"] == digest(SOURCE), "unexpected source hash")
    require(data["python_sha256"] == digest(Path(__file__)), "unexpected driver hash")
    require([r["n"] for r in data["orders"]] == list(ORDERS), "campaign orders")
    prior = {r["n"]: r for r in json.loads((DATA / "checked.json").read_bytes())["rows"]}
    checked = []
    for row in data["orders"]:
        n = row["n"]
        mod = 2 * n
        expect = prediction(n, row["rotations"])
        require(row["modulus"] == mod and expect == row["sectors"], ("residues", n))
        fixed = {r["d"]: sum(r["fixed_by_minimum"].values()) for r in row["rotations"]}
        require(fixed == {r["d"]: r["fixed"] for r in prior[n]["rotations"]}, ("fixed totals", n))
        require(sum(expect.values()) % mod == prior[n]["P_mod"], ("total residue", n))
        path = assembled / f"n{n}.json"
        if path.is_file():
            doc = json.loads(path.read_bytes())
            require(doc["n"] == n and doc["status"] == "exact", ("order", n))
            require("minimum" in doc["height_semantics"], ("fixed cut cannot be used", n))
            seen = set()
            for part in doc["parts"]:
                h = str(part["first_height"])
                require(int(h) == part["last_height"] and h not in seen, ("coarse or repeated sector", n))
                require(part["count"] % mod == expect[h], ("sector checksum", n, h))
                seen.add(h)
            require(seen == set(expect), ("incomplete partition", n))
            require(sum(p["count"] for p in doc["parts"]) == doc["P"], ("sum", n))
            checked.append(n)
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--compute", action="store_true", help="compute and save the fixed histograms in one thread")
    mode.add_argument("--replay", action="store_true", help="recompute every integer and keep the original receipt")
    parser.add_argument("--assembled", type=Path,
                        default=ROOT / "experiments/runs/projective42/assembled")
    args = parser.parse_args()
    data = compute() if args.compute else json.loads(OUT.read_bytes())
    checked = verify(data, args.assembled)
    if args.replay:
        repeat = compute()
        verify(repeat, args.assembled)
        for result in [data["orders"], repeat["orders"]]:
            for row in result:
                for rotation in row["rotations"]:
                    del rotation["elapsed_seconds"]
        require(repeat["orders"] == data["orders"], "replay of rooted counts and labelled sectors")
    if args.compute:
        temp = OUT.with_suffix(".tmp")
        temp.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        temp.replace(OUT)
    for row in data["orders"]:
        print(row["n"], row["modulus"], row["sectors"])
    print("complete sector partitions checked:", checked)


if __name__ == "__main__":
    main()
