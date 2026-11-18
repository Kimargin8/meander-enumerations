// A projective frontier as a cyclic word. Unmatched opens are paired antipodally.
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/resource.h>

using U64=std::uint64_t;
using U128=__uint128_t;

template<class W> W mask(int n) {
    return n==int(8*sizeof(W)) ? ~W(0) : (W(1)<<n)-1;
}
template<class W> int popcount(W w) {
    if constexpr (sizeof(W)==8) return std::popcount(U64(w));
    else return std::popcount(U64(w))+std::popcount(U64(w>>64));
}
static U64 mix(U64 x) {
    x^=x>>30; x*=0xbf58476d1ce4e5b9ULL;
    x^=x>>27; x*=0x94d049bb133111ebULL;
    return x^(x>>31);
}
template<class W> static U64 hash_word(W w) {
    if constexpr(sizeof(W)==8) return mix(U64(w));
    else return mix(U64(w)^std::rotl(mix(U64(w>>64)),23));
}

template<class W> struct Table {
    std::vector<W> keys;
    std::vector<U64> vals;
    std::size_t filled=0;
    void insert_no_resize(W key,U64 value) {
        auto cap=vals.size();
        std::size_t p=hash_word(key)&(cap-1);
        while(vals[p] && keys[p]!=key) p=(p+1)&(cap-1);
        if(!vals[p]) {++filled; keys[p]=key;}
        if(std::numeric_limits<U64>::max()-vals[p]<value) throw std::overflow_error("cell");
        vals[p]+=value;
    }
    void add(W key,U64 value) {
        if(filled*4>=vals.size()*3) {
            Table bigger;
            auto cap=std::max<std::size_t>(8,vals.size()*2);
            bigger.keys.resize(cap); bigger.vals.resize(cap);
            for(std::size_t p=0;p<vals.size();p++)
                if(vals[p]) bigger.insert_no_resize(keys[p],vals[p]);
            *this=std::move(bigger);
        }
        insert_no_resize(key,value);
    }
};

// When at most one essential pair is left the link pattern is planar. Recover
// its ordinary rooted Dyck word, independently of the gap used by the cyclic word.
template<class W> W planar(W w,int size) {
    int depth=0,lo=0,cut=0;
    for(int i=0;i<size;i++) {
        depth+=int((w>>i)&1)*2-1;
        if(depth<lo) {lo=depth; cut=i+1;}
    }
    std::array<int,128> stack;
    int top=0;
    W out=0;
    for(int k=0;k<size;k++) {
        int i=cut+k; if(i>=size) i-=size;
        if((w>>i)&1) stack[top++]=i;
        else {
            if(!top) throw std::logic_error("cyclic Dyck rotation");
            int j=stack[--top];
            out|=W(1)<<std::min(i,j);
        }
    }
    if(top==2) out|=W(1)<<std::min(stack[0],stack[1]);
    else if(top) throw std::logic_error("nonplanar reduction");
    return out;
}

template<class W> int forward_mate(W w,int i,int size) {
    int depth=1, j=i;
    for(int s=1;s<size;s++) {
        if(++j==size) j=0;
        depth+=int((w>>j)&1)*2-1;
        if(depth==0) return j;
    }
    return -1;
}
template<class W> int backward_mate(W w,int i,int size) {
    int depth=1,j=i;
    for(int s=1;s<size;s++) {
        if(--j<0) j=size-1;
        depth+=1-2*int((w>>j)&1);
        if(depth==0) return j;
    }
    throw std::logic_error("closing without cyclic opener");
}

template<class W> bool join(W w,int pos,int size,W& result) {
    int pair=int((w>>pos)&3);
    if(pair==1) return false; // the two adjacent ports already share an arc
    bool reroot=false;
    if(pair==0) {
        int j=backward_mate(w,pos,size);
        w^=W(1)<<j;
    } else if(pair==3) {
        int j=forward_mate(w,pos+1,size);
        if(j>=0) w^=W(1)<<j;
        else {
            int essential=popcount(w)-size/2;
            if(essential==1) return false;
            if(essential<2) throw std::logic_error("missing essential pair");
            // Walk back before the two adjacent essential ends. Their two
            // opposite partners are adjacent after skipping balanced intervals.
            int skip=essential-1, need=0;
            j=pos;
            while(skip) {
                if(--j<0) j=size-1;
                if(((w>>j)&1)==0) ++need;
                else if(need) --need;
                else --skip;
            }
            w^=W(1)<<j;
            reroot=essential<=3;
        }
    }
    W low=mask<W>(pos);
    result=(w&low)|((w>>2)&~low);
    if(reroot) result=planar(result,size-2);
    return true;
}

struct Shape {
    int l,r,a,b;
    std::uint32_t code() const {return l|(r<<7)|(a<<14)|(b<<21);}
    explicit Shape(std::uint32_t x):l(x&127),r(x>>7&127),a(x>>14&127),b(x>>21&127) {}
    Shape(int l_,int r_,int a_,int b_):l(l_),r(r_),a(a_),b(b_) {}
};

template<class W> std::string decimal(W value) {
    if(!value) return "0";
    std::string digits;
    while(value){digits.push_back('0'+int(value%10));value/=10;}
    return std::string(digits.rbegin(),digits.rend());
}

template<class W> void evaluate(int n,int h) {
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<std::uint32_t,Table<W>>;
    Layer current,next;
    current[Shape(0,0,h,h).code()].add(h==1?W(1):mask<W>(2*h),1);
    U128 answer=0;
    U64 peak=1,work=0;
    for(int t=0;t<n;t++) {
        next.clear();
        int rem=n-t-1;
        for(const auto &[code,table]:current) {
            Shape s(code);
            int L=s.l+s.a, R=s.r+s.b, size=L+R;
            bool lo=L+1<=rem, ro=R+1<=rem;
            bool lc=L!=0 && L-1<=rem, rc=R!=0 && R-1<=rem;
            int nl=s.l-(s.l>0), nr=s.r-(s.r>0);
            int na=s.a-(s.l==0), nb=s.b-(s.r==0);
            Table<W> *oo=nullptr,*oc=nullptr,*co=nullptr,*cc=nullptr;
            if(lo&&ro) oo=&next[Shape(s.l+1,s.r+1,s.a,s.b).code()];
            if(lo&&rc) oc=&next[Shape(s.l+1,nr,s.a,nb).code()];
            if(lc&&ro) co=&next[Shape(nl,s.r+1,na,s.b).code()];
            if(lc&&rc&&rem) cc=&next[Shape(nl,nr,na,nb).code()];
            auto low=mask<W>(L);
            work+=table.filled*((lo+lc)*(ro+rc));
            for(std::size_t k=0;k<table.vals.size();k++) {
                U64 v=table.vals[k];
                if(!v) continue;
                W w=table.keys[k];
                if(oo) oo->add((w&low)|(W(1)<<L)|((w&~low)<<2),v);
                if(oc) oc->add(w,v);
                if(co) co->add(w,v);
                if(cc) {
                    W dst;
                    if(join(w,L-1,size,dst)) cc->add(dst,v);
                } else if(!rem && lc && rc && size==2) answer+=v;
            }
        }
        U64 states=0;
        for(const auto &[code,table]:next) states+=table.filled;
        peak=std::max(peak,states);
        current.swap(next);
    }
    rusage u{}; getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<h<<",\"count\":"<<decimal(answer)
             <<",\"peak_states\":"<<peak<<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss
             <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"}\n";
}
int main(int argc,char**argv) {
    if(argc!=3) return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]);
    if(n<1 || n>60 || h<0 || h>n || (n-h)%2) return 2;
    if(n+h<=64) evaluate<U64>(n,h);
    else evaluate<U128>(n,h);
}
