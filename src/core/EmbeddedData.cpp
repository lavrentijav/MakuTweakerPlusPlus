#include "core/EmbeddedData.h"

#include <windows.h>

#include <algorithm>
#include <cctype>

namespace maku::embed {
namespace {

int ComparePathIgnoreCase(const std::string_view a, const std::string_view b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        const int diff = std::tolower(ca) - std::tolower(cb);
        if (diff != 0) return diff;
    }
    return 0;
}

#ifdef MAKU_EMBED_DATA
std::optional<Buffer> LoadResourceById(const int id) {
    const HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return std::nullopt;
    HGLOBAL loaded = LoadResource(mod, res);
    if (!loaded) return std::nullopt;
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(mod, res);
    if (!data || size == 0) return std::nullopt;
    return Buffer{static_cast<const std::uint8_t*>(data), static_cast<std::size_t>(size)};
}
#endif

} // namespace

std::string NormalizePath(const std::string_view path) {
    std::string out(path);
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    while (!out.empty() && out.front() == '/') out.erase(out.begin());
    return out;
}

bool Has(const std::string_view virtualPath) {
#ifdef MAKU_EMBED_DATA
    const std::string key = NormalizePath(virtualPath);
    for (std::size_t i = 0; i < detail::kEntryCount; ++i) {
        if (ComparePathIgnoreCase(key, detail::kEntries[i].path) == 0) return true;
    }
#endif
    return false;
}

std::optional<Buffer> Get(const std::string_view virtualPath) {
#ifdef MAKU_EMBED_DATA
    const std::string key = NormalizePath(virtualPath);
    for (std::size_t i = 0; i < detail::kEntryCount; ++i) {
        if (ComparePathIgnoreCase(key, detail::kEntries[i].path) == 0)
            return LoadResourceById(detail::kEntries[i].resourceId);
    }
#endif
    return std::nullopt;
}

} // namespace maku::embed
