#include <cassert>
#include <cstdio>
#include <numeric>
#include "politics/Config.h"
using namespace politics;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static int sumPm(const Result& r) { int s = 0; for (auto& p : r.parties) s += p.permille; return s; }
static int sumSeats(const Result& r) { int s = 0; for (auto& p : r.parties) s += p.seats; return s; }

int main() {
    const auto wt = WeightTable::defaults();

    // Weights match the spec table.
    CHECK(wt.at(Party::CRP, Trait::Age61_90) == 2.0);
    CHECK(wt.at(Party::NPP, Trait::R1) == 1.0);
    CHECK(wt.at(Party::GAP, Trait::HighIncome) == 0.0);

    // Empty city: equal split, flagged, totals still exact.
    { Result r = compute(CityShares{}, wt);
      CHECK(r.noData); CHECK(sumPm(r) == 1000); CHECK(sumSeats(r) == 25);
      for (auto& p : r.parties) CHECK(p.permille == 200); }

    // Totals are exactly 100% / full council over many pseudo-random cities.
    { unsigned x = 12345;
      for (int n = 0; n < 5000; ++n) {
          CityShares s;
          for (auto& v : s.v) { x = x * 1664525u + 1013904223u; v = (x >> 8) / double(1 << 24); }
          for (int seats : {5, 25, 31, 101}) {
              Settings st; st.councilSeats = seats;
              Result r = compute(s, wt, st);
              CHECK(sumPm(r) == 1000); CHECK(sumSeats(r) == seats);
          } } }

    // Directional checks: raising a trait never lowers the RAW score of a party
    // that likes it. (Percent support is relative across parties, so it can fall
    // if a rival gains faster - that is intended.)
    { CityShares a; a[Trait::LowDensity] = 0.2; a[Trait::HighIncome] = 0.2;
      CityShares b = a; b[Trait::LowDensity] = 0.9;
      for (Party p : {Party::CRP, Party::NPP})
          CHECK(compute(b, wt).parties[int(p)].raw > compute(a, wt).parties[int(p)].raw);
      CHECK(compute(b, wt).parties[int(Party::DPP)].raw == compute(a, wt).parties[int(Party::DPP)].raw); }

    // Dense, educated, young city favours DPP; low-density old rural favours NPP/CRP.
    { CityShares urban; urban[Trait::HighDensity] = .8; urban[Trait::MedDensity] = .2;
      urban[Trait::HighEducation] = .8; urban[Trait::Age21_30] = .4; urban[Trait::Age11_20] = .2;
      CHECK(compute(urban, wt).largest == Party::DPP);
      CityShares rural; rural[Trait::LowDensity] = .9; rural[Trait::Age61_90] = .4; rural[Trait::R1] = .9;
      rural[Trait::MidIncome] = .6;
      Party w = compute(rural, wt).largest; CHECK(w == Party::NPP || w == Party::CRP); }

    // Normalisation option: NLP (11 traits) must not win just for having more traits.
    { CityShares s; for (auto& v : s.v) v = 0.3; s.finalize();
      Result on = compute(s, wt);
      Settings off; off.normalizeByWeight = false;
      Result raw = compute(s, wt, off);
      CHECK(raw.largest == Party::NLP);
      CHECK(on.parties[int(Party::NLP)].permille < raw.parties[int(Party::NLP)].permille); }

    // Input is clamped; derived Age11_30 = Age11_20 + Age21_30.
    { CityShares s; s[Trait::Age11_20] = .3; s[Trait::Age21_30] = .25; s[Trait::LowDensity] = 7; s.finalize();
      CHECK(s[Trait::LowDensity] == 1.0); CHECK(s[Trait::Age11_30] > 0.549 && s[Trait::Age11_30] < 0.551); }

    // Tie handling.
    { std::array<double, kPartyCount> w{1, 1, 1, 1, 1};
      auto a = apportion(w, 25); for (int v : a) CHECK(v == 5);
      auto b = apportion(w, 7); CHECK(std::accumulate(b.begin(), b.end(), 0) == 7); }

    // Config parsing and overrides.
    { Config c = parseConfig("[Council]\nSeats=41\nNormalizeByWeight=0\n[Weights]\nCRP.Age61_90=3\nNLP.CO2=0\nBOGUS.X=1\n");
      CHECK(c.settings.councilSeats == 41); CHECK(!c.settings.normalizeByWeight);
      CHECK(c.weights.at(Party::CRP, Trait::Age61_90) == 3.0);
      CHECK(c.weights.at(Party::NLP, Trait::CO2) == 0.0);
      CHECK(c.warnings.size() == 1);
      Config bad = parseConfig("[Council]\nSeats=9999\n"); CHECK(bad.settings.councilSeats == 25); }

    // [Assumed] section.
    { Config c = parseConfig("[Assumed]\nAge61_90=40\nHighEducation=55\nNope=3\n");
      CHECK(c.assumed[Trait::Age61_90] > 0.399 && c.assumed[Trait::Age61_90] < 0.401);
      CHECK(c.assumed[Trait::HighEducation] > 0.549 && c.assumed[Trait::HighEducation] < 0.551);
      CHECK(c.assumed[Trait::Age31_50] > 0.349 && c.assumed[Trait::Age31_50] < 0.351);  // default kept
      CHECK(c.warnings.size() == 1); }

    std::printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
    return fails ? 1 : 0;
}
