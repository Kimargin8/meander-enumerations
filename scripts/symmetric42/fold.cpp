// Count labelled completions of the reflected exterior pairing with two fixed ends.
// Both halves of the ONE unknown disk matching are read from the exterior gap.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

using U=uint64_t;
struct State { U bits; unsigned len,left,defect,end; };
static constexpr U one=1;
static U low(unsigned k){return (one<<k)-1;}
static U erase(U s,unsigned i){return (s&low(i))|((s>>(i+1))<<i);}
static U insert(U s,unsigned i,U b,unsigned width){return (s&low(i))|(b<<i)|((s>>i)<<(i+width));}
static unsigned partner(const State &s,unsigned i){
    bool opening=(s.bits>>i)&1;
    int balance=1, step=opening?1:-1, p=i;
    while(balance){
        p+=step;
        if(unsigned(p)>=s.len)throw std::logic_error("invalid planar word");
        if(unsigned(p)!=s.defect)balance+=bool((s.bits>>p)&1)==opening?1:-1;
    }
    return p;
}
// Consume two ports and their new edge; an already matched pair closes a forbidden loop.
static bool join(State &s,unsigned a,unsigned b){
    if(a>b)std::swap(a,b);
    if(a==s.defect || b==s.defect){
        s.defect=partner(s,a==s.defect?b:a);
        s.bits&=~(one<<s.defect);
    }else{
        auto p=partner(s,a);
        if(p==b)return false;
        auto q=partner(s,b);
        if(p>q)std::swap(p,q);
        s.bits|=one<<p;
        s.bits&=~(one<<q);
    }
    s.bits=erase(erase(s.bits,b),a);
    s.defect-=unsigned(s.defect>b)+unsigned(s.defect>a);
    s.len-=2;
    return true;
}
// State size adapts to the largest unforced cut for the chosen n.
static unsigned width;
static U code(State s){
    if(s.len>=width || s.defect>=s.len || s.left>s.len) throw std::logic_error("unsupported cut");
    return s.bits | (one<<s.len) | (U(s.left)<<width) | (U(s.defect)<<(width+6)) | (U(s.end)<<(width+12));
}
static State decode(U k){
    U marked=k&low(width);
    unsigned len=63-__builtin_clzll(marked);
    return {marked^(one<<len),len,unsigned((k>>width)&63),unsigned((k>>(width+6))&63),unsigned(k>>(width+12))};
}
static U mix(U x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
struct Table {
    struct Item{U key,weight;};
    std::vector<Item> slots;
    size_t used=0;
    explicit Table(size_t cap=16):slots(cap){}
    void place(U key,U value){
        auto mask=slots.size()-1;
        auto i=mix(key)&mask;
        while(slots[i].key && slots[i].key!=key)i=(i+1)&mask;
        if(!slots[i].key){slots[i]={key,value};++used;}
        else {if(UINT64_MAX-slots[i].weight<value)throw std::overflow_error("weight");slots[i].weight+=value;}
    }
    void add(U key,U value){
        if((used+1)*10>=slots.size()*8){
            Table larger(slots.size()*2);
            for(auto e:slots)if(e.key)larger.place(e.key,e.weight);
            std::swap(*this,larger);
        }
        place(key,value);
    }
};
static unsigned max_width(int n){
    unsigned maxlen=1;
    for(int t=0;t<n;t++)for(int u=(t+1)%2;u<=t+1;u+=2)for(int v=t%2;v<=t;v+=2){
        int rem=n-1-t;
        if(std::abs(u-v)>2*rem+1)continue;
        int pairs=std::max(0,std::min(u-rem,v-rem-1));
        maxlen=std::max(maxlen,unsigned(u+v-2*pairs));
    }
    return maxlen;
}
static bool prune(State &s,int rem,bool by_height){
    int u=s.left,v=s.len-s.left;
    if(std::abs(u-v)>2*rem+1)return false;
    // At most rem left closings and rem+1 right closings remain. Forced outer
    // endpoints in both stacks must be paired in the completed disk matching.
    int guaranteed=std::min(u-rem,v-rem-1);
    if(by_height){
        int end=s.end;
        if(std::abs(u-end)>rem || std::abs(v-end)>rem+1)return false;
        // Among remaining left walks from u to end in rem steps, at most
        // (rem+u-end)/2 steps can close a stack port. The bound on the right
        // uses rem+1 steps because it still contains the axis endpoint n.
        guaranteed=std::min((u+end-rem)/2,(v+end-rem-1)/2);
    }
    while(guaranteed-->0){
        if(!join(s,0,s.len-1))return false;
        --s.left;
        if(by_height)--s.end;
    }
    return true;
}
static U run(unsigned n,int h,bool trace){
    width=max_width(n)+1;
    if(width+18>64)throw std::runtime_error("key width exceeds 64 bits");
    Table cur;
    cur.add(code({0,1,1,0,unsigned(h<0?0:h)}),1);
    U work=0,answer=0;
    size_t peak=0;
    auto begin=std::chrono::steady_clock::now();
    for(unsigned t=1;t<n;t++){
        int rem=n-1-t;
        Table next(std::max<size_t>(16,cur.slots.size()/2));
        for(const auto &e:cur.slots)if(e.key){
            auto a=decode(e.key);
            if(a.len+2>=63)throw std::runtime_error("transient cut exceeds 64 bits");
            for(int l=0;l<2;l++)for(int r=0;r<2;r++){
                State b=a;
                unsigned u=b.left,v=b.len-u;
                if((!l&&!u)||(!r&&!v))continue;
                ++work;
                if(l && r){
                    b.bits=insert(b.bits,u,1,2);b.len+=2;++b.left;
                    if(b.defect>=u)b.defect+=2;
                }else if(l!=r){b.left+=l?1:-1;}
                else{if(!join(b,u-1,u))continue;--b.left;}
                if(prune(b,rem,h>=0))next.add(code(b),e.weight);
            }
        }
        peak=std::max(peak,next.used);
        std::swap(cur,next);
        if(trace)std::cerr<<n<<" "<<t<<" "<<cur.used<<" "<<cur.slots.size()<<"\n";
    }
    for(auto e:cur.slots)if(e.key){
        auto s=decode(e.key);
        int u=s.left,v=s.len-u;
        if(std::abs(u-v)!=1)continue;
        if(h>=0 && s.left!=s.end)continue;
        // The true exceptional endpoint n reaches the taller stack, leaving
        // both sides equal. The only completion pairs the stacks in order.
        while(s.len>1){
            if(!join(s,0,s.len-1))break;
            --s.left;
        }
        if(s.len==1){if(UINT64_MAX-answer<e.weight)throw std::overflow_error("total");answer+=e.weight;}
    }
    rusage m{};getrusage(RUSAGE_SELF,&m);
    std::cout<<n<<" "<<h<<" "<<answer<<" "<<peak<<" "<<work<<" "<<m.ru_maxrss<<" "
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<" "<<width<<std::endl;
    return answer;
}
int main(int argc,char **argv){
    if(argc<3)return 2;
    int h=argc>3?std::stoi(argv[3]):-1;
    for(int n=std::atoi(argv[1]);n<=std::atoi(argv[2]);++n){
        if(h==-2)for(int height=n%2;height<=n;height+=2)run(n,height,argc>4);
        else run(n,h,argc>4);
    }
}
