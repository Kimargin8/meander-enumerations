// Geometric look-ahead for a minimum-height cut. The test ignores connectivity
// and is therefore a necessary condition for an accepted continuation.
struct Remaining {
    int h;
    std::unordered_map<U64,bool> memo;
    explicit Remaining(int height):h(height){}
    bool can(int rem,Shape s) {
        int L=s.a+s.l,R=s.b+s.r;
        if(L>rem||R>rem||s.l+s.r+std::abs(s.a+s.b-h)<h)return false;
        if(!rem)return true;
        if(!s.a&&!s.b)return true;
        if((s.a<<7|s.l)>(s.b<<7|s.r)){std::swap(s.a,s.b);std::swap(s.l,s.r);std::swap(L,R);}
        U64 key=(U64(rem)<<28)|s.code();
        auto it=memo.find(key);if(it!=memo.end())return it->second;
        int nl=s.l-(s.l>0),nr=s.r-(s.r>0),na=s.a-(s.l==0),nb=s.b-(s.r==0);
        bool ok=(L&&R&&can(rem-1,Shape(nl,nr,na,nb)))||
                (R&&can(rem-1,Shape(s.l+1,nr,s.a,nb)))||
                (L&&can(rem-1,Shape(nl,s.r+1,na,s.b)))||
                can(rem-1,Shape(s.l+1,s.r+1,s.a,s.b));
        memo.emplace(key,ok);
        return ok;
    }
};
