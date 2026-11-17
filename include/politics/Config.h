// Minimal INI reader for Politics.ini. Format:
//   [Council]  Seats=25   NormalizeByWeight=1
//   [Weights]  CRP.Age61_90=2   NLP.CO2=0   (0 removes a trait from a party)
#pragma once
#include <cstdlib>
#include <sstream>
#include <string>
#include "Engine.h"

namespace politics {

struct Config {
    Settings    settings;
    WeightTable weights = WeightTable::defaults();
    // Shares (0..1) used for traits the game cannot report (the five age brackets),
    // or as a fallback when a game value is unavailable. Set in [Assumed], in percent.
    int port = 9031;                    // [Server] Port, local web view
    CityShares assumed = defaultAssumed();
    std::vector<std::string> warnings;   // unknown keys etc., for the plugin log

    static CityShares defaultAssumed() {
        CityShares a;
        a[Trait::Age11_20] = 0.15; a[Trait::Age21_30] = 0.15; a[Trait::Age31_50] = 0.35;
        a[Trait::Age51_60] = 0.15; a[Trait::Age61_90] = 0.20;
        a[Trait::HighEducation] = 0.30;
        return a;
    }
};

inline std::string trim(std::string s) {
    auto ws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (!s.empty() && ws(s.front())) s.erase(s.begin());
    while (!s.empty() && ws(s.back())) s.pop_back();
    return s;
}

inline Config parseConfig(const std::string& text) {
    Config c;
    std::istringstream in(text);
    std::string line, section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        auto eq = line.find('=');
        if (eq == std::string::npos) { c.warnings.push_back("bad line: " + line); continue; }
        std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (section == "Council") {
            if (k == "Seats") {
                int n = std::atoi(v.c_str());
                if (n >= 5 && n <= 200) c.settings.councilSeats = n;
                else c.warnings.push_back("Seats out of range 5..200: " + v);
            } else if (k == "NormalizeByWeight") c.settings.normalizeByWeight = (v != "0");
            else c.warnings.push_back("unknown Council key: " + k);
        } else if (section == "Weights") {
            auto dot = k.find('.');
            if (dot == std::string::npos) { c.warnings.push_back("bad weight key: " + k); continue; }
            std::string pk = k.substr(0, dot), tk = k.substr(dot + 1);
            int pi = -1, ti = -1;
            for (int i = 0; i < kPartyCount; ++i) if (pk == info(static_cast<Party>(i)).key) pi = i;
            for (int i = 0; i < kTraitCount; ++i) if (tk == traitKey(static_cast<Trait>(i))) ti = i;
            double w = std::atof(v.c_str());
            if (pi < 0 || ti < 0 || w < 0) { c.warnings.push_back("bad weight: " + k + "=" + v); continue; }
            c.weights.w[pi][ti] = w;
        } else if (section == "Server") {
            if (k == "Port") {
                int n = std::atoi(v.c_str());
                if (n >= 1024 && n <= 65535) c.port = n;
                else c.warnings.push_back("Port out of range 1024..65535: " + v);
            } else c.warnings.push_back("unknown Server key: " + k);
        } else if (section == "Assumed") {
            int ti = -1;
            for (int i = 0; i < kTraitCount; ++i) if (k == traitKey(static_cast<Trait>(i))) ti = i;
            double pct = std::atof(v.c_str());
            if (ti < 0 || pct < 0 || pct > 100) { c.warnings.push_back("bad assumed value: " + k + "=" + v); continue; }
            c.assumed.v[ti] = pct / 100.0;
        }
    }
    return c;
}

}  // namespace politics
