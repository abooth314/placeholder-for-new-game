#pragma once
#include "math/hyperbolic.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace curved {
struct MapRecipe {
    std::string id, name, description;
    double radius = 70, relief = 3;
    int rivers = 0;
    std::array<float, 3> grass{.37f, .51f, .24f}, water{.15f, .43f, .59f};
};
struct SurfaceSample {
    double height;
    bool walkable, water, bridge, road;
    std::array<float, 3> color;
};
struct NavEdge {
    int node;
    double length;
};
struct NavNode {
    h3::Vec base, point;
    SurfaceSample sample;
    std::vector<NavEdge> edges;
};
struct Triangle {
    std::array<int, 3> nodes;
};
struct SurfaceMove {
    h3::Vec point;
    double travelled;
};
class Terrain {
  public:
    Terrain(MapRecipe recipe, std::uint32_t seed, double radius = 48, int grid_size = 73);
    const h3::Space &space() const {
        return space_;
    }
    const MapRecipe &recipe() const {
        return recipe_;
    }
    std::uint32_t seed() const {
        return seed_;
    }
    SurfaceSample sample(h3::Vec base) const;
    h3::Vec surface(h3::Vec p) const;
    bool legal(h3::Vec p) const;
    bool segment_legal(h3::Vec a, h3::Vec b) const;
    double surface_length(h3::Vec a, h3::Vec b, int panels = 8) const;
    SurfaceMove advance(h3::Vec p, h3::Vec goal, double metres) const;
    bool line_of_sight(h3::Vec a, h3::Vec b) const;
    int nearest(h3::Vec p, int component = -1) const;
    int component(int node) const {
        return components_.at(node);
    }
    std::vector<int> path(int from, int to) const;
    double route_length(const std::vector<int> &route) const;
    const std::vector<NavNode> &nodes() const {
        return nodes_;
    }
    const std::vector<Triangle> &triangles() const {
        return triangles_;
    }
    const std::array<int, 4> &homes() const {
        return homes_;
    }
    double extent() const {
        return extent_;
    }
    int grid_size() const {
        return grid_;
    }

  private:
    SurfaceSample field(h3::Vec base, bool slopes) const;
    h3::Space space_;
    MapRecipe recipe_;
    std::uint32_t seed_;
    int grid_;
    double extent_, phase_;
    std::vector<NavNode> nodes_;
    std::vector<int> cells_;
    std::vector<Triangle> triangles_;
    std::vector<int> components_;
    std::array<int, 4> homes_{};
};
} // namespace curved
