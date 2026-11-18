// Independent bidirectional enumeration of face moments using ballot words up to the midpoint.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using U = std::uint64_t;
using Pairing = std::array<unsigned char,26>;
struct Word {
    std::array<int,13> mate{};
    std::vector<int> ends;
    unsigned odd_open=0, even_close=0;
};
struct Moments {
    U count=0, lt=0, lb=0, lp=0, rt=0, rb=0, rp=0;
};

std::vector<Word> words(unsigned m) {
    std::vector<Word> all;
    for (unsigned w=0;w<(1u<<m);++w) {
        Word p;
        bool valid=true;
        for (unsigned i=0;i<m;++i) {
            if ((w>>i)&1u) {
                p.ends.push_back(i);
                p.odd_open+=i%2;
            } else {
                if (p.ends.empty()) {valid=false;break;}
                int j=p.ends.back();p.ends.pop_back();
                p.mate[i]=j;p.mate[j]=i;
                p.even_close+=(i%2==0);
            }
        }
        if (!valid) continue;
        for (unsigned j=0;j<p.ends.size();++j) p.mate[p.ends[j]]=m+j;
        all.push_back(p);
    }
    return all;
}

bool one_loop(const Pairing& a,const Pairing& b,unsigned size) {
    unsigned v=0,length=0;
    do {v=a[b[v]];length+=2;} while (v);
    return length==size;
}

void calculate(unsigned m) {
    // Every result and intermediate product is <= (m+1)^2 Catalan(m)^2 < 2^64
    // for m <= 13, since the joined words specify an ordered matching pair.
    if (m<1 || m>13) throw std::runtime_error("Supported midpoint length 1..13");
    auto ballot=words(m);
    std::map<std::pair<unsigned,unsigned>,std::map<Pairing,Moments>> buckets;
    for (const Word& up:ballot) for (const Word& down:ballot) {
        unsigned u=up.ends.size(),v=down.ends.size(),k=u+v;
        Pairing pairing;pairing.fill(255);
        unsigned visited=0;
        for (unsigned i=0;i<k;++i) {
            if (pairing[i]!=255) continue;
            unsigned x=i<u?up.ends[i]:down.ends[i-u];
            bool to_lower=i<u;
            for (;;) {
                visited|=1u<<x;
                unsigned y=(to_lower?down:up).mate[x];
                if (y>=m) {
                    unsigned end=y-m+(to_lower?u:0);
                    pairing[i]=end;pairing[end]=i;
                    break;
                }
                x=y;to_lower=!to_lower;
            }
        }
        if (visited!=(1u<<m)-1) continue; // A whole loop is trapped in this half.
        Moments& s=buckets[{u,v}][pairing];
        ++s.count;
        s.lt+=up.odd_open;s.lb+=down.odd_open;s.lp+=up.odd_open*down.odd_open;
        s.rt+=up.even_close;s.rb+=down.even_close;s.rp+=up.even_close*down.even_close;
    }
    U n=0,et=0,eb=0,product=0,states=0,joins=0;
    for (const auto& [uv,group]:buckets) {
        states+=group.size();
        for (const auto& [a,l]:group) for (const auto& [b,r]:group) {
            if (!one_loop(a,b,uv.first+uv.second)) continue;
            ++joins;
            // The outer face contributes 1 to both hemispheres; the reversed
            // second half contributes one for each closure at its even indices.
            U x=l.lt+l.count,y=l.lb+l.count,xy=l.lp+l.lt+l.lb+l.count;
            n+=l.count*r.count;
            et+=x*r.count+l.count*r.rt;
            eb+=y*r.count+l.count*r.rb;
            product+=xy*r.count+x*r.rb+y*r.rt+l.count*r.rp;
        }
    }
    std::cout<<m<<" "<<n<<" "<<et<<" "<<eb<<" "<<product<<" "
             <<(m+1)*(et+eb)-2*product<<" "<<ballot.size()<<" "<<states<<" "<<joins<<"\n";
}

int main(int argc,char** argv) {
    for (int i=1;i<argc;++i) calculate(static_cast<unsigned>(std::atoi(argv[i])));
}
