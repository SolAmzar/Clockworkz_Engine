// Lebon refactor game object to have multiple colliders.
// glm need to replace later with own Math library in game version.
// also used to support light blockage based on shape of collider when doing ray casting

// Give credits to physics guy
#pragma once
#include <glm/glm.hpp>

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
        glm::vec2 offset{0.0f};
        bool blocksLight = true;
        bool solid = true;

        // Virtual destructor for polymorphic use
        virtual ~Collider() = default;

        // World-space helpers (to be overridden)
        virtual glm::vec2 worldMin(const glm::vec2 &objPos) const { return objPos; }
        virtual glm::vec2 worldMax(const glm::vec2 &objPos) const { return objPos; }
        virtual float radius() const { return 0.0f; }
    };

    struct AABBCollider : public Collider
    {
        glm::vec2 halfExtents{0.5f};

        AABBCollider() { type = ColliderType::AABB; }

        glm::vec2 worldMin(const glm::vec2 &objPos) const override
        {
            return objPos + offset - halfExtents;
        }
        glm::vec2 worldMax(const glm::vec2 &objPos) const override
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