/******************************************************************************/
/*!
\file           CollisionSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          CollisionSystem - detects AABB overlaps.

                Third system in Phase 3. Queries entities with Transform and
                Collider components and checks for intersection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include <glm/vec2.hpp>
#include <utility>
#include <vector>

// Forward declarations
namespace Components {
struct Transform;
struct Collider;
} // namespace Components

namespace Systems {

/**
 * @brief CollisionSystem - collision detection logic
 *
 * Detects Axis-Aligned Bounding Box (AABB) overlaps between entities.
 * Current implementation provides detection only (no resolution).
 */
class CollisionSystem {
public:
  /**
   * @brief Update collision detection
   * @param world ECS world containing entities
   */
  void update(ECS::World &world);
  void setGridCellSize(float size) { m_cellSize = size; }

  /**
   * @brief Get list of current collisions
   * @return Vector of colliding entity pairs
   */
  const std::vector<std::pair<ECS::Entity, ECS::Entity>> &
  getCollisions() const {
    return m_collisions;
  }

private:
  std::vector<std::pair<ECS::Entity, ECS::Entity>> m_collisions;
  float m_cellSize{64.0f};

  // Helper for AABB intersection
  bool checkAABB(const Components::Transform &tA,
                 const Components::Collider &cA,
                 const Components::Transform &tB,
                 const Components::Collider &cB, glm::vec2 &penetration);

  bool checkCircleCircle(const Components::Transform &tA,
                         const Components::Collider &cA,
                         const Components::Transform &tB,
                         const Components::Collider &cB,
                         glm::vec2 &penetration);

  bool checkAABBCircle(const Components::Transform &tBox,
                       const Components::Collider &cBox,
                       const Components::Transform &tCirc,
                       const Components::Collider &cCirc,
                       glm::vec2 &penetration);
};

} // namespace Systems
