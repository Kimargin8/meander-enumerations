// Independent geometric transfer. Classify internal and through endpoints
// at their arrival; the two local stacks and one unmatched FIFO determine A.
#include <bits/stdc++.h>
#include <sys/resource.h>
namespace storage {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "compact.cpp"
#pragma GCC diagnostic pop
}
using storage::U;
using storage::Wide;
using storage::CompactTable;
constexpr unsigned mark=63;
static unsigned word_width,position_bits,queue_bits;
static unsigned digits(unsigned maximum){
    return maximum?32-__builtin_clz(maximum):0;
}
static U mask(unsigned bits){return (U(1)<<bits)-1;}
struct Ports{
    int size,left,delta,opened;
    unsigned tag[64];
};
static U encode(const Ports &p,int h){
    U bits=U(1)<<p.size,seen=0;
    unsigned defect=mark;
    unsigned nesting[64];int depth=0;
    for(int i=0;i<p.size;++i){
        auto tag=p.tag[i];
        if(tag==mark){if(defect!=mark)throw std::logic_error("second marked port");defect=i;continue;}
        auto flag=U(1)<<tag;
        if(!(seen&flag)){bits|=U(1)<<i;seen|=flag;nesting[depth++]=tag;}
        else if(!depth||nesting[--depth]!=tag)throw std::logic_error("crossing path tags");
    }
    if(depth||defect==mark||p.size>=int(word_width))throw std::logic_error("port width");
    return bits|(U(defect)<<word_width)|(U(p.left)<<(word_width+position_bits))
               |(U(p.delta+h)<<(word_width+2*position_bits))
               |(U(p.opened)<<(word_width+2*position_bits+queue_bits));
}
static Ports unpack(U key,int h){
    Ports p{};
    U bits=key&mask(word_width);
    p.size=63-__builtin_clzll(bits);
    unsigned d=(key>>word_width)&mask(position_bits);
    p.left=(key>>(word_width+position_bits))&mask(position_bits);
    p.delta=int((key>>(word_width+2*position_bits))&mask(queue_bits))-h;
    p.opened=key>>(word_width+2*position_bits+queue_bits);
    unsigned stack[64];int size=0;
    for(int i=0;i<p.size;++i){
        if(unsigned(i)==d)p.tag[i]=mark;
        else if((bits>>i)&1){p.tag[i]=i;stack[size++]=i;}
        else{if(!size)throw std::logic_error("invalid open word");p.tag[i]=stack[--size];}
    }
    if(size)throw std::logic_error("invalid closing word");
    return p;
}
struct Move {int consume;bool through,opens;};
struct Options {Move moves[2];int count=0;};
static Options options(int depth,int opened,int through,int j,int h,int close_pos,int fifo_pos){
    Options o;
    if(opened<j)o.moves[o.count++]={-1,false,true};
    if(depth)o.moves[o.count++]={close_pos,false,false};
    else if(through<h)o.moves[o.count++]={fifo_pos,true,false};
    return o;
}
static bool advance(const Ports &p,Move l,Move r,Ports &b){
    // When both new vertices wait for each other the fixed edge closes a cycle.
    if(l.through&&r.through&&p.delta==0)return false;
    int a=l.consume,c=r.consume;
    unsigned x=a<0?unsigned(p.size):p.tag[a], y=c<0?unsigned(p.size):p.tag[c];
    if(a>=0 && c>=0 && x==y)return false;
    unsigned joined=(x==mark||y==mark)?mark:std::min(x,y);
    b.size=0;b.delta=p.delta+int(l.through)-int(r.through);b.opened=p.opened+int(l.opens);
    auto keep=[&](int i){
        if(i==a||i==c)return;
        unsigned v=p.tag[i];
        b.tag[b.size++]=(v==x||v==y)?joined:v;
    };
    for(int i=0;i<p.left;++i)keep(i);
    if(a<0)b.tag[b.size++]=joined;
    b.left=b.size;
    if(c<0)b.tag[b.size++]=joined;
    for(int i=p.left;i<p.size;++i)keep(i);
    return true;
}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[3]);
    if(std::stoi(argv[2])!=n || n<1 || n>58 || h<0 || h>n || (n-h)%2)return 3;
    int j=(n-h)/2;
    word_width=2*j+2;position_bits=digits(2*j+1);queue_bits=digits(2*h);
    if(word_width+2*position_bits+queue_bits+digits(j)>64)throw std::runtime_error("FIFO key width");
    CompactTable cur;Ports p{1,1,0,1,{mark}};
    if(j)cur.add(encode(p,h),1);
    if(h){p.delta=1;p.opened=0;cur.add(encode(p,h),1);}
    size_t peak=cur.used;U attempts=0;
    auto start=std::chrono::steady_clock::now();
    for(int t=1;t<n;++t){
        CompactTable next;
        cur.visit([&](U key,Wide weight){
            auto p=unpack(key,h);
            int a=p.left-std::max(0,p.delta),b=p.size-p.left-std::max(0,-p.delta);
            int x=p.opened,y=x+(p.size-1)/2-p.left+p.delta;
            int left_through=t-2*x+a,right_through=t-1-2*y+b;
            if(a<0||b<0||x<0||y<0||x>j||y>j||left_through<0||right_through<0)
                throw std::logic_error("geometric counts");
            auto ls=options(a,x,left_through,j,h,p.left-1,p.delta<0?p.size-1:-1);
            auto rs=options(b,y,right_through,j,h,p.left,p.delta>0?0:-1);
            for(int i=0;i<ls.count;++i)for(int k=0;k<rs.count;++k){
                ++attempts;
                Ports out;
                if(advance(p,ls.moves[i],rs.moves[k],out))next.add(encode(out,h),weight);
            }
        });
        peak=std::max(peak,next.used);std::swap(cur,next);
    }
    Wide total=0;
    cur.visit([&](U key,Wide w){
        auto s=unpack(key,h);
        if(s.size!=1)return;
        int y=s.opened+(s.size-1)/2-s.left+s.delta;
        if(s.opened!=j||y!=j)return;
        if((s.left==0&&s.delta==0) || (s.left==1&&s.delta==1)){
            if(~Wide(0)-total<w)throw std::overflow_error("total");
            total+=w;
        }
    });
    double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    rusage ru{};getrusage(RUSAGE_SELF,&ru);
    std::cout<<n<<" "<<h<<" "<<storage::decimal(total)<<" "<<peak<<" "<<attempts<<" "
             <<ru.ru_maxrss<<" "<<sec<<" "<<word_width<<"\n";
}
