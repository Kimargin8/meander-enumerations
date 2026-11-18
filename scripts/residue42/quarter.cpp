// Rooted square roots of simultaneous reversal on projective meanders.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

struct Search {
    using Wide=unsigned __int128;
    int n, m;
    std::array<int,105> p{}, odd{}, before{}, after{};
    std::array<Wide,210> region{};
    uint64_t nodes=0, count=0;

    explicit Search(int order) : n(order), m((n-1)/2) {
        if (n<5 || n>101 || n%4!=1) throw std::invalid_argument("n=5,9,...,101");
        p.fill(-1);
        p[0]=0;
        for (int i=0; i<n; ++i) {
            odd[i]=(2*i+n)%(2*n);
            before[i]=(i-m+n)%n;
            after[i]=(i+m)%n;
        }
    }
    void toggle(int i) {
        int a=2*p[i], b=odd[p[before[i]]];
        if (a>b) std::swap(a,b);
        for (int v=a+1; v<b; ++v) region[v]^=Wide(1)<<i;
    }
    void pair_orbits(int a, int b, uint64_t remaining) {
        int cycle[4]{a,b,n-a,n-b};
        for (int t=0; t<4; ++t) p[cycle[t]]=cycle[(t+1)%4];
        Wide domains=0;
        for (int u:cycle) domains|=(Wide(1)<<u)|(Wide(1)<<after[u]);
        int drawn[8], used=0;
        bool allowed=true;
        for (auto q=domains;q;q&=q-1) {
            uint64_t low=static_cast<uint64_t>(q);
            int d=low ? __builtin_ctzll(low) : 64+__builtin_ctzll(static_cast<uint64_t>(q>>64));
            if (p[d]<0 || p[before[d]]<0) continue;
            if (region[2*p[d]]!=region[odd[p[before[d]]]]) { allowed=false; break; }
            toggle(d);
            drawn[used++]=d;
        }
        if (allowed) visit(remaining);
        while (used) toggle(drawn[--used]);
        for (int u:cycle) p[u]=-1;
    }
    void visit(uint64_t remaining) {
        if (++nodes==0) throw std::overflow_error("nodes");
        if (!remaining) {
            if (++count==0) throw std::overflow_error("roots");
            return;
        }
        int a=-1, best=-1;
        for (auto q=remaining; q; q&=q-1) {
            int u=__builtin_ctzll(q);
            int score=(p[before[u]]>=0)+(p[after[u]]>=0)+
                      (p[before[n-u]]>=0)+(p[after[n-u]]>=0);
            if (score>best) { best=score; a=u; }
        }
        remaining^=uint64_t(1)<<a;
        for (auto q=remaining;q;q&=q-1) {
            int b=__builtin_ctzll(q);
            auto rest=remaining^(uint64_t(1)<<b);
            pair_orbits(a,b,rest);
            pair_orbits(a,n-b,rest);
        }
    }
    void run() {
        auto start=std::chrono::steady_clock::now();
        visit((uint64_t(1)<<(m+1))-2);
        auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"{\"n\":"<<n<<",\"Q\":"<<count<<",\"nodes\":"<<nodes
                 <<",\"seconds\":"<<seconds<<"}"<<std::endl;
    }
};

int main(int argc,char** argv) {
    if (argc!=3) return 1;
    for (int n=std::atoi(argv[1]); n<=std::atoi(argv[2]); n+=4) Search(n).run();
}
