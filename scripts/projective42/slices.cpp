// Orient every chord around the central face. Two ordinary stacks and the
// order of their zero visits suffice; frozen incoming ports close at the seam.
#define PROJECTIVE42_MERGED_NO_MAIN
#include "merged.cpp"
#include "minpack.hpp"

static int advance(int phase,int L,int R) {
    if(phase==5 || (L&&R))return phase;
    bool left=L==0;
    if(!phase)return left?1:2;
    if(phase==1)return left?1:3;
    if(phase==2)return left?4:2;
    if(phase==3)return left?5:3;
    return left?4:5;
}
static bool one_cycle(U64 word,int h) {
    int mates[64],stack[64],top=0;
    for(int i=0;i<2*h;i++) {
        if(word>>i&1)stack[top++]=i;
        else {int j=stack[--top];mates[j]=i;mates[i]=j;}
    }
    int i=0,j=0;
    do {i=mates[i];i=i<h?i+h:i-h;++j;}while(i);
    return j==h;
}
struct Feasible {
    int n,h,k;
    std::unordered_map<std::uint32_t,bool> known;
    bool can(int rem,int L,int R,int phase) {
        if(L<0||R<0||L+R<h||L+R>n-2*h||std::abs(L-h+k)>rem||std::abs(R-k)>rem)return false;
        if(!rem)return phase==5;
        std::uint32_t id=L|(R<<7)|(phase<<14)|(rem<<18);
        auto it=known.find(id);if(it!=known.end())return it->second;
        bool ok=false;
        for(int dx:{-1,1})for(int dy:{-1,1}) {
            int x=L+dx,y=R+dy;
            if(!ok&&x>=0&&y>=0)ok=can(rem-1,x,y,advance(phase,x,y));
        }
        known.emplace(id,ok);return ok;
    }
};
struct SliceTarget {
    PackedGap* table=nullptr;
    int stamp=-1;
    void add(U64 w,PairMass v) const {
        if(stamp>=0)v.gap=U128(stamp)*v.count;
        table->add(w,v);
    }
};
int main(int argc,char**argv) {
    if(argc!=3)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]);
    if(n<3||n>60||h<=0||h>n||(n-h)%2)return 2;
    auto begin=std::chrono::steady_clock::now();
    U64 work=0,peak=0;U128 answer=0,rooted=0;
    std::unordered_map<U64,bool> terminal;
    std::vector<U128> splits;
    for(int k=0;k<=h/2;k++) {
        int phase0=advance(0,k,h-k);
        Feasible possible{n,h,k,{}};
        if(!possible.can(n,k,h-k,phase0)){splits.push_back(0);continue;}
        using Layer=std::unordered_map<int,PackedGap>;
        Layer source;
        int code=k|((h-k)<<7)|(phase0<<14);
        source.try_emplace(code,2*h,n).first->second.add(mask<U64>(h),{1,0});
        for(int t=0;t<n;t++) {
            Layer next;int rem=n-t-1;
            auto make=[&](int L,int R,int p)->SliceTarget {
                if(L<0||R<0)return {};
                p=advance(p,L,R);
                if(!possible.can(rem,L,R,p))return {};
                auto& g=next.try_emplace(L|(R<<7)|(p<<14),h+L+R,n).first->second;
                return {&g,rem&&L+R==h?t+1:-1};
            };
            std::vector<int> keys;
            U64 count=0;
            for(auto&[s,g]:source)if(g.used){keys.push_back(s);count+=g.used;}
            peak=std::max(peak,count);std::sort(keys.begin(),keys.end());
            for(auto id:keys) {
                auto node=source.extract(id);auto& src=node.mapped();
                int L=id&127,R=(id>>7)&127,p=id>>14,cut=h+L,size=cut+R;
                auto oo=make(L+1,R+1,p),oc=make(L+1,R-1,p),co=make(L-1,R+1,p),cc=make(L-1,R-1,p);
                work+=src.used*((oo.table!=nullptr)+(oc.table!=nullptr)+(co.table!=nullptr)+(cc.table!=nullptr));
                U64 low=mask<U64>(cut);
                for(auto e:src.cells)if(e) {
                    auto v=src.get(e);U64 w=e&src.km;
                    if(oo.table)oo.add((w&low)|(U64(1)<<cut)|((w&~low)<<2),v);
                    if(oc.table)oc.add(w,v);
                    if(co.table)co.add(w,v);
                    if(cc.table){U64 d;if(join(w,cut-1,size,d))cc.add(d,v);}
                }
            }
            source=std::move(next);
        }
        U128 part=0,base=0;
        for(auto&[s,g]:source)for(U64 e:g.cells)if(e){
            auto w=e&g.km;
            auto [it,inserted]=terminal.try_emplace(w);
            if(inserted)it->second=one_cycle(w,h);
            if(it->second){auto v=g.get(e);base+=v.count;part+=U128(n)*v.count-v.gap;}
        }
        splits.push_back(part);
        int factor=2*k==h?1:2;answer+=factor*part;rooted+=factor*base;
    }
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<h<<",\"count\":"<<decimal(answer)<<",\"rooted\":"<<decimal(rooted)
             <<",\"transitions\":"<<work<<",\"peak_states\":"<<peak<<",\"rss_kib\":"<<u.ru_maxrss
             <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()
             <<",\"by_start_split\":[";
    for(size_t j=0;j<splits.size();j++){if(j)std::cout<<",";std::cout<<decimal(splits[j]);}
    std::cout<<"]}\n";
}
