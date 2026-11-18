// Restore both incoming streams, including their original record order.
#include <bits/stdc++.h>
#include <sys/resource.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
namespace disk {
#include "spill.cpp"
}
#pragma GCC diagnostic pop
using namespace disk::folded;
using disk::Stream;
namespace fs=std::filesystem;

int main(int argc,char**argv){
    if(argc!=8)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]),old_n=std::stoi(argv[3]),t=std::stoi(argv[4]);
    fs::path root=argv[5];
    disk::Metadata m{};std::ifstream marker(root/"progress.txt");
    marker>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak;
    if(!marker||m.n!=n||m.h!=h||m.t!=t||old_n>=n)throw std::runtime_error("incorrect lifted checkpoint");
    int delta=n-old_n,B=old_n-(n-h)/2;
    unsigned large_width=sector_width(n,h),small_width=sector_width(old_n,h-delta);
    std::map<int,std::array<U,2>> sizes;
    auto visit=[&](auto group,auto record){
        U seen=0,hash=0;int last=-n-1;
        Stream in(root/disk::layer(t),"rb");
        while(seen<m.count){
            int d=int(in.get())-n;U count=in.pop();
            if(!count||count>m.count-seen||d<=last||d>n)throw std::runtime_error("invalid group");
            last=d;seen+=count;group(d);
            for(U i=0;i<count;++i){
                U key=in.pop();Wide w=in.value();hash+=disk::digest(key,w);
                width=large_width;State s=decode(key);
                int k=h-int(s.end),x=(t+1-int(s.left)-k)/2,y=(t-int(s.len-s.left)-k)/2;
                bool extra=x<=t-B||y<=t-1-B;
                record(d,s,key,w,extra);
            }
        }
        if(hash!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("lifted checksum");
    };
    visit([](int){},[&](int d,State,U,Wide,bool extra){++sizes[d][extra];});
    Stream first(argv[6],"wb"),second(argv[7],"wb");
    visit([&](int d){
        if(sizes[d][0]){first.byte(old_n+d);first.push(sizes[d][0]);}
        if(sizes[d][1]){second.byte(n+d);second.push(sizes[d][1]);}
    },[&](int,State s,U key,Wide w,bool extra){
        if(extra){second.push(key);second.weight(w);}
        else{s.end-=delta;width=small_width;first.push(code(s));first.weight(w);}
    });
    first.finish();second.finish();
}
