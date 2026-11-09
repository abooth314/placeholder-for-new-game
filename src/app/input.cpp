#include "app/input.hpp"
namespace curved {
Input::Input()
    : keys_{SDL_SCANCODE_W, SDL_SCANCODE_S,   SDL_SCANCODE_A, SDL_SCANCODE_D,     SDL_SCANCODE_R,
            SDL_SCANCODE_C, SDL_SCANCODE_Q,   SDL_SCANCODE_E, SDL_SCANCODE_SPACE, SDL_SCANCODE_V,
            SDL_SCANCODE_P, SDL_SCANCODE_X,   SDL_SCANCODE_F, SDL_SCANCODE_O,     SDL_SCANCODE_M,
            SDL_SCANCODE_K, SDL_SCANCODE_TAB, SDL_SCANCODE_G, SDL_SCANCODE_H} {}
const char *action_name(Action a) {
    static const char *names[] = {"Move forward",
                                  "Move back",
                                  "Move left",
                                  "Move right",
                                  "Fly up",
                                  "Fly down",
                                  "Orbit left",
                                  "Orbit right",
                                  "Pause / resume",
                                  "Toggle free flight",
                                  "Possess / release unit",
                                  "Use unit ability",
                                  "Focus battle",
                                  "Overview",
                                  "Battlefields",
                                  "Diplomacy",
                                  "War council",
                                  "Native inspector",
                                  "Field guide"};
    return names[static_cast<unsigned>(a)];
}
void Input::begin_frame() {
    pressed_.fill(false);
}
void Input::clear() {
    down_.fill(false);
    pressed_.fill(false);
}
void Input::rebind(Action a) {
    pending_ = a;
    clear();
}
bool Input::set_binding(Action a, int code) {
    if (code <= SDL_SCANCODE_UNKNOWN || code >= SDL_SCANCODE_COUNT || code == SDL_SCANCODE_ESCAPE)
        return false;
    const auto index = static_cast<unsigned>(a);
    const auto prior = keys_[index];
    for (unsigned i = 0; i < keys_.size(); ++i)
        if (i != index && keys_[i] == code)
            keys_[i] = prior;
    keys_[index] = static_cast<SDL_Scancode>(code);
    return true;
}
bool Input::event(const SDL_Event &e, bool capture) {
    if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        clear();
        return false;
    }
    if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP)
        return false;
    if (pending_) {
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.scancode != SDL_SCANCODE_ESCAPE)
                set_binding(*pending_, e.key.scancode);
            pending_.reset();
            clear();
            return true;
        }
        return false;
    }
    if (capture) {
        clear();
        return false;
    }
    for (unsigned i = 0; i < keys_.size(); ++i)
        if (keys_[i] == e.key.scancode) {
            down_[i] = e.type == SDL_EVENT_KEY_DOWN;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
                pressed_[i] = true;
        }
    return false;
}
bool Input::down(Action a) const {
    return down_[static_cast<unsigned>(a)];
}
bool Input::pressed(Action a) const {
    return pressed_[static_cast<unsigned>(a)];
}
} // namespace curved
