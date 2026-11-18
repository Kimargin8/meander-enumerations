// Merging heights commutes with transfer since the bare cyclic matching, the
// remaining anchors and the stack lengths determine all legal continuations.
#define PROJECTIVE42_NO_MAIN
#include "fold.cpp"
template<class W> void combined(int n,int h,int lo) {
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<std::uint32_t,Group<W>>;
    Layer current;
    for(int seed=lo;seed<=h;seed+=2) {
        Shape initial(0,0,seed,seed);
    auto& first=current.try_emplace(initial.code(),initial,seed).first->second;
    first.slots.add(first.codec.compress(seed==1?W(1):mask<W>(2*seed)),1);
    }
    U128 answer=0;
    U64 peak=1,work=0;
    for(int t=0;t<n;t++) {
        Layer next;
        int rem=n-t-1;
        auto target=[&](Shape s) {
            int left=(s.a<<7)|s.l,right=(s.b<<7)|s.r;
            int flip=left>right?1:left==right?2:0;
            if(flip==1){std::swap(s.a,s.b);std::swap(s.l,s.r);}
            int maxseed=std::min({h,t+1+s.a-s.l,t+1+s.b-s.r});
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
        U64 states=0;
        for(const auto& [code,group]:next) states+=group.slots.filled;
        peak=std::max(peak,states); current=std::move(next);
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<h<<",\"count\":"<<decimal(answer)
             <<",\"peak_states\":"<<peak<<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss
             <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"}\n";
}
int main(int argc,char**argv) {
    if(argc!=4) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),h=std::stoi(argv[3]);
    if(n<1||n>60||lo<0||lo>h||h>n||(n-h)%2||(h-lo)%2) return 2;
    if(n+h<=64) combined<U64>(n,h,lo); else combined<U128>(n,h,lo);
}
