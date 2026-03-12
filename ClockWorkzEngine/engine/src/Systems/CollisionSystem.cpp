/******************************************************************************/
/*!
\file           CollisionSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon] [Syafiq] [Amzar]
\brief          CollisionSystem implementation - OBB/Circle intersection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/Layer.hpp"
#include "Components/Transform.hpp"
#include "Math/Vec2.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>


namespace Systems {

// Returns the AABB (in world space) that encloses the rotated collider.
// Used for spatial grid broadphase only — not for narrowphase.
static void getWorldAABB(const Components::Transform &t,
                         const Components::Collider &c,
                         vec2 &outMin, vec2 &outMax) {
  vec2 center = t.position + rotate(vec2(c.offset.x - t.pivot.x,
                                         c.offset.y - t.pivot.y), t.rotation);

  if (c.shape == Components::Collider::Shape::Circle) {
    float radius = c.radius * std::max(t.scale.x, t.scale.y);
    outMin = center - vec2(radius, radius);
    outMax = center + vec2(radius, radius);
    return;
  }

  vec2 half(c.size.x * t.scale.x * 0.5f, c.size.y * t.scale.y * 0.5f);
  float absC = std::abs(std::cos(t.rotation));
  float absS = std::abs(std::sin(t.rotation));
  vec2 extents(absC * half.x + absS * half.y,
               absS * half.x + absC * half.y);

  outMin = center - extents;
  outMax = center + extents;
}

void CollisionSystem::update(ECS::World &world,
                             const std::vector<bool> *layerEnabled) {
  m_collisions.clear();

  // Collect all entities with Transform + Collider
  // We need to collect them first because we cannot perform nested iteration
  // efficiently/safely with the current world.each() implementation
  std::vector<ECS::Entity> entities;
  world.each<Components::Transform, Components::Collider>(
      [&](ECS::Entity e, Components::Transform &, Components::Collider &) {
        if (layerEnabled) {
          if (auto *layer = world.getComponent<Components::Layer>(e)) {
            int idx = layer->index;
            if (idx >= 0 && idx < static_cast<int>(layerEnabled->size()) &&
                !(*layerEnabled)[idx]) {
              return;
            }
          }
        }
        entities.push_back(e);
      });

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

  std::unordered_map<CellKey, std::vector<ECS::Entity>, CellKeyHash> grid;
  grid.reserve(entities.size() * 2);

  auto entityKey = [](ECS::Entity e) -> std::uint64_t {
    return (static_cast<std::uint64_t>(e.id) << 32) |
           static_cast<std::uint64_t>(e.generation);
  };

  for (auto e : entities) {
    auto *t = world.getComponent<Components::Transform>(e);
    auto *c = world.getComponent<Components::Collider>(e);
    if (!t || !c) continue;

    vec2 aabbMin, aabbMax;
    getWorldAABB(*t, *c, aabbMin, aabbMax);

    int minX = static_cast<int>(std::floor(aabbMin.x / m_cellSize));
    int maxX = static_cast<int>(std::floor(aabbMax.x / m_cellSize));
    int minY = static_cast<int>(std::floor(aabbMin.y / m_cellSize));
    int maxY = static_cast<int>(std::floor(aabbMax.y / m_cellSize));

    for (int x = minX; x <= maxX; ++x)
      for (int y = minY; y <= maxY; ++y)
        grid[{x, y}].push_back(e);
  }

  std::unordered_set<PairKey, PairKeyHash> tested;
  tested.reserve(entities.size() * 4);

  for (const auto &cell : grid) {
    const auto &cellEntities = cell.second;
    if (cellEntities.size() < 2) continue;

    for (size_t i = 0; i < cellEntities.size(); ++i) {
      for (size_t j = i + 1; j < cellEntities.size(); ++j) {
        auto eA = cellEntities[i];
        auto eB = cellEntities[j];

        std::uint64_t keyA = entityKey(eA);
        std::uint64_t keyB = entityKey(eB);
        if (keyA > keyB) std::swap(keyA, keyB);
        PairKey pairKey{keyA, keyB};
        if (tested.find(pairKey) != tested.end()) continue;
        tested.insert(pairKey);

        auto *tA = world.getComponent<Components::Transform>(eA);
        auto *cA = world.getComponent<Components::Collider>(eA);
        auto *tB = world.getComponent<Components::Transform>(eB);
        auto *cB = world.getComponent<Components::Collider>(eB);

        if (tA && cA && tB && cB) {
          if ((cA->collisionMask & cB->collisionCategory) == 0) continue;
          if ((cB->collisionMask & cA->collisionCategory) == 0) continue;

          bool isSolidCollision   = cA->isSolid && cB->isSolid;
          bool isTriggerCollision = cA->isTrigger || cB->isTrigger;

          if (isSolidCollision || isTriggerCollision) {
            vec2 penetration{};
            bool collided = false;

            if (cA->shape == Components::Collider::Shape::Box &&
                cB->shape == Components::Collider::Shape::Box) {
              collided = checkAABB(*tA, *cA, *tB, *cB, penetration);
            } else if (cA->shape == Components::Collider::Shape::Circle &&
                       cB->shape == Components::Collider::Shape::Circle) {
              collided = checkCircleCircle(*tA, *cA, *tB, *cB, penetration);
            } else if (cA->shape == Components::Collider::Shape::Box) {
              collided = checkAABBCircle(*tA, *cA, *tB, *cB, penetration);
            } else {
              vec2 p;
              if (checkAABBCircle(*tB, *cB, *tA, *cA, p)) {
                penetration = -p;
                collided = true;
              }
            }

            if (collided) {
              m_collisions.push_back({eA, eB});

              if (isSolidCollision && !isTriggerCollision) {
                bool aMoves = cA->moveable;
                bool bMoves = cB->moveable;

                if (aMoves && !bMoves) {
                  tA->position += penetration;
                } else if (!aMoves && bMoves) {
                  tB->position -= penetration;
                } else if (aMoves && bMoves) {
                  float totalMass = cA->mass + cB->mass;
                  float ratioA = cB->mass / totalMass;
                  float ratioB = cA->mass / totalMass;
                  tA->position += penetration * ratioA;
                  tB->position -= penetration * ratioB;
                }
              }
            }
          }
        }
      }
    }
  }
}

bool CollisionSystem::checkAABB(const Components::Transform &tA,
                                const Components::Collider &cA,
                                const Components::Transform &tB,
                                const Components::Collider &cB,
                                vec2 &penetration) {
  // World centers (collider offset rotated by entity rotation, minus pivot)
  vec2 centerA = tA.position + rotate(vec2(cA.offset.x - tA.pivot.x,
                                            cA.offset.y - tA.pivot.y), tA.rotation);
  vec2 centerB = tB.position + rotate(vec2(cB.offset.x - tB.pivot.x,
                                            cB.offset.y - tB.pivot.y), tB.rotation);

  vec2 halfA(cA.size.x * tA.scale.x * 0.5f, cA.size.y * tA.scale.y * 0.5f);
  vec2 halfB(cB.size.x * tB.scale.x * 0.5f, cB.size.y * tB.scale.y * 0.5f);

  // Local axes of each OBB (unit vectors along local X and Y)
  float cosA = std::cos(tA.rotation), sinA = std::sin(tA.rotation);
  float cosB = std::cos(tB.rotation), sinB = std::sin(tB.rotation);
  vec2 axA[2] = {{ cosA,  sinA }, {-sinA,  cosA }};
  vec2 axB[2] = {{ cosB,  sinB }, {-sinB,  cosB }};

  vec2 d = centerB - centerA;

  float minOverlap = std::numeric_limits<float>::max();
  vec2 mtvAxis{};

  // SAT: test 4 axes (2 face normals per box)
  vec2 axes[4] = { axA[0], axA[1], axB[0], axB[1] };
  for (const auto &axis : axes) {
    float dist  = std::abs(dot(d, axis));
    float projA = std::abs(dot(axA[0], axis)) * halfA.x +
                  std::abs(dot(axA[1], axis)) * halfA.y;
    float projB = std::abs(dot(axB[0], axis)) * halfB.x +
                  std::abs(dot(axB[1], axis)) * halfB.y;
    float overlap = projA + projB - dist;

    if (overlap <= 0.f) return false; // separating axis found

    if (overlap < minOverlap) {
      minOverlap = overlap;
      // MTV pushes A away from B — ensure axis points from B toward A
      mtvAxis = (dot(d, axis) > 0.f) ? -axis : axis;
    }
  }

  penetration = mtvAxis * minOverlap;
  return true;
}

bool CollisionSystem::checkCircleCircle(const Components::Transform &tA,
                                        const Components::Collider &cA,
                                        const Components::Transform &tB,
                                        const Components::Collider &cB,
                                        vec2 &penetration) {
  vec2 centerA = tA.position + rotate(vec2(cA.offset.x - tA.pivot.x,
                                            cA.offset.y - tA.pivot.y), tA.rotation);
  vec2 centerB = tB.position + rotate(vec2(cB.offset.x - tB.pivot.x,
                                            cB.offset.y - tB.pivot.y), tB.rotation);

  vec2  diff    = centerA - centerB;
  float distSq  = dot(diff, diff);
  float radA    = cA.radius * std::max(tA.scale.x, tA.scale.y);
  float radB    = cB.radius * std::max(tB.scale.x, tB.scale.y);
  float radiusSum = radA + radB;

  if (distSq >= radiusSum * radiusSum) return false;

  float dist = std::sqrt(distSq);
  if (dist < 1e-4f) {
    penetration = vec2(radiusSum, 0.0f);
  } else {
    penetration = (diff / dist) * (radiusSum - dist);
  }

  return true;
}

bool CollisionSystem::checkAABBCircle(const Components::Transform &tBox,
                                      const Components::Collider &cBox,
                                      const Components::Transform &tCirc,
                                      const Components::Collider &cCirc,
                                      vec2 &penetration) {
  // 1. Box and circle centers in world space
  vec2 boxCenter = tBox.position + rotate(vec2(cBox.offset.x - tBox.pivot.x,
                                               cBox.offset.y - tBox.pivot.y), tBox.rotation);
  vec2 circCenter = tCirc.position + rotate(vec2(cCirc.offset.x - tCirc.pivot.x,
                                                  cCirc.offset.y - tCirc.pivot.y), tCirc.rotation);
  float radCirc = cCirc.radius * std::max(tCirc.scale.x, tCirc.scale.y);

  // 2. Transform circle center into box local space (un-rotate around box center)
  vec2 localCirc = rotate(circCenter - boxCenter, -tBox.rotation);

  // 3. Box half-extents in local space
  vec2 boxHalf(cBox.size.x * tBox.scale.x * 0.5f,
               cBox.size.y * tBox.scale.y * 0.5f);

  // 4. Closest point on box to circle center (in local space)
  vec2 closestLocal = clamp(localCirc, -boxHalf, boxHalf);

  // 5. Distance from closest point to circle center
  vec2  diffLocal = localCirc - closestLocal;
  float distSq    = dot(diffLocal, diffLocal);

  if (distSq > radCirc * radCirc) return false;

  float dist = std::sqrt(distSq);

  // 6. Penetration in local space
  vec2 penetrationLocal;
  if (dist < 1e-4f) {
    // Circle center is inside box — find minimum separation axis
    vec2 overlap = boxHalf + vec2(radCirc, radCirc) - abs_vec2(localCirc);
    if (overlap.x < overlap.y) {
      float sign = (localCirc.x < 0.f) ? -1.f : 1.f;
      penetrationLocal = vec2(-sign * overlap.x, 0.f);
    } else {
      float sign = (localCirc.y < 0.f) ? -1.f : 1.f;
      penetrationLocal = vec2(0.f, -sign * overlap.y);
    }
  } else {
    // Circle center is outside box — push along contact normal
    penetrationLocal = -(diffLocal / dist) * (radCirc - dist);
  }

  // 7. Rotate penetration back to world space
  penetration = rotate(penetrationLocal, tBox.rotation);
  return true;
}

} // namespace Systems
