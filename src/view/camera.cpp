#include "view/camera.hpp"
#include <algorithm>
#include <cmath>
namespace curved {
Camera CameraRig::view(const h3::Space &h, double aspect) const {
    if (free) {
        auto c = flying_;
        c.aspect = aspect;
        return c;
    }
    const auto f = h.frame(target);
    const auto horizontal = f.east * std::sin(yaw) + f.north * std::cos(yaw);
    const auto direction = horizontal * std::cos(pitch) + f.up * std::sin(pitch);
    const double r = distance / h.radius();
    return {h.exp(target, direction * distance),
            f.east * std::cos(yaw) - f.north * std::sin(yaw),
            f.up * std::cos(pitch) - horizontal * std::sin(pitch),
            target * (-std::sinh(r)) - direction * std::cosh(r),
            .872664625997,
            aspect};
}
void CameraRig::toggle_free(const h3::Space &h, double aspect) {
    if (!free) {
        flying_ = view(h, aspect);
        free = true;
        follow = false;
    } else {
        const auto b = h.ground(flying_.position);
        const double d = h.dist(h3::origin(), b);
        target = d > 65 ? h.point(65, std::atan2(b.z, b.x)) : b;
        distance = 42;
        pitch = .9;
        free = false;
    }
}
void CameraRig::orbit(double x, double y) {
    if (free)
        look(-x * .005, -y * .004);
    else {
        yaw -= x * .006;
        pitch = std::clamp(pitch + y * .004, .17, 1.53);
    }
}
void CameraRig::look(double y, double p) {
    if (!free)
        return;
    const auto r = flying_.right, f = flying_.forward;
    flying_.right = r * std::cos(y) - f * std::sin(y);
    flying_.forward = f * std::cos(y) + r * std::sin(y);
    const auto u = flying_.up, forward = flying_.forward;
    flying_.up = u * std::cos(p) - forward * std::sin(p);
    flying_.forward = forward * std::cos(p) + u * std::sin(p);
}
void CameraRig::move(const h3::Space &h, double right, double rise, double forward) {
    if (free) {
        const auto old = flying_.position;
        const auto p = h.exp(old, flying_.right * right + h.frame(old).up * rise + flying_.forward * forward);
        if (h.dist(h3::origin(), p) > 180)
            return;
        flying_.right = h3::unit(h.transport(old, p, flying_.right));
        flying_.up = h3::unit(h.transport(old, p, flying_.up));
        flying_.forward = h3::unit(h.transport(old, p, flying_.forward));
        flying_.position = p;
    } else {
        const auto p = h.offset(target, right * std::cos(yaw) - forward * std::sin(yaw), 0,
                                -right * std::sin(yaw) - forward * std::cos(yaw));
        if (h.dist(h3::origin(), h.ground(p)) < 68)
            target = p;
        follow = false;
    }
}
void CameraRig::focus(h3::Vec p, double zoom) {
    target = p;
    distance = zoom;
    pitch = .85;
    free = false;
    follow = false;
}
void CameraRig::overview(h3::Vec p) {
    target = p;
    distance = 96;
    pitch = 1.48;
    free = false;
    follow = false;
}
Projection CameraRig::project(h3::Vec p, const Camera &c, int width, int height) {
    const double x = h3::dot(p, c.right), y = h3::dot(p, c.up), z = h3::dot(p, c.forward),
                 t = -h3::dot(p, c.position), f = 1 / std::tan(c.fov * .5);
    if (z <= .000001 || z / t < .00005)
        return {0, 0, z, false};
    const double nx = x * f / (c.aspect * z), ny = y * f / z;
    return {(nx * .5 + .5) * width, (.5 - ny * .5) * height, z, std::abs(nx) < 1.05 && std::abs(ny) < 1.1};
}
} // namespace curved
