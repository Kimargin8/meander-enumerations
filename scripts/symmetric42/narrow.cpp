// Trade sequential reads for memory: build just one output difference group.
#include <bits/stdc++.h>
#include <sys/resource.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
namespace disk {
#include "spill.cpp"
}
#pragma GCC diagnostic pop
namespace fs=std::filesystem;
using namespace disk::folded;
using disk::Stream;
using disk::Metadata;

struct Cursor {
    Stream file;
    U remaining, group_size=0;
    const int n;
    int difference,previous;
    Cursor(const fs::path& path,U count,int order):
        file(path,"rb"),remaining(count),n(order),difference(0),previous(-order-1){advance();}
    void advance(){
        if(!remaining){
            if(getc_unlocked(file.f)!=EOF)throw std::runtime_error("extra input");
            difference=n+5;group_size=0;return;
        }
        difference=int(file.get())-n;group_size=file.pop();
        if(difference<=previous||difference>n||!group_size||group_size>remaining)
            throw std::runtime_error("invalid input group");
        previous=difference;remaining-=group_size;
    }
};

static void transfer(State b,Wide weight,int n,int t,bool l,bool r,
                     CompactTable& table,U& attempted){
    unsigned u=b.left,v=b.len-u;
    if((!l&&!u)||(!r&&!v))return;
    ++attempted;
    if(l&&r){
        b.bits=insert(b.bits,u,1,2);b.len+=2;++b.left;
        if(b.defect>=u)b.defect+=2;
    }else if(l!=r)b.left+=l?1:-1;
    else{if(!join(b,u-1,u))return;--b.left;}
    if(prune(b,n-1-t,true))table.add(code(b),weight);
}

static void next_layer(const fs::path& dir,Metadata& m,U budget){
    const int n=m.n,t=m.t+1;
    auto src=dir/disk::layer(m.t),dst=dir/disk::layer(t),tmp=dir/"pending.bin";
    U old_bytes=fs::file_size(src);
    if(old_bytes>budget)throw std::runtime_error("input exceeds disk budget");
    Cursor up(src,m.count,n),flat(src,m.count,n),down(src,m.count,n);
    U read_hash=0,written_hash=0,count=0,attempted=0;
    Stream output(tmp,"wb");output.limit=budget-old_bytes;
    for(int d=-n-2;d<=n+2;++d){
        CompactTable table;
        auto consume=[&](Cursor& input,int change){
            if(input.difference!=d-change)return;
            for(U i=0;i<input.group_size;++i){
                U key=input.file.pop();Wide w=input.file.value();
                State s=decode(key);
                if(int(2*s.left)-int(s.len)!=input.difference)throw std::runtime_error("input routing");
                if(change==0){
                    read_hash+=disk::digest(key,w);
                    transfer(s,w,n,t,false,false,table,attempted);
                    transfer(s,w,n,t,true,true,table,attempted);
                }else transfer(s,w,n,t,change>0,change<0,table,attempted);
            }
            input.advance();
        };
        consume(up,2);consume(flat,0);consume(down,-2);
        if(!table.used)continue;
        if(std::abs(d)>n)throw std::logic_error("output routing");
        output.byte(d+n);output.push(table.used);count+=table.used;
        table.visit([&](U key,Wide w){output.push(key);output.weight(w);written_hash+=disk::digest(key,w);});
    }
    if(up.group_size||flat.group_size||down.group_size||read_hash!=m.hash)
        throw std::runtime_error("checkpoint checksum");
    output.finish();up.file.finish();flat.file.finish();down.file.finish();
    U bytes=fs::file_size(tmp);
    fs::rename(tmp,dst);
    m={m.n,m.h,t,count,written_hash,std::max(m.peak,count),m.work+attempted,
       m.traffic+3*old_bytes+bytes,std::max(m.disk_peak,old_bytes+bytes)};
    disk::save(dir,m);fs::remove(src);
    std::cerr<<t<<' '<<m.count<<' '<<m.work<<' '<<m.disk_peak<<'\n';
}

int main(int argc,char**argv){
    if(argc!=6)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]),stop=std::stoi(argv[5]);
    if(n<1||n>61||h<0||h>n||(n-h)%2||stop<0||stop>=n)return 2;
    fs::path dir=argv[3];
    U budget=std::stoull(argv[4])<<20;
    if(!fs::exists(dir/"progress.txt"))disk::solve(n,h,dir,budget,0);
    Metadata m{};std::ifstream marker(dir/"progress.txt");
    marker>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak;
    if(!marker||m.n!=n||m.h!=h||m.t<0||m.t>=n)throw std::runtime_error("invalid checkpoint");
    width=sector_width(n,h);
    if(width+18>64)throw std::runtime_error("key width");
    while(m.t<stop)next_layer(dir,m,budget);
    if(m.t==n-1)disk::solve(n,h,dir,budget,m.t);
}
