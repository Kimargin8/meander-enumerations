#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Open addressing for complete binary strings. A hash tag is only a filter:
// every hit is confirmed by comparing the length and all stored bytes.
class CompactCache {
    static constexpr uint32_t page_bits=20, page_size=uint32_t(1)<<page_bits;
    static constexpr uint32_t absent=std::numeric_limits<uint32_t>::max();
    struct Slot { uint64_t count=0; uint32_t offset=absent, tag=0; };
    std::vector<Slot> table;
    std::vector<std::unique_ptr<char[]>> pages;
    uint32_t cursor=0;
    size_t occupied=0, limit;

    char* position(uint32_t offset) const {
        return pages[offset>>page_bits].get()+(offset&(page_size-1));
    }
    bool equal(const Slot& slot,const std::string& key,uint32_t tag) const {
        if (slot.tag!=tag) return false;
        const char* data=position(slot.offset);
        return static_cast<unsigned char>(data[0])==key.size() &&
               std::memcmp(data+1,key.data(),key.size())==0;
    }
public:
    explicit CompactCache(size_t cap):limit(cap) {
        if (!cap) return;
        size_t buckets=1;
        while (buckets-buckets/5<cap) buckets*=2;
        table.resize(buckets);
    }
    size_t size() const { return occupied; }
    bool find(const std::string& key,uint64_t& count) const {
        if (table.empty()) return false;
        uint64_t hash=std::hash<std::string>{}(key);
        size_t at=hash&(table.size()-1);
        while (table[at].offset!=absent) {
            if (equal(table[at],key,uint32_t(hash>>32))) {
                count=table[at].count;
                return true;
            }
            at=(at+1)&(table.size()-1);
        }
        return false;
    }
    void insert(const std::string& key,uint64_t count) {
        if (occupied==limit) return;
        if (key.size()>=256) throw std::invalid_argument("key too long");
        uint64_t hash=std::hash<std::string>{}(key);
        size_t at=hash&(table.size()-1);
        while (table[at].offset!=absent) {
            if (equal(table[at],key,uint32_t(hash>>32))) return;
            at=(at+1)&(table.size()-1);
        }
        uint32_t need=static_cast<uint32_t>(key.size()+1);
        if ((cursor&(page_size-1))+need>page_size) {
            uint64_t next=(uint64_t(cursor)+page_size)&~uint64_t(page_size-1);
            if (next>=absent) return;
            cursor=uint32_t(next);
        }
        if (uint64_t(cursor)+need>=absent) return;
        if ((cursor>>page_bits)==pages.size()) pages.emplace_back(new char[page_size]);
        char* data=position(cursor);
        data[0]=static_cast<char>(key.size());
        std::memcpy(data+1,key.data(),key.size());
        table[at]=Slot{count,cursor,uint32_t(hash>>32)};
        cursor+=need;
        ++occupied;
    }
};
