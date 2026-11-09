#pragma once
#include "math/hyperbolic.hpp"
#include <array>
namespace curved {
struct Camera {
    h3::Vec position, right, up, forward;
    double fov = .872664625997, aspect = 1;
};
struct Projection {
    double x, y, depth;
    bool visible;
};
class CameraRig {
  public:
    h3::Vec target = h3::origin();
    double yaw = .35, pitch = .91, distance = 52;
    bool free = false, follow = false;
    Camera view(const h3::Space &h, double aspect) const;
    void toggle_free(const h3::Space &h, double aspect);
    void orbit(double dx, double dy);
    void move(const h3::Space &h, double right, double rise, double forward);
    void look(double yaw, double pitch);
    void focus(h3::Vec p, double zoom = 36);
    void overview(h3::Vec p);
    static Projection project(h3::Vec p, const Camera &camera, int width, int height);

  private:
    Camera flying_;
};
} // namespace curved
