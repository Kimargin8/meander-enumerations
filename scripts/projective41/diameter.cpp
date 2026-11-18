// Mark the two exceptional reflection vertices at the start and end of the scan.
#define main antipodal_seams_entry
#include "seams.cpp"
#undef main

static Weight diameter(int n,int h,std::size_t &peak,Weight &work) {
    std::string key(4+2*h+2,0);
    key[2]=key[3]=h;
    for (int i=0;i<h;i++) {
        key[4+i]=h+i;
        key[4+h+i]=i;
    }
    key[4+2*h]=2*h+1;
    key[5+2*h]=2*h;
    Buckets cur{{key,1}},next;
    Weight ans=0;
    for (int t=0;t<=n;t++) {
        next.clear();
        for (const auto &entry:cur) {
            const auto &s=entry.first;
            int l=s[0],r=s[1],a=s[2],b=s[3],ports=int(s.size())-4;
            auto ls=(t==0 ? std::vector<Option>{{l,a,ports-2,false}} : options(l,a,0,l+r,n-t));
            auto rs=(t==n ? std::vector<Option>{{r,b,ports-1,false}} : options(r,b,l,l+r+a,n-t-1));
            for (auto x:ls) for (auto y:rs) {
                ++work;
                if (!x.create && !y.create && s[4+x.pop]==y.pop) {
                    if (t==n && ports==2) ans+=entry.second;
                    continue;
                }
                std::array<int,128> src{},number{},mate{};
                for (int k=0;k<ports;k++) mate[k]=s[4+k];
                int X=x.create?ports:x.pop, Y=y.create?ports+1:y.pop;
                int i=x.create?X:mate[X],j=y.create?Y:mate[Y];
                mate[i]=j;
                mate[j]=i;
                int count=0;
                auto take=[&](int z) {if(z!=x.pop && z!=y.pop){src[count]=z;number[z]=count++;}};
                for (int k=0;k<l;k++) take(k);
                if (x.create) take(X);
                for (int k=l;k<l+r;k++) take(k);
                if (y.create) take(Y);
                for (int k=l+r;k<ports;k++) take(k);
                std::string dst(4+count,0);
                dst[0]=x.stack;dst[1]=y.stack;dst[2]=x.anchors;dst[3]=y.anchors;
                for (int k=0;k<count;k++) dst[4+k]=number[mate[src[k]]];
                auto &v=next[dst];
                if (std::numeric_limits<Weight>::max()-v<entry.second) throw std::overflow_error("coefficient");
                v+=entry.second;
            }
        }
        peak=std::max(peak,next.size());
        next.swap(cur);
    }
    return ans;
}

int main(int argc,char **argv) {
    if (argc!=3) return 2;
    int from=std::atoi(argv[1]),to=std::atoi(argv[2]);
    if (from<1 || from>to || to>32) return 3;
    for (int n=from;n<=to;n++) {
        auto start=std::chrono::steady_clock::now();
        Weight sum=0,work=0;
        std::size_t peak=0;
        std::cout<<"{\"n\":"<<n<<",\"by_height\":[";
        for (int h=n%2;h<=n;h+=2) {
            Weight x=diameter(n,h,peak,work);
            if (h!=n%2) std::cout<<',';
            std::cout<<x;
            sum+=x;
        }
        struct rusage usage{};
        getrusage(RUSAGE_SELF,&usage);
        std::chrono::duration<double> sec=std::chrono::steady_clock::now()-start;
        std::cout<<"],\"count\":"<<sum<<",\"peak_states\":"<<peak<<",\"transitions\":"<<work
                 <<",\"seconds\":"<<sec.count()<<",\"process_peak_kib\":"<<usage.ru_maxrss<<"}"<<std::endl;
    }
}
