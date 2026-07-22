/******************************************************************************/
/*!
\file           NavVersion.hpp
\project        ClockWorkz Engine
\brief          Monotonic navigation-version counter for pathfinding grids.

Bumped whenever a collider's solidity or movability actually changes (see the
EntityRef collider setters) — e.g. a door opening. Pathfinding scripts read
World.navVersion() and rebuild their navigation grid when it changes, so enemies
route through doors that open mid-play instead of using a stale grid.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <cstdint>

namespace Scripting {

/// Current navigation version. Changes when a blocking obstacle appears/clears.
uint32_t navVersion();

/// Increment the navigation version. Call only on an actual obstacle change.
void bumpNavVersion();

} // namespace Scripting
