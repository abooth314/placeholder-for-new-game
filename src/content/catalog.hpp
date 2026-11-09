#pragma once
#include "sim/values.hpp"
#include "world/terrain.hpp"
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace curved {
enum class Capability : unsigned { Move = 1, Melee = 2, Projectile = 4, Heal = 8, Siege = 16, Guard = 32 };
enum class AttackMode { Melee, Projectile, Heal };
struct AttackDef {
    std::string id, sound;
    AttackMode mode;
    double damage, range, cooldown, flight_speed, arc, splash, building_multiplier;
};
struct UnitDef {
    std::string id, name, attack, appearance, behavior, equipment;
    unsigned capabilities = 0;
    double health = 100, speed = 3, formation_depth = 0, radius = .6, weight = 1;
    bool discover = true, menu = true, spawn = true, reinforce = true, ignore_injured = false;
    bool has(Capability c) const {
        return (capabilities & static_cast<unsigned>(c)) != 0;
    }
};
struct FactionDef {
    std::string id, name, short_name;
    std::array<float, 3> color;
};
enum class Primitive { Box, Cone, Roof, Beam, Ellipsoid };
struct PartRecipe {
    Primitive primitive;
    std::array<float, 3> center, size, color;
    std::array<float, 3> end{};
    float tint = 0, motion = 0;
    int sides = 8;
};
struct VisualRecipe {
    std::string id;
    std::vector<PartRecipe> parts;
};
struct SoundRecipe {
    std::string id;
    double duration, frequency, end_frequency, noise, tone, gain;
    int priority;
};
struct BehaviorDef {
    std::string id, source, location;
};
struct EquipmentDef {
    std::string id;
    std::vector<Modifier> modifiers;
};
struct ScenarioDef {
    int starting_per_faction = 16;
    double reinforce_seconds = 2.5;
    std::array<std::array<int, 4>, 4> relations{};
};
struct DiplomacyPreset {
    std::string id, name;
    std::array<std::array<int, 4>, 4> relations;
};
struct Catalog {
    std::map<std::string, UnitDef> units;
    std::map<std::string, AttackDef> attacks;
    std::map<std::string, VisualRecipe> visuals;
    std::map<std::string, SoundRecipe> sounds;
    std::map<std::string, BehaviorDef> behaviors;
    std::map<std::string, EquipmentDef> equipment;
    std::vector<FactionDef> factions;
    std::vector<MapRecipe> maps;
    std::vector<DiplomacyPreset> presets;
    ScenarioDef scenario;
    std::vector<std::string> packs;
    std::string fingerprint;
};
// Transactional: caller's catalog is untouched when any candidate fails.
Catalog load_catalog(const std::filesystem::path &assets, const std::filesystem::path &user);
unsigned capability_from_name(const std::string &name);
} // namespace curved
