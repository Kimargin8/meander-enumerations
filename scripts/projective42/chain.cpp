// Weighted minimum cuts, compressed tables and the three-arc cutoff.
#define PROJECTIVE42_MERGED_NO_MAIN
#include "merged.cpp"
#include "minpack.hpp"
template<class W> struct ChainArm {
    PackedGroup<W>* g=nullptr;
    int flip=0,size=0,stamp=-1;
    void add(W w,PairMass v) const {
        if(flip==1)w=fast_reflect(w,size);
        if(flip==2)w=std::min(w,fast_reflect(w,size));
        if(stamp>=0)v.gap=U128(stamp)*v.count;
        g->slots.add(g->codec.compress(w),v);
    }
};
template<class W> void minimum(int n,int h) {
    using Layer=std::unordered_map<int,PackedGroup<W>>;
    using Times=std::unordered_map<int,Layer>;
    auto begin=std::chrono::steady_clock::now();
    std::unordered_map<int,Times> queues;
    auto block=[](int a,int b){return a|(b<<7);};
    Shape initial(0,0,h,h);
    bool possible=n<3||3*h<=n;
    if(possible) {
        auto& first=queues[block(h,h)][0].try_emplace(initial.code(),initial,h,n).first->second;
        first.slots.add(first.codec.compress(h==1?W(1):mask<W>(2*h)),{1,0});
    }
    std::vector<U128> answers(n+1),roots(n+1);
    U64 work=0,maxblock=0;
    std::vector<U64> states(n);
    for(int b=h;b>=0;b--)for(int a=b;a>=0;a--) {
        int id=block(a,b);
        if(!queues.contains(id)) continue;
        auto times=std::move(queues.at(id));queues.erase(id);
        for(int t=0;t<n;t++) {
            if(!times.contains(t))continue;
            auto current=std::move(times.at(t));times.erase(t);
            int rem=n-t-1;
            auto dest=[&](Shape s)->ChainArm<W>{
                int q=s.l+s.r+std::abs(s.a+s.b-h);
                if(q<h) return {};
                int kx=s.a*128+s.l,ky=s.b*128+s.r;
                int flip=kx>ky?1:kx==ky?2:0;
                if(flip==1){std::swap(s.a,s.b);std::swap(s.l,s.r);}
                int dst=block(s.a,s.b);
                auto& layer=(dst==id?times:queues[dst])[t+1];
                auto& g=layer.try_emplace(s.code(),s,h,n).first->second;
                return {&g,flip,s.l+s.a+s.r+s.b,h==0?0:q==h?t+1:-1};
            };
            U64 count=0;std::vector<int> keys;
            for(auto&[code,g]:current)if(g.slots.used){keys.push_back(code);count+=g.slots.used;}
            states[t]+=count;maxblock=std::max(maxblock,count);
            std::sort(keys.begin(),keys.end());
            for(int code:keys) {
                auto node=current.extract(code);auto& src=node.mapped();
                Shape s(code);
                int L=s.a+s.l,R=s.b+s.r,size=L+R;
                bool left=L+1<=rem,right=R+1<=rem,lc=L&&L-1<=rem,rc=R&&R-1<=rem;
                int nl=s.l-(s.l>0),nr=s.r-(s.r>0),na=s.a-(s.l==0),nb=s.b-(s.r==0);
                ChainArm<W> oo,oc,co,cc;
                if(left&&right)oo=dest(Shape(s.l+1,s.r+1,s.a,s.b));
                if(s.l==s.r&&s.a==s.b)oo.flip=0;
                if(left&&rc)oc=dest(Shape(s.l+1,nr,s.a,nb));
                if(lc&&right)co=dest(Shape(nl,s.r+1,na,s.b));
                if(lc&&rc&&rem&&size>2)cc=dest(Shape(nl,nr,na,nb));
                const auto lm=mask<W>(L);
                work+=src.slots.used*((oo.g!=nullptr)+(oc.g!=nullptr)+(co.g!=nullptr)+(cc.g!=nullptr)+(L==1&&R==1));
                for(U64 e:src.slots.cells)if(e) {
                    PairMass mass=src.slots.get(e);
                    W w=src.codec.expand(e&src.slots.km);
                    if(oo.g)oo.add((w&lm)|(W(1)<<L)|((w&~lm)<<2),mass);
                    if(oc.g)oc.add(w,mass);
                    if(co.g)co.add(w,mass);
                    if(cc.g) {
                        W d;if(join(w,L-1,size,d))cc.add(d,mass);
                    } else if(L==1&&R==1) {
                        answers[t+1]+=U128(t+1)*mass.count-mass.gap;roots[t+1]+=mass.count;
                    }
                }
            }
        }
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<h<<",\"count\":"<<decimal(answers[n])<<",\"rooted\":"<<decimal(roots[n])
             <<",\"transitions\":"<<work<<",\"peak_states\":"<<*std::max_element(states.begin(),states.end())
             <<",\"max_block_states\":"<<maxblock<<",\"rss_kib\":"<<u.ru_maxrss<<",\"seconds\":"
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<",\"prefix_counts\":[";
    for(int i=0;i<=n;i++){if(i)std::cout<<",";std::cout<<decimal(answers[i]);}
    std::cout<<"]}\n";
}
int main(int argc,char**argv) {
    if(argc!=3)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]);
    if(n<1||n>60||h<0||h>n||(n-h)%2)return 2;
    if(n+h<=64)minimum<U64>(n,h);else minimum<U128>(n,h);
}
