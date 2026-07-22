# A\* Pathfinding — Re-Assessment Explanation (IMR)

**Student:** Ching Wei An
**Course:** CSD2451SU26 (Re-assessment)
**Feature (rubric row 2 — "Exceptional / your pick"):** A\* pathfinding for enemy
navigation, an advanced AI engine feature.

This document is written to be pasted into the IMR spreadsheet's
**"Provide explanation (student)"** column. It covers what the feature is, how to
test it, and every new/updated code file (rubric row 5).

---

## 1. What the feature is

Enemies previously chased the player by steering in a straight line and walked
into walls. This feature adds **A\* grid pathfinding** so enemies compute a route
**around** walls and obstacles to reach the player.

Design summary:
- A navigation grid is built from the level's wall geometry at **100-unit** cell
  resolution (matching the tile size).
- **A\*** searches the grid with **8-directional movement**, an **octile-distance**
  heuristic (admissible/consistent), **diagonal corner-cut prevention**, and a
  **400-iteration cap** so an unsolvable map can never freeze a frame.
- Enemies follow the resulting waypoints, recomputing every `pathInterval`
  (default 0.5s), with a direct-steering fallback when no route exists.
- Applied to **both** enemy types: the normal enemy (`enemy_ai.lua`) and the moth
  (`moth_ai.lua`).

### Two non-obvious engine problems solved

1. **Merged multi-tile wall colliders.** The engine's `TilemapCollisionBuilder`
   greedily merges strips of wall tiles into single large colliders at runtime
   (e.g. a `width 4000 × height 100` collider = 40 tiles). The naive grid marked
   only the collider's *centre* cell, leaving the rest of the wall walkable, so
   enemies phased through. The grid now marks **every cell a wall's AABB
   overlaps**, using bounds supplied by the engine.

2. **Dynamic obstacles (doors) opening mid-play.** Closed doors must block; when
   a door opens the grid must update. A monotonic `navVersion` counter is bumped
   when a collider's solidity/movability actually changes, and enemies rebuild
   their grid on the next recalc only when that version changes.

---

## 2. How to test it

1. Open the solution in **Visual Studio 2022** and run the **`LightsOut`** target
   with **F5** (Debug, x64). Running via F5 sets the working directory so the Lua
   scripts are read live.
2. Load a level that contains walls — `Level_Save_05` is a good stress test (it
   has 66 merged wall colliders). Let an enemy detect the player.
3. Open **View → Output → Debug** and observe:
   - `[EnemyAI] Grid built — blocked cells: N` — grid populated from wall extents.
   - `[EnemyAI] Path recalc — waypoints: M …` — A\* producing a route each recalc.
4. Watch the enemy route **around** walls instead of through them.
5. **Doors:** on a level with a door, collect the required fragments to open it;
   a chasing enemy re-routes through the opened doorway on its next recalc.

**Debug output toggle (rubric row 7 — no debug info in Release by default):** all
pathfinding debug text is gated by a single flag `local DEBUG` at the top of
`enemy_ai.lua` (the A\* state block). It is `true` for verification and must be set
**`false`** for the Release/final build to silence all `[EnemyAI]` output.

---

## 3. New and updated code files (rubric row 5)

### New files

| File | Purpose |
|------|---------|
| `ClockWorkzEngine/engine/include/Scripting/NavVersion.hpp` | Declares `Scripting::navVersion()` / `bumpNavVersion()` — a monotonic counter, bumped on a real obstacle change, that pathfinding scripts watch to rebuild their grid. Full file header comment included. |

### Updated engine (C++) files

| File | Change (function-level) |
|------|-------------------------|
| `ClockWorkzEngine/engine/src/Systems/ScriptSystem.cpp` | **`World.eachWall` binding** (in `ScriptSystem::update`): iterates colliders and, for each static solid obstacle, passes its **world-space AABB** `(minX,minY,maxX,maxY)` to the Lua callback. Filter is `isSolid && !moveable && !isTrigger && !isNavExcludedEntity`. Added **`isNavExcludedEntity`** helper (excludes only the player and enemies by tag / script path, so doors still count as walls). Added **`World.navVersion`** binding. New includes: `Transform.hpp`, `NavVersion.hpp`, `<cmath>`. |
| `ClockWorkzEngine/engine/src/Scripting/EntityRef.cpp` | Defined the `navVersion()` / `bumpNavVersion()` counter (TU-local). **`EntityRef::setColliderSolid`** and **`setColliderMoveable`** now bump the nav version **only when the value actually changes**, so a door opening invalidates enemy grids exactly once (and per-frame "re-assert closed" calls do not spam rebuilds). |

### Updated gameplay (Lua) files

| File | Change (function-level) |
|------|-------------------------|
| `ClockWorkzEngine/game/projects/LightsOut/assets/scripts/components/enemy_ai.lua` | Full A\* implementation: `worldToCell`, `isBlocked`, `nearestFreeCell` (goal relaxation), `markCell`, `buildGrid` (marks all AABB-overlapped cells), `heuristic` (octile), `findPath` (A\*). `onStart` builds the grid; the Chase state follows waypoints and rebuilds the grid when `World.navVersion()` changes. `DEBUG`/`dbg()` gate all debug output. |
| `ClockWorkzEngine/game/projects/LightsOut/assets/scripts/components/moth_ai.lua` | Ported the same A\* pathfinding into the moth's Chase state (grid build in `onStart`, waypoint following, `navVersion` grid rebuild). Wall bumps now trigger a re-route instead of dropping to Idle. |

All C++ functions carry function-level comments and the new header file carries a
full file-header comment, as required.

---

## 4. Related documentation

- `docs/AStar_Pathfinding_Progress.md` — detailed progress report including the
  root-cause analysis of the phase-through bug and the door/`navVersion` design.

---

## 5. Notes for other rubric rows

- **Row 3 (no fatal errors):** engine and game build & link in Debug x64; Release
  x64 verified as part of this submission. Both the game (`LightsOut.exe`) and the
  editor (`CWZEditor.exe`) build.
- **Row 4 (0 warnings / no leaks):** memory-leak detection is enabled in Debug in
  `game/projects/LightsOut/src/main.cpp` — `#define _CRTDBG_MAP_ALLOC` (line 51)
  and `_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF)` (line 79); see also
  `engine/src/Core/Engine.cpp` (line 27 and the `_CrtDumpMemoryLeaks` note near
  line 354). The A\* code added no new compiler warnings.
- **Row 7 (no debug info in Release):** see the `DEBUG` toggle in §2.
