# A\* Pathfinding — Effort Breakdown (Time Log)

**Student:** Ching Wei An
**Course:** CSD2451SU26 (Re-assessment)
**Feature:** A\* pathfinding for enemy navigation (both the standard enemy and the
moth), including runtime navigation-grid construction and dynamic-obstacle
handling.

This is an effort estimate for implementing the feature **from scratch, solo**.
Time is dominated by investigation, debugging, and testing rather than by the
number of lines written — the final feature is ~413 lines of code, but the bulk
of the effort went into two non-obvious engine problems (see rows 3–4).

---

## Effort by subtask

| # | Subtask | Estimated effort | What it involved |
|---|---------|-----------------|------------------|
| 1 | Engine familiarisation & Lua binding | **4 h** | Understanding the ECS + sol2 scripting layer well enough to add a new C++ binding (`World.eachWall`); wiring it through, compiling and linking, and learning the `Collider`/`Transform` fields it needs |
| 2 | Core A\* implementation | **8 h** | 8-directional search, octile-distance heuristic, diagonal corner-cut prevention, 400-iteration safety cap, open-list handling, and the grid/world coordinate math (`worldToCell`, half-cell waypoint offsets) |
| 3 | Debugging the "enemies phase through walls" root cause | **11 h** | Discovering that the engine's `TilemapCollisionBuilder` greedily **merges** wall tiles into large runtime colliders, so marking only a collider's centre cell left most of a wall walkable; reproducing it, reading the tilemap builder, and reworking the grid to mark **every** AABB-overlapped cell |
| 4 | Dynamic obstacles — doors opening mid-play | **5 h** | Designing the `navVersion` invalidation counter; discovering that `door_system.lua` re-asserts `moveable=false` every frame (which spammed grid rebuilds) and fixing the counter to bump **only on an actual change**; refining the wall filter so closed doors count as obstacles but the player/enemies do not |
| 5 | Goal relaxation & robustness | **3 h** | `nearestFreeCell` spiral search so a player hugging a wall still yields a reachable target; direct-steering fallback when no route exists |
| 6 | Porting A\* to the moth enemy | **3 h** | Mirroring the pathfinding into `moth_ai.lua`'s Chase state, including `navVersion` grid rebuild and re-route-on-wall-bump behaviour |
| 7 | Testing, tuning & cleanup | **6 h** | In-game verification via debug output, tuning recalc interval / waypoint pop radius / iteration cap, gating all debug output behind a `DEBUG` flag, and testing across levels |
| | **Total** | **~40 h** | |

---

## Notes on the estimate

- **Line count understates the work.** The feature is ~413 lines of executable
  code, but a large share of the 40 hours was spent reading engine code, adding
  diagnostic prints, and reproducing bugs — activity that produces no new lines.
  Roughly 10 lines/hour reflects debugging-heavy work, not slow typing.
- **The two engine bugs dominate.** Subtasks 3 and 4 (~16 h combined) were not
  "write the algorithm" work — they were investigations into non-obvious engine
  behaviour (runtime collider merging; per-frame collider re-assertion) that only
  surface once the algorithm is already running.
- **Prior A\* experience would reduce subtask 2** by several hours; the estimate
  assumes a first correct implementation of the algorithm and its grid math.
