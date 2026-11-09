#include "math/hyperbolic.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace curved::h3 {
double dot(Vec a, Vec b) {
    const long double v = -static_cast<long double>(a.t) * b.t + static_cast<long double>(a.x) * b.x +
                          static_cast<long double>(a.y) * b.y + static_cast<long double>(a.z) * b.z;
    return static_cast<double>(v);
}
bool finite(Vec p) {
    return std::isfinite(p.t) && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool valid(Vec p) {
    return finite(p) && p.t > 0 && p.t < std::cosh(12.0) && std::abs(dot(p, p) + 1) < 2e-8 * (1 + p.t * p.t);
}
static void check(Vec p) {
    if (!valid(p))
        throw std::domain_error("Invalid or out-of-domain H3 point");
}
double norm(Vec v) {
    if (!finite(v))
        throw std::domain_error("Non-finite tangent");
    const double n = dot(v, v);
    if (n < -1e-8 * (1 + v.t * v.t))
        throw std::domain_error("Timelike vector used as tangent");
    return std::sqrt(std::max(0.0, n));
}
Vec normalize(Vec p) {
    if (!finite(p))
        throw std::domain_error("Cannot normalize non-finite point");
    p.t = std::hypot(1.0, std::hypot(p.x, std::hypot(p.y, p.z)));
    check(p);
    return p;
}
Vec project(Vec p, Vec v) {
    check(p);
    if (!finite(v))
        throw std::domain_error("Invalid tangent");
    return v + p * dot(p, v);
}
Vec unit(Vec v) {
    const auto n = norm(v);
    if (n < 1e-13)
        throw std::domain_error("Zero direction");
    return v * (1 / n);
}
Space::Space(double radius) : l_(radius) {
    if (!std::isfinite(l_) || l_ < 4 || l_ > 256)
        throw std::domain_error("L must be finite in [4,256] metres");
}
double Space::dist(Vec p, Vec q) const {
    check(p);
    check(q);
    const Vec d = p - q;
    // The half-chord avoids acosh(1+epsilon) cancellation at close range.
    return 2 * l_ * std::asinh(.5 * std::sqrt(std::max(0.0, dot(d, d))));
}
Vec Space::exp(Vec p, Vec v) const {
    check(p);
    v = project(p, v);
    const double n = norm(v), r = n / l_;
    if (r > 12)
        throw std::domain_error("Exp step exceeds numerical domain");
    const double s = n < 1e-7 ? (1 + r * r / 6) / l_ : std::sinh(r) / n;
    return normalize(p * std::cosh(r) + v * s);
}
Vec Space::log(Vec p, Vec q) const {
    const double d = dist(p, q), r = d / l_;
    if (d < 1e-12)
        return {};
    const Vec delta = q - p;
    // q-cosh(r)p, expressed through the small chord rather than two large terms.
    const Vec v = delta - p * (2 * std::pow(std::sinh(r * .5), 2));
    return project(p, v * (r < 1e-7 ? l_ * (1 - r * r / 6) : d / std::sinh(r)));
}
Vec Space::interpolate(Vec p, Vec q, double f) const {
    if (!std::isfinite(f) || f < 0 || f > 1)
        throw std::domain_error("Geodesic fraction outside [0,1]");
    return exp(p, log(p, q) * f);
}
Vec Space::transport(Vec p, Vec q, Vec v) const {
    check(p);
    check(q);
    v = project(p, v);
    return project(q, v + (p + q) * (dot(q, v) / (1 - dot(p, q))));
}
Vec Space::point(double r, double a, double h) const {
    if (!std::isfinite(r) || !std::isfinite(a) || r < 0 || r / l_ > 10)
        throw std::domain_error("Invalid polar point");
    return lift({std::cosh(r / l_), std::sinh(r / l_) * std::cos(a), 0, std::sinh(r / l_) * std::sin(a)}, h);
}
Vec Space::ground(Vec p) const {
    check(p);
    const double c = std::hypot(1.0, p.y);
    return {p.t / c, p.x / c, 0, p.z / c};
}
double Space::height(Vec p) const {
    check(p);
    return l_ * std::asinh(p.y);
}
Vec Space::lift(Vec b, double h) const {
    if (!std::isfinite(h) || std::abs(h / l_) > 10)
        throw std::domain_error("Invalid normal height");
    b = ground(b);
    const double c = std::cosh(h / l_);
    return normalize({b.t * c, b.x * c, std::sinh(h / l_), b.z * c});
}
Vec Space::from_klein(double x, double z) const {
    const double k = x * x + z * z;
    if (!std::isfinite(k) || k >= 1 - 1e-10)
        throw std::domain_error("Klein coordinates outside bounded open disk");
    const double t = 1 / std::sqrt(1 - k);
    return {t, x * t, 0, z * t};
}
std::array<double, 2> Space::klein(Vec p) const {
    check(p);
    return {p.x / p.t, p.z / p.t};
}
Frame Space::frame(Vec p, double yaw) const {
    const Vec b = ground(p);
    const double h = height(p) / l_;
    const Vec e = transport(origin(), b, {0, 1, 0, 0}), n = transport(origin(), b, {0, 0, 0, 1});
    const Vec u = {b.t * std::sinh(h), b.x * std::sinh(h), std::cosh(h), b.z * std::sinh(h)};
    return {e * std::cos(yaw) - n * std::sin(yaw), u, e * std::sin(yaw) + n * std::cos(yaw)};
}
Vec Space::offset(Vec p, double x, double y, double z, double yaw) const {
    const auto f = frame(p, yaw);
    return exp(p, f.east * x + f.up * y + f.north * z);
}
Vec Space::isometry(Vec p, Vec from, Vec to) const {
    check(p);
    check(from);
    check(to);
    if (dist(from, to) < 1e-9)
        return p;
    const Vec m = unit(to - from);
    return normalize(p - m * (2 * dot(p, m)));
}
} // namespace curved::h3
