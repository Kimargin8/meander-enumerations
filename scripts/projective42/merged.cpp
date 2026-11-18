// Once ports are in cyclic order a local closing and an anchor closing
// consume the very same end. The separate anchor counts are unobservable.
#define PROJECTIVE42_NO_MAIN
#include "fold.cpp"
#include "fast.hpp"

template<class W> struct MergedGroup {
    W fixed,keep;
    int lower;
    Slots slots;
    MergedGroup(int L,int R,int t,int lo,int hi) {
        int h0=std::max({lo,L-t,R-t}), h1=std::min({hi,L+t,R+t});
        if(h0>h1) throw std::logic_error("infeasible group");
        auto anchors=[t](int height,int length){return std::max(0,(height-t+length)/2);};
        int size=L+R;
        fixed=0;
        if(size>=2*t+4) {
            int i=h1-anchors(h1,R), j=anchors(h0,L);
            int p=size-anchors(h0,R), q=size+anchors(h1,L)-h1;
            if(i<j) fixed|=mask<W>(j-i)<<i;
            if(p<q) fixed|=mask<W>(q-p)<<p;
        }
        keep=mask<W>(size)^fixed;
        lower=std::popcount(U64(keep));
        int width=popcount(keep);
        if(width>=63) throw std::length_error("combined key width, split height interval");
        slots.init(width);
    }
    U64 compress(W w) const {
        if((w&fixed)!=fixed) throw std::logic_error("anchor implication");
        U64 result=_pext_u64(U64(w),U64(keep));
        if constexpr(sizeof(W)>8) result|=_pext_u64(U64(w>>64),U64(keep>>64))<<lower;
        return result;
    }
    W expand(U64 key) const {
        W result=_pdep_u64(key,U64(keep));
        if constexpr(sizeof(W)>8) result|=W(_pdep_u64(key>>lower,U64(keep>>64)))<<64;
        return result|fixed;
    }
};

template<class W> struct Arm {
    MergedGroup<W>* group=nullptr;
    int flip=0,size=0;
    void add(W w,U128 v) const {
        if(flip==1) w=fast_reflect(w,size);
        if(flip==2) w=std::min(w,fast_reflect(w,size));
        group->slots.add(group->compress(w),v);
    }
};

template<class W> void merged(int n,int lo,int hi) {
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<int,MergedGroup<W>>;
    Layer current;
    for(int h=lo;h<=hi;h+=2) {
        auto& group=current.try_emplace(h|(h<<7),h,h,0,lo,hi).first->second;
        group.slots.add(group.compress(h==1?W(1):mask<W>(2*h)),1);
    }
    U128 answer=0;
    U64 peak=1,work=0;
    for(int t=0;t<n;t++) {
        Layer next;
        int rem=n-t-1;
        auto dest=[&](int L,int R) {
            int flip=L>R?1:L==R?2:0;
            if(flip==1) std::swap(L,R);
            auto& g=next.try_emplace(L|(R<<7),L,R,t+1,lo,hi).first->second;
            return Arm<W>{&g,flip,L+R};
        };
        std::vector<int> keys;
        for(auto& [code,g]:current) keys.push_back(code);
        std::sort(keys.begin(),keys.end());
        for(int code:keys) {
            auto node=current.extract(code);
            auto& src=node.mapped();
            int L=code&127,R=code>>7,size=L+R;
            bool left=L+1<=rem,right=R+1<=rem;
            bool lc=L && L-1<=rem,rc=R && R-1<=rem;
            Arm<W> oo,oc,co,cc;
            if(left&&right) oo=dest(L+1,R+1);
            if(L==R) oo.flip=0;
            if(left&&rc) oc=dest(L+1,R-1);
            if(lc&&right) co=dest(L-1,R+1);
            if(lc&&rc&&rem) cc=dest(L-1,R-1);
            auto low=mask<W>(L);
            work+=src.slots.filled*((left+lc)*(right+rc));
            for(U64 entry:src.slots.cells) if(entry) {
                U128 v=src.slots.value(entry);
                W w=src.expand(entry&src.slots.keymask);
                if(oo.group) oo.add((w&low)|(W(1)<<L)|((w&~low)<<2),v);
                if(oc.group) oc.add(w,v);
                if(co.group) co.add(w,v);
                if(cc.group) {
                    W d;
                    if(join(w,L-1,size,d)) cc.add(d,v);
                } else if(!rem && lc && rc && size==2) answer+=v;
            }
        }
        U64 states=0;for(auto& [code,g]:next) states+=g.slots.filled;
        peak=std::max(peak,states);
        current=std::move(next);
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<hi<<",\"first_height\":"<<lo<<",\"count\":"<<decimal(answer)
             <<",\"peak_states\":"<<peak<<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss
             <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"}\n";
}
#ifndef PROJECTIVE42_MERGED_NO_MAIN
int main(int argc,char**argv) {
    if(argc!=3&&argc!=4) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),hi=argc==4?std::stoi(argv[3]):lo;
    if(n<1||n>60||lo<0||lo>hi||hi>n||(n-hi)%2||(hi-lo)%2) return 2;
    if(n+hi<=64) merged<U64>(n,lo,hi); else merged<U128>(n,lo,hi);
}
#endif
