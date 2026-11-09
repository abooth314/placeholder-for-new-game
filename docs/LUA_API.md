# Lua native policy API 1

The sandbox uses Lua 5.4.9, a 2 MiB tracked allocator per behavior and a
20,000-instruction budget per load or planning call, with a hook every 100
instructions. Text source is limited to 64 KiB. Each file must return one
function. Policies run on staggered actor plans, at most sixteen calls per
native tick. There is no per-hit/projectile callback or writable Lua simulation.

Base/table/math and limited string libraries are available. No OS, I/O,
package/module loading, network, debug, coroutine, load/loadfile/dofile,
collectgarbage, print/warn, pcall/xpcall, string pattern matching/dump, or random
functions are exposed. Error-catching functions are removed so a script cannot
catch the instruction hook and continue indefinitely. Host data are copied
tables, not pointers/userdata. Standard C-library operations are constrained by
the memory/source/input limits; this is a prototype policy sandbox, not an
isolation boundary for adversarial native code.

The function receives self:

| Field | Meaning |
| --- | --- |
| ref | Live RefId, exact Lua integer; no storage index |
| health / max_health | Native current/effective maximum hit points |
| ignore_injured | Authored per-unit exception option |

Actual exposed functions:

| Function | Context, arguments and return | Failure and timing |
| --- | --- | --- |
| native.observe(radius) | Current policy actor; finite metres in [0,24]; returns up to 24 nearest observation tables | Invalid range/context raises a bounded error; snapshot from this planning phase |
| native.guard(ref) | Observed live allied healer RefId; actor must have guard capability; returns true | Invalid eligibility/lifetime/capability raises error; native owner revalidates before applying |
| native.advance() | No arguments; returns true and selects normal company behavior | Commits the native fallback/company plan after successful return |
| native.flank() | No arguments; returns true | Native owner chooses a legal offset/path; refuses to override a company retreat |
| native.cancel() | No arguments; returns true | Cancels movement intent for this plan; existing cooldowns/projectiles continue |

Observation fields: ref, distance (ambient H³ metres), health, max_health,
ally, enemy, healer. Neutral observations have ally=false and enemy=false.
The snapshot is bounded and sorted by intrinsic distance, then RefId. At most
four intent calls per policy invocation; last successful choice wins. The whole
choice is discarded if the call fails. A failed script is reported in the
native inspector and the actor keeps its native tactical plan. Requests resolve
through current native handles and diplomacy before movement/combat.

A working example, used by the enabled pack:

    return function(self)
        if self.ignore_injured then
            native.flank()
            return
        end
        local patient = nil
        for _, actor in ipairs(native.observe(24)) do
            if actor.ally and actor.healer and actor.health < actor.max_health * 0.8 then
                patient = actor.ref
                break
            end
        end
        if patient then native.guard(patient) else native.advance() end
    end

The checked-in guardian.lua additionally ranks patient urgency. Lua cannot
spawn arbitrary primitives, mutate health, override movement integration,
fire unchecked projectiles, read arbitrary filesystem data or transfer player
control. Scheduling/reinforcement/mutation APIs without a current prototype
consumer have deliberately not been exposed.
