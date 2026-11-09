#pragma once
#include "content/catalog.hpp"
#include "content/lua_policy.hpp"
#include "sim/entities.hpp"
#include "world/spatial.hpp"
#include <memory>

namespace curved {
struct Squad {
    std::uint32_t id;
    int faction;
    std::vector<Handle> members;
    h3::Vec center, objective;
    ActorState state = ActorState::Gathering;
    double ratio = 1, morale = 1;
};
struct Fortress {
    Handle handle;
    int faction;
    h3::Vec position;
    double health = 2600, max_health = 2600, rebuild_at = 0;
    bool ruined = false;
};
enum class Feedback { Melee, Arrow, Heal, Siege, Impact, Collapse, Hoof, Magic };
struct BattleEvent {
    std::string sound;
    h3::Vec position;
    Feedback kind;
    int faction;
};
struct VisualEffect {
    h3::Vec position;
    Feedback kind;
    int faction;
    double age = 0, life = .6;
};
struct Projectile {
    h3::Vec position, previous, from_base, to_base;
    double from_height, to_height, duration, age = 0, arc, damage, splash, building_multiplier;
    int faction;
    RefId source;
    bool siege;
};
struct FactionStats {
    unsigned alive = 0, kills = 0, losses = 0, spawned = 0;
    double healing = 0;
};
struct WorldStats {
    std::uint64_t ticks = 0, melee = 0, shots = 0, hits = 0, heals = 0, ruins = 0, rebuilds = 0, retreats = 0,
                  paths = 0, path_failures = 0, lua_calls = 0, lua_errors = 0;
    std::uint64_t actor_overflow = 0, projectile_overflow = 0, effect_overflow = 0, event_overflow = 0;
    std::uint64_t lua_budget_skips = 0;
    std::size_t peak_actors = 0, peak_projectiles = 0, peak_effects = 0;
    double last_tick_ms = 0;
};
struct PlayerIntent {
    double east = 0, north = 0;
    bool primary = false, ability = false;
    Handle target;
};
class World {
  public:
    World(const Catalog &catalog, const Terrain &terrain, bool populate = true, unsigned initial_limit = 160);
    void step();
    Handle spawn(const std::string &base, int faction, std::optional<h3::Vec> near = {});
    std::vector<Handle> spawn_detachment(const std::string &base, int faction, unsigned count);
    bool set_relation(int from, int to, int value, bool mirrored = false);
    bool set_preset(const std::string &id);
    int relation(int from, int to) const;
    bool possess(Handle handle);
    void release();
    Handle possessed() const {
        return possessed_;
    }
    void player_intent(PlayerIntent intent);
    bool move_to(Handle handle, h3::Vec target);
    bool use_primary(Handle source, Handle target = {});
    bool add_modifier(Handle handle, Modifier modifier);
    bool remove_modifier(Handle handle, const std::string &source);
    bool set_value(Handle handle, Value value, double number);
    bool damage_actor(Handle target, double damage, int source_faction = -1);
    bool damage_fortress(Handle target, double damage, int source_faction);
    const Actor *inspect(Handle handle) const {
        return entities_.get(handle);
    }
    Handle resolve(RefId ref) const {
        return entities_.resolve(ref);
    }
    std::vector<Handle> actors() const {
        return entities_.live();
    }
    const std::vector<Squad> &squads() const {
        return squads_;
    }
    const std::array<Fortress, 4> &fortresses() const {
        return fortresses_;
    }
    const std::vector<Projectile> &projectiles() const {
        return projectiles_;
    }
    const std::vector<VisualEffect> &effects() const {
        return effects_;
    }
    const std::vector<BattleEvent> &events() const {
        return events_;
    }
    const std::array<FactionStats, 4> &factions() const {
        return faction_stats_;
    }
    const WorldStats &stats() const {
        return stats_;
    }
    const std::vector<std::string> &errors() const {
        return errors_;
    }
    const Terrain &terrain() const {
        return terrain_;
    }
    const Catalog &catalog() const {
        return catalog_;
    }
    double time() const {
        return stats_.ticks * limits::step;
    }
    unsigned population_limit = 160;
    double reinforce_rate = 1;
    bool combat = true;
    std::uint64_t digest() const;
    void replan();

  private:
    void plan_squad(Squad &squad);
    void plan_actor(Actor &actor);
    void route(Actor &actor, h3::Vec goal);
    void move(Actor &actor);
    void attack(Actor &actor, Handle target = {});
    void step_projectiles();
    void impact(const Projectile &projectile);
    void feedback(const std::string &sound, h3::Vec p, Feedback kind, int faction);
    void finish_deaths();
    Handle nearest_target(const Actor &actor, bool support, double range) const;
    double random();
    Squad *squad(std::uint32_t id);
    const Catalog &catalog_;
    const Terrain &terrain_;
    Entities entities_;
    SpatialIndex spatial_;
    std::vector<Squad> squads_;
    std::array<Fortress, 4> fortresses_{};
    std::array<std::array<int, 4>, 4> relations_;
    std::array<FactionStats, 4> faction_stats_{};
    std::vector<Projectile> projectiles_;
    std::vector<VisualEffect> effects_;
    std::vector<BattleEvent> events_;
    std::vector<Handle> deaths_;
    std::map<std::string, std::unique_ptr<LuaPolicy>> policies_;
    std::vector<std::string> errors_;
    WorldStats stats_;
    Handle possessed_;
    PlayerIntent input_;
    std::uint32_t rng_, next_squad_ = 1;
    std::size_t lua_plans_ = 0;
    double reinforce_clock_ = 0;
    bool plans_invalid_ = true;
};
} // namespace curved
