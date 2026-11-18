# Meander enumerations and certificates

**Iaroslav Argunov** · 18 November 2026

This collection gives exact counts for two families of meanders. The first counts ways to join labelled points in a disk so that identifying opposite points gives one circuit. The second counts centrally symmetric shapes of an open meander. Each table is accompanied by a manuscript, the programs that produced it, and checks on the enumeration.

| Family | Last exact term | Manuscript | Full sequence |
|---|---:|---|---|
| Projective meanders | P(47) = 23,025,690,740,071,248,492,028 | [A bit transfer for connected projective meanders](papers/projective.pdf) | [A006663, indices 0–47](oeis/b006663.txt) |
| Centrally symmetric open meander shapes | K(45) = 12,201,962,983,536,099,355,104 | [Symmetric open meanders with two marked ends](papers/symmetric.pdf) | [A223096, indices 0–45](oeis/b223096.txt) |

The projective table includes the conventional value P(0) = 1. An index k in the symmetric table corresponds to 2k + 1 crossings, so K(45) counts shapes with 91 crossings. The published tables located for the comparison reach P(22) and K(32). The latter includes Hunter Hogan’s values for K(23) through K(32). See the [comparison with earlier work](data/priority.md) for the sources and the scope of the search.

## Companion manuscripts

* [Congruences for closed meanders from rotations and curve interchange](papers/modular.pdf) gives residues of ordinary closed-meander counts. At order 39, the result is M(39) ≡ 16 (mod 312). [Table of residues](data/ordinary_residues.csv)
* [Exact certificates for seven semi-meander diagonals](papers/rainbow.pdf) proves the seven previously conjectured generating functions at deficits 5 through 11 and computes functions at deficits 12 and 13. [Numerators and denominators](oeis/A400429-rational-functions.json)

## Verify the tables

From the root of the extracted distribution, run the following command with Python 3.10 or later. It checks file digests, integer identities, reference values and small cases by direct enumeration.

```sh
python3 verify.py --tables-only
```

The [reproduction guide](REPRODUCING.md) gives commands for independent runs and the memory and disk requirements for the larger cases. The [sequence tables](oeis/README.md) document the indexing of each downloadable file. The [manuscript sources](papers/) contain the LaTeX and figures used to build the four PDFs.

## Cite this edition

For a result from one manuscript, cite Iaroslav Argunov, the title shown above, and the 18 November 2026 edition. For the data and programs, use the [versioned release](https://github.com/Kimargin8/meander-enumerations/releases/tag/v2026.11.18) or the [citation metadata](CITATION.cff). Credits and reuse terms for comparison data and third-party code appear in the [source notices](NOTICE.md).
