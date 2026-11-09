#include "content/lua_policy.hpp"
#include "content/catalog.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace curved {
struct LuaPolicy::Impl {
    lua_State *state = nullptr;
    std::size_t bytes = 0;
    int instructions = 0, function = LUA_NOREF, commands = 0;
    const PolicyContext *context = nullptr;
    PolicyDecision decision;
    static constexpr std::size_t limit = 2 * 1024 * 1024;
    static void *allocate(void *ud, void *p, std::size_t old, std::size_t size) {
        auto &me = *static_cast<Impl *>(ud);
        if (!p)
            old = 0;
        if (size == 0) {
            std::free(p);
            me.bytes -= old;
            return nullptr;
        }
        if (size > limit || me.bytes - old > limit - size)
            return nullptr;
        void *q = std::realloc(p, size);
        if (q)
            me.bytes = me.bytes - old + size;
        return q;
    }
    static Impl &owner(lua_State *L) {
        return **static_cast<Impl **>(lua_getextraspace(L));
    }
    static void hook(lua_State *L, lua_Debug *) {
        auto &me = owner(L);
        me.instructions += 100;
        if (me.instructions > 20000)
            luaL_error(L, "instruction budget (20000) exceeded");
    }
    static int observe(lua_State *L) {
        auto &me = owner(L);
        const double r = luaL_checknumber(L, 1);
        if (!me.context || !std::isfinite(r) || r < 0 || r > 24)
            return luaL_error(L, "observe radius must be finite in [0,24]");
        lua_newtable(L);
        int i = 0;
        for (auto &o : me.context->nearby)
            if (o.distance <= r && i < 24) {
                lua_createtable(L, 0, 7);
                lua_pushinteger(L, static_cast<lua_Integer>(o.ref));
                lua_setfield(L, -2, "ref");
                lua_pushnumber(L, o.distance);
                lua_setfield(L, -2, "distance");
                lua_pushnumber(L, o.health);
                lua_setfield(L, -2, "health");
                lua_pushnumber(L, o.max_health);
                lua_setfield(L, -2, "max_health");
                lua_pushboolean(L, o.ally);
                lua_setfield(L, -2, "ally");
                lua_pushboolean(L, o.enemy);
                lua_setfield(L, -2, "enemy");
                lua_pushboolean(L, o.healer);
                lua_setfield(L, -2, "healer");
                lua_rawseti(L, -2, ++i);
            }
        return 1;
    }
    static int choose(lua_State *L) {
        auto &me = owner(L);
        if (!me.context || ++me.commands > 4)
            return luaL_error(L, "policy command budget exceeded");
        const auto intent = static_cast<PolicyIntent>(lua_tointeger(L, lua_upvalueindex(1)));
        std::uint64_t target = 0;
        if (intent == PolicyIntent::Guard) {
            if (!me.context->guard_capability)
                return luaL_error(L, "actor lacks guard capability");
            const auto n = luaL_checkinteger(L, 1);
            if (n <= 0)
                return luaL_error(L, "invalid RefId");
            target = static_cast<std::uint64_t>(n);
            auto it = std::find_if(me.context->nearby.begin(), me.context->nearby.end(),
                                   [&](auto &o) { return o.ref == target && o.ally && o.healer; });
            if (it == me.context->nearby.end())
                return luaL_error(L, "guard requires an observed allied support actor");
        }
        me.decision = {intent, target};
        lua_pushboolean(L, true);
        return 1;
    }
    static int initialize(lua_State *L) {
        auto &me = owner(L);
        luaL_requiref(L, "_G", luaopen_base, 1);
        lua_pop(L, 1);
        luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(L, 1);
        luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(L, 1);
        luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(L, 1);
        for (const char *key :
             {"dofile", "loadfile", "load", "collectgarbage", "print", "warn", "pcall", "xpcall"}) {
            lua_pushnil(L);
            lua_setglobal(L, key);
        }
        lua_getglobal(L, "math");
        lua_pushnil(L);
        lua_setfield(L, -2, "random");
        lua_pushnil(L);
        lua_setfield(L, -2, "randomseed");
        lua_pop(L, 1);
        lua_getglobal(L, "string");
        for (const char *key : {"find", "match", "gmatch", "gsub", "dump"}) {
            lua_pushnil(L);
            lua_setfield(L, -2, key);
        }
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushcfunction(L, observe);
        lua_setfield(L, -2, "observe");
        for (auto [name, intent] : {std::pair{"guard", PolicyIntent::Guard},
                                    {"advance", PolicyIntent::Advance},
                                    {"flank", PolicyIntent::Flank},
                                    {"cancel", PolicyIntent::Hold}}) {
            lua_pushinteger(L, static_cast<int>(intent));
            lua_pushcclosure(L, choose, 1);
            lua_setfield(L, -2, name);
        }
        lua_setglobal(L, "native");
        (void)me;
        return 0;
    }
    static int invoke(lua_State *L) {
        auto &me = owner(L);
        const auto &context = *me.context;
        lua_rawgeti(L, LUA_REGISTRYINDEX, me.function);
        lua_createtable(L, 0, 4);
        lua_pushinteger(L, static_cast<lua_Integer>(context.self));
        lua_setfield(L, -2, "ref");
        lua_pushnumber(L, context.health);
        lua_setfield(L, -2, "health");
        lua_pushnumber(L, context.max_health);
        lua_setfield(L, -2, "max_health");
        lua_pushboolean(L, context.ignore_injured);
        lua_setfield(L, -2, "ignore_injured");
        lua_call(L, 1, 0);
        return 0;
    }
    ~Impl() {
        if (state)
            lua_close(state);
    }
};
LuaPolicy::LuaPolicy(const BehaviorDef &definition) : impl_(std::make_unique<Impl>()) {
    auto &me = *impl_;
    me.state = lua_newstate(Impl::allocate, &me);
    if (!me.state)
        throw std::runtime_error("Lua allocation failed");
    *static_cast<Impl **>(lua_getextraspace(me.state)) = &me;
    lua_sethook(me.state, Impl::hook, LUA_MASKCOUNT, 100);
    lua_pushcfunction(me.state, Impl::initialize);
    if (lua_pcall(me.state, 0, 0, 0) != LUA_OK)
        throw std::runtime_error("Lua sandbox initialization failed");
    if (luaL_loadbufferx(me.state, definition.source.data(), definition.source.size(),
                         definition.location.c_str(), "t") != LUA_OK ||
        lua_pcall(me.state, 0, 1, 0) != LUA_OK) {
        const std::string error = lua_tostring(me.state, -1) ? lua_tostring(me.state, -1) : "Lua error";
        throw std::runtime_error(definition.id + ": " + error);
    }
    if (!lua_isfunction(me.state, -1))
        throw std::runtime_error(definition.id + ": policy file must return a function");
    me.function = luaL_ref(me.state, LUA_REGISTRYINDEX);
}
LuaPolicy::~LuaPolicy() = default;
PolicyDecision LuaPolicy::plan(const PolicyContext &context) {
    auto &me = *impl_;
    auto *L = me.state;
    me.context = &context;
    me.instructions = 0;
    me.commands = 0;
    me.decision = {};
    lua_pushcfunction(L, Impl::invoke);
    const int result = lua_pcall(L, 0, 0, 0);
    me.context = nullptr;
    if (result != LUA_OK) {
        const std::string error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua budget/allocation error";
        lua_settop(L, 0);
        throw std::runtime_error(error);
    }
    return me.decision;
}
std::size_t LuaPolicy::memory_bytes() const {
    return impl_->bytes;
}
void LuaPolicy::validate(const BehaviorDef &definition) {
    LuaPolicy policy(definition);
}
} // namespace curved
