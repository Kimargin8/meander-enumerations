// Stream successive frontier layers by stack-height difference.
#include <bits/stdc++.h>
#include <sys/resource.h>
namespace folded {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "compact.cpp"
#pragma GCC diagnostic pop
}
using namespace folded;
namespace fs=std::filesystem;

static std::string layer(int t){return "layer"+std::to_string(t)+".bin";}
struct Metadata{
    int n,h,t;
    U count,hash,peak,work,traffic,disk_peak;
};
static void save(const fs::path &dir,const Metadata &m){
    auto staging=dir/"progress.tmp";
    std::ofstream output(staging);
    output<<m.n<<' '<<m.h<<' '<<m.t<<' '<<m.count<<' '<<m.hash<<' '<<m.peak<<' '
          <<m.work<<' '<<m.traffic<<' '<<m.disk_peak<<'\n';
    output.close();
    if(!output)throw std::runtime_error("checkpoint write");
    fs::rename(staging,dir/"progress.txt");
}
struct Stream{
    FILE *f;
    std::array<char,1<<20> buffer;
    U written=0, limit=UINT64_MAX;
    Stream(const fs::path &p,const char *mode):f(std::fopen(p.c_str(),mode)){
        if(!f)throw std::runtime_error("could not open "+p.string());
        if(setvbuf(f,buffer.data(),_IOFBF,buffer.size()))throw std::runtime_error("stream buffer");
    }
    ~Stream(){if(f)fclose(f);}
    void finish(){auto p=f;f=nullptr;if(fclose(p))throw std::runtime_error("disk write");}
    void room(U bytes){
        if(limit-written<bytes)throw std::runtime_error("two-layer disk limit");
        written+=bytes;
    }
    void push(U x){room(8);if(fwrite_unlocked(&x,8,1,f)!=1)throw std::runtime_error("disk write");}
    U pop(){U x;if(fread_unlocked(&x,8,1,f)!=1)throw std::runtime_error("truncated file");return x;}
    void byte(unsigned x){room(1);if(putc_unlocked(x,f)==EOF)throw std::runtime_error("disk write");}
    unsigned get(){int x=getc_unlocked(f);if(x==EOF)throw std::runtime_error("truncated weight");return x;}
    void weight(Wide x){while(x>=128){byte((unsigned(x)&127)|128);x>>=7;}byte(x);}
    Wide value(){
        Wide x=0;
        for(int shift=0;shift<=126;shift+=7){
            auto b=get();
            if(shift==126 && b>3)throw std::runtime_error("invalid wide weight");
            x|=Wide(b&127)<<shift;if(b<128)return x;
        }
        throw std::runtime_error("invalid weight");
    }
};
static U digest(U key,Wide w){return mix(key^mix(U(w))^mix(U(w>>64)+12345));}

static void solve(int n,int h,const fs::path &dir,U budget,int stop){
    if(n<1||n>61||h<0||h>n||(n-h)%2)throw std::invalid_argument("sector");
    width=sector_width(n,h);
    if(width+18>64)throw std::runtime_error("key too wide");
    fs::create_directories(dir);
    Metadata m{};
    if(fs::exists(dir/"progress.txt")){
        std::ifstream in(dir/"progress.txt");
        in>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak;
        if(!in||m.n!=n||m.h!=h||m.t<0||m.t>=n)throw std::runtime_error("invalid checkpoint");
    }else{
        auto key=code({0,1,1,0,unsigned(h)});
        Stream first(dir/layer(0),"wb");
        first.byte(1+n);first.push(1);first.push(key);first.weight(1);first.finish();
        m={n,h,0,1,digest(key,1),1,0,18,18};save(dir,m);
    }
    auto start=std::chrono::steady_clock::now();
    while(m.t<n-1 && m.t<stop){
        const int t=m.t+1;
        auto old_path=dir/layer(m.t), out_path=dir/layer(t), temp=dir/"pending.bin";
        auto old_bytes=fs::file_size(old_path);
        Stream in(old_path,"rb"),out(temp,"wb");
        if(old_bytes>budget)throw std::runtime_error("saved layer exceeds disk budget");
        out.limit=budget-old_bytes;
        std::map<int,std::unique_ptr<CompactTable>> pending;
        U seen=0,read_hash=0,new_count=0,new_hash=0,work=0;
        auto emit=[&](int limit){
            while(!pending.empty() && pending.begin()->first<=limit){
                auto &p=*pending.begin();
                out.byte(p.first+n);out.push(p.second->used);new_count+=p.second->used;
                p.second->visit([&](U key,Wide w){
                    out.push(key);out.weight(w);new_hash+=digest(key,w);
                });
                pending.erase(pending.begin());
                auto pos=ftello(out.f);
                if(pos<0||U(pos)+old_bytes>budget)throw std::runtime_error("two-layer disk limit");
            }
        };
        int last=-n-3;
        while(seen<m.count){
            int d=int(in.get())-n;
            if(d<=last||d < -n||d>n)throw std::runtime_error("unordered input");
            emit(d-4);last=d;
            U cnt=in.pop();
            if(!cnt||cnt>m.count-seen)throw std::runtime_error("group size");
            seen+=cnt;
            for(U i=0;i<cnt;++i){
                auto key=in.pop();auto w=in.value();read_hash+=digest(key,w);
                State a=decode(key);
                if(int(2*a.left)-int(a.len)!=d)throw std::runtime_error("group routing");
                for(int l=0;l<2;++l)for(int r=0;r<2;++r){
                    State b=a;unsigned u=b.left,v=b.len-u;
                    if((!l&&!u)||(!r&&!v))continue;
                    ++work;
                    if(l&&r){
                        b.bits=insert(b.bits,u,1,2);b.len+=2;++b.left;
                        if(b.defect>=u)b.defect+=2;
                    }else if(l!=r)b.left+=l?1:-1;
                    else{if(!join(b,u-1,u))continue;--b.left;}
                    if(prune(b,n-1-t,true)){
                        int dst=d+2*(l-r);
                        auto &table=pending[dst];
                        if(!table)table=std::make_unique<CompactTable>();
                        table->add(code(b),w);
                    }
                }
            }
            emit(d-2);
        }
        if(read_hash!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("checkpoint checksum");
        emit(n);out.finish();in.finish();
        auto bytes=fs::file_size(temp);
        fs::rename(temp,out_path);
        m={n,h,t,new_count,new_hash,std::max(m.peak,new_count),m.work+work,m.traffic+old_bytes+bytes,
           std::max<U>(m.disk_peak,old_bytes+bytes)};
        save(dir,m);fs::remove(old_path);
        std::cerr<<t<<" "<<m.count<<" "<<bytes<<"\n";
    }
    if(m.t<n-1)return;
    Stream in(dir/layer(m.t),"rb");
    U seen=0,check=0;Wide total=0;
    while(seen<m.count){
        in.get();U cnt=in.pop();if(!cnt||cnt>m.count-seen)throw std::runtime_error("final group");
        seen+=cnt;
        for(U i=0;i<cnt;++i){
            auto key=in.pop();auto w=in.value();check+=digest(key,w);
            State s=decode(key);
            if(s.left!=s.end || std::abs(int(2*s.left)-int(s.len))!=1)continue;
            while(s.len>1){if(!join(s,0,s.len-1))break;--s.left;}
            if(s.len==1){if(~Wide(0)-total<w)throw std::overflow_error("total");total+=w;}
        }
    }
    if(check!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("final checksum");
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<n<<" "<<h<<" "<<decimal(total)<<" "<<m.peak<<" "<<m.work<<" "<<usage.ru_maxrss
             <<" "<<sec<<" "<<width<<" "<<m.traffic<<" "<<m.disk_peak<<"\n";
}
int main(int argc,char**argv){
    if(argc!=6)return 2;
    solve(std::stoi(argv[1]),std::stoi(argv[2]),argv[3],std::stoull(argv[4])<<20,std::stoi(argv[5]));
}
