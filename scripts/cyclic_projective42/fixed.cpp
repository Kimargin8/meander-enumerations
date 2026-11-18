// Independent finite check of the fixed diagrams. Enumerate balanced words in a fundamental interval.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const int n = std::stoi(argv[1]), d = std::stoi(argv[2]);
    if (n < 1 || n > 100 || d < 2 || 2 * n % d) return 1;
    const int len = 2*n, block = len/d;
    if (block % 2) { std::cout << n << ' ' << d << " 0 0\n"; return 0; }
    if (block > 40) return 1;
    std::vector<int> word(block), mates(len), pending(block+1);
    std::fill(word.begin()+block/2,word.end(),1);
    std::uint64_t fixed = 0, words = 0;
    do {
        ++words;
        int height = 0, low = 0, root = 0;
        for (int i=0; i<block; ++i) {
            height += word[i] ? 1 : -1;
            if (height < low) { low = height; root = i+1; }
        }
        // Rotate the block to a Dyck word. The reconstructed short arcs repeat d times.
        int top = 0;
        for (int j=0; j<block; ++j) {
            const int i = (root+j)%block;
            if (word[i]) { pending[top++] = j; continue; }
            if (!top) throw std::logic_error("unbalanced stack");
            const int start = root + pending[--top], end = root+j;
            for (int repeat=0; repeat<d; ++repeat) {
                const int a=(start+repeat*block)%len, b=(end+repeat*block)%len;
                mates[a]=b; mates[b]=a;
            }
        }
        int v=0, steps=0;
        do { v=mates[v]+n; if(v>=len) v-=len; ++steps; } while(v);
        fixed += steps == n;
    } while (std::next_permutation(word.begin(), word.end()));
    std::cout << n << ' ' << d << ' ' << words << ' ' << fixed << '\n';
}
