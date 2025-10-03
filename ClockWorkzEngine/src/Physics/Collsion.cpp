/******************************************************************************/
/*!
\file           Collision.cpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Defines the collision functions declared in Collision.hpp.
                Responsibilities include:
                - Implementing static overlap test for two AABBs
                - Implementing swept AABB test for moving AABBs over a time interval
                - Providing accurate collision timing and detection for 2D games
				- Serving as a lightweight, header-only utility for AABB collision detection
All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/

#include "Physics/Collision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Gameplay {

    // --- Static overlap ----------------------------------------------------------
    bool RectRect_Static(AABB const& a, AABB const& b) noexcept {
        // Separating axis test on X and Y
        if (a.max.x <= b.min.x) return false;
        if (a.min.x >= b.max.x) return false;
        if (a.max.y <= b.min.y) return false;
        if (a.min.y >= b.max.y) return false;
        return true;
    }

    // --- Swept (dynamic) ---------------------------------------------------------
    // Robust slab-based swept AABB. Works by testing "time interval of overlap"
    // per axis, then intersecting intervals across axes.
    static inline void axisInterval(float aMin, float aMax,
        float bMin, float bMax,
        float vRel, float dt,
        float& tEnter, float& tExit) noexcept
    {
        if (vRel == 0.0f) {
            // No relative motion on this axis: must already overlap for any chance
            if (aMax <= bMin || aMin >= bMax) { tEnter = 1.f; tExit = 0.f; } // empty
            return;
        }

        const float invV = 1.0f / vRel;
        const float t0 = (bMin - aMax) * invV; // when a's max meets b's min
        const float t1 = (bMax - aMin) * invV; // when a's min meets b's max
        float tA = std::min(t0, t1);
        float tB = std::max(t0, t1);

        // Intersect with the valid time window [0, dt]
        tEnter = std::max(tEnter, tA);
        tExit = std::min(tExit, tB);
    }

    bool RectRect_Swept(AABB const& a, glm::vec2 const& va,
        AABB const& b, glm::vec2 const& vb,
        float dt, float& tFirst) noexcept
    {
        // Early out: if currently overlapping, treat as instant hit
        if (RectRect_Static(a, b)) { tFirst = 0.0f; return true; }

        const glm::vec2 vRel = vb - va;  // move "b" relative to "a"

        float tEnter = 0.0f;     // earliest time they begin to overlap
        float tExit = dt;       // latest  time they still overlap

        axisInterval(a.min.x, a.max.x, b.min.x, b.max.x, vRel.x, dt, tEnter, tExit);
        if (tEnter > tExit) return false; // separated on X in time

        axisInterval(a.min.y, a.max.y, b.min.y, b.max.y, vRel.y, dt, tEnter, tExit);
        if (tEnter > tExit) return false; // separated on Y in time

        // Must collide within [0, dt] and with consistent ordering
        if (tExit < 0.0f || tEnter > dt) return false;

        tFirst = std::max(0.0f, std::min(tEnter, dt));
        return true;
    }

} // namespace Gameplay
