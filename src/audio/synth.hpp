#pragma once
#include "content/catalog.hpp"
#include <filesystem>
#include <map>
#include <vector>
namespace curved {
inline constexpr int audio_rate = 48000;
std::vector<float> synthesize(const SoundRecipe &recipe, std::uint32_t seed);
void write_audio_demo(const Catalog &catalog, const std::filesystem::path &path);
} // namespace curved
