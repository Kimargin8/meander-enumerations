// Count noncrossing completions of a prescribed pairing, with one resulting circuit.
// Each remaining disk has cyclically ordered ports. The exterior joins form an involution.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int limit_ports = 78;
using Ports = std::array<int,limit_ports>;
using Clock = std::chrono::steady_clock;
using Count = unsigned long long;

struct Disks {
    Ports next{}, mate{};
    int used = 0;
};

class Counter {
    std::unordered_map<std::string, Count> cache;
    std::size_t cap;
    Clock::time_point stop;
    unsigned long long calls=0;
public:
    Counter(std::size_t states, unsigned seconds):cap(states),stop(Clock::now()+std::chrono::seconds(seconds)) {}
    std::size_t states() const {return cache.size();}
    unsigned long long visits() const {return calls;}

    // Pair two ports inside a disk; this splits the remaining boundary cyclically.
    // The premature-loop case must have been handled by the caller.
    void splice(Disks &g, int a, int b) const {
        int x=g.mate[a], y=g.mate[b];
        g.mate[x]=y;
        g.mate[y]=x;
        int pa=a, pb=b;
        while(g.next[pa]!=a) pa=g.next[pa];
        while(g.next[pb]!=b) pb=g.next[pb];
        if(pa!=b) g.next[pa]=g.next[b];
        if(pb!=a) g.next[pb]=g.next[a];
        g.next[a]=g.next[b]=-1;
        g.used-=2;
    }

    Count evaluate(Disks g) {
        if((++calls % 8192)==0 && Clock::now()>=stop) throw std::runtime_error("time cap");
        // All degree-two disks have one possible chord.
        for(int a=0;a<limit_ports;++a) {
            int b=g.next[a];
            if(b<0 || g.next[b]!=a) continue;
            if(g.mate[a]==b) return g.used==2;
            splice(g,a,b);
        }

        Ports lens{}, previous{};
        int shortest=limit_ports+1;
        for(int a=0;a<limit_ports;++a) {
            if(g.next[a]<0 || lens[a]) continue;
            int length=0, v=a;
            do { previous[g.next[v]]=v; v=g.next[v]; ++length; } while(v!=a);
            do {lens[v]=length; v=g.next[v];} while(v!=a);
            shortest=std::min(shortest,length);
        }
        if(shortest>limit_ports) throw std::logic_error("empty state without final circuit");
        // A root flag determines an order of all reachable disks and ports.
        // Minimize over shortest-disk roots and simultaneous reversal; ignoring
        // independent reflections of other disks can only miss cache hits.
        std::string key;
        int anchor=-1;
        for(int root=0;root<limit_ports;++root) if(lens[root]==shortest) {
            for(const Ports *rot : {static_cast<const Ports*>(&g.next),static_cast<const Ports*>(&previous)}) {
                Ports labels,order;
                labels.fill(-1);
                int count=0;
                auto append_disk=[&](int at) {
                    int v=at;
                    do {labels[v]=count;order[count++]=v; v=(*rot)[v];} while(v!=at);
                };
                append_disk(root);
                for(int j=0;j<count;++j) {
                    int t=g.mate[order[j]];
                    if(labels[t]<0) append_disk(t);
                }
                if(count!=g.used) return 0;
                std::string candidate;
                candidate.reserve(count*2);
                for(int j=0;j<count;++j) {
                    candidate.push_back(static_cast<char>(labels[(*rot)[order[j]]]));
                    candidate.push_back(static_cast<char>(labels[g.mate[order[j]]]));
                }
                if(anchor<0 || candidate<key) {key=std::move(candidate);anchor=root;}
            }
        }
        auto found=cache.find(key);
        if(found!=cache.end()) return found->second;
        Count total=0;
        int b=g.next[anchor];
        for(int j=1;j<shortest;j+=2) {
            if(g.mate[anchor]!=b) {
                Disks sub=g;
                splice(sub,anchor,b);
                Count value=evaluate(sub);
                if(value>std::numeric_limits<Count>::max()-total) throw std::overflow_error("count exceeds 64 bits");
                total+=value;
            }
            b=g.next[g.next[b]];
        }
        if(cache.size()>=cap) throw std::runtime_error("state cap");
        cache.emplace(std::move(key),total);
        return total;
    }
};

int main(int argc,char **argv) {
    if(argc<4 || argc>5) {
        std::cerr<<"Usage: surgery n max_states seconds [projective|two-defects|rainbow|stream]\n";
        return 1;
    }
    int n=std::stoi(argv[1]);
    // All additions are checked even when the Catalan bound exceeds 64 bits.
    if(n<1 || 2LL*n>limit_ports) return 1;
    Disks g;
    g.next.fill(-1); g.mate.fill(-1);g.used=2*n;
    for(int i=0;i<2*n;++i) {g.next[i]=(i+1)%(2*n); g.mate[i]=(i+n)%(2*n);}
    const std::string pattern=(argc==5 ? argv[4] : "projective");
    if(pattern=="two-defects") {
        for(int i=0;i<2*n;++i) g.mate[i]=(2*n-i)%(2*n);
        g.mate[0]=n; g.mate[n]=0;
    } else if(pattern=="rainbow") {
        for(int i=0;i<2*n;++i) g.mate[i]=2*n-1-i;
    } else if(pattern!="projective" && pattern!="stream") return 1;
    Counter counter(std::stoull(argv[2]),std::stoul(argv[3]));
    auto start=Clock::now();
    try {
        if(pattern=="stream") {
            std::string line;
            while(std::getline(std::cin,line)) {
                std::istringstream input(line);
                for(int i=0;i<2*n;++i) {
                    int v;
                    if(!(input>>v) || v<0 || v>=2*n || v==i) throw std::invalid_argument("not a matching");
                    g.mate[i]=v;
                }
                std::string extra;
                if(input>>extra) throw std::invalid_argument("extra input");
                for(int i=0;i<2*n;++i) if(g.mate[g.mate[i]]!=i) throw std::invalid_argument("not an involution");
                std::cout<<counter.evaluate(g)<<"\n";
            }
            return 0;
        }
        auto value=counter.evaluate(g);
        std::cout<<n<<" "<<value<<" "<<counter.states()<<" "<<counter.visits()<<" "
                 <<std::chrono::duration<double>(Clock::now()-start).count()<<"\n";
    } catch(const std::exception &e) {
        std::cerr<<e.what()<<", states "<<counter.states()<<", calls "<<counter.visits()<<"\n"; return 2;
    }
}
