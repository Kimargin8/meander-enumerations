// Two simultaneous scans of an antipodally identified noncrossing matching.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/resource.h>
#include <chrono>

using Weight = std::uint64_t;
using Buckets = std::unordered_map<std::string,Weight>;

struct Option {
    int stack, anchors, pop;
    bool create;
};

static std::vector<Option> options(int stack, int anchors, int base, int anchor_base, int remaining) {
    std::vector<Option> out;
    if (stack+1+anchors <= remaining)
        out.push_back({stack+1,anchors,-1,true});
    if (stack && stack-1+anchors <= remaining)
        out.push_back({stack-1,anchors,base+stack-1,false});
    if (!stack && anchors && anchors-1 <= remaining)
        out.push_back({0,anchors-1,anchor_base,false});
    return out;
}

static Weight evaluate(int n,int h,std::size_t &peak,std::uint64_t &work) {
    std::string start(4+2*h,0);
    start[2]=start[3]=h;
    for (int j=0;j<h;j++) {
        start[4+j]=2*h-1-j;
        start[4+2*h-1-j]=j;
    }
    Buckets cur{{start,1}},next;
    Weight answer=0;
    for (int t=0;t<n;t++) {
        next.clear();
        int remaining=n-t-1;
        for (const auto &entry:cur) {
            const auto &key=entry.first;
            int l=key[0],r=key[1],a=key[2],b=key[3],size=l+r+a+b;
            auto ls=options(l,a,0,l+r,remaining);
            auto rs=options(r,b,l,l+r+a,remaining);
            for (auto x:ls) for (auto y:rs) {
                ++work;
                if (!x.create && !y.create && key[4+x.pop]==y.pop) {
                    if (!remaining && size==2) answer+=entry.second;
                    continue;
                }
                std::array<int,128> old{},idx{},paired{};
                for (int i=0;i<size;i++) paired[i]=key[4+i];
                int xx=x.create?size:x.pop, yy=y.create?size+1:y.pop;
                if (x.create && y.create) {paired[xx]=yy;paired[yy]=xx;}
                else if (x.create) {paired[xx]=paired[yy];paired[paired[yy]]=xx;}
                else if (y.create) {paired[yy]=paired[xx];paired[paired[xx]]=yy;}
                else {int u=paired[xx],v=paired[yy];paired[u]=v;paired[v]=u;}
                int count=0;
                auto append=[&](int j){if (j!=x.pop && j!=y.pop) {old[count]=j;idx[j]=count++;}};
                for (int j=0;j<l;j++) append(j);
                if (x.create) append(xx);
                for (int j=l;j<l+r;j++) append(j);
                if (y.create) append(yy);
                for (int j=l+r;j<size;j++) append(j);
                std::string dst(4+count,0);
                dst[0]=x.stack;dst[1]=y.stack;dst[2]=x.anchors;dst[3]=y.anchors;
                for (int j=0;j<count;j++) dst[4+j]=idx[paired[old[j]]];
                auto &v=next[dst];
                if (std::numeric_limits<Weight>::max()-v<entry.second) throw std::overflow_error("coefficient");
                v+=entry.second;
            }
        }
        peak=std::max(peak,next.size());
        cur.swap(next);
    }
    return answer;
}

int main(int argc,char **argv) {
    if (argc!=3) return 2;
    int from=std::atoi(argv[1]),to=std::atoi(argv[2]);
    if (from<1 || to>32 || from>to) return 3;
    for (int n=from;n<=to;n++) {
        auto start=std::chrono::steady_clock::now();
        Weight sum=0,work=0;
        std::size_t peak=0;
        std::cout<<"{\"n\":"<<n<<",\"by_height\":[";
        for (int h=n%2;h<=n;h+=2) {
            Weight x=evaluate(n,h,peak,work);
            if (h!=n%2) std::cout<<',';
            std::cout<<x;
            sum+=x;
        }
        struct rusage usage{};
        getrusage(RUSAGE_SELF,&usage);
        std::chrono::duration<double> sec=std::chrono::steady_clock::now()-start;
        std::cout<<"],\"count\":"<<sum<<",\"peak_states\":"<<peak<<",\"transitions\":"<<work
                 <<",\"seconds\":"<<sec.count()<<",\"process_peak_kib\":"<<usage.ru_maxrss<<"}"<<std::endl;
    }
    return 0;
}
