#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace maku::embed {

struct Buffer {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

/// Virtual path with forward slashes, e.g. "loc/en.json", "assets/icons/MakuTweaker.ico".
std::string NormalizePath(std::string_view path);

bool Has(std::string_view virtualPath);
std::optional<Buffer> Get(std::string_view virtualPath);

/// UTF-8 text (loc JSON).
inline std::optional<std::string_view> GetUtf8(const std::string_view virtualPath) {
    const auto buf = Get(virtualPath);
    if (!buf || !buf->data || buf->size == 0) return std::nullopt;
    return std::string_view(reinterpret_cast<const char*>(buf->data), buf->size);
}

namespace detail {
struct Entry {
    const char* path;
    int resourceId;
};
extern const Entry kEntries[];
extern const std::size_t kEntryCount;
} // namespace detail

} // namespace maku::embed
