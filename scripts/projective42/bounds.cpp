// Audit the counting bounds against every actual, unfolded shape through n=20.
#define main word_entry_for_bounds
#include "word.cpp"
#undef main
#include <map>

using ShapeCounts=std::unordered_map<std::uint32_t,std::array<U64,2>>;
static U64 binom[22][23];
static U128 forbidden[21];
static std::array<ShapeCounts,21> walks;

static void prepare(int n) {
    binom[0][0]=1;
    for(int t=1;t<=n;t++) for(int j=0;j<=t;j++)
        binom[t][j]=(j?binom[t-1][j-1]:0)+binom[t-1][j];
    std::array<U128,21> f{};
    f[0]=1; f[1]=25;
    for(int j=2;j<=n;j++) f[j]=25*f[j-1]-16*f[j-2];
    for(int t=0;t<=n;t++) for(int u=0;u<=t;u++) for(int v=0;v<=t;v++)
        forbidden[t]+=f[std::min(u,v)]*f[std::abs(u-v)]*f[t-std::max(u,v)];
    walks[0][Shape(0,0,0,0).code()][0]=1;
    for(int t=0;t<n;t++) for(const auto &[code,count]:walks[t]) {
        Shape s(code); // a,b here denote used anchors (negative running minima).
        for(int x=0;x<=1;x++) for(int y=0;y<=1;y++) {
            int l=x?s.l+1:std::max(0,s.l-1),r=y?s.r+1:std::max(0,s.r-1);
            int c=s.a+(!x&&!s.l),d=s.b+(!y&&!s.r);
            walks[t+1][Shape(l,r,c,d).code()][x&&y]+=count[0]+((x||y)?count[1]:0);
        }
    }
}
static U64 ballot(int t,int d){
    if(d>t||(d-t)%2) return 0;
    int j=(t+d)/2;
    return binom[t][j]-binom[t][j+1];
}
static void check(int n,int h) {
    using Layer=std::unordered_map<std::uint32_t,Table<U64>>;
    Layer current;
    current[Shape(0,0,h,h).code()].add(h==1?1:mask<U64>(2*h),1);
    U64 groups=0,states=0,histories=0;
    for(int t=0;t<n;t++) {
        Layer next;
        int rem=n-t-1;
        for(const auto &[code,table]:current) {
            if(!table.filled) continue;
            Shape s(code);
            int L=s.a+s.l,R=s.b+s.r,size=L+R;
            int c=h-s.a,d=h-s.b,dL=s.l+c,dR=s.r+d,D=dL+dR;
            int u=s.a+s.b-h;
            U64 fixed=0;
            if(u>=2) fixed=(mask<U64>(u)<<(h-s.b))|(mask<U64>(u)<<(size-s.b));
            U64 ways=0;
            for(size_t j=0;j<table.vals.size();j++) if(table.vals[j]) {
                if((table.keys[j]&fixed)!=fixed) throw std::logic_error("fixed bits");
                ways+=table.vals[j];
            }
            int free=size-std::popcount(fixed);
            auto lim=walks[t].at(Shape(s.l,s.r,c,d).code());
            if(free>D+2 || size>2*(n-t) || table.filled>(U64(1)<<free) ||
               ways>ballot(t,dL)*ballot(t,dR) || ways>lim[0]+lim[1] ||
               (U128(ways)<<(D+2*t))>forbidden[t]) throw std::logic_error("group bound");
            ++groups;states+=table.filled;histories+=ways;
            const bool left=L+1<=rem,right=R+1<=rem,lc=L&&L-1<=rem,rc=R&&R-1<=rem;
            const int nl=s.l-(s.l>0),nr=s.r-(s.r>0),na=s.a-(s.l==0),nb=s.b-(s.r==0);
            Table<U64> *oo=nullptr,*oc=nullptr,*co=nullptr,*cc=nullptr;
            if(left&&right) oo=&next[Shape(s.l+1,s.r+1,s.a,s.b).code()];
            if(left&&rc) oc=&next[Shape(s.l+1,nr,s.a,nb).code()];
            if(lc&&right) co=&next[Shape(nl,s.r+1,na,s.b).code()];
            if(lc&&rc&&rem) cc=&next[Shape(nl,nr,na,nb).code()];
            auto low=mask<U64>(L);
            for(size_t j=0;j<table.vals.size();j++) if(U64 v=table.vals[j]) {
                U64 w=table.keys[j],out;
                if(oo) oo->add((w&low)|(U64(1)<<L)|((w&~low)<<2),v);
                if(oc) oc->add(w,v);
                if(co) co->add(w,v);
                if(cc && join(w,L-1,size,out)) cc->add(out,v);
            }
        }
        current=std::move(next);
    }
    std::cout<<"{\"n\":"<<n<<",\"h\":"<<h<<",\"nonempty_shapes\":"<<groups<<",\"states\":"<<states
             <<",\"prefixes_with_multiplicity\":"<<histories<<",\"status\":\"passed\"}\n";
}
int main(int argc,char**argv) {
    if(argc!=2) return 2;
    int n=std::stoi(argv[1]);if(n<1||n>20) return 2;
    prepare(n);
    for(int i=1;i<=n;i++) for(int h=i%2;h<=i;h+=2) check(i,h);
}
