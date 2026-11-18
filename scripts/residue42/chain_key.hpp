#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

// Each run of free variables in domain order is an oriented chain. Its
// two outer domain ports are anchored at known physical boundary positions.
// Renumber entire chains, then store only their lengths and face orders.
template<class A, class B>
std::string chain_key(int n, const A& perm, const A& before, const A& after,
                      const A& odd, const B& face) {
    constexpr int max = 80;
    int slots[max], offsets[max], lengths[max], active=0, chains=0;
    int physical[max], chain_order[max], face_first[max], face_last[max], next[max], prev[max];
    int face_size[max]{}, balance[max]{};
    std::array<uint64_t,max> rank{}, new_rank{};
    for (int a=0; a<n; ++a) {
        if (perm[a]>=0 || perm[before[a]]<0) continue;
        offsets[chains]=active;
        int i=a;
        slots[active++]=odd[perm[before[a]]];
        do {
            slots[active++]=2*i;
            slots[active++]=odd[i];
            i=after[i];
        } while (perm[i]<0);
        slots[active++]=2*perm[i];
        int length=active-offsets[chains];
        lengths[chains]=length;
        chain_order[chains]=chains;
        for (int k=0; k<length; ++k) {
            int v=slots[offsets[chains]+k];
            rank[v]=static_cast<uint64_t>(1+80*length+k);
            balance[face[v]]+=k==0 || k==length-1 ? -1 : 1;
        }
        ++chains;
    }
    for (int v=0; v<2*n; ++v) if (rank[v]) {
        int f=face[v];
        if (!face_size[f]++) face_first[f]=v;
        else { next[face_last[f]]=v; prev[v]=face_last[f]; }
        face_last[f]=v;
    }
    for (int f=0; f<=n; ++f) if (face_size[f]) {
        if (balance[f]<0) return {};
        next[face_last[f]]=face_first[f];
        prev[face_first[f]]=face_last[f];
    }
    for (int k=0; k<active; ++k) {
        int v=slots[k];
        uint64_t a=rank[next[v]], b=rank[prev[v]];
        new_rank[v]=rank[v]*5931+std::min(a,b)*1369+std::max(a,b)*31837+
                    static_cast<unsigned>(face_size[face[v]])*13847u;
    }
    std::sort(chain_order,chain_order+chains,[&](int a, int b){
        if (lengths[a]!=lengths[b]) return lengths[a]<lengths[b];
        for (int k=0; k<lengths[a]; ++k) {
            auto x=new_rank[slots[offsets[a]+k]],y=new_rank[slots[offsets[b]+k]];
            if (x!=y) return x<y;
        }
        return a<b;
    });
    std::string result;
    result.reserve(active+3*chains+2);
    result.push_back(static_cast<char>(chains));
    int label[max], assigned=0;
    for (int i=0; i<chains; ++i) {
        int c=chain_order[i];
        result.push_back(static_cast<char>(lengths[c]));
        for (int k=0; k<lengths[c]; ++k) {
            int v=slots[offsets[c]+k];
            physical[assigned]=v;
            label[v]=assigned++;
        }
    }
    bool done[max]{};
    for (int k=0; k<active; ++k) {
        int first=physical[k], f=face[first];
        if (done[f]) continue;
        done[f]=true;
        result.push_back(static_cast<char>(face_size[f]));
        bool forward=label[next[first]]<=label[prev[first]];
        int v=first;
        do {
            result.push_back(static_cast<char>(label[v]));
            v=forward ? next[v] : prev[v];
        } while(v!=first);
    }
    return result;
}
