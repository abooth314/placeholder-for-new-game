#include "app/settings.hpp"
#include "assets/mesh.hpp"
#include "audio/synth.hpp"
#include "content/catalog.hpp"
#include "ui/console.hpp"
#include "view/camera.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace curved;
namespace fs = std::filesystem;
static unsigned assertions = 0;
static void require(bool value, const char *text) {
    ++assertions;
    if (!value)
        throw std::runtime_error(text);
}
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected test name and project root");
        const std::string test = argv[1];
        const fs::path root = argv[2];
        if (test == "camera") {
            h3::Space h(30);
            CameraRig rig;
            rig.target = h.point(23, .8, 4);
            for (int i = 0; i < 1000; ++i) {
                if (i == 0)
                    rig.toggle_free(h, 1.6);
                rig.move(h, .02, .006, .012);
                rig.look(.0007, -.0002);
                auto camera = rig.view(h, 1.6);
                require(h3::valid(camera.position), "free camera remains valid H3 point");
                for (auto v : {camera.right, camera.up, camera.forward}) {
                    require(std::abs(h3::dot(camera.position, v)) < 1e-8, "camera frame remains tangent");
                    require(std::abs(h3::norm(v) - 1) < 1e-8, "camera axes retain norm");
                }
                require(std::abs(h3::dot(camera.right, camera.forward)) < 1e-8, "camera remains orthogonal");
            }
            rig.toggle_free(h, 1.6);
            auto camera = rig.view(h, 1.6);
            auto p = CameraRig::project(rig.target, camera, 1600, 1000);
            require(p.visible && std::abs(p.x - 800) < 1e-7 && std::abs(p.y - 500) < 1e-7,
                    "orbit projection centers the intrinsic target");
        } else if (test == "recipes") {
            auto c = load_catalog(root / "assets", root / "user");
            std::set<std::uint64_t> sounds;
            for (auto &[id, r] : c.visuals) {
                auto vertices = evaluate_mesh(r);
                require(vertices.size() > 20 && vertices.size() <= 120000,
                        "recipe generated bounded solid mesh");
                for (auto &v : vertices)
                    require(std::isfinite(v.x + v.y + v.z + v.nx + v.ny + v.nz),
                            "procedural vertices finite");
                (void)id;
            }
            for (auto &[id, r] : c.sounds) {
                auto data = synthesize(r, 9137);
                require(data.size() == static_cast<std::size_t>(std::ceil(r.duration * audio_rate)),
                        "recipe duration exact");
                double energy = 0, peak = 0;
                std::uint64_t hash = 0;
                for (float x : data) {
                    energy += x * x;
                    peak = std::max(peak, std::abs(static_cast<double>(x)));
                    hash = hash * 1099511628211ULL + static_cast<std::uint64_t>((x + 1) * 65536);
                }
                require(energy / data.size() > .00001 && peak <= .951,
                        "procedural sound is non-silent and bounded");
                sounds.insert(hash);
                (void)id;
            }
            require(sounds.size() == c.sounds.size(), "sound recipes produce distinct signals");
        } else if (test == "input_settings") {
            Input input;
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = SDL_SCANCODE_W;
            input.event(event, false);
            require(input.down(Action::Forward), "semantic movement key");
            input.event(event, true);
            require(!input.down(Action::Forward), "captured keyboard clears movement");
            input.rebind(Action::Possess);
            require(input.event(event, true), "binding editor captures key");
            require(input.binding(Action::Possess) == SDL_SCANCODE_W &&
                        input.binding(Action::Forward) == SDL_SCANCODE_P,
                    "conflicting binding swaps rather than doubles actions");
            require(!input.down(Action::Forward) && !input.pressed(Action::Possess),
                    "rebinding emits no gameplay action");
            Settings original;
            original.speed = 2;
            original.master = .31f;
            original.width = 1280;
            original.curvature = 40;
            auto path = root / "out/tests/settings with spaces.json";
            original.save(path, input);
            Settings restored;
            Input bindings;
            restored.load(path, bindings);
            require(restored.error.empty() && restored.speed == 2 && restored.curvature == 40 &&
                        std::abs(restored.master - .31) < .0001 &&
                        bindings.binding(Action::Possess) == SDL_SCANCODE_W,
                    "settings and bindings persist");
        } else if (test == "console") {
            auto c = load_catalog(root / "assets", root / "user");
            Terrain t(c.maps[0], 8147);
            auto w = std::make_unique<World>(c, t, false);
            auto result = execute_console(*w, "spawn wardens:lantern_warden 0");
            require(result.success && result.selection, "console resolves BaseId through normal pipeline");
            const auto ref = std::to_string(result.selection.ref);
            require(execute_console(*w, "possess " + ref).success, "console possession uses native owner");
            require(!execute_console(*w, "set " + ref + " health.current 999").success,
                    "unsupported current-health write rejected");
            require(execute_console(*w, "mod " + ref + " spell:test movement.max_speed 1 1.2 5").success,
                    "console source modifier");
            require(execute_console(*w, "unmod " + ref + " spell:test").success, "console source removal");
            require(!execute_console(*w, "select 999999").success, "console stale reference rejected");
        } else
            throw std::runtime_error("Unknown presentation test");
        std::cout << "PASS " << test << " (" << assertions << " assertions)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
