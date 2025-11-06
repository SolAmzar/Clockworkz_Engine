/******************************************************************************/
/*!
\file           Collider.hpp
\project        Lights Out
\author(s)      [Lebon], 50%
\brief          Declares collider shapes for 2D physics and lighting systems.

                Responsibilities:
                - Defines base Collider class with shared properties such as
                  position offset, solidity, and light-blocking behavior.
                - Implements AABBCollider and CircleCollider subclasses with
                  shape-specific world-space computations.
                - Used by Physics2D for collision detection and raycasting
                  (e.g., light occlusion).

                Notes (REMOVE AFTER EDIT):
                - glm::vec2 currently used for vector math; replace with
                  custom Math library in final engine version.
                - Credit to Physics developer for foundational collision logic.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/glm.hpp>
#include "Math/Vec2.hpp"

namespace Gameplay
{

    enum class ColliderType
    {
        AABB,
        Circle,
        None
    };

    struct Collider
    {
        ColliderType type = ColliderType::None;
        vec2 offset{0.0f};
        bool blocksLight = true;
        bool solid = true;

        // Virtual destructor for polymorphic use
        virtual ~Collider() = default;

        // World-space helpers (to be overridden)
        virtual vec2 worldMin(const vec2 &objPos) const { return objPos; }
        virtual vec2 worldMax(const vec2 &objPos) const { return objPos; }
        virtual float radius() const { return 0.0f; }
    };

    struct AABBCollider : public Collider
    {
        vec2 halfExtents{0.5f, 0.5f};

        AABBCollider() { type = ColliderType::AABB; }

        vec2 worldMin(vec2 const& objPos) const override { return objPos + offset - halfExtents; }
        vec2 worldMax(vec2 const& objPos) const override
        {
            return objPos + offset + halfExtents;
        }
    };

    struct CircleCollider : public Collider
    {
        float r = 0.5f;

        CircleCollider() { type = ColliderType::Circle; }

        float radius() const override { return r; }
    };

}