// Independent signed-permutation implementation, using the invariant interval.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

class IntervalCount {
    static constexpr int maximum=121;
    int n,m;
    std::array<int,2*maximum> label{}, face{};
    std::array<int,maximum> parent{};
    uint64_t nodes=0, count=0;

    int index(int v) const { return v<0 ? 2*n+v : v; }
    int opposite(int v) const { return v<n ? v+n : v-n; }
    void relabel(int a, int b, int from, int to) {
        if (a>b) std::swap(a,b);
        for (int v=a+1; v<b; ++v) if (face[v]==from) face[v]=to;
    }
    void bind(int a, int b, uint64_t rest) {
        int sources[8], images[8];
        int v[4]{a,b,-a,-b};
        for (int j=0;j<4;++j) {
            sources[j]=index(v[j]);
            images[j]=index(v[(j+1)%4]);
            sources[4+j]=opposite(sources[j]);
            images[4+j]=opposite(images[j]);
        }
        for (int j=0;j<8;++j) label[sources[j]]=images[j];
        int used[8], total=0;
        bool ok=true;
        for (int src:sources) {
            int first=src&~1, d=first/2;
            if (label[first]<0 || label[first+1]<0 || std::find(used,used+total,d)!=used+total) continue;
            int x=label[first], y=label[first+1];
            if (face[x]!=face[y]) { ok=false; break; }
            parent[d]=face[x];
            relabel(x,y,face[x],d+1);
            used[total++]=d;
        }
        if (ok) visit(rest);
        while (total) {
            int d=used[--total], i=2*d;
            relabel(label[i],label[i+1],d+1,parent[d]);
        }
        for (int src:sources) label[src]=-1;
    }
    void visit(uint64_t free) {
        if (++nodes==0) throw std::overflow_error("nodes");
        if (!free) {
            if (++count==0) throw std::overflow_error("count");
            return;
        }
        int a=-1, best=-1;
        for (uint64_t q=free;q;q&=q-1) {
            int x=__builtin_ctzll(q);
            int s=(label[x-1]>=0)+(label[x+1]>=0);
            if (s>best) { best=s; a=x; }
        }
        free^=uint64_t(1)<<a;
        constexpr uint64_t even=0x5555555555555555ULL;
        for (uint64_t q=free & ((a%2) ? ~even : even);q;q&=q-1) {
            int b=__builtin_ctzll(q);
            uint64_t rest=free^(uint64_t(1)<<b);
            bind(a,b,rest);
            bind(a,-b,rest);
        }
    }
public:
    explicit IntervalCount(int order):n(order),m((order-1)/2) {
        if (n<9 || n>maximum || n%8!=1) throw std::invalid_argument("n=9,17,...,121");
        label.fill(-1);
        label[0]=0;
        label[n]=n;
    }
    void run() {
        auto start=std::chrono::steady_clock::now();
        visit((uint64_t(1)<<(m+1))-2);
        double secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"{\"n\":"<<n<<",\"Q\":"<<count<<",\"nodes\":"<<nodes
                 <<",\"seconds\":"<<secs<<"}"<<std::endl;
    }
};
int main(int argc,char** argv) {
    if (argc!=3) return 1;
    for (int n=std::atoi(argv[1]);n<=std::atoi(argv[2]);n+=8) IntervalCount(n).run();
}
