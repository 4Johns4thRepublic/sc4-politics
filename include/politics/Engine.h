// SC4 Politics - core engine. Pure C++17, no game dependencies, so it is
// unit-testable off-game and linkable into a 32-bit GZCOM DLL unchanged.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace politics {

// ---------------------------------------------------------------- parties
enum class Party { DPP, GAP, NLP, CRP, NPP, Count };
constexpr int kPartyCount = static_cast<int>(Party::Count);

struct PartyInfo {
    const char* key;      // INI key prefix
    const char* name;
    uint8_t r, g, b;      // colour used by graph, chart and council UI
};

// Colours are fixed here so every UI element reads from one place.
inline const PartyInfo& info(Party p) {
    static const std::array<PartyInfo, kPartyCount> t = {{
        {"DPP", "Democratic Progressive Party", 220,  40,  40},  // Red
        {"GAP", "Green Alliance Party",          40, 170,  70},  // Green
        {"NLP", "New Liberal Party",            240, 210,  30},  // Yellow
        {"CRP", "Conservative Reform Party",    110, 190, 240},  // Light blue
        {"NPP", "National Peoples Party",        20,  50, 140},  // Dark blue
    }};
    return t[static_cast<int>(p)];
}

// -------------------------------------------------------- characteristics
enum class Trait {
    LowDensity, MedDensity, HighDensity,
    MidIncome, HighIncome,
    HighEducation,
    Age11_20, Age21_30, Age11_30 /*derived*/, Age31_50, Age51_60, Age61_90,
    R1, R2, R3,
    CO2,      // Commercial office CO$$
    IHT, ID, IAg,
    Count
};
constexpr int kTraitCount = static_cast<int>(Trait::Count);

inline const char* traitKey(Trait t) {
    static const char* k[kTraitCount] = {
        "LowDensity", "MedDensity", "HighDensity", "MidIncome", "HighIncome",
        "HighEducation", "Age11_20", "Age21_30", "Age11_30", "Age31_50",
        "Age51_60", "Age61_90", "R1", "R2", "R3", "CO2", "IHT", "ID", "IAg"};
    return k[static_cast<int>(t)];
}

// Each value is a SHARE in [0,1] of the relevant base (e.g. fraction of
// residents in medium-density zones). Age11_30 is derived by the engine.
struct CityShares {
    std::array<double, kTraitCount> v{};
    double& operator[](Trait t) { return v[static_cast<int>(t)]; }
    double operator[](Trait t) const { return v[static_cast<int>(t)]; }
    void finalize() {
        for (auto& x : v) x = std::clamp(x, 0.0, 1.0);
        (*this)[Trait::Age11_30] =
            std::min(1.0, (*this)[Trait::Age11_20] + (*this)[Trait::Age21_30]);
    }
};

// ---------------------------------------------------------------- weights
// weight = number of '+' in the spec ("Age 61-90++" => 2). 0 = unused.
struct WeightTable {
    std::array<std::array<double, kTraitCount>, kPartyCount> w{};
    double& at(Party p, Trait t) { return w[int(p)][int(t)]; }
    double at(Party p, Trait t) const { return w[int(p)][int(t)]; }

    static WeightTable defaults() {
        WeightTable t;
        auto set = [&](Party p, std::initializer_list<Trait> l, double x = 1.0) {
            for (Trait tr : l) t.at(p, tr) = x;
        };
        set(Party::DPP, {Trait::MedDensity, Trait::HighDensity, Trait::HighEducation,
                         Trait::Age21_30, Trait::Age11_20});
        set(Party::GAP, {Trait::MidIncome, Trait::Age11_20, Trait::Age61_90});
        set(Party::NLP, {Trait::MedDensity, Trait::HighDensity, Trait::HighIncome,
                         Trait::HighEducation, Trait::Age61_90, Trait::Age31_50,
                         Trait::Age11_30, Trait::R3, Trait::R2, Trait::CO2, Trait::IHT});
        set(Party::CRP, {Trait::LowDensity, Trait::HighIncome, Trait::Age51_60,
                         Trait::R3, Trait::R2, Trait::ID, Trait::IAg});
        set(Party::CRP, {Trait::Age61_90}, 2.0);  // "Age 61-90++"
        set(Party::NPP, {Trait::LowDensity, Trait::MidIncome, Trait::Age61_90, Trait::R1});
        return t;
    }
};

// ---------------------------------------------------------------- results
struct Settings {
    int  councilSeats = 25;
    bool normalizeByWeight = true;  // stops parties with more traits winning by default
};

struct PartyResult {
    Party   party;
    double  raw = 0;      // weighted score
    int     permille = 0; // support in 0.1% units; all five sum to exactly 1000
    int     seats = 0;
    double  percent() const { return permille / 10.0; }
};

struct Result {
    std::array<PartyResult, kPartyCount> parties;
    Party largest = Party::DPP;
    bool  tie = false;         // largest seat count shared
    bool  noData = false;      // all raw scores zero -> equal split used
};

// Largest-remainder apportionment of `total` units over non-negative `weights`.
// Ties on remainder are broken by larger weight, then lower index (deterministic).
inline std::array<int, kPartyCount> apportion(const std::array<double, kPartyCount>& w,
                                              int total) {
    std::array<int, kPartyCount> out{};
    double sum = std::accumulate(w.begin(), w.end(), 0.0);
    if (sum <= 0 || total <= 0) return out;
    std::array<double, kPartyCount> rem{};
    int used = 0;
    for (int i = 0; i < kPartyCount; ++i) {
        double q = w[i] / sum * total;
        out[i] = static_cast<int>(q);
        rem[i] = q - out[i];
        used += out[i];
    }
    std::array<int, kPartyCount> idx;
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        if (rem[a] != rem[b]) return rem[a] > rem[b];
        return w[a] > w[b];
    });
    for (int k = 0; used < total; ++k, ++used) out[idx[k % kPartyCount]]++;
    return out;
}

inline Result compute(CityShares s, const WeightTable& wt, const Settings& st = {}) {
    s.finalize();
    Result r;
    std::array<double, kPartyCount> raw{};
    for (int p = 0; p < kPartyCount; ++p) {
        double score = 0, wsum = 0;
        for (int t = 0; t < kTraitCount; ++t) {
            double w = wt.w[p][t];
            if (w <= 0) continue;
            score += w * s.v[t];
            wsum  += w;
        }
        raw[p] = (st.normalizeByWeight && wsum > 0) ? score / wsum : score;
    }
    double total = std::accumulate(raw.begin(), raw.end(), 0.0);
    std::array<double, kPartyCount> basis = raw;
    if (total <= 0) {                 // empty city / no data
        basis.fill(1.0);
        r.noData = true;
    }
    auto pm    = apportion(basis, 1000);
    auto seats = apportion(basis, st.councilSeats);
    int best = 0;
    for (int p = 0; p < kPartyCount; ++p) {
        r.parties[p] = {static_cast<Party>(p), raw[p], pm[p], seats[p]};
        if (seats[p] > seats[best] ||
            (seats[p] == seats[best] && pm[p] > pm[best])) best = p;
    }
    r.largest = static_cast<Party>(best);
    for (int p = 0; p < kPartyCount; ++p)
        if (p != best && seats[p] == seats[best] && pm[p] == pm[best]) r.tie = true;
    return r;
}

}  // namespace politics
