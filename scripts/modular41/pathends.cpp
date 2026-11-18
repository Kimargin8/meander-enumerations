// Count connected completions of a reflected matching with two free ends.
// Rank h is the number of A chords between the two fixed half intervals.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <sys/resource.h>

using Count=std::uint64_t;
using Table=std::unordered_map<std::string,Count>;
struct Action {int stack,anchor,port;bool make;};
struct Choices {Action action[3];int count=0;};
Choices choices(int s,int a,int base,int abase,int remain,bool active,int sentinel){
    Choices v;
    if(!active){v.action[v.count++]={s,a,sentinel,false};return v;}
    if(s+1+a<=remain)v.action[v.count++]={s+1,a,-1,true};
    if(s && s-1+a<=remain)v.action[v.count++]={s-1,a,base+s-1,false};
    if(!s && a && a-1<=remain)v.action[v.count++]={0,a-1,abase,false};
    return v;
}

void add(Count &dst,Count w){
    if(UINT64_MAX-dst<w)throw std::overflow_error("coefficient");
    dst+=w;
}
Count sector(int n,int h,std::size_t &peak,Count &work){
    // H matching between half-intervals identifies exposed positions in
    // the same order, because we scan the second half backwards.
    // A seed pair of sentinel ports supplies the external link 0--n.
    std::string seed(4+2*h+2,0);
    seed[2]=seed[3]=h;
    for(int i=0;i<h;i++){seed[4+i]=h+i;seed[4+h+i]=i;}
    seed[4+2*h]=2*h+1;seed[5+2*h]=2*h;
    Table prev{{seed,1}},next;
    Count answer=0;
    for(int t=0;t<=n;t++){
        next.clear();
        for(const auto &kv:prev){
            auto &p=kv.first;
            int u=p[0],v=p[1],a=p[2],b=p[3],size=static_cast<int>(p.size())-4;
            // t=0 consumes the left endpoint, t=n consumes the right.
            auto l=choices(u,a,0,u+v,n-t-1,t!=n,size-1);
            auto r=choices(v,b,u,u+v+a,n-t,t!=0,size-1);
            // After the t=n vertex the right half must also be fully consumed.
            if(t==n)r=choices(v,b,u,u+v+a,0,true,size-1);
            for(int x=0;x<l.count;x++)for(int y=0;y<r.count;y++){
                auto L=l.action[x],R=r.action[y];
                ++work;
                int i=L.make?size:L.port,j=R.make?size+1:R.port;
                if(!L.make&&!R.make&&p[4+i]==j){
                    if(t==n&&size==2)add(answer,kv.second);
                    continue;
                }
                unsigned char old[90],ren[90],mate[90];
                std::copy_n(reinterpret_cast<const unsigned char*>(p.data()+4),size,mate);
                if(L.make&&R.make){mate[i]=j;mate[j]=i;}
                else if(L.make){int z=mate[j];mate[i]=z;mate[z]=i;}
                else if(R.make){int z=mate[i];mate[j]=z;mate[z]=j;}
                else{int s=mate[i],q=mate[j];mate[s]=q;mate[q]=s;}
                int k=0;
                auto retain=[&](int z){if(z!=L.port&&z!=R.port){old[k]=z;ren[z]=k++;}};
                for(int z=0;z<u;z++)retain(z);
                if(L.make)retain(i);
                for(int z=u;z<u+v;z++)retain(z);
                if(R.make)retain(j);
                for(int z=u+v;z<size;z++)retain(z);
                std::string key(4+k,0);
                key[0]=L.stack;key[1]=R.stack;key[2]=L.anchor;key[3]=R.anchor;
                for(int z=0;z<k;z++)key[4+z]=ren[mate[old[z]]];
                add(next[std::move(key)],kv.second);
            }
        }
        peak=std::max(peak,next.size());
        prev.swap(next);
    }
    return answer;
}
int main(int argc,char**argv){
    if(argc!=3)return 2;
    int lo=std::atoi(argv[1]),hi=std::atoi(argv[2]);
    if(lo<1||hi>39||hi<lo)return 3;
    for(int n=lo;n<=hi;n++){
        auto start=std::chrono::steady_clock::now();
        Count answer=0,work=0;std::size_t peak=0;
        for(int h=n%2;h<=n;h+=2)add(answer,sector(n,h,peak,work));
        rusage used{};getrusage(RUSAGE_SELF,&used);
        std::cout<<n<<" "<<answer<<" "<<work<<" "<<peak<<" "<<used.ru_maxrss<<" "
                 <<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
    }
}
