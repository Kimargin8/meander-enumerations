"""Exact arithmetic checks for the forbidden-factor estimate in the projective paper."""
import json
from fractions import Fraction
from itertools import product
from math import comb


def dyck(n):
    words=[]
    for w in product((-1,1),repeat=2*n):
        h=0
        for step in w:
            h+=step
            if h<0: break
        else:
            if h==0: words.append(w)
    return words


def dyck_intervals(w):
    intervals=set()
    for i in range(len(w)):
        h=0
        for j in range(i,len(w)):
            h+=w[j]
            if h<0: break
            if h==0: intervals.add((i,j))
    return intervals


def exhaustive_counts(orientation,last):
    # At each node only a suffix can introduce the first forbidden factor.
    # Scaling each letter by 4 gives integral weights at u=2.
    out=[0]*(last+1)
    signs=[(1,1),(1,-1),(-1,1),(-1,-1)]
    def visit(steps,weight):
        depth=len(steps)
        out[depth]+=weight
        if depth==last: return
        for x,y in signs:
            step=(orientation[0]*x,orientation[1]*y)
            steps.append(step)
            a=b=0
            forbidden=False
            for i,j in reversed(steps):
                a+=i
                b+=j
                if a>0 or b>0: break
                if a==b==0:
                    forbidden=True
                    break
            if not forbidden:
                visit(steps,weight*(1<<(2+x+y)))
            steps.pop()
    visit([],1)
    return out


I=[1]
for j in range(1,7):
    ws=[dyck_intervals(w)-{(0,2*j-1)} for w in dyck(j)]
    I.append(sum(not (a&b) for a in ws for b in ws))
assert I==[1,1,2,8,46,322,2546]
G=[1]
for n in range(1,9):
    G.append(25*G[-1]-sum(I[j]*4**(2*j)*G[n-2*j] for j in range(1,n//2+1)))
for orientation in product((-1,1),repeat=2):
    assert exhaustive_counts(orientation,8)==G

u=Fraction(41,20)
S=(u+1/u)**2
B5=sum((Fraction(comb(2*j,j),j+1)/S**j)**2 for j in range(6))
upper=S/B5
assert upper**156 < (2*u)**203
assert 2**406*10000**515<17271**515
print(json.dumps(dict(irreducible=I,scaled_u2_words=G,orientations=4,u=str(u),S=str(S),
                      lower_B5=str(B5),upper_lambda=str(upper),upper_alpha="203/156",
                      upper_gamma="17271/10000",comparisons="exact rational powers"),indent=2))
