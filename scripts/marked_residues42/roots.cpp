// On a centrally symmetric road the upper edges determine the reflected lower edges.
// Enumerate only the signed symbol orbits allowed by the specified second symmetry.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using W=unsigned __int128;

static std::string decimal(W x) {
    std::string s;
    do { s.push_back('0'+x%10); x/=10; } while(x);
    std::reverse(s.begin(),s.end());
    return s;
}

struct Search {
    int n,m,mode;
    std::array<int,120> word,inv;
    std::vector<std::pair<int,int>> edges;
    W calls=0;
    Search(int order,int type):n(order),m(2*n),mode(type) {
        word.fill(-1); inv.fill(-1);
        bool odd=mode==2 || mode==4;
        word[0]=inv[0]=odd?n:0; word[n]=inv[n]=odd?0:n;
    }
    int neg(int i) const { return (m-i)%m; }
    int half(int i) const { return (n+i)%m; }
    W visit() {
        ++calls;
        int i=0,best=-1;
        for(int a=1;a<n;++a) if(word[a]<0) {
            int score=(word[a-1]>=0)+(word[a+1]>=0);
            if(score>best) {best=score; i=a;}
        }
        if(!i) return 1;
        W count=0;
        bool odd=mode==2 || mode==4;
        for(int j=(i%2)^int(odd);j<m;j+=2) {
            if(inv[j]>=0) continue;
            int placed[4],used=0;
            auto put=[&](int a,int b) {
                if(word[a]>=0 || inv[b]>=0) return word[a]==b;
                word[a]=b; inv[b]=a; placed[used++]=a;
                return true;
            };
            auto pair=[&](int a,int b) {return put(a,b) && put(neg(a),neg(b));};
            bool ok=pair(i,j);
            if(mode==0) ok=ok && pair(half(i),neg(half(j)));
            else ok=ok && pair(j,mode>=3?neg(i):i);
            auto before=edges.size();
            for(int k=0;ok && k<used;++k) {
                int a=placed[k]&~1;
                if(word[a]<0 || word[a+1]<0) continue;
                bool seen=false;
                for(int t=0;t<k;++t) if((placed[t]&~1)==a) seen=true;
                if(seen) continue;
                auto u=std::min(word[a],word[a+1]),v=std::max(word[a],word[a+1]);
                for(auto [x,y]:edges)
                    if((u<x && x<v)!=(u<y && y<v)) {ok=false;break;}
                if(ok) edges.emplace_back(u,v);
            }
            if(ok) count+=visit();
            edges.resize(before);
            for(int k=0;k<used;++k) {int a=placed[k]; inv[word[a]]=-1;word[a]=-1;}
        }
        return count;
    }
};

int main(int argc,char**argv) {
    if(argc!=3) return 2;
    int n=std::stoi(argv[1]),t=std::stoi(argv[2]);
    if(n<3 || n>59 || !(n%2) || t<0 || t>4) return 3;
    Search s(n,t);
    auto start=std::chrono::steady_clock::now();
    auto value=s.visit();
    std::cout<<n<<" "<<t<<" "<<decimal(value)<<" "<<decimal(s.calls)<<" "
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
}
