// Out-of-core independent verifier, grouped by the number of left local openings.
#include <bits/stdc++.h>
#include <sys/resource.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
namespace model {
#include "fifo.cpp"
}
namespace disk {
#include "spill.cpp"
}
#pragma GCC diagnostic pop
namespace fs=std::filesystem;
using model::U;
using model::Wide;
using model::CompactTable;
using disk::Stream;

int main(int argc,char**argv){
    if(argc!=6)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]),j=(n-h)/2,stop=std::stoi(argv[5]);
    if(n<1||n>58||h<0||h>n||(n-h)%2)return 3;
    U budget=std::stoull(argv[4])<<20;
    fs::path dir=argv[3];
    model::word_width=2*j+2;model::position_bits=model::digits(2*j+1);model::queue_bits=model::digits(2*h);
    auto shift=model::word_width+2*model::position_bits+model::queue_bits;
    if(shift+model::digits(j)>64)throw std::runtime_error("FIFO key width");
    fs::create_directories(dir);
    disk::Metadata m{};
    if(fs::exists(dir/"progress.txt")){
        std::ifstream in(dir/"progress.txt");
        in>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak;
        if(!in||m.n!=n||m.h!=h||m.t<0||m.t>=n)throw std::runtime_error("invalid checkpoint");
    }else{
        Stream first(dir/disk::layer(0),"wb");
        m={n,h,0,0,0,0,0,0,0};
        auto seed=[&](model::Ports p){
            U key=model::encode(p,h);first.byte(p.opened);first.push(1);first.push(key);first.weight(1);
            ++m.count;m.hash+=disk::digest(key,1);
        };
        if(h)seed({1,1,1,0,{model::mark}});
        if(j)seed({1,1,0,1,{model::mark}});
        first.finish();m.peak=m.count;m.traffic=m.disk_peak=fs::file_size(dir/disk::layer(0));disk::save(dir,m);
    }
    auto start=std::chrono::steady_clock::now();Wide total=0;
    while(m.t<=n-1 && m.t<=stop){
        bool final=m.t==n-1;
        if(!final && m.t==stop)return 0;
        int step=m.t+1;
        auto old=dir/disk::layer(m.t),temp=dir/"pending.bin",dest=dir/disk::layer(step);
        U previous_bytes=fs::file_size(old);
        if(previous_bytes>budget)throw std::runtime_error("saved layer exceeds disk budget");
        Stream in(old,"rb"),out(temp,"wb");out.limit=budget-previous_bytes;
        std::map<int,std::unique_ptr<CompactTable>> next;
        U read=0,ih=0,count=0,oh=0,attempts=0;int last=-1;
        auto emit=[&](int last_group){
            while(!next.empty()&&next.begin()->first<=last_group){
                auto &entry=*next.begin();out.byte(entry.first);out.push(entry.second->used);
                count+=entry.second->used;
                entry.second->visit([&](U key,Wide w){out.push(key);out.weight(w);oh+=disk::digest(key,w);});
                next.erase(next.begin());
            }
        };
        while(read<m.count){
            int group=in.get();U records=in.pop();
            if(group<=last||group>j||!records||records>m.count-read)throw std::runtime_error("group header");
            emit(group-1);last=group;read+=records;
            for(U record=0;record<records;++record){
                U key=in.pop();Wide w=in.value();ih+=disk::digest(key,w);
                auto p=model::unpack(key,h);
                if(group!=p.opened)throw std::runtime_error("group routing");
                int a=p.left-std::max(0,p.delta),b=p.size-p.left-std::max(0,-p.delta);
                int x=p.opened,y=x+(p.size-1)/2-p.left+p.delta;
                if(final){
                    if(p.size==1&&x==j&&y==j&&((p.left==0&&p.delta==0)||(p.left==1&&p.delta==1))){
                        if(~Wide(0)-total<w)throw std::overflow_error("total");
                        total+=w;
                    }
                    continue;
                }
                auto ls=model::options(a,x,step-2*x+a,j,h,p.left-1,p.delta<0?p.size-1:-1);
                auto rs=model::options(b,y,step-1-2*y+b,j,h,p.left,p.delta>0?0:-1);
                for(int i=0;i<ls.count;++i)for(int k=0;k<rs.count;++k){
                    ++attempts;model::Ports dst;
                    if(!model::advance(p,ls.moves[i],rs.moves[k],dst))continue;
                    auto &table=next[dst.opened];if(!table)table=std::make_unique<CompactTable>();
                    table->add(model::encode(dst,h),w);
                }
            }
            emit(group);
        }
        if(ih!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("checkpoint checksum");
        in.finish();
        if(final){out.finish();fs::remove(temp);break;}
        emit(j);out.finish();U bytes=fs::file_size(temp);fs::rename(temp,dest);
        m={n,h,step,count,oh,std::max(count,m.peak),m.work+attempts,m.traffic+previous_bytes+bytes,
           std::max(m.disk_peak,previous_bytes+bytes)};
        disk::save(dir,m);fs::remove(old);
    }
    rusage ru{};getrusage(RUSAGE_SELF,&ru);
    auto secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<n<<" "<<h<<" "<<model::storage::decimal(total)<<" "<<m.peak<<" "<<m.work<<" "<<ru.ru_maxrss
             <<" "<<secs<<" "<<model::word_width<<" "<<m.traffic<<" "<<m.disk_peak<<"\n";
}
