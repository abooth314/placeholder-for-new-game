#include "assets/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace curved {
namespace {
struct V {
    float x, y, z;
    V operator+(V b) const {
        return {x + b.x, y + b.y, z + b.z};
    }
    V operator-(V b) const {
        return {x - b.x, y - b.y, z - b.z};
    }
    V operator*(float s) const {
        return {x * s, y * s, z * s};
    }
};
float length(V a) {
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}
V unit(V a) {
    const auto n = length(a);
    return n > 1e-8f ? a * (1 / n) : V{0, 1, 0};
}
V cross(V a, V b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
class Builder {
  public:
    std::vector<Vertex> vertices;
    void triangle(V a, V b, V c, const PartRecipe &p, int level = 0) {
        const float ab = length(b - a), bc = length(c - b), ca = length(a - c);
        if (std::max({ab, bc, ca}) > 3.2f && level < 5) {
            if (ab >= bc && ab >= ca) {
                const auto m = (a + b) * .5f;
                triangle(a, m, c, p, level + 1);
                triangle(m, b, c, p, level + 1);
            } else if (bc >= ca) {
                const auto m = (b + c) * .5f;
                triangle(a, b, m, p, level + 1);
                triangle(a, m, c, p, level + 1);
            } else {
                const auto m = (c + a) * .5f;
                triangle(a, b, m, p, level + 1);
                triangle(m, b, c, p, level + 1);
            }
            return;
        }
        const auto n = unit(cross(b - a, c - a));
        for (auto v : {a, b, c})
            vertices.push_back(
                {v.x, v.y, v.z, n.x, n.y, n.z, p.color[0], p.color[1], p.color[2], p.tint, p.motion});
    }
    void quad(V a, V b, V c, V d, const PartRecipe &p) {
        triangle(a, b, c, p);
        triangle(a, c, d, p);
    }
    void part(const PartRecipe &p) {
        const V center{p.center[0], p.center[1], p.center[2]};
        const float sx = p.size[0], sy = p.size[1], sz = p.size[2];
        const float tau = 2 * std::numbers::pi_v<float>;
        if (p.primitive == Primitive::Box) {
            const V v[] = {{-sx / 2, -sy / 2, -sz / 2}, {sx / 2, -sy / 2, -sz / 2}, {sx / 2, sy / 2, -sz / 2},
                           {-sx / 2, sy / 2, -sz / 2},  {-sx / 2, -sy / 2, sz / 2}, {sx / 2, -sy / 2, sz / 2},
                           {sx / 2, sy / 2, sz / 2},    {-sx / 2, sy / 2, sz / 2}};
            const int faces[][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3},
                                    {1, 2, 6, 5}, {3, 7, 6, 2}, {0, 1, 5, 4}};
            for (auto &f : faces)
                quad(center + v[f[0]], center + v[f[1]], center + v[f[2]], center + v[f[3]], p);
        } else if (p.primitive == Primitive::Roof) {
            const V a = center + V{-sx / 2, 0, -sz / 2}, b = center + V{sx / 2, 0, -sz / 2},
                    c = center + V{0, sy, -sz / 2}, d = center + V{-sx / 2, 0, sz / 2},
                    e = center + V{sx / 2, 0, sz / 2}, f = center + V{0, sy, sz / 2};
            triangle(a, c, b, p);
            triangle(d, e, f, p);
            quad(a, d, f, c, p);
            quad(b, c, f, e, p);
        } else if (p.primitive == Primitive::Cone) {
            for (int j = 0; j < p.sides; ++j) {
                const float a = tau * j / p.sides, b = tau * (j + 1) / p.sides;
                const V v = center + V{sx * std::cos(a), 0, sx * std::sin(a)},
                        w = center + V{sx * std::cos(b), 0, sx * std::sin(b)},
                        u = center + V{sz * std::cos(a), sy, sz * std::sin(a)},
                        t = center + V{sz * std::cos(b), sy, sz * std::sin(b)};
                quad(v, u, t, w, p);
                triangle(center, v, w, p);
                triangle(center + V{0, sy, 0}, t, u, p);
            }
        } else if (p.primitive == Primitive::Beam) {
            const V end{p.end[0], p.end[1], p.end[2]},
                direction = unit(end - center),
                east = unit(cross(direction, std::abs(direction.y) > .95f ? V{1, 0, 0} : V{0, 1, 0})),
                north = cross(direction, east);
            auto ring = [&](V origin, float a) {
                return origin + east * (sx * std::cos(a)) + north * (sx * std::sin(a));
            };
            for (int j = 0; j < p.sides; ++j) {
                const float a = tau * j / p.sides, b = tau * (j + 1) / p.sides;
                quad(ring(center, a), ring(center, b), ring(end, b), ring(end, a), p);
                triangle(end, ring(end, a), ring(end, b), p);
            }
        } else {
            auto pt = [&](float a, float v) {
                return center +
                       V{sx * std::cos(a) * std::sin(v), sy * std::cos(v), sz * std::sin(a) * std::sin(v)};
            };
            for (int ring = 0; ring < 4; ++ring)
                for (int j = 0; j < p.sides; ++j) {
                    const float a = tau * j / p.sides, b = tau * (j + 1) / p.sides,
                                u = std::numbers::pi_v<float> * ring / 4,
                                v = std::numbers::pi_v<float> * (ring + 1) / 4;
                    quad(pt(a, u), pt(b, u), pt(b, v), pt(a, v), p);
                }
        }
    }
};
} // namespace
std::vector<Vertex> evaluate_mesh(const VisualRecipe &recipe) {
    Builder b;
    for (auto &p : recipe.parts)
        b.part(p);
    if (b.vertices.size() > 120000)
        throw std::runtime_error(recipe.id + ": generated mesh vertex budget exceeded");
    return std::move(b.vertices);
}
} // namespace curved
