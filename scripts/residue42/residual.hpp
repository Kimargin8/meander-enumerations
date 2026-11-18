#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// The future is determined by face boundary orders, residual domain chords
// and the two slots that must be renamed together by the unknown involution.
// Produce a complete encoding up to independent renumbering of the free symbols.
template <class A, class B>
std::string residual_key(int n, const A& perm, const A& before, const A& odd,
                         const B& sides) {
    constexpr int max = 80;
    std::array<uint64_t, max> face_ids{};
    std::array<int, max> first{}, last{}, color{}, match{}, twin{}, next{}, prev{}, balance{};
    std::array<bool, max> used{};
    int faces=0;
    for (int i=0; i<n; ++i) {
        int j=before[i];
        if (perm[i] >= 0 && perm[j] >= 0) continue;
        int e=2*(perm[i] < 0 ? i : perm[i]);
        int o=odd[perm[j] < 0 ? j : perm[j]];
        match[e]=o;
        match[o]=e;
        used[e]=used[o]=true;
    }
    for (int i=0; i<n; ++i) {
        int e=2*i, o=odd[i];
        if (perm[i] < 0) {
            color[e]=1;
            color[o]=2;
            twin[e]=o;
            twin[o]=e;
        } else {
            twin[e]=e;
            twin[o]=o;
        }
    }
    for (int v=0; v<2*n; ++v) if (used[v]) {
        int f=static_cast<int>(std::find(face_ids.begin(), face_ids.begin()+faces, sides[v])-face_ids.begin());
        if (f==faces) {
            face_ids[faces++]=sides[v];
            first[f]=v;
        } else {
            next[last[f]]=v;
        }
        last[f]=v;
        balance[f]+=(color[v] ? 1 : -1);
    }
    for (int f=0; f<faces; ++f) {
        if (balance[f]<0) return {};
        next[last[f]]=first[f];
    }
    std::array<uint64_t,max> rank{}, older{};
    for (int v=0; v<2*n; ++v) if (used[v]) {
        prev[next[v]]=v;
        rank[v]=static_cast<unsigned>(color[v]+1);
    }
    for (int step=0; step<4; ++step) {
        older=rank;
        for (int v=0; v<2*n; ++v) if (used[v]) {
            rank[v]=older[v]*10234631+older[match[v]]*42324617+
                    older[twin[v]]*328669+(older[next[v]]+older[prev[v]])*43627+
                    std::min(older[next[v]],older[prev[v]])*41;
        }
    }
    // A face can be reflected independently of all other faces. Use the
    // intrinsic neighborhoods adjacent to a maximal-rank slot to orient it.
    // Arbitrary ties only lose cache hits.
    for (int f=0; f<faces; ++f) {
        int pivot=first[f];
        for (int v=next[pivot]; v!=first[f]; v=next[v]) if (rank[v]>rank[pivot]) pivot=v;
        int v=next[pivot], w=prev[pivot];
        while (v!=pivot && rank[v]==rank[w]) { v=next[v]; w=prev[w]; }
        if (rank[v]<rank[w]) {
            v=pivot;
            do {
                std::swap(next[v],prev[v]);
                v=prev[v];
            } while(v!=pivot);
        }
    }
    std::array<int,max> component{}, q{}, id{};
    std::vector<std::string> parts;
    for (int base=0; base<2*n; ++base) if (used[base] && component[base]==0) {
        // Rank ties are resolved by the complete rooted encoding.
        int end=1;
        q[0]=base;
        component[base]=base+1;
        uint64_t preferred=0;
        for (int i=0; i<end; ++i) {
            int v=q[i];
            preferred=std::max(preferred,rank[v]);
            for (int w : {next[v], match[v], twin[v]}) if (!component[w]) {
                q[end++]=w;
                component[w]=base+1;
            }
        }
        std::string best;
        for (int root=0; root<2*n; ++root) if (component[root]==base+1 && rank[root]==preferred) {
            id.fill(-1);
            id[root]=0;
            q[0]=root;
            int tail=1;
            std::string sig;
            sig.reserve(4*end+1);
            sig.push_back(static_cast<char>(end));
            for (int i=0; i<tail; ++i) {
                int v=q[i];
                sig.push_back(static_cast<char>(color[v]));
                for (int w : {next[v], match[v], twin[v]}) {
                    if (id[w]<0) {
                        id[w]=tail;
                        q[tail++]=w;
                    }
                    sig.push_back(static_cast<char>(id[w]));
                }
            }
            if (best.empty() || sig<best) best=std::move(sig);
        }
        parts.push_back(std::move(best));
    }
    std::sort(parts.begin(),parts.end());
    std::string result;
    for (const auto& part : parts) result+=part;
    return result;
}
