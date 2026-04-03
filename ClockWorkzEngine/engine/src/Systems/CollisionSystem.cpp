/******************************************************************************/
/*!
\file           CollisionSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon] [Syafiq] [Sol-Amzar]
\brief          CollisionSystem implementation - OBB/Circle intersection.

All content Â© 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/Layer.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Math/Vec2.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>


namespace Systems {

// Baumgarte correction constants â€” apply partial correction to reduce jitter.
// kPercent: fraction of penetration corrected per frame (< 1 prevents overshoot).
// kSlop: minimum penetration depth to correct (filters micro-jitter).
static constexpr float kCorrectPercent = 0.8f;
static constexpr float kCorrectSlop    = 0.5f;  // pixels

// Returns the AABB (in world space) that encloses the rotated collider.
// Used for spatial grid broadphase only â€” not for narrowphase.
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

// Apply Baumgarte-style position correction: scale down by kPercent and ignore
// penetrations smaller than kSlop to prevent micro-jitter.
static vec2 baumgarte(vec2 penetration) {
  float mag = magnitude(penetration);
  if (mag < kCorrectSlop) return vec2(0.f, 0.f);
  float scale = kCorrectPercent * (mag - kCorrectSlop) / mag;
  return penetration * scale;
}

void CollisionSystem::update(ECS::World &world,
                             const std::vector<bool> *layerEnabled) {
  m_collisions.clear();

  // Reuse scratch buffers â€” clear preserves allocated capacity.
  m_entities.clear();
  m_grid.clear();
  m_tested.clear();

  // Collect all entities with Transform + Collider
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
        m_entities.push_back(e);
      });

  m_grid.reserve(m_entities.size() * 2);
  m_tested.reserve(m_entities.size() * 4);

  auto entityKey = [](ECS::Entity e) -> std::uint64_t {
    return (static_cast<std::uint64_t>(e.id) << 32) |
           static_cast<std::uint64_t>(e.generation);
  };

  for (auto e : m_entities) {
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
        m_grid[{x, y}].push_back(e);
  }

  for (const auto &cell : m_grid) {
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
        if (m_tested.find(pairKey) != m_tested.end()) continue;
        m_tested.insert(pairKey);

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

            std::uint64_t eAKey = entityKey(eA);

            if (cA->shape == Components::Collider::Shape::Box &&
                cB->shape == Components::Collider::Shape::Box) {
              collided = checkAABB(*tA, *cA, *tB, *cB, penetration);
            } else if (cA->shape == Components::Collider::Shape::Circle &&
                       cB->shape == Components::Collider::Shape::Circle) {
              collided = checkCircleCircle(*tA, *cA, *tB, *cB, penetration, eAKey);
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

                // Baumgarte-corrected penetration: partial correction with slop
                // prevents jitter from full-depth snapping.
                vec2 corrected = baumgarte(penetration);
                if (magnitude(corrected) < 1e-6f) continue;

                vec2 normal = normalize(penetration);

                if (aMoves && !bMoves) {
                  tA->position += corrected;
                  // Cancel A's velocity component pointing into B
                  if (auto *vA = world.getComponent<Components::Velocity>(eA)) {
                    float vDotN = dot(vA->velocity, normal);
                    if (vDotN < 0.f) vA->velocity -= vDotN * normal;
                  }
                } else if (!aMoves && bMoves) {
                  tB->position -= corrected;
                  // Cancel B's velocity component pointing into A
                  if (auto *vB = world.getComponent<Components::Velocity>(eB)) {
                    float vDotN = dot(vB->velocity, -normal);
                    if (vDotN < 0.f) vB->velocity -= vDotN * (-normal);
                  }
                } else if (aMoves && bMoves) {
                  float totalMass = cA->mass + cB->mass;
                  float ratioA = cB->mass / totalMass;
                  float ratioB = cA->mass / totalMass;
                  tA->position += corrected * ratioA;
                  tB->position -= corrected * ratioB;
                  // Cancel each entity's velocity into the other
                  if (auto *vA = world.getComponent<Components::Velocity>(eA)) {
                    float vDotN = dot(vA->velocity, normal);
                    if (vDotN < 0.f) vA->velocity -= vDotN * normal;
                  }
                  if (auto *vB = world.getComponent<Components::Velocity>(eB)) {
                    float vDotN = dot(vB->velocity, -normal);
                    if (vDotN < 0.f) vB->velocity -= vDotN * (-normal);
                  }
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

  vec2 d = centerB - centerA;

  // Fast-path: both boxes axis-aligned (most common case â€” walls, player, tiles).
  // Simple interval overlap is much cheaper than 4-axis SAT.
  static constexpr float kRotEps = 1e-4f;
  if (std::abs(tA.rotation) < kRotEps && std::abs(tB.rotation) < kRotEps) {
    float overlapX = halfA.x + halfB.x - std::abs(d.x);
    float overlapY = halfA.y + halfB.y - std::abs(d.y);
    if (overlapX <= 0.f || overlapY <= 0.f) return false;
    if (overlapX < overlapY)
      penetration = { (d.x < 0.f ? overlapX : -overlapX), 0.f };
    else
      penetration = { 0.f, (d.y < 0.f ? overlapY : -overlapY) };
    return true;
  }

  // General OBB SAT: test 4 axes (2 face normals per box)
  float cosA = std::cos(tA.rotation), sinA = std::sin(tA.rotation);
  float cosB = std::cos(tB.rotation), sinB = std::sin(tB.rotation);
  vec2 axA[2] = {{ cosA,  sinA }, {-sinA,  cosA }};
  vec2 axB[2] = {{ cosB,  sinB }, {-sinB,  cosB }};

  float minOverlap = std::numeric_limits<float>::max();
  vec2 mtvAxis{};

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
      // MTV pushes A away from B â€” ensure axis points from B toward A
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
                                        vec2 &penetration,
                                        std::uint64_t keyA) {
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
    // Circles perfectly overlapping â€” use entity key bit for a deterministic
    // push direction that doesn't oscillate between frames.
    if (keyA & 1u)
      penetration = vec2(radiusSum, 0.f);
    else
      penetration = vec2(0.f, radiusSum);
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
    // Circle center is inside box â€” find minimum separation axis
    vec2 overlap = boxHalf + vec2(radCirc, radCirc) - abs_vec2(localCirc);
    if (overlap.x < overlap.y) {
      float sign = (localCirc.x < 0.f) ? -1.f : 1.f;
      penetrationLocal = vec2(-sign * overlap.x, 0.f);
    } else {
      float sign = (localCirc.y < 0.f) ? -1.f : 1.f;
      penetrationLocal = vec2(0.f, -sign * overlap.y);
    }
  } else {
    // Circle center is outside box â€” push along contact normal
    penetrationLocal = -(diffLocal / dist) * (radCirc - dist);
  }

  // 7. Rotate penetration back to world space
  penetration = rotate(penetrationLocal, tBox.rotation);
  return true;
}

} // namespace Systems
