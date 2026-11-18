// Rooted involutive permutations for odd-order projective meanders.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include "residual.hpp"
#include "chain_key.hpp"
#include "compact_cache.hpp"

class Enumerator {
    static constexpr int maximum = 39;
    int n;
    std::array<int, maximum> perm{}, before{}, after{}, odd{};
    // Each drawn chord splits a face. Its domain plus one is a fresh face ID.
    std::array<unsigned char, maximum * 2> sides{};
    std::array<unsigned char, maximum> parent_face{};
    uint64_t solutions = 0, nodes = 0;
    int cache_level;
    int cache_step;
    int select_mode;
    bool chains;
    size_t cache_limit;
    bool compact_table;
    CompactCache dictionary;
    uint64_t hits = 0, misses = 0;
    uint64_t impossible_faces = 0;
    std::unordered_map<std::string, uint64_t> memo;

    void relabel(int left, int right, int from, int to) {
        if (left > right) std::swap(left, right);
        for (int v = left + 1; v < right; ++v)
            if (sides[v] == from) sides[v] = static_cast<unsigned char>(to);
    }

    int score(int i) const {
        return (perm[before[i]] >= 0) + (perm[after[i]] >= 0);
    }

    bool put_chord(int domain) {
        int left = 2 * perm[domain], right = odd[perm[before[domain]]];
        if (sides[left] != sides[right]) return false;
        parent_face[domain]=sides[left];
        relabel(left, right, sides[left], domain+1);
        return true;
    }

    void add(int i, int j, uint64_t free) {
        perm[i] = j;
        perm[j] = i;
        int candidates[4] = {i, after[i], j, after[j]};
        int added[4], count = 0;
        bool valid = true;
        for (int p = 0; p < 4; ++p) {
            int d = candidates[p];
            if (std::find(candidates, candidates + p, d) != candidates + p) continue;
            if (perm[d] < 0 || perm[before[d]] < 0) continue;
            if (!put_chord(d)) {
                valid = false;
                break;
            }
            added[count++] = d;
        }
        if (valid) visit(free & ~(uint64_t(1) << i) & ~(uint64_t(1) << j));
        while (count) {
            int d = added[--count];
            relabel(2 * perm[d], odd[perm[before[d]]], d+1, parent_face[d]);
        }
        perm[i] = perm[j] = -1;
    }

    void visit(uint64_t free) {
        if (++nodes == 0) throw std::overflow_error("node count");
        if (!free) {
            if (++solutions == 0) throw std::overflow_error("solution count");
            return;
        }
        int remaining = __builtin_popcountll(free);
        std::string key;
        uint64_t old_sum = solutions;
        bool use_cache = cache_level && (remaining == cache_level || remaining == cache_level-1 ||
                (cache_step >= 2 && remaining >= 8 && remaining < cache_level &&
                 (cache_level-remaining) % cache_step < 2));
        if (use_cache) {
            key = chains ? chain_key(n, perm, before, after, odd, sides)
                         : residual_key(n, perm, before, odd, sides);
            if (key.empty()) { ++impossible_faces; return; }
            uint64_t cached=0;
            bool present;
            if (compact_table) present=dictionary.find(key,cached);
            else {
                auto found=memo.find(key);
                present=found!=memo.end();
                if (present) cached=found->second;
            }
            if (present) {
                if (solutions > std::numeric_limits<uint64_t>::max() - cached)
                    throw std::overflow_error("cached solution count");
                solutions += cached;
                ++hits;
                return;
            }
            ++misses;
        }
        int i = -1, best = -1;
        uint64_t possible = free;
        if (select_mode && remaining>=5) {
            constexpr int wild=maximum+1;
            uint64_t offer_e[wild+1]{}, offer_o[wild+1]{}, need_e[wild+1]{}, need_o[wild+1]{};
            int need_face_e[maximum], need_face_o[maximum];
            for (uint64_t q = free; q; q &= q-1) {
                int v = __builtin_ctzll(q);
                uint64_t b=uint64_t(1)<<v;
                offer_e[sides[2*v]]|=b;
                offer_o[sides[odd[v]]]|=b;
                need_face_e[v]=perm[before[v]]<0 ? wild : sides[odd[perm[before[v]]]];
                need_face_o[v]=perm[after[v]]<0 ? wild : sides[2*perm[after[v]]];
                need_e[need_face_e[v]]|=b;
                need_o[need_face_o[v]]|=b;
            }
            bool feasible=true;
            for (int f=0; f<=n; ++f) {
                int e=__builtin_popcountll(offer_e[f])-__builtin_popcountll(need_e[f]);
                int o=__builtin_popcountll(offer_o[f])-__builtin_popcountll(need_o[f]);
                if (e<0 || o<0) { feasible=false; break; }
                if (e>0) { offer_e[wild]|=offer_e[f]; need_e[f]|=need_e[wild]; }
                if (o>0) { offer_o[wild]|=offer_o[f]; need_o[f]|=need_o[wild]; }
            }
            possible=0;
            if (feasible) for (uint64_t q=free; q; q&=q-1) {
                int v=__builtin_ctzll(q);
                uint64_t choices=offer_e[need_face_e[v]] & offer_o[need_face_o[v]]
                             & need_e[sides[2*v]] & need_o[sides[odd[v]]];
                int count=__builtin_popcountll(choices);
                int priority = count <= 1 ? 100-count : (select_mode==1 ? 70-count : score(v));
                if (priority>best) {
                    best=priority;
                    possible=choices;
                    i=v;
                }
                if (!count) break;
            }
        } else {
            for (uint64_t q = free; q; q &= q - 1) {
                int v = __builtin_ctzll(q);
                int weight = score(v);
                if (weight > best) { i = v; best = weight; }
                if (weight == 2) break;
            }
        }
        for (uint64_t q = possible; q; q &= q - 1) {
            int j = __builtin_ctzll(q);
            // Check known endpoints before forming the temporary involution.
            if (perm[before[i]] >= 0 && sides[2*j] != sides[odd[perm[before[i]]]]) continue;
            if (perm[after[i]] >= 0 && sides[odd[j]] != sides[2*perm[after[i]]]) continue;
            if (perm[before[j]] >= 0 && sides[2*i] != sides[odd[perm[before[j]]]]) continue;
            if (perm[after[j]] >= 0 && sides[odd[i]] != sides[2*perm[after[j]]]) continue;
            add(i, j, free);
        }
        if (!key.empty()) {
            if (compact_table) dictionary.insert(key,solutions-old_sum);
            else if (memo.size() < cache_limit) memo.emplace(std::move(key), solutions-old_sum);
        }
    }

public:
    explicit Enumerator(int order, int level, int step, int select, bool compact, size_t cap, bool flat)
      : n(order), cache_level(level), cache_step(step), select_mode(select), chains(compact), cache_limit(cap),
        compact_table(flat),dictionary(flat ? cap : 0) {
        if (n < 3 || n > maximum || n % 2 == 0) throw std::invalid_argument("odd 3..39");
        int k = (n-1)/2;
        perm.fill(-1);
        for (int i = 0; i < n; ++i) {
            before[i] = (i-k+n)%n;
            after[i] = (i+k)%n;
            odd[i] = (2*i+n)%(2*n);
        }
    }
    void run(int low, int high) {
        auto start = std::chrono::steady_clock::now();
        if (low==0) {
            add(0, 0, (uint64_t(1) << n) - 1);
        } else {
            if (low<1 || high<low || high>=n) throw std::invalid_argument("branch range 1..n-1");
            uint64_t free=(uint64_t(1)<<n)-2;
            perm[0]=0;
            for (int j=low; j<=high; ++j) {
                uint64_t old=solutions;
                auto mark=std::chrono::steady_clock::now();
                add((n-1)/2, j, free);
                double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-mark).count();
                std::cout<<"{\"n\":"<<n<<",\"branch\":"<<j<<",\"count\":"<<(solutions-old)
                         <<",\"keys\":"<<(compact_table?dictionary.size():memo.size())<<",\"seconds\":"<<seconds<<"}"<<std::endl;
            }
        }
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        bool complete=low==0 || (low==1 && high==n-1);
        std::cout << "{\"n\":" << n << (complete ? ",\"E\":" : ",\"branch_sum\":") << solutions
                  << ",\"first_branch\":" << low << ",\"last_branch\":" << high
                  << ",\"nodes\":" << nodes
                  << ",\"cache_level\":" << cache_level << ",\"cache_step\":" << cache_step
                  << ",\"selection\":" << select_mode << ",\"chain_key\":" << (chains ? "true":"false")
                  << ",\"hits\":" << hits
                  << ",\"misses\":" << misses << ",\"keys\":" << (compact_table?dictionary.size():memo.size())
                  << ",\"compact_table\":" << (compact_table ? "true" : "false")
                  << ",\"impossible_faces\":" << impossible_faces << ",\"cache_limit\":" << cache_limit
                  << ",\"seconds\":" << elapsed << "}" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 3 || argc > 11 || argc==9) return 1;
    int level = argc >= 4 ? std::atoi(argv[3]) : 0;
    int step = argc >= 5 ? std::atoi(argv[4]) : 0;
    int select = argc >= 6 ? std::atoi(argv[5]) : 0;
    bool compact = argc >= 7 && std::atoi(argv[6]) != 0;
    size_t cap=argc>=8 ? std::stoull(argv[7]) : 2000000;
    int low=argc>=10 ? std::atoi(argv[8]) : 0, high=argc>=10 ? std::atoi(argv[9]) : 0;
    bool flat=argc==11 && std::atoi(argv[10])!=0;
    for (int n=std::atoi(argv[1]); n<=std::atoi(argv[2]); n+=2)
        Enumerator(n,level,step,select,compact,cap,flat).run(low,high);
}
