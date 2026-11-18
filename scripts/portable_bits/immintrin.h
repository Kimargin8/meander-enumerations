#pragma once
#include <cstdint>

// Portable semantics for the two BMI2 operations used by the antipodal counter.
inline std::uint64_t _pext_u64(std::uint64_t input,std::uint64_t mask) {
    std::uint64_t out=0, bit=1;
    while(mask) {
        const std::uint64_t lowest=mask&(-mask);
        if(input&lowest) out|=bit;
        mask^=lowest;bit<<=1;
    }
    return out;
}

inline std::uint64_t _pdep_u64(std::uint64_t input,std::uint64_t mask) {
    std::uint64_t out=0;
    while(mask) {
        const std::uint64_t lowest=mask&(-mask);
        if(input&1) out|=lowest;
        mask^=lowest;input>>=1;
    }
    return out;
}
