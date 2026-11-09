# Content and external mods

Both shipped content and mods use schema 1 / native API 1. Paths resolve from
the executable's asset root, never from a hardcoded developer directory.
Normally assets/core contains the shipped pack; user/mods/<folder> contains
user-writable external packs. user/enabled-mods.json explicitly selects folders:

    {"schema":1,"enabled":["wardens"]}

The application loads core first and enabled folders in listed order. Each
pack.json declares id, schema, api, dependencies, replace and files. A dependency
must already be loaded. Namespaced definition IDs use lowercase letters,
digits, underscores, hyphens and a colon. Pack paths cannot escape their pack;
absolute paths and duplicate JSON keys are rejected. Unknown fields, enum names,
invalid numeric ranges, missing references and incompatible capabilities fail
with pack/file/definition context.

The loader constructs a complete candidate Catalog and validates all references
and Lua source before returning it. An exception cannot half-publish a catalog.
Duplicate records require replace:true and replace the complete record.
There is no field merge, hot reload, migration or arbitrary native plug-in ABI.
Restart the application to load edited content; rebuild the world for map
parameter changes exposed in the UI.

Definition collections:

| Collection | Actual consumer |
| --- | --- |
| factions | Four identities, names/colors, statistics, diplomacy |
| units | Values, capability composition, attack/appearance/behavior/equipment links, formation depth and reinforcement weight |
| attacks | melee, projectile and heal primitives; siege composes projectile with siege capability and building/splash parameters |
| equipment | Source-owned actor modifiers applied to independent new instances |
| maps | Seeded native terrain evaluator parameters: radius, relief, river count, palette |
| scenario / presets | Initial populations, reinforcement timing, outgoing relations and live diplomacy presets |
| behaviors | Relative Lua policy files returning bounded tactical functions |
| visuals / sounds | Procedural recipe evaluators described in RECIPES.md |

Unit policy fields are distinct: discover permits catalog discovery,
menu controls deployment-menu exposure, spawn permits native spawn requests,
reinforce controls automatic reinforcement eligibility, and capabilities
control legal mechanics. The UI requires discover+menu; native spawns additionally
require spawn. Reinforcements require spawn+reinforce and positive weight.
Hiding a definition does not remove existing actors or its capabilities.

The enabled independent Wardens pack adds Lantern Warden and Road Vanguard,
their editable mesh recipe, travelling equipment and guardian.lua. The Warden
chooses an injured allied healer to guard, otherwise joins the company.
The Vanguard's ignore_injured flag makes the **same Lua function** choose a flank.
There is no C++ branch naming either unit in gameplay. The application's
diagnostic fixture names the Warden only to verify its actual rendered presence.

Try editing a unit's name, health/speed, attack link, colors/parts, equipment,
spawn policies or Lua guard threshold in user/mods/wardens, then restart.
Compatible new compositions need no C++ rebuild. A new primitive such as a
mounted charge, spear brace, champion cleave, shield absorption, terrain
collision shape or new spell integrator needs a native capability/evaluator.

Budgets: at most 16 packs, 16 definition files per pack, 2 MiB per JSON file,
64 KiB per script, 64 unit/attack/visual definitions, 32 sounds, 16 behaviors,
four factions, and twelve map records. Mesh parts, sound ranges and Lua have
additional limits in their canonical contracts. Diagnostics are local; there
is no mod download or network execution feature.

tools/author-starter-content.py is an optional one-time authoring example.
It writes only missing starter files and never runs during build/gameplay.
The checked-in JSON and Lua are the runtime authoring sources.
