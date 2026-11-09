# Simulation, identities and control

One World owns gameplay. It runs at exactly 1/30 s per step, independently of
render quality. The application clamps raw wall delta to 0.1 s, accumulated
backlog to 0.4 s and catch-up to 12 steps per rendered frame. Profiles report
clamped wall time and dropped backlog steps. Speed scales accumulated time;
pause drops accumulated backlog. No performance target from Mandelrift is used.

Ordering in each step:

1. Clear last step's event stream; expire visual effects; rebuild fallen keeps
   whose 32-second timer has elapsed; consider round-robin reinforcements.
2. Rebuild spatial candidates. Plan each company on a staggered 23-tick cadence,
   or immediately after invalidation. Actor plans recur after 12–16 ticks.
3. Expire modifiers, decrement cooldowns, accept the latest controller intent,
   plan AI/Lua if due, and commit legal surface movement in RefId order.
4. Rebuild spatial candidates from moved positions. Resolve native attacks in
   RefId order, then advance projectiles and impacts. Earlier lethal attacks
   prevent that victim attacking later in this phase; this is ordered combat.
5. Destroy dead actors' storage, prune companies and clear dead possession.
   Events are available to presentation until the next step.

Public user requests are validated on the application thread between steps.
Spawns, diplomacy, possession, base-value/modifier requests and debug damage
commit at that boundary and are observable by the next render. Movement intents
execute on the next fixed step. Lua decisions commit only after a successful
bounded policy call, in that actor's planning phase. Failed Lua calls leave
native fallback behavior. There are no callbacks for each projectile/hit.

BaseIds are stable namespaced definition strings. RefIds are independent live
instance identities, monotonically allocated within a world and prefixed by
its epoch. A Handle contains RefId, generation and epoch; RefId lookup resolves
through the owning storage map. It contains no public vector index or pointer.
Slot reuse advances generation. Another world's handle, a dead actor and a stale
RefId all fail validation. Death makes the actor unaddressable immediately,
before its slot is reclaimed at cleanup. Keeps remain positioned objects in
their ruined/rebuild lifecycle; they are not corpses masquerading as live actors.

Faction, controller, selected actor and attack target are distinct identities.
Each actor has independent health, morale, targets, equipment and modifiers even
when it shares a BaseId. Composition uses move/melee/projectile/heal/siege/guard
capabilities, attack primitives and authored formation depth, not unit names.

Companies use local allied/enemy health-based strength, morale, formation depth,
objectives and support positions. A ratio below 0.58 causes retreat toward
supporting allied companies or home. Nearby allies enter the actual decision.
Ranged/healing units hold distance; melee closes; siege prefers reachable keeps
in range. Native paths handle constrained approaches. The sample policy adds a
guarding decision and a data-authored flanking exception. Tactical sophistication
and collision avoidance are deliberately modest, not parity with all HTML roles.

Diplomacy is outgoing: -1 enemy, 0 neutral, +1 ally. Self is always allied.
Neutral actors neither attack nor support and do not automatically retaliate.
Asymmetric allies are allowed. Edits immediately invalidate incompatible attack
and guard targets and cached plans. Attacks/heals recheck eligibility when used;
every projectile impact rechecks its launch faction's current outgoing relation,
even when its source actor has died. No retrospective undo of applied damage.

Possession transfers one compatible actor to Player. AI/Lua stops issuing that
actor's intents; the native body, faction, squad, values, equipment, cooldowns
and lifecycle continue. Movement is relative to the orbit camera's local frame.
Left mouse or the ability action uses its data-defined attack/heal/siege
capability, with native range, targets and cooldown. Release returns that same
instance to AI and schedules a fresh plan. Death, map replacement or free-flight
switch safely releases control. There is no invulnerability or direct possession
of buildings. Controller input delivery is planned through semantic actions;
gamepad support has not been implemented or physically tested.

Central budgets are in src/sim/limits.hpp:

| Resource | Hard bound | Overflow / cleanup |
| --- | --- | --- |
| Actors | 384; default shared live limit 160 | Reject spawn; lowered limits retain existing actors; dead slots reclaimed at step end |
| Company size / companies | 12 / 64 | Join compatible nearby company, otherwise reject when full; empty groups pruned |
| Projectiles | 128 | Reject launch without consuming cooldown; impact/lifetime removes it |
| Effects / events | 256 / 128 per step | Drop new presentation feedback and increment counters; effects expire |
| Source modifiers per actor | 16 | Reject additional source/value entries; expiry/removal bounded |
| Lua plans per tick | 16 | Keep native plan; retry policy at the next actor planning cadence; count skips |
| Lua nearby observations | 24, within 24 m | Sort by intrinsic distance then RefId; truncate |
| Audio voices | 24 | Drop or replace lowest priority voice; completed voices removed |

Detachment requests expose all requested actors or none. RefId serials may have
gaps after rejected/rolled-back operations. No guarantee of contiguous IDs is
made. Presentation effects and audio never consume simulation RNG. Replay is
tested for the same seed, content and executable build; world epochs are excluded
from the digest. Cross-platform/compiler bitwise identity is not promised.
