// Supplement a stored layer from an adjacent, shorter horizon on the same deficit.
#include <bits/stdc++.h>
#include <sys/resource.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
namespace model {
#include "spill.cpp"
}
#pragma GCC diagnostic pop
namespace fs=std::filesystem;
using namespace model::folded;
using model::Stream;
using model::Metadata;

static Metadata load(const fs::path& dir){
    Metadata m{};
    std::ifstream f(dir/"progress.txt");
    if(!(f>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak))
        throw std::runtime_error("cannot read checkpoint");
    return m;
}

static bool outside(State s,int h,int t,int xmax,int ymax){
    int removed=h-int(s.end);
    int x=(t+1-int(s.left)-removed)/2;
    int y=(t-int(s.len-s.left)-removed)/2;
    return x<=xmax||y<=ymax;
}

static std::unique_ptr<CompactTable> exceptional(int n,int h,int old_n,int cut,U& moves,U& peak){
    int j=(n-h)/2;
    int xmax=cut-(old_n-j),ymax=xmax-1;
    width=sector_width(n,h);
    if(width+18>64)throw std::runtime_error("key width");
    auto layer=std::make_unique<CompactTable>();
    State initial{0,1,1,0,unsigned(h)};
    if(outside(initial,h,0,xmax,ymax))layer->add(code(initial),1);
    for(int t=1;t<=cut;++t){
        auto next=std::make_unique<CompactTable>();
        layer->visit([&](U key,Wide w){
            State original=decode(key);
            for(int l=0;l<2;++l)for(int r=0;r<2;++r){
                State s=original;unsigned u=s.left,v=s.len-u;
                if((!l&&!u)||(!r&&!v))continue;
                ++moves;
                if(l&&r){
                    s.bits=insert(s.bits,u,1,2);s.len+=2;++s.left;
                    if(s.defect>=u)s.defect+=2;
                }else if(l!=r)s.left+=l?1:-1;
                else{if(!join(s,u-1,u))continue;--s.left;}
                if(prune(s,n-1-t,true)&&outside(s,h,t,xmax,ymax))next->add(code(s),w);
            }
        });
        layer=std::move(next);peak=std::max<U>(peak,layer->used);
        std::cerr<<t<<' '<<layer->used<<' '<<moves<<'\n';
    }
    return layer;
}

// Stream the smaller complement with groups ordered by height difference.
static void exceptional_disk(int n,int h,int old_n,int cut,const fs::path& dir,U budget){
    const int xmax=cut-(old_n-(n-h)/2),ymax=xmax-1;
    fs::create_directories(dir);
    auto identity=dir/"exception.txt";
    if(fs::exists(identity)){
        int a,b;std::ifstream in(identity);in>>a>>b;
        if(!in||a!=old_n||b!=cut)throw std::runtime_error("incompatible exception bound");
    }else{std::ofstream out(identity);out<<old_n<<' '<<cut<<'\n';}
    width=sector_width(n,h);
    if(width+18>64)throw std::runtime_error("key width");
    if(!fs::exists(dir/"progress.txt")){
        Stream f(dir/model::layer(0),"wb");
        State s{0,1,1,0,unsigned(h)};bool keep=outside(s,h,0,xmax,ymax);
        if(keep){f.byte(1+n);f.push(1);f.push(code(s));f.weight(1);}
        f.finish();U size=fs::file_size(dir/model::layer(0));
        model::save(dir,{n,h,0,keep,keep?model::digest(code(s),1):0,keep,0,size,size});
    }
    Metadata m=load(dir);
    if(m.n!=n||m.h!=h||m.t>cut)throw std::runtime_error("wrong exception checkpoint");
    while(m.t<cut){
        int t=m.t+1;
        fs::path src=dir/model::layer(m.t),temp=dir/"pending.bin",dst=dir/model::layer(t);
        U old_bytes=fs::file_size(src);
        if(old_bytes>budget)throw std::runtime_error("disk limit");
        Stream in(src,"rb"),out(temp,"wb");out.limit=budget-old_bytes;
        std::map<int,std::unique_ptr<CompactTable>> groups;
        U count=0,hash=0,read_hash=0,moves=0,seen=0;
        auto flush=[&](int through){
            while(!groups.empty()&&groups.begin()->first<=through){
                auto& entry=*groups.begin();
                out.byte(n+entry.first);out.push(entry.second->used);count+=entry.second->used;
                entry.second->visit([&](U key,Wide w){out.push(key);out.weight(w);hash+=model::digest(key,w);});
                groups.erase(groups.begin());
            }
        };
        int last=-n-1;
        while(seen<m.count){
            int d=int(in.get())-n;U cnt=in.pop();
            if(d<=last||d>n||!cnt||cnt>m.count-seen)throw std::runtime_error("invalid group");
            last=d;seen+=cnt;flush(d-4);
            for(U i=0;i<cnt;++i){
                U key=in.pop();Wide w=in.value();read_hash+=model::digest(key,w);State a=decode(key);
                int k=h-int(a.end);
                const int x=(t-int(a.left)-k)/2, y=(t-1-int(a.len-a.left)-k)/2;
                for(int l=0;l<2;++l)for(int r=0;r<2;++r){
                    State s=a;unsigned u=s.left,v=s.len-u;
                    if((!l&&!u)||(!r&&!v))continue;
                    ++moves;
                    if(x+!l>xmax&&y+!r>ymax)continue;
                    if(l&&r){
                        s.bits=insert(s.bits,u,1,2);s.len+=2;++s.left;
                        if(s.defect>=u)s.defect+=2;
                    }else if(l!=r)s.left+=l?1:-1;
                    else{if(!join(s,u-1,u))continue;--s.left;}
                    if(prune(s,n-1-t,true)){
                        auto& table=groups[d+2*(l-r)];
                        if(!table)table=std::make_unique<CompactTable>();
                        table->add(code(s),w);
                    }
                }
            }
            flush(d-2);
        }
        if(read_hash!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("exception checksum");
        flush(n);out.finish();in.finish();U bytes=fs::file_size(temp);
        fs::rename(temp,dst);
        m={n,h,t,count,hash,std::max(m.peak,count),m.work+moves,m.traffic+old_bytes+bytes,
           std::max(m.disk_peak,old_bytes+bytes)};
        model::save(dir,m);fs::remove(src);
        std::cerr<<t<<' '<<m.count<<' '<<m.work<<'\n';
    }
    std::cout<<m.count<<' '<<m.work<<' '<<m.peak<<'\n';
}

struct SourceGroup {
    Metadata m;
    Stream in;
    U seen=0,group=0,check=0;
    int d,previous;
    explicit SourceGroup(const fs::path& p):
        m(load(p)),in(p/model::layer(m.t),"rb"),d(0),previous(-m.n-1){advance();}
    void advance(){
        if(seen==m.count){
            if(check!=m.hash||getc_unlocked(in.f)!=EOF)throw std::runtime_error("invalid input checksum");
            d=100;group=0;return;
        }
        d=int(in.get())-m.n;group=in.pop();
        if(d<=previous||d>m.n||!group||group>m.count-seen)throw std::runtime_error("bad group");
        previous=d;
    }
};

static void merge_files(int n,int h,int old_n,int t,const fs::path& old_dir,const fs::path& extra_dir,
                        const fs::path& dest){
    SourceGroup old(old_dir),extra(extra_dir);int shift=n-old_n;
    if(old.m.n!=old_n||old.m.h!=h-shift||old.m.t!=t||extra.m.n!=n||extra.m.h!=h||extra.m.t!=t)
        throw std::runtime_error("mismatched layer");
    int stored_n,stored_t;std::ifstream settings(extra_dir/"exception.txt");settings>>stored_n>>stored_t;
    if(!settings||stored_n!=old_n||stored_t!=t)throw std::runtime_error("invalid exception provenance");
    fs::create_directories(dest);
    if(fs::exists(dest/"progress.txt"))throw std::runtime_error("output must be fresh");
    U hash=0,count=0;
    int xmax=t-(old_n-(n-h)/2),ymax=xmax-1;
    const unsigned old_width=sector_width(old_n,old.m.h),new_width=sector_width(n,h);
    fs::path tmp=dest/"pending.bin";
    Stream out(tmp,"wb");
    for(int d=-n;d<=n;++d){
        U number=(old.d==d?old.group:0)+(extra.d==d?extra.group:0);
        if(!number)continue;
        out.byte(n+d);out.push(number);count+=number;
        auto copy=[&](SourceGroup& input,bool is_old){
            if(input.d!=d)return;
            for(U i=0;i<input.group;++i){
                U key=input.in.pop();Wide w=input.in.value();input.check+=model::digest(key,w);
                width=is_old?old_width:new_width;State s=decode(key);
                if(int(2*s.left)-int(s.len)!=d)throw std::runtime_error("bad routing");
                if(is_old)s.end+=shift;
                if(outside(s,h,t,xmax,ymax)==is_old)throw std::logic_error("horizon overlap");
                width=new_width;U newer=code(s);
                out.push(newer);out.weight(w);hash+=model::digest(newer,w);
            }
            input.seen+=input.group;input.advance();
        };
        copy(old,true);copy(extra,false);
    }
    if(old.group||extra.group||count!=old.m.count+extra.m.count)throw std::runtime_error("unmerged group");
    old.in.finish();extra.in.finish();out.finish();
    U bytes=fs::file_size(tmp),prior=fs::file_size(old_dir/model::layer(t))+fs::file_size(extra_dir/model::layer(t));
    fs::rename(tmp,dest/model::layer(t));
    model::save(dest,{n,h,t,count,hash,std::max({count,old.m.peak,extra.m.peak}),old.m.work+extra.m.work,
                old.m.traffic+extra.m.traffic+prior+bytes,std::max({old.m.disk_peak,extra.m.disk_peak,prior+bytes})});
    std::cout<<count<<' '<<hash<<' '<<old.m.count<<' '<<extra.m.count<<'\n';
}

int main(int argc,char**argv){
    if(argc!=6&&argc!=8&&argc!=9)return 2;
    int n=std::stoi(argv[1]),h=std::stoi(argv[2]),old_n=std::stoi(argv[3]),t=std::stoi(argv[4]);
    int shift=n-old_n,old_h=h-shift,j=(n-h)/2;
    if(n<2||n>61||h>n||h<0||(n-h)%2||shift<1||old_h<0||t<0||t>=old_n)return 2;
    if(argc==8&&std::string(argv[5])=="--exception-disk"){
        exceptional_disk(n,h,old_n,t,argv[6],std::stoull(argv[7])<<20);return 0;
    }
    if(argc==9&&std::string(argv[5])=="--merge-checkpoint"){
        merge_files(n,h,old_n,t,argv[6],argv[7],argv[8]);return 0;
    }
    if(argc==9)return 2;
    U moves=0,peak=0;
    auto extra=exceptional(n,h,old_n,t,moves,peak);
    if(argc==6){
        if(std::string(argv[5])!="--exception-only")return 2;
        std::cout<<extra->used<<' '<<moves<<' '<<peak<<'\n';return 0;
    }
    if(std::string(argv[5])!="--lift")return 2;
    fs::path old_dir=argv[6],dest=argv[7];
    fs::create_directories(dest);
    if(fs::exists(dest/"progress.txt"))throw std::runtime_error("output must be fresh");
    Metadata m{};
    std::ifstream marker(old_dir/"progress.txt");
    marker>>m.n>>m.h>>m.t>>m.count>>m.hash>>m.peak>>m.work>>m.traffic>>m.disk_peak;
    if(!marker||m.n!=old_n||m.h!=old_h||m.t!=t)throw std::runtime_error("incorrect source layer");
    const unsigned old_width=sector_width(old_n,old_h),new_width=sector_width(n,h);
    width=new_width;
    std::map<int,std::unique_ptr<Stream>> side;
    std::map<int,U> extra_count;
    extra->visit([&](U key,Wide w){
        State s=decode(key);int d=int(2*s.left)-int(s.len);
        auto& f=side[d];
        if(!f)f=std::make_unique<Stream>(dest/("extra"+std::to_string(d)+".bin"),"wb");
        f->push(key);f->weight(w);++extra_count[d];
    });
    U extra_bytes=0;
    for(auto& p:side){extra_bytes+=p.second->written;p.second->finish();}
    side.clear();extra.reset();
    fs::path source=old_dir/model::layer(t),temp=dest/"pending.bin";
    Stream input(source,"rb"),output(temp,"wb");
    U old_seen=0,old_hash=0,hash=0,count=0,old_group=0;
    int old_d=old_n+1;
    auto advance=[&](){
        if(old_seen==m.count){old_d=n+1;old_group=0;return;}
        old_d=int(input.get())-old_n;old_group=input.pop();
        if(!old_group||old_group>m.count-old_seen)throw std::runtime_error("bad group");
    };
    advance();
    for(int d=-n;d<=n;++d){
        U number=(old_d==d?old_group:0)+extra_count[d];
        if(!number)continue;
        count+=number;output.byte(d+n);output.push(number);
        if(old_d==d){
            for(U i=0;i<old_group;++i){
                U key=input.pop();Wide w=input.value();old_hash+=model::digest(key,w);
                width=old_width;
                State s=decode(key);
                if(int(2*s.left)-int(s.len)!=d)throw std::runtime_error("bad old routing");
                s.end+=shift;width=new_width;
                if(outside(s,h,t,t-(old_n-j),t-(old_n-j)-1))throw std::logic_error("overlapping parts");
                U newer=code(s);output.push(newer);output.weight(w);hash+=model::digest(newer,w);
            }
            old_seen+=old_group;advance();
        }
        if(extra_count[d]){
            fs::path fragment=dest/("extra"+std::to_string(d)+".bin");
            Stream smaller(fragment,"rb");
            for(U i=0;i<extra_count[d];++i){
                U key=smaller.pop();Wide w=smaller.value();
                output.push(key);output.weight(w);hash+=model::digest(key,w);
            }
            if(getc_unlocked(smaller.f)!=EOF)throw std::runtime_error("extra fragment");
            smaller.finish();fs::remove(fragment);
        }
    }
    if(old_seen!=m.count||old_hash!=m.hash||getc_unlocked(input.f)!=EOF)throw std::runtime_error("old checksum");
    input.finish();output.finish();
    U bytes=fs::file_size(temp),old_bytes=fs::file_size(source);
    fs::rename(temp,dest/model::layer(t));
    Metadata updated{n,h,t,count,hash,std::max({m.peak,peak,count}),m.work+moves,
                     m.traffic+old_bytes+2*extra_bytes+bytes,
                     std::max(m.disk_peak,old_bytes+extra_bytes+bytes)};
    model::save(dest,updated);
    std::cout<<updated.count<<' '<<updated.hash<<' '<<m.count<<' '<<moves<<' '<<extra_bytes<<'\n';
}
