#pragma once
#include "sim/entities.hpp"
#include <map>
#include <utility>

namespace curved {
class SpatialIndex {
  public:
    void rebuild(const Entities &entities, const h3::Space &space);
    std::vector<Handle> query(h3::Vec p, double range, const h3::Space &space) const;

  private:
    static constexpr double cell = .08;
    std::map<std::pair<int, int>, std::vector<Handle>> buckets_;
};
} // namespace curved
