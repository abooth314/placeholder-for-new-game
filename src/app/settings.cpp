#include "app/settings.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace curved {
void Settings::load(const std::filesystem::path &path, Input &input) {
    if (!std::filesystem::exists(path))
        return;
    try {
        if (std::filesystem::file_size(path) > 16384)
            throw std::runtime_error("Settings file too large");
        std::ifstream file(path);
        nlohmann::json j;
        file >> j;
        if (j.at("schema") != 1)
            throw std::runtime_error("Unsupported settings version");
        auto num = [&](const char *key, double fallback, double low, double high) {
            const double n = j.value(key, fallback);
            if (!std::isfinite(n) || n < low || n > high)
                throw std::runtime_error(std::string("Invalid setting ") + key);
            return n;
        };
        Settings candidate;
        Input bindings = input;
        candidate.master = static_cast<float>(num("master", .45, 0, 1));
        candidate.effects = static_cast<float>(num("effects", .7, 0, 1));
        candidate.ambience = static_cast<float>(num("ambience", .16, 0, 1));
        candidate.curvature = num("curvature", 48, 30, 100);
        candidate.speed = num("speed", 1, .5, 4);
        candidate.population = static_cast<int>(num("population", 160, 40, 384));
        candidate.reinforce_rate = num("reinforce_rate", 1, 0, 3);
        candidate.combat = j.value("combat", true);
        candidate.width = static_cast<int>(num("width", 1440, 960, 3840));
        candidate.height = static_cast<int>(num("height", 900, 640, 2160));
        candidate.seed = static_cast<int>(num("seed", 8147, 1, 99999999));
        candidate.map = static_cast<int>(num("map", 0, 0, 11));
        candidate.mute = j.value("mute", false);
        candidate.detailed = j.value("detailed", true);
        candidate.health_bars = j.value("health_bars", true);
        candidate.vsync = j.value("vsync", true);
        if (j.contains("bindings")) {
            auto a = j.at("bindings").get<std::vector<int>>();
            if (a.size() != static_cast<unsigned>(Action::Count))
                throw std::runtime_error("Invalid action binding count");
            for (unsigned i = 0; i < a.size(); ++i)
                if (!bindings.set_binding(static_cast<Action>(i), a[i]))
                    throw std::runtime_error("Invalid action binding");
        }
        *this = candidate;
        input = bindings;
    } catch (const std::exception &e) {
        error = e.what();
    }
}
void Settings::save(const std::filesystem::path &path, const Input &input) const {
    nlohmann::json j = {{"schema", 1},
                        {"population", population},
                        {"reinforce_rate", reinforce_rate},
                        {"combat", combat},
                        {"master", master},
                        {"effects", effects},
                        {"ambience", ambience},
                        {"mute", mute},
                        {"curvature", curvature},
                        {"speed", speed},
                        {"width", std::clamp(width, 960, 3840)},
                        {"height", std::clamp(height, 640, 2160)},
                        {"seed", seed},
                        {"map", map},
                        {"detailed", detailed},
                        {"health_bars", health_bars},
                        {"vsync", vsync}};
    std::vector<int> keys;
    for (unsigned i = 0; i < static_cast<unsigned>(Action::Count); ++i)
        keys.push_back(input.binding(static_cast<Action>(i)));
    j["bindings"] = keys;
    std::filesystem::create_directories(path.parent_path());
    auto temp = path;
    temp += ".tmp";
    {
        std::ofstream f(temp);
        f << j.dump(2) << '\n';
        if (!f)
            throw std::runtime_error("Settings write failed");
    }
    std::error_code error;
    std::filesystem::rename(temp, path, error);
    if (error) {
        std::filesystem::copy_file(temp, path, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(temp);
    }
}
} // namespace curved
