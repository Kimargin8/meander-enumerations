# Reproducing the meander enumerations

At disk order 3 there are five noncrossing matchings on labels 0 through 5. The table gives a small example that can be checked by following the edges. The projective column adds the pairs 03, 14 and 25, and asks whether there is one circuit. The marked column adds 15 and 24, and asks whether the path from 0 to 3 covers every label.

| Matching | Projective circuit | Marked path |
|---|---|---|
| 01, 23, 45 | yes | yes |
| 01, 25, 34 | no | yes |
| 03, 12, 45 | no | no |
| 05, 12, 34 | yes | yes |
| 05, 14, 23 | no | yes |

Thus P(3) = 2, D(3) = 4 and K(2) = D(3)/2 = 2. This last index corresponds to five crossings in the open shape.

## Read the data

* **data/projective.json** contains P, the fixed-cut sectors where available, and paths to detailed evidence. In the supporting records, a fixed-cut height and the minimum height over every rotated cut are different statistics. Their respective totals count the same labelled objects.
* **data/symmetric.json** contains the shape index k, the disk order n=k+1, K and its labelled count D. The one-crossing base case is explicit.
* **data/open_shapes.json** lists physical crossing counts, the labelled and symmetric inputs, and the divisor. **data/ordinary_residues.json** contains genuine closed and open-shape congruences at orders beyond the exact ordinary table.
* **data/published.json** gives the public comparison tables, sources, checksums and separately identified extensions. **data/priority.md** explains the published frontiers. **experiments/runs** holds the chosen calculation records. For state weights larger than 64 bits the counters use checked wide arithmetic or a proven Catalan bound.
* **data/projective_sectors_additional.csv** lists the measured single heights, fixed-cut bands, compatible prefixes from order 31 onwards and formula-evaluated minima. **data/symmetric_sectors_additional.csv** lists the completed marked height runs, including those used in full rows. Each row identifies a calculation and its source. Several implementations may independently supply the same contribution. The OEIS height table gives each distinct projective contribution once and preserves the range of a combined band. The marked terminal-weight table retains both summands and the source horizon. Full sequence terms appear in the primary tables.
* **experiments/runs/rainbow42** contains the exact rational functions, coefficient order low degree first. In particular, each arch record includes the reduced numerator for the series starting at index zero. The published candidate arrays use a different origin and the factor stated in the paper.
* The [sequence supplement](oeis/README.md) gives the b-files and indexed tables. The [source notices](NOTICE.md) identify the comparison data and their reuse terms.

## Reproduce

Extract the archive under Linux or WSL. The supplied verification command uses Python 3.10 or later, GCC with C++20 support and 128-bit integers, GNU time at /usr/bin/time, and prlimit from util-linux. The Python verifier uses the standard library. Disk-backed jobs require a writable local filesystem. An optional independent ordinary open-meander replay uses the pinned Rust project identified in NOTICE.md.

~~~sh
python3 verify.py
~~~

The default verifies file digests, the complete tables, the integer polynomial certificates and every available comparison with an external table. It directly enumerates small Catalan cases, recomputes P23 and K23, and reruns the fifth ordinary winding diagonal. For selected full computations use

~~~sh
python3 verify.py --tables-only
python3 verify.py --projective 39
python3 verify.py --symmetric 38
python3 verify.py --diagonal 11 --arch
~~~

The P option uses the minimum-cut transfer. The K option runs the in-memory marked-path transfer for each height. For a large K where the in-memory transfer exceeds available memory, compile the supplied segmented layer implementation and use its per-height checkpoints as described in **data/resources.md**. Timings are observations on a shared machine. The data record the largest individual process and summed computation times where available.

The projective and marked transfers have at most four choices at a paired step. At projective orders up to 60, even a gap-weighted sum is at most \(60\cdot4^{60}<2^{126}\) per sector. For the marked transfer, 128-bit weights cover orders through 61 with at most \(4^{61}=2^{122}\) histories per sector. Its 64-bit state keys cover every height through disk order 46. The height-one sector at order 47 requires a 66-bit key. Distributed sequence totals and polynomial arithmetic are also verified with Python integers.

For example, the following independent command regenerates the available earlier orders on a single diagonal of the marked triangle from a fresh small disk-backed run. Keep comparison results outside the extracted archive. A completed sector will be skipped on a rerun, and an interrupted sector resumes from its last complete layer when given the same scratch path and settings.

~~~sh
python3 scripts/symmetric42/diagonal_campaign.py 20 8 --min-order 17 --scratch /tmp/meanders-work --results /tmp/meanders-check --mib 512 --disk-mib 512 --seconds 120
~~~

To reproduce a large projective minimum sector with prefix totals, compile the method named in its evidence record. For example, `g++ -O3 -std=c++20 -march=native scripts/projective42/smart.cpp -o smart` followed by `./smart 35 5` computes the minimum-five sector and the compatible earlier orders. On machines without BMI2 replace `-march=native` by `-I scripts/portable_bits`. The default full verification demonstrates both the independent identity checks and an exact small-order replay.

For a sequential disk-backed replay of the symmetric orders 43 through 46, use the command below on a filesystem with sufficient space. Its output directory holds the sector records, embedded terminal layers and a summary with D and K. Repeating the command with the same directory resumes the calculation. The default limits allocate up to 8 GiB of address space, up to 32 GiB for a pair of live layers and a 15 GiB free-space reserve. The launcher also accepts smaller terminal orders for a quick trial.

~~~sh
python3 scripts/symmetric42/reproduce_terminal.py /tmp/my-meanders-k45
~~~

On Linux/WSL, add `--packed` to compress consecutive layers using the system Zstandard library (libzstd.so.1). The attached small-order comparisons verify byte-for-byte recovery of the same state weights from both storage formats.

For a sequential projective replay at the last tabulated order, the following command evaluates the minimum-height boundary contribution from the local ordinary inputs and runs each remaining height with the implementation identified in the full calculation. It checks the sector values and total, and keeps individual completed heights in the requested directory. Use --order 35 for a smaller example.

~~~sh
python3 reproduce_projective.py /tmp/my-projective-replay
~~~

The scalar congruences provide independent checks at high orders. A complete replay of a large table takes more work than verification of the distributed integer certificates. **SHA256SUMS.txt** provides a byte-level integrity check for this copy of the distribution.

The independent rotational check takes only a small fraction of the full running time. It enumerates the fundamental Dyck words, reconstructs the fixed matching counts and compares each minimum-height sector at orders 44 through 47 modulo twice its order. To repeat it from scratch, run `python3 scripts/cyclic_projective42/minimum_check.py --replay`.

The second check `python3 scripts/cyclic_projective42/base_face_check.py --replay` works directly with the faces of the quotient disk. Its largest case tests every marked face of the 576,018 connected base matchings of order 15.

## Rebuild the manuscripts

The sources in **papers** use standard LaTeX mathematics, Latin Modern, booktabs, longtable, graphicx, geometry and hyperref. The two diagrams are supplied as vector PDFs. From the papers directory a TeX installation with these packages can build the manuscripts with

~~~sh
latexmk -pdf projective.tex symmetric.tex modular.tex rainbow.tex
~~~

The distributed PDFs were built with Tectonic. Numerical tables in the distributed TeX sources are already expanded. Each paper gives its counting convention, its computational checks and its references.
