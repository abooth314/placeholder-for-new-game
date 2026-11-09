#pragma once
#include <filesystem>
#include <string>
namespace curved {
// SDL and Lua accept UTF-8 narrow paths; path::string() uses the Windows native
// narrow code page and can transliterate characters such as Greek Omega.
inline std::string utf8_path(const std::filesystem::path &path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}
} // namespace curved
