// Direct Catalan enumeration. Heights are computed by membership of chord
// endpoints in two open semicircles, without any frontier or partial paths.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using U64=std::uint64_t;
int main(int argc,char**argv){
    if(argc!=2)return 2;
    int last=std::stoi(argv[1]);if(last<1||last>15)return 2;
    for(int n=1;n<=last;n++){
        std::array<int,32> mates{},stack{},q{};
        std::array<U64,16> by_height{},min_weight{};
        std::vector<U64> marked(n%2?1<<n:0);
        const U64 open[]={0,1,1,2,3,8,14,42,81,262,538,1828,3926,13820,30694,110954};
        U64 all=0,connected=0,periodic=0;
        int max_minimum=0;
        std::function<void(int,int)> generate=[&](int i,int top){
            if(i<2*n){
                if(top<2*n-i) {stack[top]=i;generate(i+1,top+1);}
                if(top){int j=stack[top-1];mates[i]=j;mates[j]=i;
                    generate(i+1,top-1);stack[top-1]=j;}
                return;
            }
            ++all;
            for(int t=0;t<n;t++) {
                int ht=0;
                for(int j=t;j<t+n;j++) {
                    int p=mates[j];ht+=p<t||p>=t+n;
                }
                q[t]=ht;
            }
            int l=0,r=0,a=q[0],b=q[0];
            for(int t=0;t<n;t++) {
                if(q[t]!=l+r+std::abs(a+b-q[0]))throw std::logic_error("rotated cut formula");
                if(mates[t]>=n)--a;else l+=mates[t]>t?1:-1;
                if(mates[t+n]<n)--b;else r+=mates[t+n]>t+n?1:-1;
            }
            int h=*std::min_element(q.begin(),q.begin()+n);
            max_minimum=std::max(max_minimum,h);
            if(n>=3&&3*h>n)throw std::logic_error("three arcs");
            int j=0,steps=0;
            do{j=mates[j];j=j<n?j+n:j-n;++steps;}while(j);
            if(steps!=n)return;
            if(n>1) {
                int changes[32]{},initial=0;
                for(int x=0;x<2*n;x++)if(x<mates[x]){
                    int y=mates[x],i=x,end=y;
                    if(y-x==n)throw std::logic_error("diameter in connected diagram");
                    if(y-x>n){i=y;end=x;++initial;}
                    changes[i]=1;changes[end]=-1;
                }
                int first=initial,other=initial;
                for(int x=0;x<n;x++) other+=changes[x];
                if(first+other!=q[0])throw std::logic_error("central start");
                int minL=n+1,maxL=-1,minR=n+1,maxR=-1,mixed=0;
                for(int t=0;t<=n;t++) {
                    if(first<0||other<0||first+other!=q[t%n])
                        throw std::logic_error("central cut sum");
                    if(!first){minL=std::min(minL,t);maxL=t;}
                    if(!other){minR=std::min(minR,t);maxR=t;}
                    if(t<n){mixed+=changes[t]!=changes[t+n];first+=changes[t];other+=changes[t+n];}
                }
                if(minL>maxR||minR>maxL)throw std::logic_error("disjoint zero spans");
                if(h==q[0]&&mixed<3*h)throw std::logic_error("variation");
            }
            ++connected;++by_height[q[0]];
            if(n%2) {
                unsigned set=0;
                for(int t=0;t<n;t++) if(q[t]==1)set|=1u<<t;
                ++marked[set];
            }
            if(q[0]!=h)return;
            int previous=n-1;
            while(previous>0&&q[previous]!=h)--previous;
            min_weight[h]+=n-previous;
            for(int shift=1;shift<2*n;shift++)if((2*n)%shift==0){
                bool fixed=true;
                for(int v=0;v<2*n&&fixed;v++)
                    fixed=(mates[(v+shift)%(2*n)]==(mates[v]+shift)%(2*n));
                if(fixed){++periodic;break;}
            }
        };
        generate(0,0);
        U64 weight=0;
        for(auto s:min_weight)weight+=s;
        if(weight!=connected)throw std::logic_error("root weighting");
        if(n%2) {
            for(int i=0;i<n;i++)for(int s=0;s<(1<<n);s++)
                if(!(s>>i&1))marked[s]+=marked[s|(1<<i)];
            for(unsigned s=1;s<marked.size();s++) {
                U64 expected=open[n];
                if(std::popcount(s)>=2) {
                    const int first=std::countr_zero(s);int previous=first;
                    expected=2;
                    for(int t=first+1;t<n;t++)if(s>>t&1){expected*=open[t-previous];previous=t;}
                    expected*=open[first+n-previous];
                }
                if(expected!=marked[s])throw std::logic_error("marked cuts factorization");
            }
        }
        std::cout<<"{\"n\":"<<n<<",\"all\":"<<all<<",\"connected\":"<<connected
                 <<",\"max_minimum\":"<<max_minimum<<",\"periodic_min_roots\":"<<periodic
                 <<",\"marked_subsets_verified\":"<<(marked.empty()?0:marked.size()-1)<<",\"fixed_cut\":[";
        for(int h=n%2;h<=n;h+=2){if(h>n%2)std::cout<<",";std::cout<<by_height[h];}
        std::cout<<"],\"minimum_cut\":[";
        for(int h=n%2;h<=n;h+=2){if(h>n%2)std::cout<<",";std::cout<<min_weight[h];}
        std::cout<<"]}\n";
    }
}
