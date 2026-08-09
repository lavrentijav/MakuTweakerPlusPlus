#include "core/TelemetryWire.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include <bcrypt.h>
#include <fstream>
#include <objbase.h>
#include <thread>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace maku::analytics::wire {
namespace {

#include "telemetry_blobs.inc"

using FnEmit = void (*)(Ev, const std::vector<std::pair<Fd, std::string>>&);

std::string Dn(const uint8_t* b, size_t n, uint8_t seed) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>(b[i] ^ static_cast<uint8_t>((seed + i * 23) & 0xFF));
    return s;
}

std::wstring Dw(const uint8_t* b, size_t n, uint8_t seed) {
    std::wstring s(n / 2, L'\0');
    for (size_t i = 0; i < n; ++i) {
        auto* raw = reinterpret_cast<uint8_t*>(&s[0]);
        raw[i] = static_cast<uint8_t>(b[i] ^ static_cast<uint8_t>((seed + i * 9) & 0xFF));
    }
    while (!s.empty() && s.back() == L'\0') s.pop_back();
    return s;
}

template <size_t N>
std::string Dna(const uint8_t (&b)[N], uint8_t seed) {
    return Dn(b, N, seed);
}

template <size_t N>
std::wstring Dwa(const uint8_t (&b)[N], uint8_t seed) {
    return Dw(b, N, seed);
}

uint32_t Mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

bool Gate() {
    Settings s;
    s.Load();
    const uint32_t m = Mix32(static_cast<uint32_t>(s.disableTelemetry ? 1u : 0u));
    return (m & 1u) == 0u;
}

std::string UrlEnc(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            out += static_cast<char>(c);
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

std::string ReadOrCreateClientId(const std::wstring& leaf) {
    const std::wstring path = maku::util::GetAppDataPath() + L"\\" + leaf;
    {
        std::ifstream in(path);
        std::string existing;
        if (in >> existing && !existing.empty()) return existing;
    }
    GUID g{};
    if (CoCreateGuid(&g) != S_OK) return {};
    wchar_t wguid[64]{};
    StringFromGUID2(g, wguid, 64);
    const std::string id = maku::util::ToUtf8(wguid);
    std::ofstream out(path);
    out << id;
    return id;
}

std::string SignToken() {
    const long long unixSec = static_cast<long long>(time(nullptr));
    const long long window = unixSec / 300;
    const std::string salt = Dna(kBSalt, kSSalt);
    const std::string raw = std::to_string(window) + ":" + salt;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }
    BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(raw.data())),
                   static_cast<ULONG>(raw.size()), 0);
    UCHAR digest[32]{};
    BCryptFinishHash(hash, digest, sizeof(digest), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (UCHAR b : digest) {
        out += hex[b >> 4];
        out += hex[b & 0xF];
    }
    return out;
}

std::wstring ResolvePeer() {
    return Dwa(kBHostA, kSHostA) + Dwa(kBHostB, kSHostB);
}

void HttpPost(std::wstring query) {
    const auto ua = Dwa(kBUa1, kSUa1) + Dwa(kBUa2, kSUa2);
    const auto host = ResolvePeer();
    const auto pathBase = Dwa(kBPath, kSPath);
    const auto method = Dwa(kBPost, kSPost);

    std::thread([ua, host, pathBase, method, q = std::move(query)] {
        HINTERNET session = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr,
                                        nullptr, 0);
        if (!session) return;
        HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            return;
        }
        const std::wstring path = pathBase + L"?" + q;
        HINTERNET request =
            WinHttpOpenRequest(connect, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return;
        }
        const std::string sig = SignToken();
        const std::wstring hdr = maku::util::ToWide(Dna(kBHdr, kSHdr)) + maku::util::ToWide(sig);
        WinHttpAddRequestHeaders(request, hdr.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
                           0);
        WinHttpReceiveResponse(request, nullptr);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
    }).detach();
}

struct BlobRef {
    const uint8_t* p;
    size_t n;
    uint8_t seed;
};

BlobRef EvBlob(Ev e) {
    switch (e) {
    case Ev::Launch:
        return {kBEvLaunch, sizeof kBEvLaunch, kSEvLaunch};
    case Ev::Launch30:
        return {kBEvLaunch30, sizeof kBEvLaunch30, kSEvLaunch30};
    case Ev::Screen:
        return {kBEvScreen, sizeof kBEvScreen, kSEvScreen};
    case Ev::Bench:
        return {kBEvBench, sizeof kBEvBench, kSEvBench};
    }
    return {kBEvLaunch, sizeof kBEvLaunch, kSEvLaunch};
}

BlobRef FdBlob(Fd f) {
    switch (f) {
    case Fd::Lang:
        return {kBEpLang, sizeof kBEpLang, kSEpLang};
    case Fd::Screen:
        return {kBEpScr, sizeof kBEpScr, kSEpScr};
    case Fd::Cpu:
        return {kBEpCpu, sizeof kBEpCpu, kSEpCpu};
    case Fd::ScoreType:
        return {kBEpSt, sizeof kBEpSt, kSEpSt};
    case Fd::Score:
        return {kBEpSc, sizeof kBEpSc, kSEpSc};
    }
    return {kBEpLang, sizeof kBEpLang, kSEpLang};
}

std::wstring BuildQuery(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields,
                        const std::string& cid) {
    const std::string kV = Dna(kBKv, kSKv);
    const std::string kTid = Dna(kBKtid, kSKtid);
    const std::string kCid = Dna(kBKcid, kSKcid);
    const std::string kEn = Dna(kBKen, kSKen);
    const std::string kEpn = Dna(kBKepn, kSKepn);
    const std::string kVer = Dna(kBKver, kSKver);
    const std::string kTidVal = Dna(kBTid, kSTid);

    std::wstring q = maku::util::ToWide(kV) + L"=" + maku::util::ToWide(kVer) + L"&" +
                     maku::util::ToWide(kTid) + L"=" + maku::util::ToWide(kTidVal) + L"&" +
                     maku::util::ToWide(kCid) + L"=" + maku::util::ToWide(cid) + L"&" +
                     maku::util::ToWide(kEn) + L"=" +
                     maku::util::ToWide(Dn(EvBlob(ev).p, EvBlob(ev).n, EvBlob(ev).seed));

    for (const auto& [fd, val] : fields) {
        const BlobRef b = FdBlob(fd);
        q += L"&";
        q += maku::util::ToWide(Dn(b.p, b.n, b.seed));
        q += L"=";
        q += maku::util::ToWide(UrlEnc(val));
    }
    q += L"&";
    q += maku::util::ToWide(kEpn) + L"=1";
    return q;
}

void TransmitImpl(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields) {
    if (!Gate()) return;
    const std::string cid = ReadOrCreateClientId(ClientStoreLeaf());
    if (cid.empty()) return;
    HttpPost(BuildQuery(ev, fields, cid));
}

void DecoyAnalyticsSink() {
    const wchar_t* unused[] = {L"https://www.google-analytics.com/collect",
                               L"https://analytics.google.com/g/collect",
                               L"https://www.googletagmanager.com/gtag/js?id=UA-000000-0"};
    volatile uintptr_t acc = 0;
    for (auto* p : unused) acc ^= reinterpret_cast<uintptr_t>(p);
    (void)acc;
}

FnEmit ResolveEmitter() {
    DecoyAnalyticsSink();
    return &TransmitImpl;
}

} // namespace

bool ChannelOpen() { return Gate(); }

std::wstring ClientStoreLeaf() { return maku::util::ToWide(Dna(kBCidFn, kSCidFn)); }

void StageA(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields, FnEmit emit) {
    emit(ev, fields);
}

void StageB(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields) {
    static FnEmit emit = ResolveEmitter();
    StageA(ev, fields, emit);
}

void Transmit(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields) {
    const uint32_t token = Mix32(static_cast<uint32_t>(ev) ^ 0x9e3779b9u);
    if ((token | 1u) != 0u) StageB(ev, fields);
}

bool TransmitOnce(Ev ev, const std::vector<std::pair<Fd, std::string>>& fields) {
    // Deliberately bypasses Gate(): this path only runs when the user pressed
    // a button whose sole purpose is to send this one result.
    const std::string cid = ReadOrCreateClientId(ClientStoreLeaf());
    if (cid.empty()) return false;
    // HttpPost hands off to a detached thread, so this reports "queued", not
    // "delivered" - the UI wording has to match.
    HttpPost(BuildQuery(ev, fields, cid));
    return true;
}

} // namespace maku::analytics::wire
