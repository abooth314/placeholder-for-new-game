#include "audio/audio.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
namespace curved {
struct Audio::Impl {
    struct Voice {
        const std::vector<float> *buffer;
        h3::Vec position;
        double cursor = 0, rate = 1;
        int priority = 0;
    };
    const Catalog &catalog;
    SDL_AudioStream *stream = nullptr;
    std::map<std::string, std::vector<float>> buffers;
    std::map<std::string, std::uint64_t> last;
    std::vector<Voice> voices;
    AudioStats stats;
    std::uint32_t random = 9137;
    double low = 0, clock = 0;
    explicit Impl(const Catalog &c) : catalog(c) {
        voices.reserve(24);
    }
    double noise() {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random / 2147483648.0 - 1;
    }
    ~Impl() {
        if (stream)
            SDL_DestroyAudioStream(stream);
    }
};
Audio::Audio(const Catalog &catalog) : impl_(std::make_unique<Impl>(catalog)) {
    auto &a = *impl_;
    unsigned seed = 811;
    for (auto &[id, r] : catalog.sounds) {
        auto samples = synthesize(r, seed++);
        a.stats.cached_bytes += samples.size() * sizeof(float);
        a.buffers.emplace(id, std::move(samples));
    }
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, audio_rate};
    auto open = [&]() {
        a.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        return a.stream && SDL_ResumeAudioStreamDevice(a.stream);
    };
    a.stats.device = open();
#ifdef _WIN32
    if (!a.stats.device && !SDL_getenv("SDL_AUDIO_DRIVER")) {
        if (a.stream) {
            SDL_DestroyAudioStream(a.stream);
            a.stream = nullptr;
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "directsound");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO))
            a.stats.device = open();
    }
#endif
    if (a.stats.device) {
        const char *driver = SDL_GetCurrentAudioDriver();
        a.stats.driver = driver ? driver : "unknown";
    } else
        a.stats.error = SDL_GetError();
}
Audio::~Audio() = default;
void Audio::events(const std::vector<BattleEvent> &events, const h3::Space &h, const Camera &camera) {
    auto &a = *impl_;
    if (muted || !a.stats.device)
        return;
    for (auto &e : events) {
        auto it = a.buffers.find(e.sound);
        if (it == a.buffers.end())
            continue;
        if (h.dist(camera.position, e.position) > 150)
            continue;
        auto &last = a.last[e.sound];
        const auto now = SDL_GetTicks();
        if (last && now - last < 35) {
            ++a.stats.dropped;
            continue;
        }
        last = now;
        const int priority = a.catalog.sounds.at(e.sound).priority;
        if (a.voices.size() >= 24) {
            auto low = std::min_element(a.voices.begin(), a.voices.end(),
                                        [](auto &x, auto &y) { return x.priority < y.priority; });
            if (low->priority >= priority) {
                ++a.stats.dropped;
                continue;
            }
            a.voices.erase(low);
            ++a.stats.dropped;
        }
        a.voices.push_back({&it->second, e.position, 0, .94 + (a.noise() + 1) * .06, priority});
        ++a.stats.played;
        a.stats.peak_voices = std::max(a.stats.peak_voices, static_cast<unsigned>(a.voices.size()));
    }
}
void Audio::pump(const h3::Space &h, const Camera &camera) {
    auto &a = *impl_;
    if (!a.stream)
        return;
    const int queued = SDL_GetAudioStreamQueued(a.stream);
    if (queued < 0) {
        a.stats.error = SDL_GetError();
        return;
    }
    int missing = std::max(0, 4096 * static_cast<int>(sizeof(float)) - queued);
    while (missing > 0) {
        constexpr int frames = 512;
        std::array<float, frames * 2> out{};
        for (auto &v : a.voices) {
            const double distance = h.dist(camera.position, v.position);
            const auto tangent = h.log(camera.position, v.position);
            const double length = h3::norm(tangent);
            const double pan = std::clamp(h3::dot(tangent, camera.right) / std::max(.001, length), -.95, .95);
            const double volume = std::pow(std::max(0.0, 1 - distance / 150), 1.5) * effects;
            const float left = static_cast<float>(volume * std::sqrt((1 - pan) * .5)),
                        right = static_cast<float>(volume * std::sqrt((1 + pan) * .5));
            for (int i = 0; i < frames; ++i) {
                const auto at = static_cast<std::size_t>(v.cursor);
                if (at >= v.buffer->size())
                    break;
                out[i * 2] += (*v.buffer)[at] * left;
                out[i * 2 + 1] += (*v.buffer)[at] * right;
                v.cursor += v.rate;
            }
        }
        std::erase_if(a.voices, [](auto &v) { return v.cursor >= v.buffer->size(); });
        for (int i = 0; i < frames; ++i) {
            a.low = .995 * a.low + .005 * a.noise();
            const double bird = std::fmod(a.clock, 13.0);
            const double chirp = bird < .22 ? std::sin(2 * std::numbers::pi * (1700 + bird * 3500) * bird) *
                                                  std::sin(bird / .22 * std::numbers::pi) * .025
                                            : 0;
            const float ambient = static_cast<float>((a.low * .14 + chirp) * ambience);
            out[i * 2] = muted ? 0 : std::tanh((out[i * 2] + ambient) * master);
            out[i * 2 + 1] = muted ? 0 : std::tanh((out[i * 2 + 1] + ambient) * master);
            a.clock += 1.0 / audio_rate;
        }
        if (!SDL_PutAudioStreamData(a.stream, out.data(), static_cast<int>(sizeof(out)))) {
            a.stats.error = SDL_GetError();
            break;
        }
        a.stats.samples += frames;
        missing -= static_cast<int>(sizeof(out));
    }
    a.stats.voices = static_cast<unsigned>(a.voices.size());
}
const AudioStats &Audio::stats() const {
    return impl_->stats;
}
} // namespace curved
