// Isolated C1 arithmetic optimization: identical transfer and storage to u64.
// Builtin checked operations, unilateral identity, and unit-divisor fast paths.
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Key = uint64_t;
using U128 = unsigned __int128;
using Weight = uint64_t;
constexpr unsigned SHIFT = 57;
constexpr Key WORD_MASK = (Key(1) << SHIFT) - 1;

std::string decimal(U128 value) {
    if (!value) return "0";
    std::string result;
    while (value) { result += char('0' + value % 10); value /= 10; }
    std::reverse(result.begin(), result.end());
    return result;
}

uint64_t hash_key(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<class Integer>
Integer exact_sum(Integer left,Integer right) {
    Integer result;
    if (__builtin_add_overflow(left,right,&result)) throw std::overflow_error("integer sum overflow");
    return result;
}

template<class Integer>
Integer exact_product(Integer left,Integer right) {
    Integer result;
    if (__builtin_mul_overflow(left,right,&result)) throw std::overflow_error("integer product overflow");
    return result;
}

Weight checked_sum(Weight left, Weight right) {
    return exact_sum(left,right);
}

U128 checked_total_sum(U128 left,U128 right) {
    return exact_sum(left,right);
}

// Compact separate 64-bit key and 64-bit value arrays, plus 32-bit occupied slots.
// Full keys are always compared. Key zero is unused because words have a sentinel.
class FlatTable {
    std::vector<Key> keys_;
    std::vector<Weight> values_;
    std::vector<uint32_t> occupied_;
    size_t limit_;
    size_t peak_buffer_bytes_=0;
    bool track_weights_;
    Weight max_weight_=0;

    void allocate(size_t slots) {
        keys_.assign(slots, 0);
        values_.resize(slots);
        occupied_.reserve(std::min(limit_, slots - slots / 4));
        peak_buffer_bytes_=std::max(peak_buffer_bytes_,buffer_bytes());
    }
    size_t locate(Key key) const {
        size_t slot = hash_key(key) & (keys_.size() - 1);
        while (keys_[slot] && keys_[slot] != key) slot = (slot + 1) & (keys_.size() - 1);
        return slot;
    }
    void grow() {
        if (keys_.size() >= (size_t(1) << 31)) throw std::runtime_error("flat table index width exceeded");
        auto old_keys = std::move(keys_);
        auto old_values = std::move(values_);
        auto old_occupied = std::move(occupied_);
        size_t previous_bytes=old_keys.capacity()*sizeof(Key)+old_values.capacity()*sizeof(Weight)
            +old_occupied.capacity()*sizeof(uint32_t);
        allocate(old_keys.size() * 2);
        peak_buffer_bytes_=std::max(peak_buffer_bytes_,previous_bytes+buffer_bytes());
        for (uint32_t previous : old_occupied) {
            size_t slot = locate(old_keys[previous]);
            keys_[slot] = old_keys[previous]; values_[slot] = old_values[previous];
            occupied_.push_back(uint32_t(slot));
        }
    }
public:
    FlatTable(size_t expected, size_t limit, bool track_weights=false)
        : limit_(limit),track_weights_(track_weights) {
        size_t slots = 16;
        expected = std::min(expected, limit);
        while (slots - slots / 4 < expected) {
            if (slots >= (size_t(1) << 31)) throw std::runtime_error("flat table index width exceeded");
            slots *= 2;
        }
        allocate(slots);
    }
    size_t size() const { return occupied_.size(); }
    size_t slots() const { return keys_.size(); }
    size_t buffer_bytes() const {
        return keys_.capacity()*sizeof(Key)+values_.capacity()*sizeof(Weight)
            +occupied_.capacity()*sizeof(uint32_t);
    }
    size_t peak_buffer_bytes() const { return peak_buffer_bytes_; }
    Weight max_weight() const { return max_weight_; }
    const std::vector<uint32_t>& occupied() const { return occupied_; }
    Key key(size_t slot) const { return keys_[slot]; }
    Weight value(size_t slot) const { return values_[slot]; }
    Weight get(Key key) const { size_t slot = locate(key); return keys_[slot] ? values_[slot] : 0; }
    void add(Key key, Weight value) {
        size_t slot = locate(key);
        if (keys_[slot]) {
            values_[slot] = checked_sum(values_[slot],value);
            if (track_weights_) max_weight_=std::max(max_weight_,values_[slot]);
            return;
        }
        if (size() >= limit_) throw std::runtime_error("max-states limit exceeded (not an RSS limit)");
        if (size() + 1 > keys_.size() - keys_.size() / 4) { grow(); slot = locate(key); }
        keys_[slot] = key; values_[slot] = value; occupied_.push_back(uint32_t(slot));
        if (track_weights_) max_weight_=std::max(max_weight_,value);
    }
};

Key reverse_bits(Key x) {
    x = ((x & 0x5555555555555555ULL) << 1) | ((x >> 1) & 0x5555555555555555ULL);
    x = ((x & 0x3333333333333333ULL) << 2) | ((x >> 2) & 0x3333333333333333ULL);
    x = ((x & 0x0f0f0f0f0f0f0f0fULL) << 4) | ((x >> 4) & 0x0f0f0f0f0f0f0f0fULL);
    return __builtin_bswap64(x);
}

Key mirrored_word(Key word, int length) {
    if (!length) return 1;
    Key bits = (~reverse_bits(word)) >> (64 - length);
    return bits | (Key(1) << length);
}

Key canonical_key(Key word, int length, int slit, bool symmetry) {
    if (!symmetry || 2 * slit < length) return word | (Key(slit) << SHIFT);
    Key reflected = mirrored_word(word, length);
    if (2 * slit > length) return reflected | (Key(length - slit) << SHIFT);
    return std::min(word, reflected) | (Key(slit) << SHIFT);
}


Key mask(int n) { return (Key(1) << n) - 1; }
Key insert_bits(Key w, int slit, unsigned bits) {
    return (w & mask(slit)) | (Key(bits) << slit) | ((w >> slit) << (slit + 2));
}
Key remove_bits(Key w, int position) {
    return (w & mask(position)) | ((w >> (position + 2)) << position);
}


U128 checked_product(U128 left, U128 right) {
    return exact_product(left,right);
}

uint64_t binomial(int n, int k) {
    k = std::min(k, n-k);
    uint64_t result = 1;
    for (int i=1; i<=k; ++i) result = result * (n-k+i) / i;
    return result; // n <= 24 under the state-width and order guards.
}

uint64_t geodesic_factor(Key word, int length, int slit) {
    int left = slit-1, right = slit;
    uint64_t factor = 1;
    for (;;) {
        int a=0, b=0;
        while (left >= 0 && !((word >> left) & 1)) {
            int balance=1;
            --left;
            while (left >= 0) {
                balance += ((word >> left) & 1) ? -1 : 1;
                if (!balance) break;
                --left;
            }
            if (left < 0) throw std::logic_error("invalid left side subtree");
            --left; ++a;
        }
        while (right < length && ((word >> right) & 1)) {
            int balance=1;
            ++right;
            while (right < length) {
                balance += ((word >> right) & 1) ? 1 : -1;
                if (!balance) break;
                ++right;
            }
            if (right == length) throw std::logic_error("invalid right side subtree");
            ++right; ++b;
        }
        uint64_t choices = binomial(a+b,a);
        factor=exact_product(factor,choices);
        if (left < 0 || right >= length) {
            if (left >= 0 || right < length) throw std::logic_error("unmatched spanning arc");
            return factor;
        }
        // Adjacent endpoints of the containing face after its side subtrees.
        --left; ++right;
    }
}

class Slack2Tree {
    struct Node {
        int left=0, right=0, parent=0, first=0, last=0, next=0, degree=0, size=1;
        uint64_t squares=0, intrinsic=0;
    };
    struct Face {
        int a=0,b=0;
        uint64_t g=1,intrinsic=0,degree_left=0,degree_right=0;
    };
    uint64_t combinations[27][27]{};
    int threshold;
    bool narrow;
    uint64_t choose(int n,int k) const {
        if (k<0 || k>n) return 0;
        if (n<0 || n>26) throw std::logic_error("tree binomial index exceeded");
        return combinations[n][k];
    }
    template<class Local>
    static Local product(Local a,Local b) {
        return exact_product(a,b);
    }
    template<class Local>
    static void add(Local& a,Local b) {
        a=exact_sum(a,b);
    }
    template<class Local>
    static Local difference(Local a,Local b) {
        if (a<b) throw std::logic_error("negative slack2 tree binomial difference");
        return a-b;
    }
    template<class Local>
    Local calculate(Key word,int length,int slit) {
        if (!length) return 0;
        if (length>48 || length%2) throw std::logic_error("slack2 tree width exceeded");
        Node nodes[25];
        nodes[0]=Node{};
        int count=0,current=0;
        for (int p=0;p<length;++p) {
            if ((word>>p)&1) {
                if (++count>=25) throw std::logic_error("too many tree arcs");
                nodes[count]=Node{};
                Node& node=nodes[count]; Node& parent=nodes[current];
                node.left=p;node.parent=current;
                if (parent.last) nodes[parent.last].next=count;
                else parent.first=count;
                parent.last=count; ++parent.degree;
                current=count;
            } else {
                if (!current) throw std::logic_error("unbalanced tree word");
                Node& node=nodes[current]; node.right=p;
                node.squares+=uint64_t(node.degree)*node.degree;
                node.intrinsic=node.squares+3*uint64_t(node.size-1)-node.degree;
                Node& parent=nodes[node.parent];
                parent.squares+=node.squares; parent.size+=node.size;
                current=node.parent;
            }
        }
        if (current) throw std::logic_error("unbalanced tree word");
        parsed_nodes+=count;
        Face faces[25]; int number=0;
        Local total_g=1;
        for (int face_node=0;;) {
            Face face; int spanning=0;
            for (int child=nodes[face_node].first;child;child=nodes[child].next) {
                ++face_child_visits;
                const Node& node=nodes[child];
                if (node.right<slit) {
                    ++face.a;face.intrinsic+=node.intrinsic;face.degree_left+=node.degree+1;
                } else if (node.left>=slit) {
                    ++face.b;face.intrinsic+=node.intrinsic;face.degree_right+=node.degree+1;
                } else {
                    if (spanning) throw std::logic_error("multiple spanning children");
                    spanning=child;
                }
            }
            face.g=choose(face.a+face.b,face.a);
            total_g=product<Local>(total_g,face.g);
            faces[number++]=face;
            if (!spanning) break;
            face_node=spanning;
        }
        Local answer=0;
        for (int i=0;i<number;++i) {
            const Face& face=faces[i]; int a=face.a,b=face.b,n=a+b;
            Local weighted=0;
            if (!a || !b) {
                // For a one-sided (or empty) face G=1 and W=J+DL+DR+n(n+1).
                weighted=Local(face.intrinsic);
                add(weighted,Local(face.degree_left));add(weighted,Local(face.degree_right));
                add(weighted,product<Local>(n,n+1));
            } else {
                weighted=product<Local>(face.g,face.intrinsic);
                add(weighted,product<Local>(choose(n+1,a+1),face.degree_left));
                add(weighted,product<Local>(choose(n+1,a),face.degree_right));
                Local left=difference<Local>(product<Local>(n+1,choose(n,a-1)),choose(n+1,a-1));
                Local right=difference<Local>(product<Local>(n+1,choose(n,b-1)),choose(n+1,b-1));
                add(left,right);add(weighted,product<Local>(2,left));
            }
            Local quotient=total_g;
            if (face.g!=1) {
                if (total_g%face.g) throw std::logic_error("nonintegral face quotient");
                quotient/=face.g;
            }
            add(answer,product<Local>(quotient,weighted));
            if (i) {
                const Face& parent=faces[i-1]; int A=parent.a,B=parent.b,N=A+B;
                Local divisor=product<Local>(face.g,parent.g);
                Local parent_quotient=total_g;
                if (divisor!=1) {
                    if (total_g%divisor) throw std::logic_error("nonintegral parent quotient");
                    parent_quotient/=divisor;
                }
                Local cross=product<Local>(choose(n+1,a+1),choose(N+1,A));
                add(cross,product<Local>(choose(n+1,a),choose(N+1,A+1)));
                add(answer,product<Local>(parent_quotient,cross));
            }
        }
        return answer;
    }
public:
    uint64_t requests=0,calls=0,threshold_skips=0,parsed_nodes=0,face_child_visits=0;
    explicit Slack2Tree(int max_pairs,bool narrow_arithmetic):threshold(max_pairs),narrow(narrow_arithmetic) {
        for (int n=0;n<=26;++n) {
            combinations[n][0]=combinations[n][n]=1;
            for (int k=1;k<n;++k) combinations[n][k]=combinations[n-1][k-1]+combinations[n-1][k];
        }
    }
    bool factor(Key word,int length,int slit,Weight& result) {
        ++requests;
        if (!threshold || length/2>threshold) { ++threshold_skips;return false; }
        ++calls;
        U128 wide=narrow ? U128(calculate<uint64_t>(word,length,slit)) : calculate<U128>(word,length,slit);
        if (wide>UINT64_MAX) throw std::overflow_error("uint64 tail factor conversion overflow");
        result=Weight(wide);
        return true;
    }
    std::string json() const {
        return "{\"requests\":"+std::to_string(requests)+",\"calls\":"+std::to_string(calls)
            +",\"threshold_skips\":"+std::to_string(threshold_skips)+",\"max_pairs\":"+std::to_string(threshold)
            +",\"parsed_nodes\":"+std::to_string(parsed_nodes)+",\"face_child_visits\":"+std::to_string(face_child_visits)
            +",\"cache_entries\":0,\"arithmetic\":\""
            +std::string(narrow?"checked_uint64_tree_uint64_frontier":"checked_uint128_tree_uint64_frontier")+"\"}";
    }
};

int main(int argc, char** argv) try {
    int m = 12, dump_step = -1;
    bool symmetry = false, prune = true, geodesic = true;
    bool query_stdin=false;
    bool slack2_narrow=true;
    bool arithmetic_query=false,arithmetic_fail_fast=false;
    bool track_weights=false;
    int reserve_scale=2;
    int slack2_pairs=24;
    size_t cap = 2000000;
    std::string output;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--m" && i + 1 < argc) m = std::stoi(argv[++i]);
        else if (arg == "--symmetry") symmetry = true;
        else if (arg == "--no-pruning") prune = false;
        else if (arg == "--no-geodesic") geodesic = false;
        else if (arg == "--no-slack2") slack2_pairs=0;
        else if (arg == "--slack2-narrow-arithmetic") slack2_narrow=true;
        else if (arg == "--slack2-wide-arithmetic") slack2_narrow=false;
        else if (arg == "--arithmetic-query-stdin") arithmetic_query=true;
        else if (arg == "--arithmetic-fail-fast") arithmetic_fail_fast=true;
        else if (arg == "--track-max-frontier-weight") track_weights=true;
        else if (arg == "--reserve-scale" && i+1<argc) {
            std::string value=argv[++i];
            if (value!="1" && value!="2") throw std::invalid_argument("reserve-scale must be 1 or 2");
            reserve_scale=value=="1"?1:2;
        }
        else if (arg == "--slack2-query-stdin") query_stdin=true;
        else if (arg == "--slack2-max-pairs" && i+1<argc) {
            std::string value=argv[++i]; size_t consumed=0;
            if (value.empty() || value[0]=='-') throw std::invalid_argument("invalid slack2 threshold");
            unsigned long parsed=std::stoul(value,&consumed);
            if (consumed!=value.size() || parsed>24) throw std::invalid_argument("slack2 threshold must be 0..24");
            slack2_pairs=int(parsed);
        }
        else if (arg == "--max-states" && i + 1 < argc) {
            std::string value = argv[++i];
            if (value.empty() || value[0] == '-') throw std::invalid_argument("invalid state cap");
            cap = std::stoull(value);
        }
        else if (arg == "--dump-step" && i + 1 < argc) dump_step = std::stoi(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) output = argv[++i];
        else if (arg == "--help") {
            std::cout << "future_slack2_fast_frontier --m N [--symmetry] [--reserve-scale 1|2] [--slack2-max-pairs 0..24] [--no-slack2] [--slack2-wide-arithmetic] [--slack2-query-stdin] [--arithmetic-query-stdin] [--arithmetic-fail-fast] [--track-max-frontier-weight] [--no-geodesic] [--max-states N] [--dump-step N] [--output FILE]\nChecked uint64 frontier, checked uint128 products/totals; overflow aborts. No known-answer-dependent shortcuts.\n";
            return 0;
        } else throw std::invalid_argument("unknown/incomplete argument: " + arg);
    }
    if (m < 1 || m > 24 || !cap || cap > (size_t(1) << 30) || dump_step < -1 || dump_step > 2*m)
        throw std::invalid_argument("require m=1..24, positive cap, valid dump step");
    if (!prune && m > 12) throw std::invalid_argument("unpruned diagnostic limited to m<=12");
    if (arithmetic_query && query_stdin) throw std::invalid_argument("select one diagnostic mode");
    if (arithmetic_fail_fast && !arithmetic_query) throw std::invalid_argument("fail-fast is an arithmetic diagnostic option");
    if (arithmetic_query) {
        auto read_total=[]() -> U128 {
            std::string text;
            if (!(std::cin>>text) || text.empty() || text[0]=='-')
                throw std::invalid_argument("expected unsigned diagnostic operand");
            U128 value=0;
            for (char c:text) {
                if (c<'0' || c>'9') throw std::invalid_argument("invalid diagnostic operand");
                unsigned digit=c-'0';
                if (value>(~U128(0)-digit)/10) throw std::invalid_argument("diagnostic operand exceeds uint128");
                value=value*10+digit;
            }
            return value;
        };
        auto read_value=[&]() -> Weight {
            U128 value=read_total();
            if (value>UINT64_MAX) throw std::invalid_argument("frontier operand exceeds uint64");
            return Weight(value);
        };
        std::string operation,rows="[";bool first=true;
        while (std::cin>>operation) {
            if (operation!="add" && operation!="multiply" && operation!="table"
                && operation!="tablegrow" && operation!="aggregate" && operation!="multiply64")
                if (operation!="totaladd" && operation!="totalmultiply")
                    throw std::invalid_argument("unknown arithmetic operation");
            U128 a=0,b=0,c=0,result=0;
            if (operation=="aggregate") {a=read_total();b=read_value();c=read_value();}
            else if (operation=="totaladd" || operation=="totalmultiply") {a=read_total();b=read_total();}
            else {a=read_value();b=read_value();}
            bool overflow=false;
            try {
                if (operation=="add") result=checked_sum(a,b);
                else if (operation=="multiply64") result=exact_product(Weight(a),Weight(b));
                else if (operation=="multiply") result=checked_product(a,b);
                else if (operation=="aggregate") result=checked_total_sum(a,checked_product(b,c));
                else if (operation=="totaladd") result=checked_total_sum(a,b);
                else if (operation=="totalmultiply") result=checked_product(a,b);
                else {
                    FlatTable table(1,100);table.add(1,a);
                    if (operation=="tablegrow")
                        for (Key key=2;key<=40;++key) table.add(key,1);
                    table.add(1,b);result=table.get(1);
                }
            } catch (const std::overflow_error&) {
                if (arithmetic_fail_fast) throw;
                overflow=true;
            }
            if (!first) rows+=',';
            first=false;
            rows+="{\"status\":\""+std::string(overflow?"overflow":"ok")+"\",\"result\":"
                +(overflow?"null":"\""+decimal(result)+"\"")+"}";
        }
        if (!std::cin.eof()) throw std::invalid_argument("invalid diagnostic input");
        std::cout<<"{\"arithmetic\":\"checked_uint64_frontier_uint128_totals\",\"slot_bytes\":"
            <<sizeof(Key)+sizeof(Weight)<<",\"results\":"<<rows<<"]}\n";
        return 0;
    }
    auto start = std::chrono::steady_clock::now();
    Slack2Tree tree(slack2_pairs,slack2_narrow); // Binomial precomputation is inside the timed region.
    if (query_stdin) {
        std::string input,rows="[";int slit;bool first=true;
        while (std::cin>>input) {
            if (!(std::cin>>slit)) throw std::invalid_argument("missing query slit");
            int length=input=="-"?0:int(input.size()),balance=0;
            if (length>48 || length%2 || slit<0 || slit>length)
                throw std::invalid_argument("invalid query length/slit");
            Key word=Key(1)<<length;
            for (int p=0;p<length;++p) {
                if (input[p]=='(') {word|=Key(1)<<p;++balance;}
                else if (input[p]==')') --balance;
                else throw std::invalid_argument("invalid query character");
                if (balance<0) throw std::invalid_argument("unbalanced query");
            }
            if (balance) throw std::invalid_argument("unbalanced query");
            Weight factor=0;bool ok=tree.factor(word,length,slit,factor);
            if (!first) rows+=',';
            first=false;
            rows+="{\"status\":\""+std::string(ok?"ok":"fallback")+"\",\"factor\":"
                +(ok?"\""+decimal(factor)+"\"":"null")+"}";
        }
        if (!std::cin.eof()) throw std::invalid_argument("malformed query input");
        std::cout<<"{\"queries\":"<<rows<<"],\"slack2_tree\":"<<tree.json()<<"}\n";
        return 0;
    }
    FlatTable current(1, cap,track_weights);
    size_t peak_combined_table_bytes=current.buffer_bytes();
    U128 answer_total = 0;
    uint64_t absorbed_states = 0;
    uint64_t slack2_absorbed=0;
    if (geodesic && m == 1) { answer_total = 1; absorbed_states = 1; }
    else current.add(Key(5) | (Key(1) << SHIFT), 1); // One crossing; one arc.
    Weight max_frontier_weight=current.max_weight();
    size_t peak = current.size(), total = current.size();
    uint64_t total_generated = 1;
    std::vector<std::string> layers;
    layers.push_back("{\"step\":1,\"states\":" + std::to_string(current.size())
        + ",\"transitions_generated\":1,\"transitions_accepted\":1,\"absorbed_transitions\":"
        + std::to_string(absorbed_states) + ",\"added_answer\":\"" + decimal(answer_total) + "\"}");
    std::string dumped = "[]";
    auto dump = [&]() {
        std::string text = "[";
        bool first = true;
        for (uint32_t slot : current.occupied()) {
            Key key = current.key(slot); Weight value = current.value(slot);
            if (!first) text += ',';
            first = false;
            text += "[\"" + std::to_string(key) + "\",\"" + decimal(value) + "\"]";
        }
        dumped = text + ']';
    };
    if (dump_step == 1) dump();
    for (int step = 2; step <= 2*m; ++step) {
        int remaining = 2*m-step;
        FlatTable next(std::min(cap, current.size()*reserve_scale+16), cap,track_weights);
        uint64_t generated = 0, accepted = 0, absorbed = 0;
        U128 added_answer = 0;
        auto emit = [&](Key word, int length, int slit, int height, Weight value) {
            ++generated;
            if (!length && remaining) return;
            if (length && !remaining) return;
            if (prune && length-height > remaining) return;
            ++accepted;
            Weight factor=0;
            bool use_factor=false,use_slack2=false;
            if (geodesic && length-height == remaining) {
                factor=geodesic_factor(word,length,slit);use_factor=true;
            } else if (geodesic && length && length-height+2 == remaining) {
                use_factor=tree.factor(word,length,slit,factor);use_slack2=use_factor;
            }
            if (use_factor) {
                U128 contribution = checked_product(U128(value),U128(factor));
                U128 new_total=checked_total_sum(answer_total,contribution);
                U128 new_layer=checked_total_sum(added_answer,contribution);
                answer_total=new_total;added_answer=new_layer;
                ++absorbed; ++absorbed_states;
                if (use_slack2) ++slack2_absorbed;
                return;
            }
            next.add(canonical_key(word, length, slit, symmetry), value);
        };
        for (uint32_t slot : current.occupied()) {
            Key key = current.key(slot); Weight value = current.value(slot);
            Key word = key & WORD_MASK;
            int length = std::bit_width(word)-1, slit = key >> SHIFT;
            int height = 2*std::popcount(word & mask(slit))-slit;
            if (slit) emit(word, length, slit-1, height - (((word >> (slit-1)) & 1) ? 1 : -1), value);
            if (slit < length) emit(word, length, slit+1, height + (((word >> slit) & 1) ? 1 : -1), value);
            // Future closing consumes an actual adjacent future arc.
            if (slit && slit < length && ((word >> (slit-1)) & 3) == 1)
                emit(remove_bits(word, slit-1), length-2, slit-1, height-1, value);
            // Splitting a visible arc is inverse surgery of a past join.
            if (prune && length+2 > 2*remaining) continue;
            // Follow the boundary of the face containing slit. Every scanned
            // non-spanning arc occupies a disjoint interval, so the scan is O(L).
            int position = slit - 1;
            while (position >= 0) {
                if ((word >> position) & 1) {
                    // The innermost spanning arc; encountered exactly once.
                    emit(insert_bits(word, slit, 2), length+2, slit+1, height-1, value);
                    break;
                }
                int closing = position, balance = 1;
                --position;
                while (position >= 0) {
                    balance += ((word >> position) & 1) ? -1 : 1;
                    if (!balance) break;
                    --position;
                }
                if (position < 0) throw std::logic_error("missing opening partner");
                emit(insert_bits(word ^ (Key(1) << closing), slit, 0),
                     length+2, slit+1, height+1, value);
                --position;
            }
            position = slit;
            while (position < length) {
                if (!((word >> position) & 1)) break; // Already handled spanning arc.
                int opening = position, balance = 1;
                ++position;
                while (position < length) {
                    balance += ((word >> position) & 1) ? 1 : -1;
                    if (!balance) break;
                    ++position;
                }
                if (position == length) throw std::logic_error("missing closing partner");
                emit(insert_bits(word ^ (Key(1) << opening), slit, 3),
                     length+2, slit+1, height+1, value);
                ++position;
            }
        }
        peak_combined_table_bytes=std::max(peak_combined_table_bytes,
                                           current.buffer_bytes()+next.peak_buffer_bytes());
        if (track_weights) max_frontier_weight=std::max(max_frontier_weight,next.max_weight());
        current = std::move(next);
        peak = std::max(peak, current.size()); total += current.size(); total_generated += generated;
        layers.push_back("{\"step\":" + std::to_string(step) + ",\"states\":" + std::to_string(current.size())
            + ",\"transitions_generated\":" + std::to_string(generated)
            + ",\"transitions_accepted\":" + std::to_string(accepted)
            + ",\"absorbed_transitions\":" + std::to_string(absorbed)
            + ",\"added_answer\":\"" + decimal(added_answer) + "\"}");
        if (step == dump_step) dump();
    }
    U128 answer = geodesic ? answer_total : U128(current.get(1));
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::string json = "{\"implementation\":\"cpp_future_slack2_fast_checked_u64\",\"closed_order\":" + std::to_string(m)
        + ",\"open_n_equivalent\":" + std::to_string(2*m-1) + ",\"answer\":\"" + decimal(answer)
        + "\",\"symmetry\":" + (symmetry ? "true" : "false") + ",\"pruning\":" + (prune ? "true" : "false")
        + ",\"seconds\":" + std::to_string(seconds) + ",\"peak_states\":" + std::to_string(peak)
        + ",\"total_states\":" + std::to_string(total) + ",\"transitions_generated\":" + std::to_string(total_generated)
        + ",\"geodesic\":" + (geodesic ? "true" : "false")
        + ",\"absorbed_transitions\":" + std::to_string(absorbed_states)
        + ",\"slack2_absorbed_transitions\":" + std::to_string(slack2_absorbed)
        + ",\"slack2_tree\":" + tree.json()
        + ",\"frontier_weight_bits\":64,\"final_accumulator_bits\":128,\"overflow_policy\":\"abort_without_answer\""
        + ",\"max_frontier_weight\":"+(track_weights?"\""+decimal(max_frontier_weight)+"\"":"null")
        + ",\"hash_slot_bytes\":"+std::to_string(sizeof(Key)+sizeof(Weight))
        + ",\"occupied_index_bytes\":"+std::to_string(sizeof(uint32_t))
        + ",\"reserve_scale\":"+std::to_string(reserve_scale)
        + ",\"peak_combined_table_buffer_bytes\":"+std::to_string(peak_combined_table_bytes)
        + ",\"added_answer\":\"" + decimal(answer_total) + "\""
        + ",\"max_states_guard\":" + std::to_string(cap) + ",\"layers\":[";
    for (size_t i = 0; i < layers.size(); ++i) { if (i) json += ','; json += layers[i]; }
    json += "]";
    if (dump_step >= 0) json += ",\"dump_step\":" + std::to_string(dump_step) + ",\"layer_dump\":" + dumped;
    json += "}\n";
    if (output.empty()) std::cout << json;
    else {
        std::ofstream stream(output);
        if (!stream) throw std::runtime_error("cannot open output");
        stream << json;
        if (!stream) throw std::runtime_error("write failed");
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n'; return 2;
}
