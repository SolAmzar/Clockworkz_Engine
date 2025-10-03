/******************************************************************************/
/*!
\file           Collision.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Establishes basic 2D AABB collision detection functions.
                Responsibilities include:
                - Defining an AABB struct with min and max points
                - Implementing static overlap test for two AABBs
                - Implementing swept AABB test for moving AABBs over a time interval
                - Providing accurate collision timing and detection for 2D games
				- Serving as a lightweight, header-only utility for AABB collision detection
All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/

#pragma once
#include <glm/vec2.hpp>

namespace Gameplay {

    struct AABB {
        glm::vec2 min;
        glm::vec2 max;
    };

    /// Static overlap test (no velocities).
    bool RectRect_Static(AABB const& a, AABB const& b) noexcept;

    /// Swept AABB (relative motion). Returns true if they will touch within [0, dt].
    /// On hit: tFirst is the first time of contact in seconds (0..dt).
    bool RectRect_Swept(AABB const& a, glm::vec2 const& va,
        AABB const& b, glm::vec2 const& vb,
        float dt, float& tFirst) noexcept;

} // namespace Gameplay
