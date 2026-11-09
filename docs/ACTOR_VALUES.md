# Native actor values

The canonical implementation is ActorValues. String names resolve to typed IDs
at UI/content boundaries. Simulation loops use typed IDs. Attack base damage,
range, cooldown, projectile speed/arc and splash stay in attack definitions.
Faction, controller and capability eligibility are typed properties.

| Name | Units; base/default | Legal effective range | Write policy / consumer |
| --- | --- | --- | --- |
| health.current | hit points; initialized from health.max | 0..effective max | Read only; native damage/healing/death lifecycle |
| health.max | hit points; unit definition | 1..10000 | Base + source modifiers; lowering clamps current; raising never heals |
| morale.current | fraction; initially 1 | Native planning clamps .15..1.25 | Read only, tactical owner; may exceed 100% |
| movement.max_speed | metres/s; unit definition | 0..20 | Base + source modifiers, used by real surface movement |
| movement.velocity | metres/s actually travelled this step | 0..effective movement limit | Read only; zero when stationary |
| combat.damage_multiplier | factor; 1 | 0..10 | Base + source modifiers, applied to attack definition's damage |
| combat.cooldown_multiplier | factor; 1 | .1..10 | Base + source modifiers, applied to cooldown when an attack executes |
| support.heal_multiplier | factor; 1 | 0..10 | Base + source modifiers, applied to native healing |

For each writable value: effective=clamp((base+sum(additive))*product(multiplier)).
All additions precede all multiplications. Base writes must lie within that
value's legal range; effective results clamp after aggregation. Non-finite
numbers, multipliers outside [0,10], absolute additive terms above 10000 and
unknown/read-only value IDs are rejected.

A modifier has a source string, typed value, additive term, multiplier and
expiry tick. Same source+value replaces that one contribution. Different sources
stack. Removing a source removes only that source's contributions. Expiry 0 is
permanent; otherwise the modifier disappears at the beginning of the actor
phase when current tick >= expiry. Equipment uses equipment:<BaseId> sources.
At most 16 source/value contributions are retained per actor.

Reducing effective maximum health clamps current health; increasing it preserves
current health. Unrelated edits never heal or revive. Native healing is capped
by effective max and cannot revive a dead actor. Damage is nonnegative, clamps
at zero, and triggers immediate lifetime invalidation before slot cleanup.
Changing a multiplier does not rewrite cooldown already in progress or damage
already snapshotted into an airborne projectile.

Inspection and console reads use these owners. Examples:

    set <RefId> movement.max_speed 4
    mod <RefId> spell:haste movement.max_speed 0.5 1.2 8
    unmod <RefId> spell:haste
    damage <RefId> 20

set health.current is explicitly rejected. Lua's actual policy needs health
observations, not unconstrained numeric writes; both self and nearby health
tables come from these native values. Additional writable mechanics need native
validation and a real consumer before they become an API.
