#include "core/EmbeddedStore.h"
#include "core/StringUtil.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <windows.h>

#if defined(MAKUTWEAKER_ONEFILE) && MAKUTWEAKER_ONEFILE
extern std::uint8_t EmbeddedPayloadData[];
extern std::size_t EmbeddedPayloadSize;
#endif

namespace maku::embed {
namespace {

namespace fs = std::filesystem;

constexpr char kMagic[] = {'M', 'A', 'K', 'U', '\x01'};

std::wstring g_dataRoot;
bool g_ready{};

uint32_t Crc32(const uint8_t* data, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & ~((crc & 1u) - 1u));
    }
    return ~crc;
}

bool ReadU32(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
    if (static_cast<size_t>(end - p) < 4) return false;
    out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return true;
}

#if defined(MAKUTWEAKER_ONEFILE) && MAKUTWEAKER_ONEFILE

bool ExtractPayload(const fs::path& root) {
    const uint8_t* p = EmbeddedPayloadData;
    const uint8_t* end = p + EmbeddedPayloadSize;
    if (static_cast<size_t>(end - p) < 8 || memcmp(p, kMagic, 5) != 0) return false;
    p += 5;

    uint32_t count = 0;
    if (!ReadU32(p, end, count)) return false;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pathLen = 0;
        uint32_t dataLen = 0;
        if (!ReadU32(p, end, pathLen) || static_cast<size_t>(end - p) < pathLen) return false;
        std::string rel(reinterpret_cast<const char*>(p), pathLen);
        p += pathLen;
        if (!ReadU32(p, end, dataLen) || static_cast<size_t>(end - p) < dataLen) return false;

        fs::path outPath = root / fs::path(util::ToWide(rel));
        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);
        std::ofstream out(outPath, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(dataLen));
        p += dataLen;
    }
    return p == end;
}

std::wstring OneFileRoot() {
    const uint32_t sig = Crc32(EmbeddedPayloadData, EmbeddedPayloadSize);
    wchar_t stamp[32]{};
    swprintf_s(stamp, L"v_%08x", sig);

    const std::wstring base = util::GetAppDataPath() + L"\\runtime\\" + stamp;
    const fs::path marker = fs::path(base) / L".ok";
    if (!fs::exists(marker)) {
        std::error_code ec;
        fs::remove_all(base, ec);
        fs::create_directories(base, ec);
        if (!ExtractPayload(base)) return util::GetExeDirectory();
        std::ofstream ok(marker);
        ok << "1";
    }
    return base;
}

#endif

} // namespace

void Init() {
    if (g_ready) return;
    const std::wstring beside = util::GetExeDirectory();
    const std::wstring probe = beside + L"\\loc";
    if (GetFileAttributesW(probe.c_str()) != INVALID_FILE_ATTRIBUTES) {
        g_dataRoot = beside;
    }
#if defined(MAKUTWEAKER_ONEFILE) && MAKUTWEAKER_ONEFILE
    else {
        g_dataRoot = OneFileRoot();
    }
#else
    else {
        g_dataRoot = beside;
    }
#endif
    g_ready = true;
}

std::wstring DataRoot() {
    if (!g_ready) Init();
    return g_dataRoot;
}

} // namespace maku::embed
