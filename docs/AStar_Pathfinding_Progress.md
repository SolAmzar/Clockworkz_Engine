# A\* Pathfinding for Enemy AI — Progress Report

**Project:** LightsOut (ClockWorkz Engine)
**Feature:** A\* pathfinding for enemy navigation around obstacles
**Author:** Ching Wei An
**Date:** 17 June 2026
**Status:** In Progress — implementation written, not yet verified in-game

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

## 3. Outstanding Issues / To Do

| # | Task | Status |
|---|------|--------|
| 1 | Enemies still phase through walls in-game | 🔴 Core bug — unresolved |
| 2 | Confirm the C++ engine was actually rebuilt | ⬜ Unverified |
| 3 | Capture debug output (grid cell count, waypoint count) | ⬜ Pending next run |
| 4 | Diagnose root cause (empty grid / nil paths / wrong exe) | ⬜ Blocked on #3 |
| 5 | Remove temporary `print` debug statements once fixed | ⬜ |
| 6 | Tune waypoint pop radius (currently 50u) and recalc interval | ⬜ |
| 7 | Apply same pathfinding to `moth_ai.lua` | ⬜ Not started |
| 8 | Test across all levels (Level_01 → Level_Save_05) | ⬜ Not started |
| 9 | Performance check — grid-build hitch on level load | ⬜ Not started |

---

## 4. Critical Next Step

Diagnosis is blocked on one piece of information: the **debug output from the
next run**.

1. Launch via **F5 on the `LightsOut` target in Visual Studio** (not the `.exe`
   directly — the debugger sets the correct working directory so Lua source is
   read live).
2. Open **View → Output → Debug**.
3. Read the line `[EnemyAI] Grid built — blocked cells: X`.

**Interpretation:**

| Output | Meaning |
|--------|---------|
| `blocked cells: 0` | `World.eachWall` found nothing → C++ not rebuilt, or wrong executable |
| `blocked cells: N` + `waypoints: nil` | Grid OK, but A\* can't find a route |
| `blocked cells: N` + `waypoints: M` | Grid + A\* working; bug is elsewhere |

---

## 5. Honest Status Summary

- **Week 1 (grid foundation):** functionally code-complete.
- **Week 2 (A\* implementation):** written, but **not yet working in-game** —
  enemies currently still pass through obstacles.
- The feature is **not done**. The immediate blocker is gathering runtime debug
  output to determine why pathfinding is not affecting enemy movement.

---

## 6. Files Changed

| File | Change |
|------|--------|
| `engine/src/Systems/ScriptSystem.cpp` | Added `Collider.hpp` include + `World.eachWall` binding |
| `game/projects/LightsOut/assets/scripts/components/enemy_ai.lua` | Grid builder, A\*, waypoint following, debug logging |
