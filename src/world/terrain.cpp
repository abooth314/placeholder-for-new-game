#include "world/terrain.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <queue>
#include <stdexcept>

namespace curved {
Terrain::Terrain(MapRecipe recipe, std::uint32_t seed, double radius, int grid)
    : space_(radius), recipe_(std::move(recipe)), seed_(seed), grid_(grid),
      extent_(std::tanh(recipe_.radius / radius)), phase_((seed % 10007) * .000628) {
    if (grid_ < 17 || grid_ > 129 || recipe_.radius / radius > 3.5)
        throw std::invalid_argument("Terrain sampling outside supported domain");
    cells_.assign(grid_ * grid_, -1);
    for (int z = 0; z < grid_; ++z)
        for (int x = 0; x < grid_; ++x) {
            const double kx = extent_ * (2.0 * x / (grid_ - 1) - 1),
                         kz = extent_ * (2.0 * z / (grid_ - 1) - 1);
            if (kx * kx + kz * kz > extent_ * extent_)
                continue;
            const auto b = space_.from_klein(kx, kz);
            const auto s = sample(b);
            cells_[z * grid_ + x] = static_cast<int>(nodes_.size());
            nodes_.push_back({b, space_.lift(b, s.height), s, {}});
        }
    for (int z = 0; z < grid_ - 1; ++z)
        for (int x = 0; x < grid_ - 1; ++x) {
            const int a = cells_[z * grid_ + x], b = cells_[z * grid_ + x + 1],
                      c = cells_[(z + 1) * grid_ + x], d = cells_[(z + 1) * grid_ + x + 1];
            if (a >= 0 && b >= 0 && c >= 0)
                triangles_.push_back({{a, c, b}});
            if (b >= 0 && c >= 0 && d >= 0)
                triangles_.push_back({{b, c, d}});
        }
    const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {-1, 1}};
    for (int z = 0; z < grid_; ++z)
        for (int x = 0; x < grid_; ++x) {
            const int a = cells_[z * grid_ + x];
            if (a < 0 || !nodes_[a].sample.walkable)
                continue;
            for (auto &dir : directions) {
                const int nx = x + dir[0], nz = z + dir[1];
                if (nx < 0 || nz < 0 || nx >= grid_ || nz >= grid_)
                    continue;
                const int b = cells_[nz * grid_ + nx];
                if (b < 0 || !nodes_[b].sample.walkable || !segment_legal(nodes_[a].base, nodes_[b].base))
                    continue;
                const double len = surface_length(nodes_[a].base, nodes_[b].base);
                nodes_[a].edges.push_back({b, len});
                nodes_[b].edges.push_back({a, len});
            }
        }
    components_.assign(nodes_.size(), -1);
    int component_id = 0;
    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
        if (nodes_[i].sample.walkable && !nodes_[i].edges.empty() && components_[i] < 0) {
            std::vector<int> pending{i};
            components_[i] = component_id;
            for (std::size_t j = 0; j < pending.size(); ++j)
                for (auto edge : nodes_[pending[j]].edges)
                    if (components_[edge.node] < 0) {
                        components_[edge.node] = component_id;
                        pending.push_back(edge.node);
                    }
            ++component_id;
        }
    for (int i = 0; i < 4; ++i)
        homes_[i] = nearest(space_.point(recipe_.radius * .66, std::numbers::pi * (.25 + i * .5)));
    for (int i = 1; i < 4; ++i)
        if (path(homes_[0], homes_[i]).empty())
            throw std::runtime_error("Map has disconnected fortress approaches; no implicit terrain repair");
}
SurfaceSample Terrain::field(h3::Vec b, bool slopes) const {
    b = space_.ground(b);
    const double L = space_.radius();
    // Fermi coordinates of H2: du^2 + cosh^2(u/L) dv^2. Never a distance substitute.
    const double u = L * std::asinh(b.x), v = L * std::atanh(std::clamp(b.z / b.t, -.999999, .999999));
    const double radial = space_.dist(h3::origin(), b);
    const double hill =
        recipe_.relief *
        (.55 + .24 * std::sin(u * .09 + phase_) * std::cos(v * .07 - phase_) + .18 * std::cos((u + v) * .12));
    double h = 2.0 + hill;
    const double ridge = 11 * std::exp(-((u - 13) * (u - 13) + (v - 16) * (v - 16)) / 15.0);
    h += ridge;
    const double land_height = h;
    double bank = 1000;
    for (int i = 0; i < recipe_.rivers; ++i)
        bank = std::min(bank, std::abs(u - (i - (recipe_.rivers - 1) * .5) * 20));
    const bool wet = bank < 2.7;
    const double crossing = std::min({std::abs(v), std::abs(v - 23), std::abs(v + 23)});
    const bool bridge = wet && crossing < 3.3;
    if (bank < 5.2) {
        const double f = std::clamp((bank - 2.7) / 2.5, 0.0, 1.0);
        h = bridge ? 2.45 : std::lerp(-1.15, h, f * f * (3 - 2 * f));
    }
    // Raised bridge approaches match the same surface used by movement.
    if (bank < 7.5 && crossing < 3.3) {
        const double ramp = std::clamp((bank - 3.0) / 4.5, 0.0, 1.0);
        h = std::lerp(2.45, land_height, ramp * ramp * (3 - 2 * ramp));
    }
    const bool road = std::abs(L * std::asinh((b.x - b.z) / std::sqrt(2.0))) < 1.4 ||
                      std::abs(L * std::asinh((b.x + b.z) / std::sqrt(2.0))) < 1.4 ||
                      (recipe_.rivers && crossing < 2.4);
    const bool submerged = (wet && !bridge) || h < .35;
    bool walk = radial < recipe_.radius - .8 && !submerged;
    if (slopes && walk) {
        const auto f = space_.frame(b);
        const double eps = .15;
        const double dx = (field(space_.exp(b, f.east * eps), false).height -
                           field(space_.exp(b, f.east * (-eps)), false).height) /
                          (2 * eps);
        const double dz = (field(space_.exp(b, f.north * eps), false).height -
                           field(space_.exp(b, f.north * (-eps)), false).height) /
                          (2 * eps);
        walk = std::hypot(dx, dz) / std::cosh(h / L) < .95;
    }
    auto color = recipe_.grass;
    const float shade = static_cast<float>(.92 + .09 * std::sin(u * .7 + v * .3) + h * .008);
    for (auto &c : color)
        c *= shade;
    if (road && !wet)
        color = {.56f, .44f, .28f};
    if (wet)
        color = bridge ? std::array<float, 3>{.57f, .53f, .43f} : recipe_.water;
    if (!walk && !wet && radial < recipe_.radius - .8)
        color = {.49f, .49f, .43f};
    return {h, walk, submerged, bridge, road, color};
}
SurfaceSample Terrain::sample(h3::Vec b) const {
    return field(b, true);
}
h3::Vec Terrain::surface(h3::Vec p) const {
    const auto b = space_.ground(p);
    return space_.lift(b, field(b, false).height);
}
bool Terrain::legal(h3::Vec p) const {
    return sample(space_.ground(p)).walkable;
}
bool Terrain::segment_legal(h3::Vec a, h3::Vec b) const {
    a = space_.ground(a);
    b = space_.ground(b);
    const int count = std::max(1, static_cast<int>(std::ceil(space_.dist(a, b) / .35)));
    for (int i = 0; i <= count; ++i)
        if (!legal(space_.interpolate(a, b, static_cast<double>(i) / count)))
            return false;
    return true;
}
double Terrain::surface_length(h3::Vec a, h3::Vec b, int panels) const {
    a = space_.ground(a);
    b = space_.ground(b);
    const double d = space_.dist(a, b);
    if (d < 1e-10)
        return 0;
    panels = std::clamp(std::max(panels, static_cast<int>(std::ceil(d / .5))), 2, 512);
    // Sum short ambient chords along the lifted curve: convergent quadrature of
    // the actual recipe surface, including its height derivative and warp.
    auto previous = surface(a);
    double sum = 0;
    for (int i = 1; i <= panels; ++i) {
        const auto p = surface(space_.interpolate(a, b, static_cast<double>(i) / panels));
        sum += space_.dist(previous, p);
        previous = p;
    }
    return std::max(sum, space_.dist(surface(a), surface(b)));
}
SurfaceMove Terrain::advance(h3::Vec p, h3::Vec goal, double metres) const {
    if (!std::isfinite(metres) || metres < 0 || metres > 4)
        throw std::invalid_argument("Invalid surface movement budget");
    if (metres == 0)
        return {p, 0};
    const auto a = space_.ground(p), b = space_.ground(goal);
    const double d = space_.dist(a, b);
    if (d < 1e-8)
        return {surface(p), 0};
    double low = 0, high = std::min(1.0, metres / d);
    for (int i = 0; i < 12; ++i) {
        const double mid = (low + high) * .5;
        const double len = surface_length(a, space_.interpolate(a, b, mid), 4);
        if (len > metres)
            high = mid;
        else
            low = mid;
    }
    auto base = space_.interpolate(a, b, low);
    if (!segment_legal(a, base))
        return {p, 0};
    return {surface(base), surface_length(a, base, 4)};
}
bool Terrain::line_of_sight(h3::Vec a, h3::Vec b) const {
    const int n = std::max(2, static_cast<int>(std::ceil(space_.dist(a, b) / .7)));
    for (int i = 1; i < n; ++i) {
        auto p = space_.interpolate(a, b, static_cast<double>(i) / n);
        if (space_.height(p) < field(space_.ground(p), false).height + .05)
            return false;
    }
    return true;
}
int Terrain::nearest(h3::Vec p, int component_id) const {
    const auto k = space_.klein(p);
    const int x = static_cast<int>(std::round((k[0] / extent_ + 1) * .5 * (grid_ - 1))),
              z = static_cast<int>(std::round((k[1] / extent_ + 1) * .5 * (grid_ - 1)));
    int best = -1;
    double score = std::numeric_limits<double>::infinity();
    // Local chart lookup is an explicit snapping policy, not an intrinsic exact-nearest promise.
    for (int r = 0; r <= 6 && best < 0; ++r)
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx) {
                const int nx = x + dx, nz = z + dz;
                if (nx < 0 || nz < 0 || nx >= grid_ || nz >= grid_)
                    continue;
                const int id = cells_[nz * grid_ + nx];
                if (id < 0 || !nodes_[id].sample.walkable || nodes_[id].edges.empty() ||
                    (component_id >= 0 && components_[id] != component_id))
                    continue;
                const double d = space_.dist(p, nodes_[id].point);
                if (d < score) {
                    score = d;
                    best = id;
                }
            }
    if (best < 0)
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
            if (nodes_[i].sample.walkable && !nodes_[i].edges.empty() &&
                (component_id < 0 || components_[i] == component_id)) {
                const double d = space_.dist(p, nodes_[i].point);
                if (d < score) {
                    score = d;
                    best = i;
                }
            }
    if (best < 0)
        throw std::runtime_error("No traversable terrain node");
    return best;
}
std::vector<int> Terrain::path(int from, int to) const {
    if (from < 0 || to < 0 || from >= static_cast<int>(nodes_.size()) ||
        to >= static_cast<int>(nodes_.size()))
        return {};
    if (!nodes_[from].sample.walkable || !nodes_[to].sample.walkable)
        return {};
    std::vector<double> g(nodes_.size(), std::numeric_limits<double>::infinity());
    std::vector<int> parent(nodes_.size(), -1);
    std::vector<bool> closed(nodes_.size());
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    g[from] = 0;
    open.push({space_.dist(nodes_[from].point, nodes_[to].point), from});
    while (!open.empty()) {
        const int a = open.top().second;
        open.pop();
        if (closed[a])
            continue;
        if (a == to) {
            std::vector<int> route;
            for (int j = to; j != -1; j = parent[j])
                route.push_back(j);
            std::reverse(route.begin(), route.end());
            return route;
        }
        closed[a] = true;
        for (auto edge : nodes_[a].edges)
            if (!closed[edge.node] && g[a] + edge.length < g[edge.node]) {
                g[edge.node] = g[a] + edge.length;
                parent[edge.node] = a;
                open.push({g[edge.node] + space_.dist(nodes_[edge.node].point, nodes_[to].point), edge.node});
            }
    }
    return {};
}
double Terrain::route_length(const std::vector<int> &route) const {
    double d = 0;
    for (std::size_t i = 1; i < route.size(); ++i)
        d += surface_length(nodes_.at(route[i - 1]).base, nodes_.at(route[i]).base);
    return d;
}
} // namespace curved
