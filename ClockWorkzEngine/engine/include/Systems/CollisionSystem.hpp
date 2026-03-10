/******************************************************************************/
/*!
\file           CollisionSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          CollisionSystem - detects OBB/Circle overlaps.

                Third system in Phase 3. Queries entities with Transform and
                Collider components and checks for intersection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"
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
 * Detects OBB and Circle overlaps between entities using SAT.
 * Current implementation provides detection and resolution.
 */
class CollisionSystem {
public:
  /**
   * @brief Update collision detection
   * @param world ECS world containing entities
   * @param layerEnabled Optional layer enabled flags (nullptr = all enabled)
   */
  void update(ECS::World &world,
              const std::vector<bool> *layerEnabled = nullptr);
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

  bool checkAABB(const Components::Transform &tA,
                 const Components::Collider &cA,
                 const Components::Transform &tB,
                 const Components::Collider &cB, vec2 &penetration);

  bool checkCircleCircle(const Components::Transform &tA,
                         const Components::Collider &cA,
                         const Components::Transform &tB,
                         const Components::Collider &cB,
                         vec2 &penetration);

  bool checkAABBCircle(const Components::Transform &tBox,
                       const Components::Collider &cBox,
                       const Components::Transform &tCirc,
                       const Components::Collider &cCirc,
                       vec2 &penetration);
};

} // namespace Systems
