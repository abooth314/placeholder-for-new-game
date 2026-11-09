#pragma once
#include <array>

namespace curved::h3 {
struct Vec {
    double t{}, x{}, y{}, z{};
    Vec operator+(Vec b) const {
        return {t + b.t, x + b.x, y + b.y, z + b.z};
    }
    Vec operator-(Vec b) const {
        return {t - b.t, x - b.x, y - b.y, z - b.z};
    }
    Vec operator*(double s) const {
        return {t * s, x * s, y * s, z * s};
    }
};
double dot(Vec a, Vec b);
double norm(Vec v);
bool finite(Vec p);
bool valid(Vec p);
Vec normalize(Vec p);
Vec project(Vec p, Vec v);
Vec unit(Vec v);
inline Vec origin() {
    return {1, 0, 0, 0};
}
struct Frame {
    Vec east, up, north;
};
class Space {
  public:
    explicit Space(double radius = 48);
    double radius() const {
        return l_;
    }
    double dist(Vec p, Vec q) const;
    Vec exp(Vec p, Vec physical_tangent) const;
    Vec log(Vec p, Vec q) const;
    Vec interpolate(Vec p, Vec q, double fraction) const;
    Vec transport(Vec p, Vec q, Vec v) const;
    Vec point(double radial, double angle, double height = 0) const;
    Vec ground(Vec p) const;
    double height(Vec p) const;
    Vec lift(Vec base, double height) const;
    Vec from_klein(double x, double z) const;
    std::array<double, 2> klein(Vec p) const;
    Frame frame(Vec p, double yaw = 0) const;
    Vec offset(Vec p, double x, double y, double z, double yaw = 0) const;
    Vec isometry(Vec p, Vec from, Vec to) const;

  private:
    double l_;
};
} // namespace curved::h3
