// Sector transfer on half-turn orbits. Each slot holds a compact port key and
// an exact weight. Exceptional weights spill into a sparse 128-bit side table.
#define main word_driver
#include "word.cpp"
#undef main
#include <algorithm>
#include <immintrin.h>

template<class W> struct Codec {
    W keep, fixed;
    int width, lower;
    Codec(Shape s,int h) {
        int u=std::max(0,s.a+s.b-h);
        if(u<2) u=0;
        int size=s.a+s.b+s.l+s.r;
        fixed=(mask<W>(u)<<(s.a-u))|(mask<W>(u)<<(s.a+s.l+s.r));
        keep=mask<W>(size)^fixed;
        width=size-2*u;
        lower=std::popcount(U64(keep));
        if(width>=63) throw std::logic_error("key width");
    }
    U64 compress(W w) const {
        U64 result=_pext_u64(U64(w),U64(keep));
        if constexpr(sizeof(W)>8) {
            if(lower<64) result|=_pext_u64(U64(w>>64),U64(keep>>64))<<lower;
        }
        return result;
    }
    W expand(U64 key) const {
        W result=_pdep_u64(key,U64(keep));
        if constexpr(sizeof(W)>8) {
            if(lower<64) result|=W(_pdep_u64(key>>lower,U64(keep>>64)))<<64;
        }
        return result|fixed;
    }
};

struct Slots {
    std::vector<U64> cells;
    std::unordered_map<U64,U128> spills;
    U64 keymask=0,limit=0;
    int width=0;
    size_t filled=0;
    void init(int w) {
        width=w; keymask=mask<U64>(w); limit=~U64(0)>>w;
    }
    void grow() {
        std::vector<U64> expanded(std::max<size_t>(8,2*cells.size()));
        for(U64 e:cells) if(e) {
            size_t p=mix(e&keymask)&(expanded.size()-1);
            while(expanded[p]) p=(p+1)&(expanded.size()-1);
            expanded[p]=e;
        }
        cells.swap(expanded);
    }
    void add(U64 key,U128 value) {
        if(filled*4>=cells.size()*3) grow();
        size_t p=mix(key)&(cells.size()-1);
        while(cells[p] && ((cells[p]^key)&keymask)) p=(p+1)&(cells.size()-1);
        U64 old=cells[p]>>width;
        if(!old) ++filled;
        U128 combined=(old==limit?spills.at(key):U128(old))+value;
        if(combined>=limit) {
            spills[key]=combined; cells[p]=key|(limit<<width);
        } else cells[p]=key|(U64(combined)<<width);
    }
    U128 value(U64 e) const {
        U64 raw=e>>width;
        return raw==limit?spills.at(e&keymask):U128(raw);
    }
};

static U64 reverse64(U64 x) {
    x=((x&0x5555555555555555ULL)<<1)|((x>>1)&0x5555555555555555ULL);
    x=((x&0x3333333333333333ULL)<<2)|((x>>2)&0x3333333333333333ULL);
    x=((x&0x0f0f0f0f0f0f0f0fULL)<<4)|((x>>4)&0x0f0f0f0f0f0f0f0fULL);
    return __builtin_bswap64(x);
}
template<class W> W reflect(W w,int size) {
    if(!size) return w;
    W reversed=(~w)&mask<W>(size);
    if(2*popcount(w)>size) {
        // Match ordinary parentheses from the last prefix minimum.
        int level=0,lo=0,cut=0;
        for(int i=0;i<size;i++) {
            level+=2*int((w>>i)&1)-1;
            if(level<lo) {lo=level;cut=i+1;}
        }
        int missing=0;
        for(int k=size-1;k>=0;k--) {
            int i=cut+k; if(i>=size)i-=size;
            if(!((w>>i)&1)) ++missing;
            else if(missing) --missing;
            else reversed|=W(1)<<i;
        }
    }
    if constexpr(sizeof(W)==8) return reverse64(reversed)>>(64-size);
    else return ((W(reverse64(U64(reversed)))<<64)|reverse64(U64(reversed>>64)))>>(128-size);
}

template<class W> struct Group {
    Codec<W> codec;
    Slots slots;
    Group(Shape s,int h):codec(s,h) {slots.init(codec.width);}
};
template<class W> struct Target {
    Group<W>* group=nullptr;
    int flip=0,size=0;
    void add(W w,U128 v) const {
        if(flip==1) w=reflect(w,size);
        if(flip==2) w=std::min(w,reflect(w,size));
        group->slots.add(group->codec.compress(w),v);
    }
};

template<class W> void fold(int n,int h) {
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<std::uint32_t,Group<W>>;
    Layer current;
    Shape initial(0,0,h,h);
    auto& first=current.try_emplace(initial.code(),initial,h).first->second;
    first.slots.add(first.codec.compress(h==1?W(1):mask<W>(2*h)),1);
    U128 answer=0;
    U64 peak=1,work=0;
    for(int t=0;t<n;t++) {
        Layer next;
        int rem=n-t-1;
        auto target=[&](Shape s) {
            int left=(s.a<<7)|s.l,right=(s.b<<7)|s.r;
            int flip=left>right?1:left==right?2:0;
            if(flip==1){std::swap(s.a,s.b);std::swap(s.l,s.r);}
            auto& group=next.try_emplace(s.code(),s,h).first->second;
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
#ifndef PROJECTIVE42_NO_MAIN
int main(int argc,char**argv){
    if(argc!=3) return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]);
    if(n<1||n>60||h<0||h>n||(n-h)%2) return 2;
    if(n+h<=64) fold<U64>(n,h);
    else fold<U128>(n,h);
}
#endif
