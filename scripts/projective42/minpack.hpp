// Exact two-weight tables for the minimum-cut transfer. Both accumulators may
// grow to 128 bits, while the common small pair shares one slot with its key.
struct PairMass {U128 count=0,gap=0;};
struct PackedGap {
    std::vector<U64> cells;
    std::unordered_map<U64,PairMass> large;
    U64 km=0,cm=0,gm=0,marker=0;
    int width=0,cut=0;
    size_t used=0;
    PackedGap(int bits,int n):width(bits) {
        int left=64-bits;
        int overhead=std::bit_width(unsigned(n));
        cut=std::max(1,(left-overhead)/2);
        km=mask<U64>(width); cm=mask<U64>(cut); gm=mask<U64>(left-cut);
        marker=mask<U64>(left);
    }
    PairMass get(U64 e) const {
        U64 v=e>>width;
        if(v==marker) return large.at(e&km);
        return {v&cm,v>>cut};
    }
    void grow() {
        std::vector<U64> big(std::max<size_t>(8,2*cells.size()));
        for(U64 e:cells) if(e) {
            size_t p=mix(e&km)&(big.size()-1);
            while(big[p])p=(p+1)&(big.size()-1);
            big[p]=e;
        }
        cells.swap(big);
    }
    void add(U64 key,PairMass v) {
        if(4*used>=3*cells.size())grow();
        size_t p=mix(key)&(cells.size()-1);
        while(cells[p]&&((cells[p]^key)&km))p=(p+1)&(cells.size()-1);
        if(cells[p]) {
            auto old=get(cells[p]);v.count+=old.count;v.gap+=old.gap;
        } else ++used;
        U64 payload;
        if(v.count<=cm&&v.gap<=gm&&((U64(v.gap)<<cut)|U64(v.count))!=marker)
            payload=(U64(v.gap)<<cut)|U64(v.count);
        else {large[key]=v;payload=marker;}
        cells[p]=key|(payload<<width);
    }
};
template<class W> struct PackedGroup {
    Codec<W> codec;
    PackedGap slots;
    PackedGroup(Shape s,int h,int n):codec(s,h),slots(codec.width,n){}
};
template<class W> struct PackedArm {
    PackedGroup<W>* g=nullptr;
    int flip=0,size=0;
    bool reset=false;
    void add(W w,PairMass v) const {
        if(flip==1)w=fast_reflect(w,size);
        if(flip==2)w=std::min(w,fast_reflect(w,size));
        v.gap=reset?0:v.gap+v.count;
        g->slots.add(g->codec.compress(w),v);
    }
};
