#include "ui/console.hpp"
#include <cmath>
#include <sstream>
namespace curved {
Handle resolve_positioned(const World &world, RefId ref) {
    if (auto h = world.resolve(ref))
        return h;
    for (auto &f : world.fortresses())
        if (f.handle.ref == ref)
            return f.handle;
    return {};
}
ConsoleResult execute_console(World &w, const std::string &command) {
    std::istringstream s(command);
    std::string verb;
    s >> verb;
    auto error = [](std::string text) { return ConsoleResult{false, std::move(text), {}}; };
    auto end = [&]() {
        std::string extra;
        return !(s >> extra);
    };
    RefId ref = 0;
    if (verb == "release" && end()) {
        w.release();
        return {true, "Returned unit to squad AI", {}};
    }
    if (verb == "select" || verb == "inspect" || verb == "possess") {
        if (!(s >> ref) || !end())
            return error("Expected command <decimal RefId>");
        auto h = resolve_positioned(w, ref);
        if (!h)
            return error("Unknown/stale RefId");
        if (verb == "possess" && !w.possess(h))
            return error("Object has no supported actor controller");
        return {true,
                "Resolved native handle: " + std::to_string(h.ref) + " generation " +
                    std::to_string(h.generation) + " epoch " + std::to_string(h.epoch),
                h};
    }
    if (verb == "spawn") {
        std::string base;
        int faction = -1;
        if (!(s >> base >> faction) || !end())
            return error("spawn <BaseId> <faction 0..3>");
        auto h = w.spawn(base, faction);
        if (!h)
            return error("Spawn rejected: definition, policy, faction or budget");
        return {true, "Spawned " + base + " RefId " + std::to_string(h.ref), h};
    }
    if (verb == "set") {
        std::string name;
        double n;
        if (!(s >> ref >> name >> n) || !end())
            return error("set <RefId> <value name> <number>");
        auto v = ActorValues::resolve(name);
        if (!v || !ActorValues::writable(*v))
            return error(
                "Unsupported write: use a documented base value; health.current uses damage/heal lifecycle");
        if (!w.set_value(w.resolve(ref), *v, n))
            return error("Write rejected: lifetime or value range");
        return {true, "Native base value updated", w.resolve(ref)};
    }
    if (verb == "damage") {
        double amount;
        if (!(s >> ref >> amount) || !end() || !w.damage_actor(w.resolve(ref), amount))
            return error("damage <live actor RefId> <finite amount 0..100000>");
        return {true, "Native damage lifecycle applied", {}};
    }
    if (verb == "mod") {
        std::string source, name;
        double add, mul, seconds;
        if (!(s >> ref >> source >> name >> add >> mul >> seconds) || !end() || !std::isfinite(seconds) ||
            seconds < 0 || seconds > 3600)
            return error("mod <RefId> <source> <value> <add> <multiply> <seconds; 0=permanent>");
        auto v = ActorValues::resolve(name);
        if (!v ||
            !w.add_modifier(
                w.resolve(ref),
                {source, *v, add, mul,
                 seconds == 0 ? 0 : w.stats().ticks + static_cast<std::uint64_t>(std::ceil(seconds * 30))}))
            return error("Modifier rejected by native owner");
        return {true, "Source-owned modifier applied", w.resolve(ref)};
    }
    if (verb == "unmod") {
        std::string source;
        if (!(s >> ref >> source) || !end() || !w.remove_modifier(w.resolve(ref), source))
            return error("unmod <live actor RefId> <source>");
        return {true, "Only the named modifier source removed", w.resolve(ref)};
    }
    if (verb == "relation") {
        int from, to, value;
        if (!(s >> from >> to >> value) || !end() || !w.set_relation(from, to, value))
            return error("relation <from faction> <to faction> <-1|0|1>");
        return {true, "Outgoing relation updated immediately", {}};
    }
    return error("Commands: inspect, select, spawn, possess, release, set, damage, mod, unmod, relation");
}
} // namespace curved
