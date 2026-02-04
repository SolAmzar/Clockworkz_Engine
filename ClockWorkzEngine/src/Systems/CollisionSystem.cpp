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
        // Check collision if both are solid OR if one is a trigger
        bool isSolidCollision = cA->isSolid && cB->isSolid;
        bool isTriggerCollision = cA->isTrigger || cB->isTrigger;

        if (isSolidCollision || isTriggerCollision) {
          glm::vec2 penetration{0.0f, 0.0f};
          bool collided = false;

          if (cA->shape == Components::Collider::Shape::Box &&
              cB->shape == Components::Collider::Shape::Box) {
            if (checkAABB(*tA, *cA, *tB, *cB, penetration)) {
              collided = true;
            }
          } else if (cA->shape == Components::Collider::Shape::Circle &&
                     cB->shape == Components::Collider::Shape::Circle) {
            if (checkCircleCircle(*tA, *cA, *tB, *cB, penetration)) {
              collided = true;
            }
          } else {
            // Mixed Shape
            if (cA->shape == Components::Collider::Shape::Box) {
              // A is Box, B is Circle
              if (checkAABBCircle(*tA, *cA, *tB, *cB, penetration)) {
                collided = true;
              }
            } else {
              // A is Circle, B is Box
              glm::vec2 p;
              if (checkAABBCircle(*tB, *cB, *tA, *cA, p)) {
                // p is correction for B (Box). So for A (Circle), it is -p.
                penetration = -p;
                collided = true;
              }
            }
          }

          if (collided) {
            m_collisions.push_back({eA, eB});

            // Only resolve if physically solid AND neither is a trigger
            if (isSolidCollision && !isTriggerCollision) {
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
}

bool CollisionSystem::checkAABB(const Components::Transform &tA,
                                const Components::Collider &cA,
                                const Components::Transform &tB,
                                const Components::Collider &cB,
                                glm::vec2 &penetration) {
  // Calculate bounds A
  // Offset logic: Effective Offset = Rotate(ColliderOffset - Pivot)
  glm::vec2 effectiveOffsetA =
      glm::vec2(cA.offset.x - tA.pivot.x, cA.offset.y - tA.pivot.y);
  glm::vec2 rotatedOffsetA = rotateVec(effectiveOffsetA, tA.rotation);

  float centerAX = tA.position.x + rotatedOffsetA.x;
  float centerAY = tA.position.y + rotatedOffsetA.y;

  glm::vec2 sizeA(cA.size.x * tA.scale.x, cA.size.y * tA.scale.y);

  float leftA = centerAX - sizeA.x * 0.5f;
  float rightA = centerAX + sizeA.x * 0.5f;
  float topA = centerAY + sizeA.y * 0.5f;
  float bottomA = centerAY - sizeA.y * 0.5f;

  // Calculate bounds B
  glm::vec2 effectiveOffsetB =
      glm::vec2(cB.offset.x - tB.pivot.x, cB.offset.y - tB.pivot.y);
  glm::vec2 rotatedOffsetB = rotateVec(effectiveOffsetB, tB.rotation);

  float centerBX = tB.position.x + rotatedOffsetB.x;
  float centerBY = tB.position.y + rotatedOffsetB.y;

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
  glm::vec2 effectiveOffsetA =
      glm::vec2(cA.offset.x - tA.pivot.x, cA.offset.y - tA.pivot.y);
  glm::vec2 offsetA = rotateVec(effectiveOffsetA, tA.rotation);
  glm::vec2 centerA = glm::vec2(tA.position.x, tA.position.y) + offsetA;

  glm::vec2 effectiveOffsetB =
      glm::vec2(cB.offset.x - tB.pivot.x, cB.offset.y - tB.pivot.y);
  glm::vec2 offsetB = rotateVec(effectiveOffsetB, tB.rotation);
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
  // 1. Get Box Center in World Space
  glm::vec2 effectiveOffsetBox =
      glm::vec2(cBox.offset.x - tBox.pivot.x, cBox.offset.y - tBox.pivot.y);
  glm::vec2 offsetBox = rotateVec(effectiveOffsetBox, tBox.rotation);
  glm::vec2 boxCenter = glm::vec2(tBox.position.x, tBox.position.y) + offsetBox;

  // 2. Get Circle Center in World Space
  glm::vec2 effectiveOffsetCirc =
      glm::vec2(cCirc.offset.x - tCirc.pivot.x, cCirc.offset.y - tCirc.pivot.y);
  glm::vec2 offsetCirc = rotateVec(effectiveOffsetCirc, tCirc.rotation);
  glm::vec2 circCenter =
      glm::vec2(tCirc.position.x, tCirc.position.y) + offsetCirc;

  float radCirc = cCirc.radius * std::max(tCirc.scale.x, tCirc.scale.y);

  // 3. Transform Circle Center to Box Local Space
  //    (Relative to Box Center, Un-rotated)
  glm::vec2 relCirc = circCenter - boxCenter;
  glm::vec2 localCirc = rotateVec(relCirc, -tBox.rotation);

  // 4. Define Box AABB in Local Space (Centered at 0,0)
  glm::vec2 sizeBox(cBox.size.x * tBox.scale.x, cBox.size.y * tBox.scale.y);
  glm::vec2 boxHalf = sizeBox * 0.5f;

  glm::vec2 boxMin = -boxHalf;
  glm::vec2 boxMax = boxHalf;

  // 5. Find closest point on Local AABB to Local Circle
  glm::vec2 closestLocal;
  closestLocal.x = std::max(boxMin.x, std::min(localCirc.x, boxMax.x));
  closestLocal.y = std::max(boxMin.y, std::min(localCirc.y, boxMax.y));

  // 6. Calculate Distance in Local Space
  glm::vec2 diffLocal = localCirc - closestLocal;
  float distSq = glm::dot(diffLocal, diffLocal);

  if (distSq > radCirc * radCirc)
    return false;

  float dist = std::sqrt(distSq);

  // 7. Calculate Penetration in Local Space
  glm::vec2 penetrationLocal;
  if (dist < 1e-4f) {
    // Circle center is inside Box
    // Push out towards nearest face
    // Vector from center to center (here closest is center? No, closest is
    // center if inside?) If inside, closest point IS the circle center? No,
    // closest point on AABB boundary? Wait, the clamping above returns
    // localCirc if inside. So diffLocal is (0,0). Logic for inside: Find
    // distance to each edge
    float dLeft = localCirc.x - boxMin.x;
    float dRight = boxMax.x - localCirc.x;
    float dBottom = localCirc.y - boxMin.y;
    float dTop = boxMax.y - localCirc.y;

    float minD = std::min({dLeft, dRight, dBottom, dTop});

    if (minD == dLeft)
      penetrationLocal = glm::vec2(-minD - radCirc, 0); // Push Left
    else if (minD == dRight)
      penetrationLocal = glm::vec2(minD + radCirc, 0); // Push Right
    else if (minD == dBottom)
      penetrationLocal = glm::vec2(0, -minD - radCirc); // Push Down
    else
      penetrationLocal = glm::vec2(0, minD + radCirc); // Push Up

    // Wait, we want vector to move Box AWAY from Circle?
    // Standard convention: Penetration vector pushes A out of B.
    // Here A is Box, B is Circle.
    // If we return vector to move Box...
    // Let's stick to: Vector P such that BoxPos += P resolves collision.
    // If Circle is to the Right, Box must move Left.
    // If Circle is inside and slightly right, Box must move Left.

    // Simpler approach for inside:
    // Just find min separation overlap.
    // Overlap = (Half + Radius) - abs(Rel).
    // But that's Axis Aligned logic.
    // Let's use the logic:
    // We want to push the Box such that the Circle is outside.
    // If Circle is at `localCirc`, we want to push the world `boxCenter` away.
    // Actually, simpler: Calculate how to move CIRCLE out of BOX, then negate
    // for Box. Move Circle:
    if (minD == dLeft)
      penetrationLocal = glm::vec2(-minD - radCirc, 0); // Move Circle Left
    else if (minD == dRight)
      penetrationLocal = glm::vec2(minD + radCirc, 0);
    else if (minD == dBottom)
      penetrationLocal = glm::vec2(0, -minD - radCirc);
    else
      penetrationLocal = glm::vec2(0, minD + radCirc);

    // This moves Circle out.
    // So Box movement = -penetrationLocal.
    penetrationLocal = -penetrationLocal;

    // Wait, previous code logic:
    // penetration.x = (d.x < 0) ? -overlap.x : overlap.x;
    // Let's restart Inside Logic to be sure.
    // Overlap on X = (HalfX + Rad) - abs(localCirc.x)
    // Overlap on Y = (HalfY + Rad) - abs(localCirc.y)
    // Smallest overlap wins.
    glm::vec2 overlap = boxHalf + glm::vec2(radCirc) - glm::abs(localCirc);
    if (overlap.x < overlap.y) {
      float sign = (localCirc.x < 0) ? -1.0f : 1.0f;
      // Move Box opposite to Circle
      penetrationLocal = glm::vec2(-sign * overlap.x, 0);
    } else {
      float sign = (localCirc.y < 0) ? -1.0f : 1.0f;
      penetrationLocal = glm::vec2(0, -sign * overlap.y);
    }

  } else {
    // Circle Check (Outside)
    float penetrationDepth = radCirc - dist;
    glm::vec2 normalLocal =
        diffLocal / dist; // From Closest to Center (points to Circle)
    // Box needs to move AWAY from Circle.
    // Normal points to Circle. So move -Normal.
    penetrationLocal = -normalLocal * penetrationDepth;
  }

  // 8. Transform Penetration to World Space
  penetration = rotateVec(penetrationLocal, tBox.rotation);

  return true;
}

} // namespace Systems
