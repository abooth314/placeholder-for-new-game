#pragma once
#include "audio/synth.hpp"
#include "sim/world.hpp"
#include "view/camera.hpp"
#include <memory>
namespace curved {
struct AudioStats {
    bool device = false;
    unsigned voices = 0, peak_voices = 0;
    std::uint64_t played = 0, dropped = 0, samples = 0;
    std::size_t cached_bytes = 0;
    std::string error, driver;
};
class Audio {
  public:
    explicit Audio(const Catalog &catalog);
    ~Audio();
    void events(const std::vector<BattleEvent> &events, const h3::Space &space, const Camera &camera);
    void pump(const h3::Space &space, const Camera &camera);
    const AudioStats &stats() const;
    float master = .45f, effects = .7f, ambience = .16f;
    bool muted = false;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace curved
