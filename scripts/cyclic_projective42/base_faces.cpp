// Count quotient face marks entirely on the 2m base sites.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

class BaseFaces {
public:
    static constexpr int limit = 15;
    using Count = std::uint64_t;
    using Mask = std::uint32_t;

    explicit BaseFaces(int m) : order(m), sites(2 * order) {}

    void compute() {
        pairs((Mask(1) << sites) - 1);
        std::cout << "matchings " << matchings << "\nconnected " << connected
                  << "\neven_gluings " << even_gluings << '\n';
        for (int h = 0; h <= 2 * order; ++h)
            if (histogram[h]) std::cout << h << ' ' << histogram[h] << '\n';
    }

private:
    int order, sites;
    std::array<int, 2 * limit> mate{};
    Count matchings = 0, connected = 0, even_gluings = 0;
    std::array<Count, 2 * limit + 1> histogram{};

    // At the first free site the first occupied site on its right closes an
    // earlier chord. The partner must precede that site and leave an even interior.
    void pairs(Mask free) {
        if (!free) {
            ++matchings;
            if (is_connected()) {
                ++connected;
                scan_faces();
            }
            return;
        }
        const int first = __builtin_ctz(free);
        const int end = first + __builtin_ctz(~(free >> first));
        const Mask other_sites = free ^ (Mask(1) << first);
        for (int other = first + 1; other < end; other += 2) {
            mate[first] = other;
            mate[other] = first;
            pairs(other_sites ^ (Mask(1) << other));
        }
    }

    bool is_connected() const {
        // Contract the antipodal edges. The two representatives of a vertex
        // supply its two neighbors in the resulting undirected graph.
        Mask reached = 1, pending = 1;
        while (pending) {
            const int i = __builtin_ctz(pending);
            pending &= pending - 1;
            const int a = mate[i], b = mate[i + order];
            Mask neighbors = (Mask(1) << (a >= order ? a - order : a))
                           | (Mask(1) << (b >= order ? b - order : b));
            neighbors &= ~reached;
            pending |= neighbors;
            reached |= neighbors;
        }
        return reached == (Mask(1) << order) - 1;
    }

    using Faces = std::array<int, limit + 1>;
    using Gaps = std::array<int, 2 * limit>;

    int disk_color(const Gaps& gap, const Faces& color) {
        Faces parent{};
        std::iota(parent.begin(), parent.end(), 0);
        auto root = [&parent](int i) {
            while (i != parent[i]) i = parent[i];
            return i;
        };
        int vertices[2]{}, edges[2]{}, cycles[2]{};
        for (int f = 0; f <= order; ++f) ++vertices[color[f]];
        for (int i = 0; i < order; ++i) {
            const int a = gap[i], b = gap[i + order], c = color[a];
            if (c != color[b]) throw std::logic_error("opposite gaps have different colors");
            ++edges[c];
            const int x = root(a), y = root(b);
            if (x == y) ++cycles[c];
            else parent[x] = y;
        }
        for (int c = 0; c < 2; ++c) {
            if (edges[c] != order / 2 || vertices[c] - edges[c] + cycles[c] != 1)
                throw std::logic_error("disconnected side");
        }
        if (cycles[0] + cycles[1] != 1) throw std::logic_error("Euler characteristic");
        const int tree = cycles[0] == 0 ? 0 : 1;
        if (vertices[tree] != order / 2 + 1) throw std::logic_error("disk face count");
        ++even_gluings;
        return tree;
    }

    void scan_faces() {
        Faces parent{}, color{};
        Gaps gap{};
        int dist[limit + 1][limit + 1]{};
        int current = 0, last = 0;
        for (int i = 0; i < sites; ++i) {
            gap[i] = current;
            if (mate[i] < i) {
                current = parent[current];
                continue;
            }
            const int child = ++last;
            parent[child] = current;
            color[child] = 1 - color[current];
            for (int old = 0; old < child; ++old)
                dist[child][old] = dist[old][child] = 1 + dist[current][old];
            current = child;
        }
        if (current != 0 || last != order) throw std::logic_error("incomplete dual tree");
        const int side = order % 2 == 0 ? disk_color(gap, color) : -1;
        for (int f = 0; f <= order; ++f) {
            if (side >= 0 && color[f] != side) continue;
            int minimum = 2 * order;
            for (int i = 0; i < order; ++i)
                minimum = std::min(minimum, dist[f][gap[i]] + dist[f][gap[i + order]]);
            ++histogram[minimum];
        }
    }
};

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const int m = std::stoi(argv[1]);
    if (m < 1 || m > BaseFaces::limit) return 1;
    BaseFaces(m).compute();
}
