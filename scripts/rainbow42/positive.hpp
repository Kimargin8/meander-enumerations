#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>

// Nonnegative counts; additions check the fixed capacity before writing a carry.
struct Integer {
    static constexpr unsigned base=1000000000;
    std::array<std::uint32_t,12> digit{};
    unsigned used=0;
    Integer(unsigned x=0) { if(x) { used=1; digit[0]=x; } }
    bool operator!=(unsigned x) const { assert(x==0); return used!=0; }
    Integer &operator+=(const Integer &b) {
        const unsigned end=std::max(used,b.used);
        unsigned carry=0;
        for(unsigned i=0;i<end;++i) {
            unsigned v=(i<used?digit[i]:0)+(i<b.used?b.digit[i]:0)+carry;
            carry=v>=base;
            digit[i]=carry?v-base:v;
        }
        used=end;
        if(carry) {
            if(used==digit.size()) throw std::overflow_error("exact integer capacity");
            digit[used++]=1;
        }
        return *this;
    }
    friend std::ostream &operator<<(std::ostream &out,const Integer &v){
        if(!v.used) return out<<0;
        out<<v.digit[v.used-1];
        for(int i=static_cast<int>(v.used)-2;i>=0;--i) out<<std::setfill('0')<<std::setw(9)<<v.digit[i];
        return out;
    }
};
