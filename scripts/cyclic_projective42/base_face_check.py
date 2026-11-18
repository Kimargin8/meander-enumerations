"""Recompute rotation-fixed minimum sectors from quotient matchings and their face trees."""
import argparse
from datetime import datetime, timezone
from hashlib import sha256
import json
from math import comb
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
from time import perf_counter

HERE = Path(__file__).resolve().parent
DATA = HERE.parents[1] / "experiments/runs/cyclic_projective42"
SOURCE = HERE / "base_faces.cpp"
RECEIPT = DATA / "base_faces.json"
COVER = DATA / "minimum_checked.json"
ORDERS = (44, 45, 46, 47)


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def cover_histograms():
    by_base = {}
    rows = json.loads(COVER.read_bytes())["orders"]
    require([r["n"] for r in rows] == list(ORDERS), "cover orders")
    for row in rows:
        n = row["n"]
        for item in row["rotations"]:
            d = item["d"]
            require(d >= 3 and d % 2 == 1 and n % d == 0, (n, d))
            m = n // d
            if m in by_base:
                require(by_base[m] == item["fixed_by_minimum"], (m, "cover dependence"))
            by_base[m] = item["fixed_by_minimum"]
    return by_base


def count():
    flags = ["-std=c++17", "-O3", "-Wall", "-Wextra", "-pedantic"]
    rows, elapsed = [], {}
    with TemporaryDirectory() as temp:
        exe = Path(temp) / "base-faces"
        subprocess.run(["g++", *flags, SOURCE, "-o", exe], check=True)
        for m in sorted(cover_histograms()):
            start = perf_counter()
            lines = subprocess.check_output([exe, str(m)], text=True).splitlines()
            measured = {}
            for line in lines[:3]:
                name, value = line.split()
                measured[name] = int(value)
            require(set(measured) == {"matchings", "connected", "even_gluings"}, m)
            counts = {}
            for line in lines[3:]:
                h, value = line.split()
                require(h not in counts and int(value) > 0, (m, line))
                counts[h] = int(value)
            rows.append(dict(m=m, **measured, fixed_by_minimum=counts))
            elapsed[str(m)] = round(perf_counter() - start, 6)
    return dict(rows=rows, elapsed_seconds=elapsed, verification_orders=ORDERS,
                cpp_sha256=digest(SOURCE), python_sha256=digest(Path(__file__)),
                cover_sha256=digest(COVER),
                compiler=subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
                flags=flags, utc=datetime.now(timezone.utc).isoformat())


def verify(document):
    require(document["cpp_sha256"] == digest(SOURCE), "C++ source")
    require(document["python_sha256"] == digest(Path(__file__)), "driver source")
    require(document["cover_sha256"] == digest(COVER), "cover evidence")
    require(tuple(document["verification_orders"]) == ORDERS, "orders")
    by_base = cover_histograms()
    require([r["m"] for r in document["rows"]] == sorted(by_base), "base list")
    prior = {r["n"]: r["reference"] for r in json.loads((DATA / "checked.json").read_bytes())["rows"]}
    for row in document["rows"]:
        m = row["m"]
        require(row["matchings"] == comb(2 * m, m) // (m + 1), ("Catalan", m))
        # Orders one and two are outside the range of the earlier orbit receipt.
        known = {1: 1, 2: 2}[m] if m < 3 else prior[m]
        require(row["connected"] == known, ("base projective", m))
        require(row["fixed_by_minimum"] == by_base[m], ("cover histogram", m))
        marks = m + 1 if m % 2 else m // 2 + 1
        require(sum(row["fixed_by_minimum"].values()) == marks * row["connected"], ("marks", m))
        require(row["even_gluings"] == (0 if m % 2 else row["connected"]), ("even sides", m))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--compute", action="store_true", help="compute and save quotient evidence")
    mode.add_argument("--replay", action="store_true", help="recount every quotient without changing evidence")
    args = parser.parse_args()
    original = count() if args.compute else json.loads(RECEIPT.read_bytes())
    verify(original)
    if args.compute:
        temp = RECEIPT.with_suffix(".tmp")
        temp.write_text(json.dumps(original, indent=2) + "\n", encoding="utf-8")
        temp.replace(RECEIPT)
    elif args.replay:
        again = count()
        verify(again)
        require(again["rows"] == original["rows"], "quotient integer replay")
    for row in original["rows"]:
        print(row["m"], row["matchings"], row["connected"], row["fixed_by_minimum"])


if __name__ == "__main__":
    main()
