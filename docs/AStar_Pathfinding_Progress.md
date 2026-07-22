# A\* Pathfinding for Enemy AI — Progress Report

**Project:** LightsOut (ClockWorkz Engine)
**Feature:** A\* pathfinding for enemy navigation around obstacles
**Author:** Ching Wei An
**Date:** 17 June 2026 (updated 22 July 2026)
**Status:** Root cause found and fixed — grid now marks full wall extents;
pending final in-game confirmation

---

## 1. Objective

Replace the enemy's naive line-of-sight movement with A\* pathfinding so
enemies navigate around walls instead of walking straight into them while
chasing the player.

**Baseline (before changes):**
The Chase state in `enemy_ai.lua` steered directly toward the player every
frame using a normalised direction vector (`norm(px - ex, py - ey)`), with no
awareness of walls or obstacles.

---

## 2. Work Completed

### 2.1 Investigation
- Audited `enemy_ai.lua`; confirmed Chase used direct line-of-sight steering
  with no obstacle handling.
- Confirmed tile size = **100 world units** from `assets/configs/Prefab.json`
  and cross-checked wall positions in `Level_01_autosave.json`.
- Identified that walls carry **no Tag component**, so `World.eachByTag("Wall")`
  could not be used to enumerate them.

### 2.2 Engine Change — `engine/src/Systems/ScriptSystem.cpp`
- Added `#include "Components/Collider.hpp"`.
- Added new Lua binding **`World.eachWall(fn)`**, which iterates all colliders
  flagged `isSolid && blocksLight && !moveable` (i.e. wall tiles) and passes
  each to a Lua callback.
- **Note:** This is a C++ change and requires an engine rebuild to take effect.

### 2.3 Gameplay Change — `assets/scripts/components/enemy_ai.lua`
- **`buildGrid()`** — runs in `onStart`; scans wall entities via
  `World.eachWall` and records blocked cells in a sparse table at 100-unit
  resolution.
- **`findPath()`** — full A\* implementation:
  - 8-directional movement
  - Octile distance heuristic
  - Diagonal corner-cutting prevention
  - 400-iteration safety cap to avoid frame spikes on unsolvable maps
- **Chase state** rewired to follow A\* waypoints; recalculates every
  `pathInterval` seconds (default 0.5s), with a direct-steering fallback when no
  path is found.
- Fixed `worldToCell` rounding (removed `+0.5`) and added a half-cell offset to
  waypoint world coordinates so the enemy steers to cell centres.
- Added temporary debug logging for grid cell count and path recalculation.

---

## 3. Root Cause (found 22 July 2026)

**Enemies phased through walls because the grid only ever marked one cell per
wall collider — the cell containing its centre.**

The engine's `TilemapCollisionBuilder` performs a **greedy-rectangle merge** at
runtime: long strips of individual wall tiles are collapsed into a single large
collider, and the original per-tile colliders have `isSolid` disabled. Example
from `Level_Save_05.json`: a merged collider of **`width 4000 × height 100`**
(40 tiles wide) centred at `(-600, -1050)`.

The old `buildGrid()` did `worldToCell(pos.x, pos.y)` and marked that single
cell. So a 40-tile wall blocked **1** grid cell and left **39** walkable. A\*
happily routed straight through the gap → enemy walked through the wall.

(This also explains the earlier `blocked cells: 0` symptom: before the 21 July
filter fix, `eachWall` matched on `blocksLight`, which merged colliders set to
`false` — so nothing was found at all.)

### Fix applied

- **`ScriptSystem.cpp` — `World.eachWall`** now passes each wall's world-space
  AABB `(minX, minY, maxX, maxY)` to the Lua callback (computed from Transform
  position/scale + Collider offset/size), not just an entity handle.
- **`enemy_ai.lua` / `moth_ai.lua` — `buildGrid()`** now marks **every** grid
  cell the wall AABB overlaps (`floor(min/CELL)` … `floor((max-ε)/CELL)`), so
  merged multi-tile walls are fully represented.
- **Goal relaxation** — if the player's cell is itself blocked (hugging a wall),
  `findPath` retargets to the nearest free cell (`nearestFreeCell`, 3-ring
  spiral) instead of returning nil and phasing.
- Engine rebuilt (VS 2022, `CWZEngine` target) — compiles and links clean.

### 3.1 Dynamic obstacles — doors that open mid-play

The grid was built once at `onStart` and never refreshed, so a door opening
during play left a stale wall in the grid. Two problems were fixed:

- **Doors were never in the grid at all.** The old `eachWall` filter excluded
  *any* `LuaScript` entity, but doors (`door.lua`) are script-driven — so closed
  doors weren't treated as obstacles. The filter now excludes **only** the player
  and enemies (by tag / script path, via `isNavExcludedEntity`) and skips
  triggers. A closed door (solid, immovable, not a trigger) is now a wall; an
  open door drops out because it goes non-solid (`door.lua`) or moveable
  (`door_system.lua`).
- **Grid refresh on change.** A monotonic `navVersion` counter
  (`Scripting::navVersion`) is bumped inside `EntityRef::setColliderSolid` /
  `setColliderMoveable` **only when the value actually changes** — so a door
  opening bumps it once, while `door_system.lua` re-asserting `moveable=false`
  every frame on closed doors does not. Enemies/moths cache the version at
  build time and rebuild the grid at their next path recalc only when it
  differs. Cost is O(1) to detect, O(walls) only on a real change.

---

## 4. Outstanding Issues / To Do

| # | Task | Status |
|---|------|--------|
| 1 | Enemies phase through walls (merged-collider grid bug) | ✅ Fixed — see §3 |
| 2 | Confirm the C++ engine was rebuilt | ✅ Rebuilt & links clean |
| 3 | Apply same pathfinding to `moth_ai.lua` | ✅ Done (Chase state) |
| 4 | Rebuild grid on mid-level changes (doors opening) | ✅ Done — see §3.1 |
| 5 | Final in-game confirmation (grid cell count, routing) | ⬜ Pending next run |
| 6 | Remove temporary `print` debug statements once confirmed | ⬜ |
| 7 | Tune waypoint pop radius (currently 50u) and recalc interval | ⬜ |
| 8 | Test across all levels (Level_01 → Level_Save_05) | ⬜ |
| 9 | Performance check — grid-build hitch on level load | ⬜ |

---

## 5. Final Verification Step

The fix is code-complete and compiles; one in-game run confirms it end-to-end.

1. Launch via **F5 on the `LightsOut` target in Visual Studio** (the debugger
   sets the correct working directory so Lua source is read live).
2. Open **View → Output → Debug**.
3. Confirm `[EnemyAI] Grid built — blocked cells: N` now reports a count
   consistent with total wall coverage (previously it under-counted badly), and
   that enemies route **around** walls while chasing.

---

## 6. Honest Status Summary

- **Root cause identified and fixed** — the grid now represents merged
  multi-tile wall colliders at full extent, on both the C++ (`eachWall` bounds)
  and Lua (`buildGrid` cell-marking) sides.
- A\* pathfinding is now applied to **both** `enemy_ai.lua` and `moth_ai.lua`.
- Remaining work is **verification and polish** (in-game confirmation, removing
  debug prints, tuning, multi-level testing) — not core logic.

---

## 7. Files Changed

| File | Change |
|------|--------|
| `engine/src/Systems/ScriptSystem.cpp` | `World.eachWall` emits wall AABBs + includes doors (excludes only player/enemies via `isNavExcludedEntity`, skips triggers); adds `World.navVersion`; includes `Transform.hpp`/`NavVersion.hpp`/`<cmath>` |
| `engine/src/Scripting/EntityRef.cpp` | Collider setters bump `navVersion` on actual change (door open → grid rebuild) |
| `engine/include/Scripting/NavVersion.hpp` | New: `navVersion()` / `bumpNavVersion()` counter |
| `game/projects/LightsOut/assets/scripts/components/enemy_ai.lua` | AABB-based grid marking, `nearestFreeCell` goal relaxation, grid rebuild on `navVersion` change, A\*, waypoint following |
| `game/projects/LightsOut/assets/scripts/components/moth_ai.lua` | Ported A\* into Chase (with `navVersion` grid rebuild); wall bumps now reroute instead of dropping to Idle |
