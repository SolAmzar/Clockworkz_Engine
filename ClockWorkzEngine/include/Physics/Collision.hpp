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
