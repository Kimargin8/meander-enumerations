// The bounded arch transfer of Zhang and Ma, Section 2. State construction independent of diagonal.cpp.
#include "positive.hpp"
#include <string>
#include <unordered_map>
#include <vector>

struct Transitions { std::array<int,4> next{}; int length=0, accepted=0; };
class Transfer {
    int j;
    std::unordered_map<std::string,int> index;
    std::vector<std::string> states;
    int number(const std::string &key) {
        auto [at,created]=index.try_emplace(key,states.size());
        if(created) states.push_back(key);
        return at->second;
    }
    void move(const std::string &key,bool close_l,bool close_r,Transitions &record) {
        const int m=static_cast<int>(key.size())-3;
        int u=key[0], v=key[1], a=key[2], b=m-a;
        if((close_l && (!a || u==j)) || (close_r && (!b || v==j))) return;
        std::array<int,64> mate{}, left{}, right{};
        for(int i=0;i<m;++i) mate[i]=key[3+i];
        for(int i=0;i<a;++i) left[i]=i;
        for(int i=0;i<b;++i) right[i]=a+i;
        mate[m]=m+1;mate[m+1]=m;
        int closed=0;
        auto join=[&](int x,int y) {
            const int f=mate[x],g=mate[y];
            if(f==y) ++closed;
            else {mate[f]=g;mate[g]=f;}
        };
        if(close_l) {++u;join(left[--a],m);} else left[a++]=m;
        if(close_r) {++v;join(right[--b],m+1);} else right[b++]=m+1;
        const int forced=std::max(0,std::min(a-j+u,b-j+v));
        for(int i=0;i<forced;++i) join(left[i],right[i]);
        const int total=a+b-2*forced;
        if(closed) {
            record.accepted+=closed==1 && total==0 && u==j && v==j;
            return;
        }
        if(!total || total>2*j) throw std::logic_error("arch state width");
        std::array<int,64> order{},rank{};
        int len=0;
        for(int i=forced;i<a;++i) order[len++]=left[i];
        for(int i=forced;i<b;++i) order[len++]=right[i];
        for(int i=0;i<len;++i) rank[order[i]]=i;
        std::string target(3+len,0);
        target[0]=u;target[1]=v;target[2]=a-forced;
        for(int i=0;i<len;++i) target[3+i]=rank[mate[order[i]]];
        record.next[record.length++]=number(target);
    }
public:
    std::vector<Transitions> table;
    explicit Transfer(int deficit):j(deficit) {
        number(std::string(3,0));
        for(size_t i=0;i<states.size();++i) {
            const std::string key=states[i];
            Transitions row;
            for(bool close_l:{false,true}) for(bool close_r:{false,true}) move(key,close_l,close_r,row);
            table.push_back(row);
        }
        states.clear();states.shrink_to_fit();index.clear();index.rehash(0);
    }
};

int main(int argc,char **argv) {
    if(argc!=3) return 1;
    const int j=std::stoi(argv[1]), last=std::stoi(argv[2]);
    if(j<1 || j>25 || last<2*j) return 1;
    Transfer g(j);
    std::cerr<<"states "<<g.table.size()<<"\n";
    std::vector<Integer> v(g.table.size()), next(v.size());
    v[0]=1;
    for(int n=1;n<=last;++n) {
        std::fill(next.begin(),next.end(),0);
        Integer total;
        for(size_t i=0;i<v.size();++i) if(v[i]!=0) {
            const auto &row=g.table[i];
            for(int x=0;x<row.length;++x) next[row.next[x]]+=v[i];
            for(int x=0;x<row.accepted;++x) total+=v[i];
        }
        std::cout<<n<<" "<<total<<"\n";v.swap(next);
    }
}
