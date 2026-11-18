// The size of the crossing clique is intrinsic to a word and nonincreasing.
// Calculate one complete clique level at a time, sharing all initial heights.
#define PROJECTIVE42_MERGED_NO_MAIN
#include "merged.cpp"

template<class W> void intrinsic(int n,int lo,int hi) {
    using Layer=std::unordered_map<int,MergedGroup<W>>;
    using Times=std::unordered_map<int,Layer>;
    auto begin=std::chrono::steady_clock::now();
    std::unordered_map<int,Times> queues;
    for(int h=lo;h<=hi;h+=2) {
        auto& g=queues[h<2?0:h][0].try_emplace(h|(h<<7),h,h,0,lo,hi).first->second;
        g.slots.add(g.compress(h==1?W(1):mask<W>(2*h)),1);
    }
    std::vector<U128> answers(n+1);
    std::vector<U64> by_time(n+1);
    U64 work=0,by_clique=0;
    for(int k=hi;k>=0;k--) {
        if(!queues.contains(k)) continue;
        auto times=std::move(queues.at(k)); queues.erase(k);
        int lower=k<=3?0:k-2;
        for(int t=0;t<n;t++) {
            if(!times.contains(t)) continue;
            auto current=std::move(times.at(t)); times.erase(t);
            int rem=n-t-1;
            auto dest=[&](int L,int R,Times& target) {
                int flip=L>R?1:L==R?2:0;
                if(flip==1) std::swap(L,R);
                auto& g=target[t+1].try_emplace(L|(R<<7),L,R,t+1,lo,hi).first->second;
                return Arm<W>{&g,flip,L+R};
            };
            U64 count=0;
            std::vector<int> keys;
            for(auto& [code,g]:current) if(g.slots.filled) {keys.push_back(code);count+=g.slots.filled;}
            by_time[t]+=count;by_clique=std::max(by_clique,count);
            std::sort(keys.begin(),keys.end());
            for(int code:keys) {
                auto node=current.extract(code);
                auto& src=node.mapped();
                int L=code&127,R=code>>7,size=L+R;
                bool left=L+1<=rem,right=R+1<=rem;
                bool lc=L&&L-1<=rem,rc=R&&R-1<=rem;
                Arm<W> oo,oc,co,cc,down;
                if(left&&right) oo=dest(L+1,R+1,times);
                if(L==R) oo.flip=0;
                if(left&&rc) oc=dest(L+1,R-1,times);
                if(lc&&right) co=dest(L-1,R+1,times);
                if(lc&&rc&&rem&&size>2) {
                    cc=dest(L-1,R-1,times);
                    if(k) down=dest(L-1,R-1,queues[lower]);
                }
                auto lm=mask<W>(L);
                work+=src.slots.filled*((left+lc)*(right+rc));
                for(U64 e:src.slots.cells) if(e) {
                    U128 v=src.slots.value(e);
                    W w=src.expand(e&src.slots.keymask);
                    if(oo.group) oo.add((w&lm)|(W(1)<<L)|((w&~lm)<<2),v);
                    if(oc.group) oc.add(w,v);
                    if(co.group) co.add(w,v);
                    if(cc.group) {
                        W d;
                        if(join(w,L-1,size,d)) {
                            bool reduced=k && popcount(d)-(size-2)/2!=k;
                            (reduced?down:cc).add(d,v);
                        }
                    } else if(L==1&&R==1) answers[t+1]+=v;
                }
            }
        }
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<hi<<",\"first_height\":"<<lo<<",\"count\":"<<decimal(answers[n])
             <<",\"peak_states\":"<<*std::max_element(by_time.begin(),by_time.end())
             <<",\"peak_clique_states\":"<<by_clique<<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss
             <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()
             <<",\"prefix_counts\":[";
    for(int i=0;i<=n;i++){if(i) std::cout<<",";std::cout<<decimal(answers[i]);}
    std::cout<<"]}\n";
}
int main(int argc,char**argv) {
    if(argc!=3&&argc!=4) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),hi=argc==4?std::stoi(argv[3]):lo;
    if(n<1||n>60||lo<0||lo>hi||hi>n||(n-hi)%2||(hi-lo)%2) return 2;
    if(n+hi<=64) intrinsic<U64>(n,lo,hi); else intrinsic<U128>(n,lo,hi);
}
