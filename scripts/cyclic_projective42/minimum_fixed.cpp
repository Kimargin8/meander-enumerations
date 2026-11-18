// Root the fundamental word at a minimum, then traverse the actual lifted diagram.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>

class FixedSectors {
public:
    static constexpr int maximum_order = 47;
    using Count = std::uint64_t;

    FixedSectors(int order, int rotation_order)
        : n(order), length(2 * n), block(length / rotation_order), half(block / 2) {
        for (int i = 0; i < length; ++i) {
            residue[i] = i % block;
            sheet_start[i] = i - residue[i];
            antipode[i] = (i + n) % length;
        }
    }

    void enumerate() {
        visit(0, 0, 0, 0);
        std::cout << "dyck_words " << dyck_words << '\n';
        for (int h = 0; h <= block; ++h)
            for (int k = 1; k <= half; ++k)
                if (accepted[h][k])
                    std::cout << h << ' ' << k << ' ' << accepted[h][k] << '\n';
    }

private:
    int n, length, block, half;
    Count dyck_words = 0;
    std::array<int, 2 * maximum_order> residue{}, sheet_start{}, antipode{};
    std::array<int, 2 * maximum_order + 1> stack{}, mate{}, depth{};
    std::array<std::array<Count, maximum_order + 1>, 2 * maximum_order + 1> accepted{};

    void visit(int pos, int height, int openings, int returns) {
        if (pos == block) {
            ++dyck_words;
            int vertex = 0, steps = 0;
            do {
                const int partner = sheet_start[vertex] + mate[residue[vertex]];
                vertex = antipode[partner];
                ++steps;
            } while (vertex != 0);
            if (steps != n) return;

            int minimum = block;
            for (int i = 0; i < half; ++i)
                minimum = std::min(minimum, depth[i] + depth[i + half]);
            ++accepted[minimum][returns];
            return;
        }
        if (openings < half) {
            stack[height] = pos;
            depth[pos + 1] = height + 1;
            visit(pos + 1, height + 1, openings + 1, returns);
        }
        if (height > 0) {
            // Descendants may overwrite the popped slot, so restore it for our caller.
            const int other = stack[height - 1];
            mate[other] = pos;
            mate[pos] = other;
            depth[pos + 1] = height - 1;
            visit(pos + 1, height - 1, openings, returns + (height == 1));
            stack[height - 1] = other;
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const int n = std::stoi(argv[1]), d = std::stoi(argv[2]);
    if (n < 3 || n > FixedSectors::maximum_order || d < 3 || d % 2 == 0 || n % d)
        return 1;
    // The largest requested fundamental interval has 30 endpoints.
    if (n / d > 15) return 1;
    FixedSectors(n, d).enumerate();
}
