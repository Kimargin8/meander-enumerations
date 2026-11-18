// A reverse walk through one byte. Low bits precede high bits on the circle.
struct ByteCancellation {
    std::array<std::array<unsigned short,9>,256> result{};
    constexpr ByteCancellation() {
        for(int b=0;b<256;b++) for(int incoming=0;incoming<=8;incoming++) {
            int need=incoming,apart=0;
            for(int i=7;i>=0;i--) {
                if(!((b>>i)&1)) ++need;
                else if(need) --need;
                else apart|=1<<i;
            }
            result[b][incoming]=apart|(need<<8);
        }
    }
};
static constexpr ByteCancellation bytes{};

template<class W> W essential_mask(W w,int size) {
    int excess=2*popcount(w)-size;
    if(excess<=0) return 0;
    W out=0,padded=w|~mask<W>(size);
    int need=0;
    for(int shift=((size-1)/8)*8;shift>=0;shift-=8) {
        int b=(padded>>shift)&255;
        if(need>=8) need+=8-2*std::popcount(unsigned(b));
        else {
            auto rec=bytes.result[b][need];
            out|=W(rec&255)<<shift;
            need=rec>>8;
        }
    }
    // Initial unmatched closes cancel the highest unmatched linear opens.
    U64 low=U64(out);
    if constexpr(sizeof(W)==8) return _pdep_u64(mask<U64>(excess),low);
    else {
        int first=std::min(excess,std::popcount(low));
        U64 a=_pdep_u64(mask<U64>(first),low);
        U64 b=_pdep_u64(mask<U64>(excess-first),U64(out>>64));
        return W(a)|(W(b)<<64);
    }
}
template<class W> W fast_reflect(W w,int size) {
    if(!size) return w;
    W reversed=((~w)&mask<W>(size))|essential_mask(w,size);
    if constexpr(sizeof(W)==8) return reverse64(reversed)>>(64-size);
    else return ((W(reverse64(U64(reversed)))<<64)|reverse64(U64(reversed>>64)))>>(128-size);
}
