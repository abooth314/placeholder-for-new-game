#include "app/input.hpp"
#include "app/settings.hpp"
#include "audio/audio.hpp"
#include "content/catalog.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "view/renderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace curved {
namespace fs = std::filesystem;
struct Options {
    fs::path assets, user, output;
    int frames = 0, warmup = 0, population = -1, map = -1, width = 0, height = 0;
    double seconds = 0;
    bool diagnostic = false, no_vsync = false, mute = false, fill_population = false;
};
static Options options(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        auto value = [&]() {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing value for " + key);
            return std::string(argv[++i]);
        };
        if (key == "--assets")
            o.assets = fs::u8path(value());
        else if (key == "--user")
            o.user = fs::u8path(value());
        else if (key == "--output")
            o.output = fs::u8path(value());
        else if (key == "--frames")
            o.frames = std::stoi(value());
        else if (key == "--warmup-ticks")
            o.warmup = std::stoi(value());
        else if (key == "--population")
            o.population = std::stoi(value());
        else if (key == "--map")
            o.map = std::stoi(value());
        else if (key == "--width")
            o.width = std::stoi(value());
        else if (key == "--height")
            o.height = std::stoi(value());
        else if (key == "--profile-seconds")
            o.seconds = std::stod(value());
        else if (key == "--diagnostic")
            o.diagnostic = true;
        else if (key == "--no-vsync")
            o.no_vsync = true;
        else if (key == "--mute")
            o.mute = true;
        else if (key == "--fill-population")
            o.fill_population = true;
        else
            throw std::runtime_error("Unknown argument: " + key);
    }
    if (o.frames < 0 || o.frames > 1000000 || o.warmup < 0 || o.warmup > 10000 ||
        (o.population != -1 && (o.population < 40 || o.population > 384)) || !std::isfinite(o.seconds) ||
        o.seconds < 0 || o.seconds > 600)
        throw std::runtime_error("CLI value outside bounds");
    return o;
}
static double percentile(std::vector<double> v, double p) {
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>((v.size() - 1) * p)];
}
static double memory_peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
        return info.PeakWorkingSetSize / 1048576.0;
#endif
    return 0;
}
class Application {
  public:
    Options options;
    fs::path assets, user, output;
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    bool sdl = false, imgui = false, backends = false;
    Input input;
    Settings settings;
    UiState ui;
    CameraRig camera;
    Catalog catalog;
    std::unique_ptr<Terrain> terrain;
    std::unique_ptr<World> world;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Audio> audio;
    std::vector<double> frame_times, tick_times;
    std::vector<std::string> captures;
    bool restored = false, settings_roundtrip = false, capture_input = false;
    int width = 1440, height = 900;
    std::uint64_t dropped_steps = 0;
    double clamped_wall_seconds = 0;
    explicit Application(Options o) : options(std::move(o)) {}
    ~Application() {
        audio.reset();
        renderer.reset();
        world.reset();
        terrain.reset();
        if (backends) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
        }
        if (imgui)
            ImGui::DestroyContext();
        if (context)
            SDL_GL_DestroyContext(context);
        if (window)
            SDL_DestroyWindow(window);
        if (sdl)
            SDL_Quit();
    }
    void rebuild(int map) {
        map = std::clamp(map, 0, static_cast<int>(catalog.maps.size()) - 1);
        auto candidate = std::make_unique<Terrain>(catalog.maps[map], settings.seed, settings.curvature);
        auto battle = std::make_unique<World>(catalog, *candidate, true, settings.population);
        battle->reinforce_rate = settings.reinforce_rate;
        battle->combat = settings.combat;
        if (options.fill_population) {
            std::vector<std::string> units;
            for (auto &[id, u] : catalog.units)
                if (u.spawn && u.reinforce && u.weight > 0)
                    units.push_back(id);
            if (units.empty())
                throw std::runtime_error("No population fixture definitions");
            for (int faction = 0; faction < 4; ++faction)
                for (unsigned i = battle->factions()[faction].alive;
                     i < static_cast<unsigned>(settings.population) / 4; ++i)
                    if (!battle->spawn(units[i % units.size()], faction))
                        throw std::runtime_error("Population fixture could not fill requested budget");
            battle->replan();
        }
        if (world)
            for (int a = 0; a < 4; ++a)
                for (int b = 0; b < 4; ++b)
                    if (a != b)
                        battle->set_relation(a, b, world->relation(a, b));
        if (renderer)
            renderer->set_terrain(*candidate);
        world = std::move(battle);
        terrain = std::move(candidate);
        settings.map = map;
        ui.selected = {};
        ui.player_target = {};
        ui.nav_length = -1;
        camera.focus(terrain->surface(h3::origin()), 52);
        camera.yaw = .35;
        ui.message = catalog.maps[map].name + " loaded. Select a unit to inspect or possess.";
        input.clear();
    }
    void initialize() {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
            throw std::runtime_error(SDL_GetError());
        sdl = true;
        const auto base = fs::u8path(SDL_GetBasePath());
        assets = options.assets.empty() ? base / "assets" : fs::absolute(options.assets);
        user = options.user.empty() ? base / "user" : fs::absolute(options.user);
        output = options.output.empty() ? base / "diagnostics" : fs::absolute(options.output);
        settings.load(user / "settings.json", input);
        if (options.width)
            settings.width = std::clamp(options.width, 960, 3840);
        if (options.height)
            settings.height = std::clamp(options.height, 640, 2160);
        if (options.no_vsync)
            settings.vsync = false;
        if (options.mute)
            settings.mute = true;
        if (options.population >= 0)
            settings.population = options.population;
        catalog = load_catalog(assets, user);
        if (options.map >= 0)
            settings.map = options.map;
        settings.map = std::clamp(settings.map, 0, static_cast<int>(catalog.maps.size()) - 1);
        width = settings.width;
        height = settings.height;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        window = SDL_CreateWindow("Curved Earth - Asterhallow Games | Native prototype", width, height,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window, 960, 640);
        context = SDL_GL_CreateContext(window);
        if (!context)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0);
        ImGui::CreateContext();
        imgui = true;
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        io.Fonts->AddFontDefault();
        style_ui();
        if (!ImGui_ImplSDL3_InitForOpenGL(window, context) || !ImGui_ImplOpenGL3_Init("#version 330"))
            throw std::runtime_error("ImGui backend initialization failed");
        backends = true;
        rebuild(settings.map);
        renderer = std::make_unique<Renderer>(catalog, *terrain);
        audio = std::make_unique<Audio>(catalog);
        for (int i = 0; i < options.warmup; ++i)
            world->step();
        if (!settings.error.empty())
            ui.message = "Settings were rejected: " + settings.error;
        frame_times.reserve(30000);
        tick_times.reserve(3000);
        if (options.diagnostic) {
            fs::create_directories(output);
            write_audio_demo(catalog, output / "synthesized-battlefield.wav");
            SDL_Event key{};
            key.type = SDL_EVENT_KEY_DOWN;
            key.key.scancode = SDL_SCANCODE_W;
            input.event(key, true);
            capture_input = !input.down(Action::Forward);
        }
    }
    Handle pick(float x, float y, const Camera &view) {
        Handle best;
        double score = 24;
        for (auto handle : world->actors())
            if (auto *a = world->inspect(handle)) {
                const auto p =
                    CameraRig::project(terrain->space().offset(a->position, 0, 1.25, 0), view, width, height);
                if (!p.visible)
                    continue;
                const double d = std::hypot(x - p.x, y - p.y);
                if (d < score) {
                    score = d;
                    best = handle;
                }
            }
        return best;
    }
    void select_mod() {
        for (auto handle : world->actors())
            if (world->inspect(handle)->base == "wardens:lantern_warden") {
                ui.selected = handle;
                return;
            }
        throw std::runtime_error("Enabled sample mod missing from application population");
    }
    void stage(int frame) {
        if (!options.diagnostic)
            return;
        if (frame == 40) {
            select_mod();
            camera.focus(world->inspect(ui.selected)->position, 25);
        }
        if (frame == 70) {
            toggle_possession(ui, *world, camera);
            if (!world->possessed())
                throw std::runtime_error("Diagnostic possession failed");
        }
        if (frame == 130) {
            world->release();
            camera.toggle_free(terrain->space(), static_cast<double>(width) / height);
            camera.move(terrain->space(), 2, 3, 1);
            camera.look(.2, -.05);
        }
        if (frame == 170)
            rebuild(1);
        if (frame == 210)
            ui.diplomacy = true;
        if (frame == 217)
            world->set_relation(0, 1, 0);
        if (frame == 230)
            ui.diplomacy = false;
        if (frame == 242) {
            select_mod();
            ui.inspector = true;
            ui.council = false;
        }
        if (frame == 270)
            SDL_SetWindowSize(window, 960, 640);
        if (frame == 290) {
            SDL_SetWindowSize(window, 1440, 900);
            ui.inspector = false;
        }
        if (frame == 300) {
            const auto before = renderer->stats();
            ImGui_ImplOpenGL3_Shutdown();
            renderer.reset();
            SDL_GL_DestroyContext(context);
            context = SDL_GL_CreateContext(window);
            if (!context)
                throw std::runtime_error("Context restoration failed");
            SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0);
            if (!ImGui_ImplOpenGL3_Init("#version 330"))
                throw std::runtime_error("UI context restoration failed");
            renderer = std::make_unique<Renderer>(catalog, *terrain);
            restored = before.models == renderer->stats().models &&
                       before.buffers == renderer->stats().buffers &&
                       before.textures == renderer->stats().textures;
        }
        if (frame == 340) {
            settings.save(user / "settings.json", input);
            Settings readback;
            Input bindings;
            readback.load(user / "settings.json", bindings);
            settings_roundtrip = readback.error.empty() && readback.speed == settings.speed &&
                                 readback.map == settings.map && readback.curvature == settings.curvature &&
                                 bindings.binding(Action::Possess) == input.binding(Action::Possess);
        }
    }
    void capture(int frame) {
        if (!options.diagnostic)
            return;
        std::string name;
        switch (frame) {
        case 30:
            name = "01-battle";
            break;
        case 60:
            name = "02-selected-unit";
            break;
        case 100:
            name = "03-possessed-unit";
            break;
        case 150:
            name = "04-free-flight";
            break;
        case 190:
            name = "05-crossings";
            break;
        case 222:
            name = "06-directional-diplomacy";
            break;
        case 255:
            name = "07-native-inspector";
            break;
        case 275:
            name = "08-compact";
            break;
        case 320:
            name = "09-context-restored";
            break;
        default:
            return;
        }
        renderer->screenshot(output / (name + ".bmp"), width, height);
        captures.push_back(name);
    }
    void report(int frames, double elapsed) {
        fs::create_directories(output);
        const auto &s = world->stats();
        const auto &render = renderer->stats();
        const auto &sound = audio->stats();
        nlohmann::json j = {
            {"schema", 1},
            {"success", !options.diagnostic || (captures.size() == 9 && restored && settings_roundtrip &&
                                                capture_input && s.lua_errors == 0)},
            {"mode", options.diagnostic ? "deterministic-rendered-consumers" : "real-time-populated-profile"},
            {"seed", terrain->seed()},
            {"map", terrain->recipe().id},
            {"catalog_fingerprint", catalog.fingerprint},
            {"enabled_packs", catalog.packs},
            {"resolution", {width, height}},
            {"detailed", settings.detailed},
            {"vsync", settings.vsync},
            {"curvature_radius_m", terrain->space().radius()},
            {"timestep_s", limits::step},
            {"speed", settings.speed},
            {"wall_duration_s", elapsed},
            {"simulated_duration_s", world->time()},
            {"frames", frames},
            {"dropped_catchup_steps", dropped_steps},
            {"clamped_wall_seconds", clamped_wall_seconds},
            {"frame_ms",
             {{"p50", percentile(frame_times, .5)},
              {"p95", percentile(frame_times, .95)},
              {"p99", percentile(frame_times, .99)}}},
            {"simulation_tick_ms",
             {{"p50", percentile(tick_times, .5)},
              {"p95", percentile(tick_times, .95)},
              {"p99", percentile(tick_times, .99)}}},
            {"gpu", render.gpu},
            {"opengl", render.version},
            {"gpu_time_ms", nullptr},
            {"peak_working_set_mib", memory_peak()},
            {"peak_actors", s.peak_actors},
            {"population_limit", world->population_limit},
            {"peak_projectiles", s.peak_projectiles},
            {"peak_effects", s.peak_effects},
            {"melee_attacks", s.melee},
            {"projectile_shots", s.shots},
            {"projectile_hits", s.hits},
            {"heals", s.heals},
            {"retreats", s.retreats},
            {"path_failures", s.path_failures},
            {"lua_calls", s.lua_calls},
            {"lua_errors", s.lua_errors},
            {"world_digest", std::to_string(world->digest())},
            {"renderer",
             {{"draw_calls", render.draw_calls},
              {"triangles", render.triangles},
              {"mesh_count", render.models},
              {"buffers", render.buffers},
              {"textures", render.textures},
              {"mesh_bytes", render.mesh_bytes},
              {"instance_bytes", render.instance_bytes}}},
            {"audio",
             {{"device_open", sound.device},
              {"driver", sound.driver},
              {"generated_frames", sound.samples},
              {"played_events", sound.played},
              {"dropped_events", sound.dropped},
              {"peak_voices", sound.peak_voices},
              {"cache_bytes", sound.cached_bytes},
              {"listened_to", false},
              {"error", sound.error}}},
            {"budget_drops",
             {{"actors", s.actor_overflow},
              {"projectiles", s.projectile_overflow},
              {"effects", s.effect_overflow},
              {"events", s.event_overflow},
              {"lua_plans", s.lua_budget_skips}}},
            {"captures", captures},
            {"context_recreation", restored},
            {"settings_roundtrip", settings_roundtrip},
            {"synthetic_ui_capture", capture_input}};
        for (int i = 0; i < 4; ++i)
            j["factions"].push_back({{"id", catalog.factions[i].id},
                                     {"alive", world->factions()[i].alive},
                                     {"kills", world->factions()[i].kills},
                                     {"spawned", world->factions()[i].spawned}});
        std::ofstream report(output / "report.json");
        report << j.dump(2) << '\n';
        if (!report)
            throw std::runtime_error("Diagnostic report write failed");
        std::cout << j.dump(2) << '\n';
        if (!j.at("success").get<bool>())
            throw std::runtime_error("Rendered diagnostic acceptance incomplete");
    }
    int run() {
        initialize();
        bool running = true, drag = false, pending_primary = false, pending_ability = false;
        Handle input_owner;
        double accumulator = 0, average_frame = 16.67;
        int frame = 0;
        const auto started = std::chrono::steady_clock::now();
        auto previous = started;
        while (running) {
            const auto now = std::chrono::steady_clock::now();
            const double raw = std::chrono::duration<double>(now - previous).count(),
                         delta = std::clamp(raw, 0.0, .1);
            previous = now;
            if (frame > 0 && frame_times.size() < 250000)
                frame_times.push_back(raw * 1000);
            clamped_wall_seconds += std::max(0.0, raw - .1);
            average_frame = average_frame * .94 + raw * 1000 * .06;
            input.begin_frame();
            stage(frame);
            auto &io = ImGui::GetIO();
            const bool had_ui_layer = ui.blocking() || ui.inspector || input.binding();
            std::vector<SDL_Event> frame_events;
            frame_events.reserve(32);
            SDL_Event polled;
            while (SDL_PollEvent(&polled)) {
                ImGui_ImplSDL3_ProcessEvent(&polled);
                frame_events.push_back(polled);
            }
            SDL_GetWindowSizeInPixels(window, &width, &height);
            if (width < 1 || height < 1) {
                SDL_Delay(10);
                continue;
            }
            // Resolve this frame's hit testing, text focus and binding editor
            // before delivering any input to a native controller.
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            auto request =
                draw_ui(ui, *world, *renderer, *audio, camera, input, settings, width, height, average_frame);
            const bool block = ui.blocking() || input.binding();
            for (const auto &event : frame_events) {
                if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    running = false;
                const bool rebound = input.event(event, block || io.WantCaptureKeyboard || io.WantTextInput);
                if (rebound)
                    settings.save(user / "settings.json", input);
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE &&
                    !event.key.repeat && !rebound) {
                    if (had_ui_layer || ui.blocking() || ui.inspector)
                        ui.maps = ui.diplomacy = ui.council = ui.help = ui.inspector = false;
                    else if (world->possessed())
                        world->release();
                    else
                        ui.selected = {};
                    input.clear();
                    world->player_intent({});
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT &&
                    !block && !io.WantCaptureMouse)
                    drag = true;
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
                    !block && !io.WantCaptureMouse && world->possessed())
                    pending_primary = true;
                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
                    drag = false;
                if (event.type == SDL_EVENT_MOUSE_MOTION && drag && !block && !io.WantCaptureMouse)
                    camera.orbit(event.motion.xrel, event.motion.yrel);
                if (event.type == SDL_EVENT_MOUSE_WHEEL && !block && !io.WantCaptureMouse) {
                    if (camera.free)
                        camera.move(terrain->space(), 0, 0, event.wheel.y * 2);
                    else
                        camera.distance =
                            std::clamp(camera.distance * std::exp(-event.wheel.y * .12), 5.0, 125.0);
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT &&
                    !block && !io.WantCaptureMouse) {
                    auto selected = pick(event.button.x, event.button.y,
                                         camera.view(terrain->space(), static_cast<double>(width) / height));
                    if (world->possessed())
                        ui.player_target = selected;
                    else {
                        ui.selected = selected;
                        camera.follow = false;
                        ui.nav_length = -1;
                    }
                }
            }
            if (!block && !io.WantCaptureKeyboard) {
                if (input.pressed(Action::Pause))
                    ui.paused = !ui.paused;
                if (input.pressed(Action::Possess))
                    toggle_possession(ui, *world, camera);
                if (input.pressed(Action::Freecam)) {
                    world->release();
                    camera.toggle_free(terrain->space(), static_cast<double>(width) / height);
                }
                if (input.pressed(Action::Overview)) {
                    world->release();
                    camera.overview(terrain->surface(h3::origin()));
                }
                if (input.pressed(Action::Focus)) {
                    focus_battle(*world, camera);
                }
                if (input.pressed(Action::Maps))
                    ui.maps = true;
                if (input.pressed(Action::Diplomacy))
                    ui.diplomacy = true;
                if (input.pressed(Action::Council)) {
                    ui.council = !ui.council;
                    ui.inspector = false;
                }
                if (input.pressed(Action::Inspect)) {
                    ui.inspector = !ui.inspector;
                    ui.council = false;
                }
                if (input.pressed(Action::Help))
                    ui.help = true;
            }
            const bool captured =
                ui.blocking() || input.binding() || io.WantCaptureKeyboard || io.WantTextInput;
            if (world->possessed() != input_owner) {
                pending_primary = false;
                pending_ability = false;
                input_owner = world->possessed();
            }
            if (captured || ui.paused) {
                pending_primary = false;
                pending_ability = false;
            } else if (input.pressed(Action::Ability))
                pending_ability = true;
            double x = captured ? 0 : input.down(Action::Right) - input.down(Action::Left),
                   z = captured ? 0 : input.down(Action::Forward) - input.down(Action::Back),
                   rise = captured ? 0 : input.down(Action::Rise) - input.down(Action::Descend);
            float mouse_x = 0, mouse_y = 0;
            const auto mouse = SDL_GetMouseState(&mouse_x, &mouse_y);
            if (world->possessed()) {
                if (options.diagnostic && frame >= 70 && frame < 120) {
                    x = 1;
                    z = .2;
                }
                const double east = x * std::cos(camera.yaw) - z * std::sin(camera.yaw),
                             north = -x * std::sin(camera.yaw) - z * std::cos(camera.yaw);
                world->player_intent(
                    {east, north,
                     !captured && !io.WantCaptureMouse && ((mouse & SDL_BUTTON_LMASK) || pending_primary),
                     !captured && (input.down(Action::Ability) || pending_ability), ui.player_target});
            } else if (!captured) {
                const double n = std::max(1.0, std::sqrt(x * x + z * z + rise * rise));
                const bool fast = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                const double metres = delta * (fast ? 42 : 22);
                camera.move(terrain->space(), x * metres / n, rise * metres / n, z * metres / n);
                if (!camera.free)
                    camera.target = terrain->surface(camera.target);
                const double turn = (input.down(Action::OrbitRight) - input.down(Action::OrbitLeft)) * delta;
                if (camera.free)
                    camera.look(turn, 0);
                else
                    camera.yaw += turn;
            }
            const auto prior_possession = world->possessed();
            const auto *prior_actor = world->inspect(prior_possession);
            const auto prior_position = prior_actor ? prior_actor->position : camera.target;
            if (!ui.paused) {
                accumulator += options.diagnostic ? limits::step : delta * settings.speed;
                if (accumulator > .4) {
                    dropped_steps += static_cast<std::uint64_t>((accumulator - .4) / limits::step);
                    accumulator = .4;
                }
                unsigned steps = 0;
                while (accumulator + 1e-12 >= limits::step && steps++ < limits::max_catchup) {
                    world->step();
                    pending_primary = false;
                    pending_ability = false;
                    audio->events(world->events(), terrain->space(),
                                  camera.view(terrain->space(), static_cast<double>(width) / height));
                    accumulator -= limits::step;
                    tick_times.push_back(world->stats().last_tick_ms);
                }
            } else
                accumulator = 0;
            if (prior_possession && !world->possessed()) {
                ui.selected = {};
                ui.player_target = {};
                ui.message = "Your controlled unit has fallen. Select another soldier to rejoin the battle.";
                camera.focus(prior_position, 28);
            }
            if (auto *a = world->inspect(world->possessed())) {
                ui.selected = a->handle;
                camera.target = terrain->space().offset(a->position, 0, 1, 0);
            } else if (camera.follow)
                if (auto *a = world->inspect(ui.selected))
                    camera.target = a->position;
            const auto view = camera.view(terrain->space(), static_cast<double>(width) / height);
            renderer->draw(*world, view, width, height, settings.detailed);
            draw_world_overlays(ui, *world, view, width, height, settings.health_bars);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            capture(frame);
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            const bool stop = (options.frames > 0 && frame + 1 >= options.frames) ||
                              (options.diagnostic && frame + 1 >= 360) ||
                              (options.seconds > 0 && elapsed >= options.seconds);
            if (stop && !options.diagnostic)
                renderer->screenshot(output / "battlefield.bmp", width, height);
            SDL_GL_SwapWindow(window);
            audio->master = settings.master;
            audio->effects = settings.effects;
            audio->ambience = settings.ambience;
            audio->muted = settings.mute;
            audio->pump(terrain->space(), view);
            if (request.vsync_changed)
                SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0);
            if (request.rebuild) {
                try {
                    rebuild(request.map);
                    request.save = true;
                } catch (const std::exception &error) {
                    settings.curvature = terrain->space().radius();
                    settings.seed = static_cast<int>(terrain->seed());
                    ui.message = std::string("Battlefield rebuild rejected: ") + error.what();
                }
            }
            if (request.save)
                settings.save(user / "settings.json", input);
            ++frame;
            if (stop)
                running = false;
        }
        settings.width = width;
        settings.height = height;
        settings.save(user / "settings.json", input);
        if (options.frames || options.diagnostic || options.seconds)
            report(frame, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
        return 0;
    }
};
} // namespace curved
int main(int argc, char **argv) {
    bool automated = false;
    std::filesystem::path user_root;
    try {
        auto config = curved::options(argc, argv);
        automated = config.diagnostic || config.frames > 0 || config.seconds > 0;
        user_root = config.user;
        curved::Application app(std::move(config));
        return app.run();
    } catch (const std::exception &e) {
        std::cerr << "Native prototype failed: " << e.what() << '\n';
        try {
            if (user_root.empty())
                user_root = std::filesystem::u8path(SDL_GetBasePath()) / "user";
            std::filesystem::create_directories(user_root);
            std::ofstream(user_root / "last-error.txt") << "Native prototype failed: " << e.what() << '\n';
        } catch (...) {
        }
        if (!automated)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Curved-world prototype could not start", e.what(),
                                     nullptr);
        return 1;
    }
}
