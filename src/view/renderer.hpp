#pragma once
#include "sim/world.hpp"
#include "view/camera.hpp"
#include <filesystem>
#include <memory>
#include <string>
namespace curved {
struct RenderStats {
    unsigned draw_calls = 0, triangles = 0, visible_actors = 0, models = 0, buffers = 0, textures = 0;
    std::size_t mesh_bytes = 0, instance_bytes = 0;
    double submit_ms = 0;
    std::string gpu, version;
};
class Renderer {
  public:
    Renderer(const Catalog &catalog, const Terrain &terrain);
    ~Renderer();
    void set_terrain(const Terrain &terrain);
    void draw(const World &world, const Camera &camera, int width, int height, bool detailed = true);
    void screenshot(const std::filesystem::path &path, int width, int height) const;
    unsigned atlas_texture() const;
    const RenderStats &stats() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace curved
