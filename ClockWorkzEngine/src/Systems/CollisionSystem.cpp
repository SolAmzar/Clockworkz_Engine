/******************************************************************************/
/*!
\file           CollisionSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          CollisionSystem implementation - AABB intersection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/Transform.hpp"
#include <algorithm> // For std::min/max
#include <cmath>     // For std::abs
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace Systems {

// Helper to rotate vector
static glm::vec2 rotateVec(glm::vec2 v, float angle) {
  float s = std::sin(angle);
  float c = std::cos(angle);
  return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void CollisionSystem::update(ECS::World &world) {
  m_collisions.clear();

  // Collect all entities with Transform + Collider
  // We need to collect them first because we cannot perform nested iteration
  // efficiently/safely with the current world.each() implementation
  std::vector<ECS::Entity> entities;
  world.each<Components::Transform, Components::Collider>(
      [&](ECS::Entity e, Components::Transform &, Components::Collider &) {
        entities.push_back(e);
      });

  // Simple O(N^2) broad phase for now
  // Optimization (spatial partitioning) comes in later phases
  for (size_t i = 0; i < entities.size(); ++i) {
    for (size_t j = i + 1; j < entities.size(); ++j) {
      auto eA = entities[i];
      auto eB = entities[j];

      // Retrieve components directly
      auto *tA = world.getComponent<Components::Transform>(eA);
      auto *cA = world.getComponent<Components::Collider>(eA);
      auto *tB = world.getComponent<Components::Transform>(eB);
      auto *cB = world.getComponent<Components::Collider>(eB);

      // Check for valid pointers (should be valid given the query above)
      if (tA && cA && tB && cB) {
        // Only check solid objects for collision logic logic
        if (cA->isSolid && cB->isSolid) {
          glm::vec2 penetration{0.0f, 0.0f};
          if (cA->shape == Components::Collider::Shape::Box &&
              cB->shape == Components::Collider::Shape::Box) {
            if (checkAABB(*tA, *cA, *tB, *cB, penetration)) {
              m_collisions.push_back({eA, eB});
            }
          } else if (cA->shape == Components::Collider::Shape::Circle &&
                     cB->shape == Components::Collider::Shape::Circle) {
            if (checkCircleCircle(*tA, *cA, *tB, *cB, penetration)) {
              m_collisions.push_back({eA, eB});
            }
          } else {
            // Mixed Shape
            if (cA->shape == Components::Collider::Shape::Box) {
              // A is Box, B is Circle
              if (checkAABBCircle(*tA, *cA, *tB, *cB, penetration)) {
                m_collisions.push_back({eA, eB});
              }
            } else {
              // A is Circle, B is Box
              glm::vec2 p;
              if (checkAABBCircle(*tB, *cB, *tA, *cA, p)) {
                // p is correction for B (Box). So for A (Circle), it is -p.
                penetration = -p;
                m_collisions.push_back({eA, eB});
              }
            }
          }

          // Check if collision occurred (penetration non-zero or just flag)
          // The check functions return true if collision.
          // Note: check functions need to fill m_collisions only if they return
          // true? No, the loop pushes back. Wait, my replaced code pushes back
          // inside the ifs. Good.

          if (!m_collisions.empty() && m_collisions.back().first == eA &&
              m_collisions.back().second == eB) {
            // Resolution Logic

            // Resolution Logic
            // 1. Determine who moves
            bool aMoves = cA->moveable;
            bool bMoves = cB->moveable;

            if (aMoves && !bMoves) {
              // A moves full amount
              tA->position.x += penetration.x;
              tA->position.y += penetration.y;
            } else if (!aMoves && bMoves) {
              // B moves full amount (correction for B is -penetration)
              tB->position.x -= penetration.x;
              tB->position.y -= penetration.y;
            } else if (aMoves && bMoves) {
              // Both move based on Mass Ratio
              float totalMass = cA->mass + cB->mass;
              float ratioA = cB->mass / totalMass; // Inverse mass ratio
              float ratioB = cA->mass / totalMass;

              tA->position.x += penetration.x * ratioA;
              tA->position.y += penetration.y * ratioA;
              tB->position.x -= penetration.x * ratioB;
              tB->position.y -= penetration.y * ratioB;
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
                                glm::vec2 &penetration) {
  // Calculate bounds A
  glm::vec2 offsetA =
      rotateVec(glm::vec2(cA.offset.x, cA.offset.y), tA.rotation);
  float centerAX = tA.position.x + offsetA.x;
  float centerAY = tA.position.y + offsetA.y;

  glm::vec2 sizeA(cA.size.x * tA.scale.x, cA.size.y * tA.scale.y);

  float leftA = centerAX - sizeA.x * 0.5f;
  float rightA = centerAX + sizeA.x * 0.5f;
  float topA = centerAY + sizeA.y * 0.5f;
  float bottomA = centerAY - sizeA.y * 0.5f;

  // Calculate bounds B
  glm::vec2 offsetB =
      rotateVec(glm::vec2(cB.offset.x, cB.offset.y), tB.rotation);
  float centerBX = tB.position.x + offsetB.x;
  float centerBY = tB.position.y + offsetB.y;

  glm::vec2 sizeB(cB.size.x * tB.scale.x, cB.size.y * tB.scale.y);

  float leftB = centerBX - sizeB.x * 0.5f;
  float rightB = centerBX + sizeB.x * 0.5f;
  float topB = centerBY + sizeB.y * 0.5f;
  float bottomB = centerBY - sizeB.y * 0.5f;

  // AABB Intersection Test
  // Exit early if any gap is found
  if (leftA >= rightB || rightA <= leftB || bottomA >= topB ||
      topA <= bottomB) {
    return false;
  }

  // Calculate overlaps
  float overlapX = std::min(rightA, rightB) - std::max(leftA, leftB);
  float overlapY = std::min(topA, topB) - std::max(bottomA, bottomB);

  // Determine Minimum Translation Vector (MTV)
  if (overlapX < overlapY) {
    // Resolve on X axis
    // If A is to the left of B, push A left (-x). Else push A right (+x).
    // Center comparison:
    if (centerAX < centerBX)
      penetration = glm::vec2(-overlapX, 0.0f);
    else
      penetration = glm::vec2(overlapX, 0.0f);
  } else {
    // Resolve on Y axis
    if (centerAY < centerBY)
      penetration = glm::vec2(0.0f, -overlapY);
    else
      penetration = glm::vec2(0.0f, overlapY);
  }

  return true;
}

bool CollisionSystem::checkCircleCircle(const Components::Transform &tA,
                                        const Components::Collider &cA,
                                        const Components::Transform &tB,
                                        const Components::Collider &cB,
                                        glm::vec2 &penetration) {
  glm::vec2 offsetA =
      rotateVec(glm::vec2(cA.offset.x, cA.offset.y), tA.rotation);
  glm::vec2 centerA = glm::vec2(tA.position.x, tA.position.y) + offsetA;

  glm::vec2 offsetB =
      rotateVec(glm::vec2(cB.offset.x, cB.offset.y), tB.rotation);
  glm::vec2 centerB = glm::vec2(tB.position.x, tB.position.y) + offsetB;

  glm::vec2 diff = centerA - centerB;
  float distSq = glm::dot(diff, diff);

  float radA = cA.radius * std::max(tA.scale.x, tA.scale.y);
  float radB = cB.radius * std::max(tB.scale.x, tB.scale.y);

  float radiusSum = radA + radB;

  if (distSq >= radiusSum * radiusSum)
    return false;

  float dist = std::sqrt(distSq);

  if (dist < 1e-4f) {
    // Centers overlap substantially, pick arbitrary direction (e.g., X)
    penetration = glm::vec2(radiusSum, 0.0f);
  } else {
    float penetrationDepth = radiusSum - dist;
    penetration = (diff / dist) * penetrationDepth;
  }

  return true;
}

bool CollisionSystem::checkAABBCircle(const Components::Transform &tBox,
                                      const Components::Collider &cBox,
                                      const Components::Transform &tCirc,
                                      const Components::Collider &cCirc,
                                      glm::vec2 &penetration) {
  // Box center and half-extents
  glm::vec2 offsetBox =
      rotateVec(glm::vec2(cBox.offset.x, cBox.offset.y), tBox.rotation);
  glm::vec2 boxCenter = glm::vec2(tBox.position.x, tBox.position.y) + offsetBox;

  glm::vec2 sizeBox(cBox.size.x * tBox.scale.x, cBox.size.y * tBox.scale.y);
  glm::vec2 boxHalf = sizeBox * 0.5f;

  glm::vec2 boxMin = boxCenter - boxHalf;
  glm::vec2 boxMax = boxCenter + boxHalf;

  // Circle center
  glm::vec2 offsetCirc =
      rotateVec(glm::vec2(cCirc.offset.x, cCirc.offset.y), tCirc.rotation);
  glm::vec2 circCenter =
      glm::vec2(tCirc.position.x, tCirc.position.y) + offsetCirc;

  float radCirc = cCirc.radius * std::max(tCirc.scale.x, tCirc.scale.y);

  // Find closest point on AABB to Circle center
  glm::vec2 closestPoint;
  closestPoint.x = std::max(boxMin.x, std::min(circCenter.x, boxMax.x));
  closestPoint.y = std::max(boxMin.y, std::min(circCenter.y, boxMax.y));

  // Determine distance from closest point to circle center
  glm::vec2 diff = circCenter - closestPoint;
  float distSq = glm::dot(diff, diff);

  if (distSq > radCirc * radCirc)
    return false;

  // Overlap detected
  float dist = std::sqrt(distSq);

  if (dist < 1e-4f) {
    // Circle center is inside AABB.
    // Need to find nearest face to push out.
    // Vector from box center to circle center
    glm::vec2 d = circCenter - boxCenter;
    glm::vec2 overlap = boxHalf + glm::vec2(radCirc) - glm::abs(d);

    if (overlap.x < overlap.y) {
      penetration.x = (d.x < 0) ? -overlap.x : overlap.x;
      penetration.y = 0.0f;
    } else {
      penetration.x = 0.0f;
      penetration.y = (d.y < 0) ? -overlap.y : overlap.y;
    }
    // We want penetration vector to correct BOX position relative to CIRCLE?
    // Wait, the convention in checkAABB is: Vector to push A away from B?
    // Let's stick to: Vector that A should move to un-collide.
    // If we call checkAABBCircle(box, circ), we return vector for BOX.
    // So if circle is to the right, box should move left (-x).
    penetration = -penetration;
  } else {
    // Circle center is outside but close enough
    float penetrationDepth = radCirc - dist;
    // Direction from closest point to center
    glm::vec2 normal = diff / dist;
    // We want to push Box AWAY from circle.
    // diff is (Circ - Point). So normal points TO Circle.
    // Box needs to move opposite to normal.
    penetration = -normal * penetrationDepth;
  }

  return true;
}

} // namespace Systems
