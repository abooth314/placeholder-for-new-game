#include "world/spatial.hpp"
#include <cmath>
#include <stdexcept>
namespace curved {
void SpatialIndex::rebuild(const Entities &e, const h3::Space &h) {
    for (auto &[key, b] : buckets_) {
        (void)key;
        b.clear();
    }
    for (auto handle : e.live()) {
        const auto k = h.klein(e.get(handle)->position);
        buckets_[{static_cast<int>(std::floor(k[0] / cell)), static_cast<int>(std::floor(k[1] / cell))}]
            .push_back(handle);
    }
}
std::vector<Handle> SpatialIndex::query(h3::Vec p, double r, const h3::Space &h) const {
    if (!std::isfinite(r) || r < 0 || r > 200)
        throw std::invalid_argument("Invalid nearby query radius");
    const auto k = h.klein(p);
    const double chart = r / h.radius();
    std::vector<Handle> out;
    // Fermi projection to H2 is 1-Lipschitz. The Klein metric >= L^2 I,
    // so r/L contains every intrinsically close surface actor. Final tests use H3.
    const int x0 = static_cast<int>(std::floor((k[0] - chart) / cell)),
              x1 = static_cast<int>(std::floor((k[0] + chart) / cell));
    const int z0 = static_cast<int>(std::floor((k[1] - chart) / cell)),
              z1 = static_cast<int>(std::floor((k[1] + chart) / cell));
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) {
            auto it = buckets_.find({x, z});
            if (it != buckets_.end())
                out.insert(out.end(), it->second.begin(), it->second.end());
        }
    return out;
}
} // namespace curved
