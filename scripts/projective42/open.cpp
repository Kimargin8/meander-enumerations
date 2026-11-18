// Ordinary open meanders. Reading the two sides starts at the vacuum; the
// only accepted endings are one remaining path with exactly two exposed ends.
#define PROJECTIVE42_NO_MAIN
#include "fold.cpp"

struct OpenArm {
    Slots* table=nullptr;
    int flip=0,size=0;
    void add(U64 w,U128 weight) const {
        if(flip) {
            U64 r=reverse64((~w)&mask<U64>(size))>>(64-size);
            w=flip==1?r:std::min(w,r);
        }
        table->add(w,weight);
    }
};
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    int n=std::stoi(argv[1]);if(n<1||n>58)return 2;
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<int,Slots>;
    Layer current;current[0].init(0);current[0].add(0,1);
    std::vector<U128> answers(n+1);answers[0]=1;
    U64 work=0,peak=1;
    for(int t=0;t<n;t++) {
        int rem=n-t-1;
        Layer next;
        auto target=[&](int L,int R)->OpenArm{
            if(L<0||R<0||std::max(0,L-rem)+std::max(0,R-rem)>2)return {};
            int flip=L>R?1:L==R?2:0;
            if(flip==1)std::swap(L,R);
            auto [p,made]=next.try_emplace(L|(R<<7));
            if(made)p->second.init(L+R);
            return {&p->second,flip,L+R};
        };
        std::vector<int> keys;
        for(auto&[code,s]:current)if(s.filled)keys.push_back(code);
        std::sort(keys.begin(),keys.end());
        for(int code:keys){
            auto node=current.extract(code);auto& src=node.mapped();
            int L=code&127,R=code>>7,size=L+R;
            auto oo=target(L+1,R+1),oc=target(L+1,R-1),co=target(L-1,R+1);
            auto cc=size>2?target(L-1,R-1):OpenArm{};
            if(L==R)oo.flip=0;
            auto low=mask<U64>(L);
            work+=src.filled*((oo.table!=nullptr)+(oc.table!=nullptr)+(co.table!=nullptr)+(cc.table!=nullptr));
            for(U64 e:src.cells)if(e){
                U64 w=e&src.keymask;U128 v=src.value(e);
                if(oo.table)oo.add((w&low)|(U64(1)<<L)|((w&~low)<<2),v);
                if(oc.table)oc.add(w,v);
                if(co.table)co.add(w,v);
                if(cc.table){U64 d;if(join(w,L-1,size,d))cc.add(d,v);}
            }
        }
        U64 count=0;
        for(auto&[code,s]:next){
            count+=s.filled;
            if((code&127)+(code>>7)==2)
                for(U64 e:s.cells)if(e)answers[t+1]+=s.value(e);
        }
        if((t+1)%2==0){
            if(answers[t+1]&1)throw std::logic_error("side exchange");
            answers[t+1]/=2;
        }
        peak=std::max(peak,count);current=std::move(next);
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"count\":"<<decimal(answers[n])<<",\"peak_states\":"<<peak
             <<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss<<",\"seconds\":"
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<",\"open\":[";
    for(int i=0;i<=n;i++){if(i)std::cout<<",";std::cout<<decimal(answers[i]);}
    std::cout<<"]}\n";
}
