// Bucket running minima to trade a small amount of state merging for a lower
// peak working set. The bucket updates depend only on its old value and L,R.
#define PROJECTIVE42_MERGED_NO_MAIN
#include "merged.cpp"

template<class W> void coarse(int n,int lo,int hi,int stride) {
    using Layer=std::unordered_map<int,MergedGroup<W>>;
    using Times=std::unordered_map<int,Layer>;
    std::unordered_map<int,Times> pending;
    auto begin=std::chrono::steady_clock::now();
    auto bucket=[](int a,int b){return a|(b<<7);};
    for(int h=lo;h<=hi;h+=2) {
        int q=h/stride;
        auto& g=pending[bucket(q,q)][0].try_emplace(h|(h<<7),h,h,0,lo,hi).first->second;
        g.slots.add(g.compress(h==1?W(1):mask<W>(2*h)),1);
    }
    std::vector<U128> answers(n+1);
    std::vector<U64> states(n+1);
    U64 work=0,max_block=0;
    for(int b=hi/stride;b>=0;b--) for(int a=b;a>=0;a--) {
        int id=bucket(a,b);
        if(!pending.contains(id)) continue;
        auto times=std::move(pending.at(id));pending.erase(id);
        for(int t=0;t<n;t++) {
            if(!times.contains(t)) continue;
            auto current=std::move(times.at(t));times.erase(t);
            int rem=n-t-1;
            auto dest=[&](int L,int R) {
                int x=std::min(a,L/stride),y=std::min(b,R/stride);
                int kx=x*128+L,ky=y*128+R;
                int flip=kx>ky?1:kx==ky?2:0;
                if(flip==1) {std::swap(L,R);std::swap(x,y);}
                int block=bucket(x,y);
                if(block>id) throw std::logic_error("increasing minimum");
                auto& layer=(block==id?times:pending[block])[t+1];
                auto& g=layer.try_emplace(L|(R<<7),L,R,t+1,lo,hi).first->second;
                return Arm<W>{&g,flip,L+R};
            };
            std::vector<int> keys;
            U64 count=0;
            for(auto&[code,g]:current) if(g.slots.filled) {keys.push_back(code);count+=g.slots.filled;}
            states[t]+=count;max_block=std::max(max_block,count);
            std::sort(keys.begin(),keys.end());
            for(int code:keys) {
                auto node=current.extract(code);
                auto& src=node.mapped();
                int L=code&127,R=code>>7,size=L+R;
                bool left=L+1<=rem,right=R+1<=rem,lc=L&&L-1<=rem,rc=R&&R-1<=rem;
                Arm<W> oo,oc,co,cc;
                if(left&&right) oo=dest(L+1,R+1);
                if(a==b&&L==R) oo.flip=0;
                if(left&&rc) oc=dest(L+1,R-1);
                if(lc&&right) co=dest(L-1,R+1);
                if(lc&&rc&&rem&&size>2) cc=dest(L-1,R-1);
                auto lm=mask<W>(L);
                work+=src.slots.filled*((left+lc)*(right+rc));
                for(U64 e:src.slots.cells) if(e) {
                    U128 v=src.slots.value(e);
                    W w=src.expand(e&src.slots.keymask);
                    if(oo.group) oo.add((w&lm)|(W(1)<<L)|((w&~lm)<<2),v);
                    if(oc.group) oc.add(w,v);
                    if(co.group) co.add(w,v);
                    if(cc.group) {
                        W d;if(join(w,L-1,size,d)) cc.add(d,v);
                    } else if(L==1&&R==1) answers[t+1]+=v;
                }
            }
        }
    }
    rusage u{}; getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"first_height\":"<<lo<<",\"h\":"<<hi<<",\"stride\":"<<stride
             <<",\"count\":"<<decimal(answers[n])<<",\"transitions\":"<<work
             <<",\"peak_states\":"<<*std::max_element(states.begin(),states.end())
             <<",\"peak_block_states\":"<<max_block<<",\"rss_kib\":"<<u.ru_maxrss<<",\"seconds\":"
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()
             <<",\"prefix_counts\":[";
    for(int i=0;i<=n;i++){if(i)std::cout<<",";std::cout<<decimal(answers[i]);}
    std::cout<<"]}\n";
}
int main(int argc,char**argv) {
    if(argc!=5) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),hi=std::stoi(argv[3]),stride=std::stoi(argv[4]);
    if(n<1||n>60||lo<0||lo>hi||hi>n||(n-hi)%2||(hi-lo)%2||stride<1) return 2;
    if(n+hi<=64) coarse<U64>(n,lo,hi,stride); else coarse<U128>(n,lo,hi,stride);
}
