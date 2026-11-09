#pragma once
#include "content/catalog.hpp"
#include <vector>
namespace curved {
struct Vertex {
    float x, y, z, nx, ny, nz, r, g, b, tint, motion;
};
std::vector<Vertex> evaluate_mesh(const VisualRecipe &recipe);
} // namespace curved
