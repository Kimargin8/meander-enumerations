# A second modulo-eight check for the marked count

Let the order \(n>1\) be odd. Label the equator by the residues modulo \(2n\), write \(r(i)=-i\) and \(H(i)=i+n\). A connected ordinary diagram gives a permutation \(p\), obtained by following the second curve from label zero along its upper edge. Thus \(p(0)=0\) and \(p\) preserves parity. The two matchings consist of successive edges of the word, in alternating colours. Interchanging the roles of the two curves inverts \(p\).

In the symmetric marked class the lower matching is the reflection of the upper through the two vertices \(0,n\). On the road this reflection fixes the same two vertices, reverses direction and interchanges colours. Thus \(pr=rp\), and conversely this identity gives precisely the marked class of size \(D_n\). It also forces \(p(n)=n\). Reversing one orientation, interchanging the curves and simultaneously shifting both cyclic label sets by \(n\) give commuting involutions \(S(p)=rp, T(p)=p^{-1}, C(p)=HpH\). Each leaves the marked class invariant. Their action of the group of order eight is defined even in orders where it has a kernel.

In the table below \(q=pH\). Thus \(q\) reverses parity and interchanges \(0,n\), while \(p\) fixes both. Both commute with \(r\).

| Element | Fixed-point condition |
|---|---|
| Identity | All \(D_n\) diagrams |
| \(S\) | Empty, since \(p\) is onto |
| \(C\) | A marked word also commuting with \(H\), counted by \(D_{(n+1)/2}\) by the height-one reduction in the congruence paper |
| \(SC\) | \(pH=rHp\) |
| \(T,CT\) | \(p^2=1, q^2=1\), respectively |
| \(ST,SCT\) | \(p^2=r, q^2=r\), respectively |

The eight fixed counts sum to a multiple of eight. All five nontrivial classes in the last three lines have direct signed-permutation enumerations. On the nonfixed pairs of \(r\), assign a representative and its negative simultaneously. An involution with a pair of distinct absolute values uses equal within-pair signs. A square root of \(r\) uses opposite signs. The latter has no fixed absolute value. For \(SC\), assigning \(p(i)\) also determines \(p(-i),p(H i),p(-H i)\). Reject an assignment whenever completed upper chords cross. The lower chords are their reflections, and a full word is a permutation, so no further connectivity test is needed.

For example, at \(n=19\) the five counts \(SC,T,CT,ST,SCT\) are \(20\,314, 10\,372\,026, 21\,380, 0, 5\,966\). Together with \(D_{10}=5\,396\) they give \(D_{19}\equiv6\bmod 8\), in agreement with \(D_{19}=173\,762\,358\). The two enumerations, directly from noncrossing upper matchings and by the restricted word search, agree in every class through \(n=11\). All five classes were obtained through \(n=19\). The records through \(n=25\) explicitly identify which classes were finished.

The exact integer \(M_{39}\) is still unknown in these materials. Its established residue leaves two possibilities modulo 624, namely 16 and 328. Both are listed here solely to specify what the completed calculation distinguishes.

The signed-orbit recursion is in **scripts/marked_residues42/roots.cpp**. The accompanying checker reads the distributed symmetric table and compares completed cases with the full count. **protocol_sources/marked-check.py** preserves the original run driver by checksum. The portable driver's input lookup also accepts the archive's data directory.
