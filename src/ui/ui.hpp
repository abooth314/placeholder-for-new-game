#pragma once
#include "app/input.hpp"
#include "app/settings.hpp"
#include "audio/audio.hpp"
#include "view/renderer.hpp"
namespace curved {
struct UiState {
    Handle selected, player_target;
    bool maps = false, diplomacy = false, council = false, inspector = false, help = false, paused = false,
         mirror = false, show_paths = false;
    int spawn_unit = 0, spawn_faction = 0;
    char command[256]{}, ref_input[64]{};
    std::string message = "Four kingdoms are gathering. Select a unit to take control.";
    double nav_length = -1;
    bool blocking() const {
        return maps || diplomacy || council || help;
    }
};
struct UiRequests {
    bool rebuild = false, vsync_changed = false, save = false;
    int map = -1;
};
void style_ui();
UiRequests draw_ui(UiState &state, World &world, Renderer &renderer, Audio &audio, CameraRig &camera,
                   Input &input, Settings &settings, int width, int height, double frame_ms);
void draw_world_overlays(const UiState &state, const World &world, const Camera &camera, int width,
                         int height, bool health_bars);
void toggle_possession(UiState &state, World &world, CameraRig &camera);
void focus_battle(World &world, CameraRig &camera);
} // namespace curved
