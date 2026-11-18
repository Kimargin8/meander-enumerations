// Count the connected completions of a rainbow with exactly j local arcs on each side.
// The state is the two stacks and the ordered queue of the folded-ray model (Ferudun, 2026).
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "positive.hpp"

struct Edges {
    std::array<int,4> to{};
    int used=0, terminal=0;
};

class Graph {
    int deficit;
    std::vector<std::string> keys;
    std::unordered_map<std::string,int> ids;
    int intern(const std::string &key) {
        auto [it,new_state]=ids.try_emplace(key,static_cast<int>(keys.size()));
        if (new_state) keys.push_back(key);
        return it->second;
    }
    enum class Event {open, close, through};
    void extend(const std::string &key, Event a, Event b, Edges &row) {
        int opened[2]={key[0],key[1]}, depths[2]={key[2],key[3]};
        int delta=2*(opened[1]-opened[0])+depths[0]-depths[1];
        const int m=static_cast<int>(key.size())-4, l=m, r=m+1;
        if (m!=depths[0]+depths[1]+std::abs(delta) || m+2>112)
            throw std::logic_error("port invariant");
        std::array<int,112> parent;
        for (int i=0;i<m;++i) parent[i]=std::min(i,static_cast<int>(key[4+i]));
        parent[l]=parent[r]=l;
        auto find=[&](int x){while(parent[x]!=x) x=parent[x];return x;};
        bool loop=false;
        auto link=[&](int x,int y){x=find(x);y=find(y);if(x==y) loop=true;else parent[x]=y;};
        std::array<int,112> queue,ports;
        int front=0,back=std::abs(delta),size=0;
        for(int i=0;i<back;++i) queue[i]=depths[0]+depths[1]+i;
        const Event ev[2]={a,b};
        for(int side=0;side<2;++side) {
            const int start=side?key[2]:0, v=m+side, sign=side?-1:1;
            if(ev[side]==Event::close) {--depths[side];link(v,start+depths[side]);}
            for(int j=0;j<depths[side];++j) ports[size++]=start+j;
            if(ev[side]==Event::open) {ports[size++]=v;++opened[side];++depths[side];}
            if(ev[side]==Event::through) {
                if(sign*delta<0) link(v,queue[front++]); else queue[back++]=v;
                delta+=sign;
            }
        }
        while(front<back) ports[size++]=queue[front++];
        if(loop) {
            if(!size && opened[0]==deficit && opened[1]==deficit) ++row.terminal;
            return;
        }
        std::string next(4+size,0);
        next[0]=opened[0];next[1]=opened[1];next[2]=depths[0];next[3]=depths[1];
        std::array<int,112> first;
        std::fill_n(first.begin(),m+2,-1);
        for(int i=0;i<size;++i){
            const int x=find(ports[i]);
            if(first[x]<0) first[x]=i;
            else {next[4+i]=first[x];next[4+first[x]]=i;}
        }
        if(size==0) throw std::logic_error("empty component");
        row.to[row.used++]=intern(next);
    }
public:
    std::vector<Edges> edges;
    explicit Graph(int j): deficit(j) {
        intern(std::string(4,0));
        for(size_t i=0;i<keys.size();++i){
            const std::string key=keys[i];
            Edges row;
            const Event l=key[2]?Event::close:Event::through, r=key[3]?Event::close:Event::through;
            extend(key,l,r,row);
            if(key[0]<j) extend(key,Event::open,r,row);
            if(key[1]<j) extend(key,l,Event::open,row);
            if(key[0]<j && key[1]<j) extend(key,Event::open,Event::open,row);
            edges.push_back(row);
        }
        keys.clear();keys.shrink_to_fit();ids.clear();ids.rehash(0);
    }
};

int main(int argc,char**argv){
    if(argc!=3) return 1;
    int j=std::stoi(argv[1]), last=std::stoi(argv[2]);
    if(j<1 || j>13 || last<2*j) return 1;
    Graph graph(j);
    std::cerr<<"states "<<graph.edges.size()<<"\n";
    std::vector<Integer> v(graph.edges.size()),w(v.size());
    v[0]=1;
    for(int n=1;n<=last;++n){
        std::fill(w.begin(),w.end(),0);
        Integer answer=0;
        for(size_t i=0;i<v.size();++i) if(v[i]!=0) {
            const Edges &row=graph.edges[i];
            for(int k=0;k<row.used;++k) w[row.to[k]]+=v[i];
            for(int k=0;k<row.terminal;++k) answer+=v[i];
        }
        std::cout<<n<<" "<<answer<<"\n";
        v.swap(w);
    }
}
