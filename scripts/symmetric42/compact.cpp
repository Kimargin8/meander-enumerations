// Height sectors with segmented hash tables and exact wide exceptional weights.
#define main contiguous_reference_entry
#include "fold.cpp"
#undef main
#include <array>
#include <limits>
#include <unordered_map>

using Wide=unsigned __int128;
constexpr uint32_t large=std::numeric_limits<uint32_t>::max();
constexpr unsigned bucket_bits=10, bucket_count=1u<<bucket_bits;

static std::string decimal(Wide value){
    std::string result;
    do{result.push_back(char('0'+value%10));value/=10;}while(value);
    std::reverse(result.begin(),result.end());
    return result;
}

struct Bucket{
    std::vector<U> keys;
    std::vector<uint32_t> values;
    size_t used=0;
    size_t locate(U key,U hash)const{
        auto mask=keys.size()-1, i=hash&mask;
        while(keys[i] && keys[i]!=key)i=(i+1)&mask;
        return i;
    }
    void grow(){
        Bucket dst;
        auto cap=std::max<size_t>(4,keys.size()*2);
        dst.keys.resize(cap);dst.values.resize(cap);
        for(size_t i=0;i<keys.size();++i)if(keys[i]){
            auto pos=dst.locate(keys[i],mix(keys[i]));
            dst.keys[pos]=keys[i];dst.values[pos]=values[i];
        }
        keys.swap(dst.keys);values.swap(dst.values);
    }
};

struct CompactTable{
    std::array<Bucket,bucket_count> buckets;
    std::unordered_map<U,Wide> exceptions;
    size_t used=0;
    void add(U key,Wide delta){
        auto hash=mix(key);
        auto &b=buckets[hash>>(64-bucket_bits)];
        if((b.used+1)*10>=b.keys.size()*8)b.grow();
        auto pos=b.locate(key,hash);
        if(!b.keys[pos]){b.keys[pos]=key;++b.used;++used;}
        auto &weight=b.values[pos];
        if(delta<large-weight)weight+=uint32_t(delta);
        else{
            auto &exact=exceptions[key];
            if(weight!=large){exact=weight;weight=large;}
            if(~Wide(0)-exact<delta)throw std::overflow_error("128-bit state weight");
            exact+=delta;
        }
    }
    template<class Visitor> void visit(Visitor f)const{
        for(auto &b:buckets)for(size_t i=0;i<b.keys.size();++i)if(b.keys[i]){
            auto w=b.values[i];
            f(b.keys[i],w==large?exceptions.at(b.keys[i]):Wide(w));
        }
    }
};

static unsigned sector_width(int n,int h){
    unsigned result=1;
    for(int t=0;t<n;++t){
        int a=n-1-t;
        for(int u=(t+1)%2;u<=t+1;u+=2)for(int v=t%2;v<=t;v+=2){
            if(std::abs(u-h)>a || std::abs(v-h)>a+1)continue;
            int forced=std::max(0,std::min((u+h-a)/2,(v+h-a-1)/2));
            result=std::max(result,unsigned(u+v-2*forced));
        }
    }
    return result+1;
}

static void count(int n,int h){
    width=sector_width(n,h);
    if(width+18>64)throw std::runtime_error("sector exceeds 64-bit state key");
    CompactTable cur;
    cur.add(code({0,1,1,0,unsigned(h)}),1);
    size_t peak=cur.used;
    U work=0;
    auto start=std::chrono::steady_clock::now();
    for(int t=1;t<n;++t){
        CompactTable next;
        cur.visit([&](U key,Wide weight){
            auto state=decode(key);
            for(int l=0;l<2;++l)for(int r=0;r<2;++r){
                State b=state;
                unsigned u=b.left,v=b.len-u;
                if((!l&&!u)||(!r&&!v))continue;
                ++work;
                if(l && r){
                    b.bits=insert(b.bits,u,1,2);b.len+=2;++b.left;
                    if(b.defect>=u)b.defect+=2;
                }else if(l!=r)b.left+=l?1:-1;
                else{if(!join(b,u-1,u))continue;--b.left;}
                if(prune(b,n-1-t,true))next.add(code(b),weight);
            }
        });
        peak=std::max(peak,next.used);
        std::swap(cur,next);
    }
    Wide total=0;
    cur.visit([&](U key,Wide weight){
        State s=decode(key);
        if(s.left!=s.end || std::abs(int(2*s.left)-int(s.len))!=1)return;
        while(s.len>1){if(!join(s,0,s.len-1))return;--s.left;}
        if(~Wide(0)-total<weight)throw std::overflow_error("128-bit sector sum");
        total+=weight;
    });
    rusage m{};getrusage(RUSAGE_SELF,&m);
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<n<<" "<<h<<" "<<decimal(total)<<" "<<peak<<" "<<work<<" "<<m.ru_maxrss
             <<" "<<elapsed<<" "<<width<<std::endl;
}

int main(int argc,char**argv){
    if(argc!=4)return 2;
    int lo=std::stoi(argv[1]),hi=std::stoi(argv[2]),h=std::stoi(argv[3]);
    if(lo<1 || lo!=hi || hi>61 || h<0 || h>lo || (lo-h)%2)return 3;
    count(lo,h);
}
