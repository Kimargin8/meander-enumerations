// Independent finite check: generate actual matchings, detect crossings from
// the alternating-endpoint definition, and perform surgery directly on pairs.
#define PROJECTIVE42_NO_MAIN
#include "fold.cpp"
#include "fast.hpp"
#include <functional>

using Mates=std::vector<int>;
static bool crosses(int a,int b,int c,int d) {
    return (a<c&&c<b&&b<d)||(c<a&&a<d&&d<b);
}
static bool encode(const Mates& m,U64& word) {
    int n=m.size();
    U64 essential=0;
    for(int a=0;a<n;a++) if(m[a]>a)
        for(int b=a+1;b<n;b++) if(m[b]>b && crosses(a,m[a],b,m[b]))
            essential|=(U64(1)<<a)|(U64(1)<<m[a])|(U64(1)<<b)|(U64(1)<<m[b]);
    for(int a=0;a<n;a++) if(m[a]>a && (essential>>a&1))
        for(int b=a+1;b<n;b++) if(m[b]>b && (essential>>b&1) && !crosses(a,m[a],b,m[b]))
            return false;
    word=essential;
    for(int a=0;a<n;a++) if(m[a]>a && !(essential>>a&1)) {
        int b=m[a];
        bool inside=essential&(mask<U64>(b)^mask<U64>(a+1));
        word|=U64(1)<<(inside?b:a);
    }
    return true;
}

int main(int argc,char**argv) {
    if(argc!=2) return 2;
    int last=std::stoi(argv[1]);
    if(last<2 || last>18 || last%2) return 2;
    U64 total=0,patterns=0,joins=0;
    for(int size=2;size<=last;size+=2) {
        Mates m(size,-1);
        std::unordered_map<U64,U64> seen;
        std::function<void()> generate=[&](){
            int x=0;while(x<size && m[x]>=0) ++x;
            if(x<size) {
                for(int y=x+1;y<size;y++) if(m[y]<0) {
                    m[x]=y;m[y]=x;generate();m[x]=m[y]=-1;
                }
                return;
            }
            ++total;
            U64 w=0;
            if(!encode(m,w)) return;
            ++patterns;
            if(!seen.emplace(w,1).second) throw std::logic_error("word collision");
            Mates reflected(size);
            for(int i=0;i<size;i++) reflected[size-1-i]=size-1-m[i];
            U64 reversed;
            if(!encode(reflected,reversed) || reflect(w,size)!=reversed || fast_reflect(w,size)!=reversed)
                throw std::logic_error("reflection");
            for(int i=0;i<size-1;i++) {
                ++joins;
                U64 dst=0;
                bool valid=join(w,i,size,dst);
                if(valid!=(m[i]!=i+1)) throw std::logic_error("loop");
                if(!valid) continue;
                Mates after(size-2);
                auto shift=[&](int k){return k>i?k-2:k;};
                for(int j=0;j<size;j++) if(j!=i && j!=i+1) {
                    int k=m[j];
                    if(k==i) k=m[i+1]; else if(k==i+1) k=m[i];
                    after[shift(j)]=shift(k);
                }
                U64 direct;
                if(!encode(after,direct) || direct!=dst) throw std::logic_error("join differs");
            }
        };
        generate();
    }
    std::cout<<"{\"max_ports\":"<<last<<",\"all_matchings\":"<<total<<",\"admissible\":"<<patterns
             <<",\"adjacent_surgeries\":"<<joins<<",\"status\":\"passed\"}\n";
}
