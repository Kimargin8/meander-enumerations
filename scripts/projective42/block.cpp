// Process the complete time evolution of a pair of remaining anchor counts.
// All edges between pairs decrease their lexicographic order (max,min).
#define PROJECTIVE42_NO_MAIN
#include "fold.cpp"

template<class W> void blocked(int n,int first_height,int last_height) {
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<std::uint32_t,Group<W>>;
    using Times=std::unordered_map<int,Layer>;
    std::unordered_map<int,Times> queues;
    auto block=[](int a,int b){return (b<<7)|a;};
    for(int h=first_height;h<=last_height;h+=2) {
        Shape s(0,0,h,h);
        auto& g=queues[block(h,h)][0].try_emplace(s.code(),s,h).first->second;
        g.slots.add(g.codec.compress(h==1?W(1):mask<W>(2*h)),1);
    }
    U128 answer=0;
    U64 peak_block_states=0,work=0;
    std::vector<U64> states(n+1);
    for(int b=last_height;b>=0;b--) for(int a=b;a>=0;a--) {
        int id=block(a,b);
        if(!queues.contains(id)) continue;
        Times times=std::move(queues.at(id));
        queues.erase(id);
        for(int t=0;t<n;t++) {
            if(!times.contains(t)) continue;
            Layer current=std::move(times.at(t));
            times.erase(t);
            U64 count=0;
            for(auto& [code,g]:current) count+=g.slots.filled;
            states[t]+=count;
            peak_block_states=std::max(peak_block_states,count);
            int rem=n-t-1;
            auto target=[&](Shape s) {
                int left=(s.a<<7)|s.l,right=(s.b<<7)|s.r;
                int flip=left>right?1:left==right?2:0;
                if(flip==1){std::swap(s.a,s.b);std::swap(s.l,s.r);}
                int dest=block(s.a,s.b);
                if(dest>id) throw std::logic_error("non-monotone anchors");
                auto& next=(dest==id?times:queues[dest])[t+1];
                int maxseed=std::min({last_height,t+1+s.a-s.l,t+1+s.b-s.r});
                auto& group=next.try_emplace(s.code(),s,maxseed).first->second;
                return Target<W>{&group,flip,s.a+s.l+s.b+s.r};
            };
            std::vector<std::uint32_t> keys;
            for(auto& [code,g]:current) keys.push_back(code);
            std::sort(keys.begin(),keys.end());
            for(auto code:keys) {
                auto node=current.extract(code);
                auto& src=node.mapped();
                Shape s(code);
                int L=s.l+s.a,R=s.r+s.b,size=L+R;
                bool lo=L+1<=rem,ro=R+1<=rem;
                bool lc=L && L-1<=rem,rc=R && R-1<=rem;
                int nl=s.l-(s.l>0),nr=s.r-(s.r>0),na=s.a-(s.l==0),nb=s.b-(s.r==0);
                Target<W> oo,oc,co,cc;
                if(lo&&ro) oo=target(Shape(s.l+1,s.r+1,s.a,s.b));
                if(lo&&rc) oc=target(Shape(s.l+1,nr,s.a,nb));
                if(lc&&ro) co=target(Shape(nl,s.r+1,na,s.b));
                if(lc&&rc&&rem) cc=target(Shape(nl,nr,na,nb));
                auto low=mask<W>(L);
                work+=src.slots.filled*((lo+lc)*(ro+rc));
                for(U64 entry:src.slots.cells) if(entry) {
                    U128 v=src.slots.value(entry);
                    W w=src.codec.expand(entry&src.slots.keymask);
                    if(oo.group) oo.add((w&low)|(W(1)<<L)|((w&~low)<<2),v);
                    if(oc.group) oc.add(w,v);
                    if(co.group) co.add(w,v);
                    if(cc.group) {
                        W dst;
                        if(join(w,L-1,size,dst)) cc.add(dst,v);
                    } else if(!rem && lc && rc && size==2) answer+=v;
                }
            }
        }
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<last_height<<",\"first_height\":"<<first_height
             <<",\"count\":"<<decimal(answer)<<",\"peak_states\":"<<*std::max_element(states.begin(),states.end())
             <<",\"max_block_states\":"<<peak_block_states<<",\"transitions\":"<<work
             <<",\"rss_kib\":"<<u.ru_maxrss<<",\"seconds\":"
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"}\n";
}
int main(int argc,char**argv) {
    if(argc!=3 && argc!=4) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),hi=argc==4?std::stoi(argv[3]):lo;
    if(n<1||n>60||lo<0||lo>hi||hi>n||(n-hi)%2||(hi-lo)%2) return 2;
    if(n+hi<=64) blocked<U64>(n,lo,hi); else blocked<U128>(n,lo,hi);
}
