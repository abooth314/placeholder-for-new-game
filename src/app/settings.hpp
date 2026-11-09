#pragma once
#include "app/input.hpp"
#include <filesystem>
#include <string>
namespace curved {
struct Settings {
    float master = .45f, effects = .7f, ambience = .16f;
    bool mute = false, detailed = true, health_bars = true, vsync = true;
    int width = 1440, height = 900, map = 0, seed = 8147;
    int population = 160;
    double reinforce_rate = 1;
    bool combat = true;
    double curvature = 48, speed = 1;
    std::string error;
    void load(const std::filesystem::path &path, Input &input);
    void save(const std::filesystem::path &path, const Input &input) const;
};
} // namespace curved
