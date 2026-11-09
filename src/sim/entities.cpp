#include "sim/entities.hpp"
#include <stdexcept>
namespace curved {
Entities::Entities(std::uint32_t epoch) : epoch_(epoch) {
    if (epoch == 0 || epoch >= 0x7fffffff)
        throw std::invalid_argument("Invalid world epoch");
}
Handle Entities::create(std::string base, int faction, h3::Vec p, double health, double speed) {
    if (faction < 0 || faction > 3 || !h3::valid(p) || next_ >= 0x7fffffff)
        return {};
    for (std::size_t i = 0; i < slots_.size(); ++i)
        if (!slots_[i].actor) {
            auto &s = slots_[i];
            Handle h{(static_cast<RefId>(epoch_) << 32) | next_++, s.generation, epoch_};
            Actor a;
            a.handle = h;
            a.base = std::move(base);
            a.faction = faction;
            a.values = ActorValues(health, speed);
            a.position = p;
            a.move_goal = p;
            s.actor = std::move(a);
            refs_.emplace(h.ref, i);
            return h;
        }
    return {};
}
Actor *Entities::get(Handle h) {
    if (!h || h.epoch != epoch_)
        return nullptr;
    auto it = refs_.find(h.ref);
    if (it == refs_.end())
        return nullptr;
    auto &s = slots_[it->second];
    return s.generation == h.generation && s.actor && s.actor->values.alive() ? &*s.actor : nullptr;
}
const Actor *Entities::get(Handle h) const {
    return const_cast<Entities *>(this)->get(h);
}
Handle Entities::resolve(RefId ref) const {
    auto it = refs_.find(ref);
    if (it == refs_.end())
        return {};
    const auto &s = slots_[it->second];
    return s.actor && s.actor->values.alive() ? s.actor->handle : Handle{};
}
bool Entities::destroy(Handle h) {
    if (h.epoch != epoch_)
        return false;
    auto it = refs_.find(h.ref);
    if (it == refs_.end())
        return false;
    auto &s = slots_[it->second];
    if (s.generation != h.generation)
        return false;
    s.actor.reset();
    ++s.generation;
    refs_.erase(it);
    return true;
}
std::vector<Handle> Entities::live() const {
    std::vector<Handle> result;
    result.reserve(refs_.size());
    for (auto [ref, slot] : refs_) {
        (void)ref;
        auto &s = slots_[slot];
        if (s.actor && s.actor->values.alive())
            result.push_back(s.actor->handle);
    }
    return result;
}
const char *state_name(ActorState s) {
    switch (s) {
    case ActorState::Gathering:
        return "Gathering company";
    case ActorState::Advancing:
        return "Advancing";
    case ActorState::Holding:
        return "Holding the line";
    case ActorState::Retreating:
        return "Falling back to allies";
    case ActorState::Melee:
        return "Engaged in melee";
    case ActorState::Firing:
        return "Firing from the line";
    case ActorState::Healing:
        return "Tending the wounded";
    case ActorState::Bombarding:
        return "Bombarding";
    case ActorState::Guarding:
        return "Guarding allied support";
    case ActorState::Flanking:
        return "Taking a navigable flank";
    case ActorState::Player:
        return "Possessed by player";
    }
    return "Unknown";
}
} // namespace curved
