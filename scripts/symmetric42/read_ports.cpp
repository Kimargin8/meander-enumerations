// Read the two one-port entries that count a smaller order on the same diagonal.
#include <bits/stdc++.h>
#include <sys/resource.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
namespace disk {
#include "spill.cpp"
}
#pragma GCC diagnostic pop
int main(int argc,char**argv){
    if(argc!=7)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]),t=std::stoi(argv[3]),j=(n-h)/2;
    if(n<1||h<0||h>n||(n-h)%2||t<0||t>=n)return 3;
    disk::U expected=std::stoull(argv[4]),hash=std::stoull(argv[5]);
    auto width=disk::folded::sector_width(n,h);
    int a=n-1-t;
    disk::U key_l=2|(disk::U(1)<<width)|(disk::U(a+1)<<(width+12));
    disk::U key_r=2|(disk::U(a)<<(width+12));
    disk::Wide left=0,right=0;bool found_l=false,found_r=false;
    disk::Stream in(argv[6],"rb");
    disk::U seen=0,check=0;
    while(seen<expected){
        int d=int(in.get())-n;auto cnt=in.pop();
        if(!cnt||cnt>expected-seen)throw std::runtime_error("incomplete group");
        seen+=cnt;
        for(disk::U i=0;i<cnt;++i){
            auto key=in.pop();auto w=in.value();check+=disk::digest(key,w);
            if(key==key_l){if(found_l||d!=1)throw std::logic_error("left entry");found_l=true;left=w;}
            if(key==key_r){if(found_r||d!=-1)throw std::logic_error("right entry");found_r=true;right=w;}
        }
    }
    if(check!=hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("layer checksum");
    if(t+1<2*j && (left||right))throw std::logic_error("unsupported deficit");
    if(~disk::Wide(0)-left<right)throw std::overflow_error("diagonal sum");
    std::cout<<t+1<<" "<<t+1-2*j<<" "<<disk::folded::decimal(left)<<" "
             <<disk::folded::decimal(right)<<" "<<disk::folded::decimal(left+right)<<"\n";
}
