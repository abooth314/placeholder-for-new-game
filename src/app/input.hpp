#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <optional>
namespace curved {
enum class Action {
    Forward,
    Back,
    Left,
    Right,
    Rise,
    Descend,
    OrbitLeft,
    OrbitRight,
    Pause,
    Freecam,
    Possess,
    Ability,
    Focus,
    Overview,
    Maps,
    Diplomacy,
    Council,
    Inspect,
    Help,
    Count
};
const char *action_name(Action action);
class Input {
  public:
    Input();
    bool event(const SDL_Event &event, bool capture);
    bool down(Action action) const;
    bool pressed(Action action) const;
    void begin_frame();
    void clear();
    void rebind(Action action);
    bool binding() const {
        return pending_.has_value();
    }
    SDL_Scancode binding(Action action) const {
        return keys_[static_cast<unsigned>(action)];
    }
    bool set_binding(Action action, int code);

  private:
    std::array<SDL_Scancode, static_cast<unsigned>(Action::Count)> keys_;
    std::array<bool, static_cast<unsigned>(Action::Count)> down_{}, pressed_{};
    std::optional<Action> pending_;
};
} // namespace curved
