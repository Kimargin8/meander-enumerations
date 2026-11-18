// Reverse the two paths: start from the empty disk, and glue antipodal stubs
// only at the final step. The intermediate link patterns here are planar.
#define PROJECTIVE42_MERGED_NO_MAIN
#include "merged.cpp"

static bool connected(U64 w,int h) {
    if(!h) return false;
    std::array<int,64> path{},stack{};
    int top=0;
    for(int i=0;i<2*h;i++) {
        if((w>>i)&1) stack[top++]=i;
        else {
            if(!top) throw std::logic_error("non-Dyck reverse pattern");
            int j=stack[--top]; path[i]=j;path[j]=i;
        }
    }
    int j=0,len=0;
    do {j=path[j];j=j<h?j+h:j-h;++len;} while(j);
    return len==h;
}

int main(int argc,char**argv) {
    if(argc!=4) return 2;
    int n=std::stoi(argv[1]),lo=std::stoi(argv[2]),hi=std::stoi(argv[3]);
    if(n<1||n>30||lo<0||hi<lo||hi>n||(n-hi)%2||(hi-lo)%2) return 2;
    auto begin=std::chrono::steady_clock::now();
    using Layer=std::unordered_map<int,Slots>;
    Layer current;
    current[0].init(0);current[0].add(0,1);
    std::vector<U128> by_height(n+1);
    U64 peak=1,work=0;
    for(int t=0;t<n;t++) {
        Layer next;
        int rem=n-t-1;
        auto next_shape=[&](int L,int R)->Slots* {
            if(L<0||R<0||std::max({lo,L-rem,R-rem})>std::min({hi,L+rem,R+rem})) return nullptr;
            if(L>R) std::swap(L,R);
            auto [it,made]=next.try_emplace(L|(R<<7));
            if(made) it->second.init(L+R);
            return &it->second;
        };
        for(auto& [code,slots]:current) {
            int L=code&127,R=code>>7,size=L+R;
            auto oo=next_shape(L+1,R+1),cc=next_shape(L-1,R-1);
            auto oc=next_shape(L+1,R-1),co=next_shape(L-1,R+1);
            auto insert=[&](Slots* s,int a,int b,U64 w,U128 v) {
                if(a>b) w=fast_reflect(w,a+b);
                if(a==b) w=std::min(w,fast_reflect(w,a+b));
                s->add(w,v);
            };
            U64 lm=mask<U64>(L);
            for(U64 e:slots.cells) if(e) {
                U64 w=e&slots.keymask;U128 v=slots.value(e);
                if(oo){++work;insert(oo,L+1,R+1,(w&lm)|(U64(1)<<L)|((w&~lm)<<2),v);}
                if(oc){++work;insert(oc,L+1,R-1,w,v);}
                if(co){++work;insert(co,L-1,R+1,w,v);}
                if(cc) {
                    ++work;U64 d;
                    if(join(w,L-1,size,d)) insert(cc,L-1,R-1,d,v);
                    else if(!rem&&L==1&&R==1) by_height[0]+=v;
                }
            }
        }
        U64 cnt=0;for(auto& [code,s]:next) cnt+=s.filled;
        peak=std::max(peak,cnt);current=std::move(next);
    }
    U128 answer=0;
    for(auto& [code,s]:current) {
        int h=code&127;
        if(h!=code>>7 || h<lo || h>hi) throw std::logic_error("terminal");
        for(U64 e:s.cells) if(e && connected(e&s.keymask,h)) by_height[h]+=s.value(e);
    }
    for(auto a:by_height) answer+=a;
    rusage u{};getrusage(RUSAGE_SELF,&u);
    std::cout<<"{\"n\":"<<n<<",\"count\":"<<decimal(answer)<<",\"peak_states\":"<<peak
             <<",\"transitions\":"<<work<<",\"rss_kib\":"<<u.ru_maxrss<<",\"seconds\":"
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()
             <<",\"by_height\":[";
    for(int j=lo;j<=hi;j+=2){if(j>lo)std::cout<<",";std::cout<<decimal(by_height[j]);}
    std::cout<<"]}\n";
}
