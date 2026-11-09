#include "audio/synth.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <stdexcept>
namespace curved {
std::vector<float> synthesize(const SoundRecipe &r, std::uint32_t seed) {
    std::vector<float> samples(static_cast<std::size_t>(std::ceil(r.duration * audio_rate)));
    double phase = 0, low = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / audio_rate,
                     f = std::pow(r.end_frequency / r.frequency, t / r.duration) * r.frequency;
        phase += 2 * std::numbers::pi * f / audio_rate;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        const double noise = seed / 2147483648.0 - 1;
        low = .88 * low + .12 * noise;
        const double envelope =
            std::min(1.0, t / .004) * std::exp(-5 * t / r.duration) * std::min(1.0, (r.duration - t) / .02);
        samples[i] = static_cast<float>(std::clamp(
            (r.tone * (std::sin(phase) + .18 * std::sin(phase * 2.71)) + r.noise * (.6 * noise + .8 * low)) *
                envelope * r.gain,
            -.95, .95));
    }
    return samples;
}
void write_audio_demo(const Catalog &catalog, const std::filesystem::path &path) {
    std::vector<std::int16_t> data;
    for (auto &[id, r] : catalog.sounds) {
        auto samples = synthesize(r, static_cast<std::uint32_t>(data.size() + 9137));
        for (float x : samples)
            data.push_back(static_cast<std::int16_t>(x * 26000));
        data.insert(data.end(), audio_rate / 3, 0);
        (void)id;
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    auto word = [&](std::uint32_t v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            f.put(static_cast<char>((v >> (i * 8)) & 255));
    };
    const auto bytes = static_cast<std::uint32_t>(data.size() * 2);
    f.write("RIFF", 4);
    word(36 + bytes, 4);
    f.write("WAVEfmt ", 8);
    word(16, 4);
    word(1, 2);
    word(1, 2);
    word(audio_rate, 4);
    word(audio_rate * 2, 4);
    word(2, 2);
    word(16, 2);
    f.write("data", 4);
    word(bytes, 4);
    for (auto sample : data)
        word(static_cast<std::uint16_t>(sample), 2);
    if (!f)
        throw std::runtime_error("Audio demo write failed");
}
} // namespace curved
