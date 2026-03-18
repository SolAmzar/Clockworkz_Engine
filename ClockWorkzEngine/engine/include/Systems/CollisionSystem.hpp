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
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
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
  // Spatial grid key types (nested here so member vars below can use them)
  struct CellKey {
    int x, y;
    bool operator==(const CellKey &o) const { return x == o.x && y == o.y; }
  };
  struct CellKeyHash {
    std::size_t operator()(const CellKey &k) const {
      std::size_t hx = std::hash<int>{}(k.x);
      std::size_t hy = std::hash<int>{}(k.y);
      return hx ^ (hy + 0x9e3779b9 + (hx << 6) + (hx >> 2));
    }
  };
  struct PairKey {
    std::uint64_t a, b;
    bool operator==(const PairKey &o) const { return a == o.a && b == o.b; }
  };
  struct PairKeyHash {
    std::size_t operator()(const PairKey &k) const {
      std::size_t ha = std::hash<std::uint64_t>{}(k.a);
      std::size_t hb = std::hash<std::uint64_t>{}(k.b);
      return ha ^ (hb + 0x9e3779b97f4a7c15ULL + (ha << 6) + (ha >> 2));
    }
  };

  std::vector<std::pair<ECS::Entity, ECS::Entity>> m_collisions;
  float m_cellSize{64.0f};

  // Scratch buffers reused each frame — cleared but not deallocated,
  // so capacity stays allocated and avoids per-frame heap churn.
  std::vector<ECS::Entity> m_entities;
  std::unordered_map<CellKey, std::vector<ECS::Entity>, CellKeyHash> m_grid;
  std::unordered_set<PairKey, PairKeyHash> m_tested;

  bool checkAABB(const Components::Transform &tA,
                 const Components::Collider &cA,
                 const Components::Transform &tB,
                 const Components::Collider &cB, vec2 &penetration);

  bool checkCircleCircle(const Components::Transform &tA,
                         const Components::Collider &cA,
                         const Components::Transform &tB,
                         const Components::Collider &cB,
                         vec2 &penetration,
                         std::uint64_t keyA);

  bool checkAABBCircle(const Components::Transform &tBox,
                       const Components::Collider &cBox,
                       const Components::Transform &tCirc,
                       const Components::Collider &cCirc,
                       vec2 &penetration);
};

} // namespace Systems
