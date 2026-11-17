// SC4 Politics - GZCOM plugin. Samples the city each in-game month, scores the five
// parties with politics::compute(), and serves the result at http://127.0.0.1:<port>/.
// Modelled on nsgomez/gzcom-dll example directors and dsalzner/simcity4-webinterface-plugin.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "cIGZFrameWork.h"
#include "cIGZMessage2.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4Demand.h"
#include "cISC4DemandSimulator.h"
#include "cISC4ResidentialSimulator.h"
#include "cISC4ZoneManager.h"
#include "cRZBaseString.h"
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"

#include "politics/Config.h"
#include "WebPage.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

const uint32_t kDirectorId        = 0x7c4b9a31;
const uint32_t kGZMSG_CityInited  = 0x26d31ec1;
const uint32_t kGZMSG_MonthPassed = 0x66956816;

// Demand/census group ids (same ids the web-interface plugin reads).
// R$ / R$$ / R$$$ population; commercial and industrial jobs.
const uint32_t kR1 = 0x1010, kR2 = 0x1020, kR3 = 0x1030;
const uint32_t kCs1 = 0x3110, kCs2 = 0x3120, kCs3 = 0x3130, kCo2 = 0x3320, kCo3 = 0x3330;
const uint32_t kIAg = 0x4100, kID = 0x4200, kIM = 0x4300, kIHT = 0x4400;

std::string modulePath() {
    HMODULE h = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&modulePath), &h);
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(h, buf, MAX_PATH);
    std::string p(buf);
    auto pos = p.find_last_of("\\/");
    return pos == std::string::npos ? std::string() : p.substr(0, pos + 1);
}

std::mutex gLogMutex;
void logLine(const std::string& msg) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    std::ofstream f(modulePath() + "SC4Politics.log", std::ios::app);
    if (f) f << msg << "\n";
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
        else if (c < 0x20) o += ' ';
        else o += static_cast<char>(c);
    }
    return o;
}

// ---------------------------------------------------------------- shared state
std::mutex gStateMutex;
std::string gStateJson = "{\"ready\":false}";
int gMonth = 0;

// ---------------------------------------------------------------- tiny HTTP server
std::atomic<bool> gServerStarted{false};

void serveLoop(int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { logLine("[E] WSAStartup failed"); return; }
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { logLine("[E] socket() failed"); return; }
    BOOL yes = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // local machine only
    if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(ls, 8) != 0) {
        logLine("[E] Could not listen on 127.0.0.1:" + std::to_string(port) + " (port in use? change [Server] Port)");
        closesocket(ls);
        return;
    }
    logLine("[ ] Web view on http://127.0.0.1:" + std::to_string(port) + "/");
    for (;;) {
        SOCKET c = accept(ls, nullptr, nullptr);
        if (c == INVALID_SOCKET) { Sleep(50); continue; }
        DWORD to = 2000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&to), sizeof(to));
        char req[2048] = {};
        int n = recv(c, req, sizeof(req) - 1, 0);
        std::string body, type = "text/html; charset=utf-8", status = "200 OK";
        if (n > 0 && std::strncmp(req, "GET /state", 10) == 0) {
            type = "application/json";
            std::lock_guard<std::mutex> lock(gStateMutex);
            body = gStateJson;
        } else if (n > 0 && (std::strncmp(req, "GET / ", 6) == 0 || std::strncmp(req, "GET /index", 10) == 0)) {
            body = kWebPage;
        } else {
            status = "404 Not Found"; type = "text/plain"; body = "Not found";
        }
        std::string head = "HTTP/1.1 " + status + "\r\nContent-Type: " + type +
                           "\r\nContent-Length: " + std::to_string(body.size()) +
                           "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
        send(c, head.data(), static_cast<int>(head.size()), 0);
        send(c, body.data(), static_cast<int>(body.size()), 0);
        shutdown(c, SD_BOTH);
        closesocket(c);
    }
}

// ---------------------------------------------------------------- sampling
using politics::Trait;

float supply(cISC4DemandSimulator* ds, uint32_t id) {
    if (!ds) return 0.f;
    const cISC4Demand* d = ds->GetDemand(id, 0);   // census index 0 = whole city
    return d ? std::max(0.f, d->QuerySupplyValue()) : 0.f;
}

double share(double part, double whole) { return whole > 0 ? part / whole : 0.0; }

// Fills `s` from the city. Age brackets are never available from the game and always come
// from [Assumed]; HighEducation falls back to [Assumed] if the EQ value cannot be read.
void sample(cISC4City* city, const politics::Config& cfg, politics::CityShares& s,
            std::vector<std::string>& assumed) {
    s = cfg.assumed;
    for (Trait t : {Trait::Age11_20, Trait::Age21_30, Trait::Age31_50, Trait::Age51_60, Trait::Age61_90})
        assumed.push_back(politics::traitKey(t));

    // Residential density: share of zoned residential tiles.
    if (cISC4ZoneManager* zm = city->GetZoneManager()) {
        double lo = zm->GetZoneCount(cISC4ZoneManager::ZoneType::ResidentialLowDensity);
        double me = zm->GetZoneCount(cISC4ZoneManager::ZoneType::ResidentialMediumDensity);
        double hi = zm->GetZoneCount(cISC4ZoneManager::ZoneType::ResidentialHighDensity);
        double sum = lo + me + hi;
        s[Trait::LowDensity] = share(lo, sum);
        s[Trait::MedDensity] = share(me, sum);
        s[Trait::HighDensity] = share(hi, sum);
    }

    cISC4DemandSimulator* ds = city->GetDemandSimulator();
    // Wealth classes: R$ / R$$ / R$$$ population. Used both as income class and residential type.
    double r1 = supply(ds, kR1), r2 = supply(ds, kR2), r3 = supply(ds, kR3), rs = r1 + r2 + r3;
    s[Trait::R1] = share(r1, rs); s[Trait::R2] = share(r2, rs); s[Trait::R3] = share(r3, rs);
    s[Trait::MidIncome] = s[Trait::R2];
    s[Trait::HighIncome] = s[Trait::R3];

    double co2 = supply(ds, kCo2);
    double cs = supply(ds, kCs1) + supply(ds, kCs2) + supply(ds, kCs3) + co2 + supply(ds, kCo3);
    s[Trait::CO2] = share(co2, cs);

    double ag = supply(ds, kIAg), id = supply(ds, kID), iht = supply(ds, kIHT);
    double is = ag + id + supply(ds, kIM) + iht;
    s[Trait::IAg] = share(ag, is); s[Trait::ID] = share(id, is); s[Trait::IHT] = share(iht, is);

    // Education: city-wide average EQ as a fraction of the maximum (a proxy, not a headcount).
    bool eqOk = false;
    if (cISC4ResidentialSimulator* rsim = city->GetResidentialSimulator()) {
        float eq = rsim->GetGlobalEQ();
        int32_t mx = rsim->GetMaxEQ();
        logLine("[ ] EQ=" + std::to_string(eq) + " MaxEQ=" + std::to_string(mx));
        if (mx > 0 && eq >= 0) { s[Trait::HighEducation] = std::min(1.0, double(eq) / mx); eqOk = true; }
    }
    if (!eqOk) assumed.push_back(politics::traitKey(Trait::HighEducation));
}

std::string buildJson(const std::string& city, int month, const politics::Result& r, const politics::Config& cfg,
                      const politics::CityShares& s, const std::vector<std::string>& assumed) {
    char col[16];
    std::ostringstream o;
    o << "{\"ready\":true,\"city\":\"" << jsonEscape(city) << "\",\"month\":" << month
      << ",\"seats\":" << cfg.settings.councilSeats
      << ",\"largest\":\"" << politics::info(r.largest).key << "\""
      << ",\"tie\":" << (r.tie ? "true" : "false") << ",\"noData\":" << (r.noData ? "true" : "false")
      << ",\"parties\":[";
    for (int i = 0; i < politics::kPartyCount; ++i) {
        const auto& inf = politics::info(static_cast<politics::Party>(i));
        std::snprintf(col, sizeof(col), "#%02x%02x%02x", inf.r, inf.g, inf.b);
        o << (i ? "," : "") << "{\"key\":\"" << inf.key << "\",\"name\":\"" << inf.name << "\",\"color\":\"" << col
          << "\",\"permille\":" << r.parties[i].permille << ",\"seats\":" << r.parties[i].seats << "}";
    }
    o << "],\"assumed\":[";
    for (size_t i = 0; i < assumed.size(); ++i) o << (i ? "," : "") << "\"" << assumed[i] << "\"";
    o << "],\"shares\":{";
    for (int t = 0; t < politics::kTraitCount; ++t)
        o << (t ? "," : "") << "\"" << politics::traitKey(static_cast<Trait>(t)) << "\":" << s.v[t];
    o << "}}";
    return o.str();
}

// ---------------------------------------------------------------- director
class PoliticsDirector : public cRZMessage2COMDirector {
public:
    uint32_t GetDirectorID() const { return kDirectorId; }

    bool OnStart(cIGZCOM*) {
        RZGetFrameWork()->AddHook(static_cast<cIGZFrameWorkHooks*>(this));
        return true;
    }

    bool PostAppInit() {
        mCfg = politics::parseConfig(readFile(modulePath() + "Politics.ini"));
        for (const auto& w : mCfg.warnings) logLine("[W] Politics.ini: " + w);
        logLine("[ ] SC4 Politics loaded");
        cIGZMessageServer2Ptr pMsgServ;
        if (pMsgServ) {
            pMsgServ->AddNotification(this, kGZMSG_CityInited);
            pMsgServ->AddNotification(this, kGZMSG_MonthPassed);
        } else {
            logLine("[E] message server unavailable");
        }
        if (!gServerStarted.exchange(true)) std::thread(serveLoop, mCfg.port).detach();
        return true;
    }

    bool DoMessage(cIGZMessage2* pMessage) {
        uint32_t type = pMessage->GetType();
        if (type == kGZMSG_MonthPassed) ++gMonth;
        if (type == kGZMSG_MonthPassed || type == kGZMSG_CityInited) update();
        return true;
    }

private:
    void update() {
        cISC4AppPtr app;
        cISC4City* city = app ? app->GetCity() : nullptr;
        if (!city) return;
        politics::CityShares s;
        std::vector<std::string> assumed;
        sample(city, mCfg, s, assumed);
        politics::Result r = politics::compute(s, mCfg.weights, mCfg.settings);
        cRZBaseString name;
        city->GetCityName(name);
        std::string json = buildJson(name.ToChar(), gMonth, r, mCfg, s, assumed);
        { std::lock_guard<std::mutex> lock(gStateMutex); gStateJson = json; }
        logLine("[ ] month " + std::to_string(gMonth) + ": " + json);
    }

    politics::Config mCfg;
};

}  // namespace

cRZCOMDllDirector* RZGetCOMDllDirector() {
    static PoliticsDirector sDirector;
    return &sDirector;
}
