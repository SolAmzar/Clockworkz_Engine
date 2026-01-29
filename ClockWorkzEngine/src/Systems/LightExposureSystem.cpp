/******************************************************************************/
/*!
\file           LightExposureSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of LightExposureSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/LightExposureSystem.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/LightExposure.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"
#include "Math/Vec2.hpp"
#include <algorithm> // for min, max
#include <cmath>
#include <glm/glm.hpp> // For radians
#include <iostream>

namespace Systems {

// Helper for AABB-Line Intersection
// Uses Slab method or Liang-Barsky. Here simple Slab.
static bool LineIntersectsAABB(const vec2 &p1, const vec2 &p2, const vec2 &min,
                               const vec2 &max) {
  vec2 d = p2 - p1;
  vec2 invD;
  invD.x = (std::abs(d.x) < 0.0001f) ? 0.0f : 1.0f / d.x;
  invD.y = (std::abs(d.y) < 0.0001f) ? 0.0f : 1.0f / d.y;

  float tmin = 0.0f;
  float tmax = 1.0f;

  // X axis
  if (std::abs(d.x) < 0.0001f) {
    // Parallel to Y axis
    if (p1.x < min.x || p1.x > max.x)
      return false;
  } else {
    float t1 = (min.x - p1.x) * invD.x;
    float t2 = (max.x - p1.x) * invD.x;
    float axisMin = std::min(t1, t2);
    float axisMax = std::max(t1, t2);
    tmin = std::max(tmin, axisMin);
    tmax = std::min(tmax, axisMax);
  }

  // Y axis
  if (std::abs(d.y) < 0.0001f) {
    // Parallel to X axis
    if (p1.y < min.y || p1.y > max.y)
      return false;
  } else {
    float t1 = (min.y - p1.y) * invD.y;
    float t2 = (max.y - p1.y) * invD.y;
    float axisMin = std::min(t1, t2);
    float axisMax = std::max(t1, t2);
    tmin = std::max(tmin, axisMin);
    tmax = std::min(tmax, axisMax);
  }

  return tmin <= tmax;
}

void LightExposureSystem::update(ECS::World &world, double dt) {
  // 1. Reset all LightExposure states to false initially
  world.each<Components::LightExposure>(
      [&](ECS::Entity, Components::LightExposure &light) {
        light.inLight = false;
      });

  // Pre-collect potential occluders to avoid nested world.each performance hit
  // if possible Or just iterate inside (simpler for now, optimization later)

  // 2. Iterate through all Torch sources
  world.each<Components::Transform, Components::TorchComponent>(
      [&](ECS::Entity torchEnt, Components::Transform &tTrans,
          Components::TorchComponent &torch) {
        if (!torch.active)
          return;

        // Check Battery
        if (world.hasComponent<Components::Battery>(torchEnt)) {
          auto *batt = world.getComponent<Components::Battery>(torchEnt);
          if (batt->charge <= 0.0f)
            return;
        }

        // Calculate Torch World Properties
        float torchAngleRad = tTrans.rotation;
        // Convert local offset to world logic
        vec2 localOffset = torch.offset;

        // Manual rotation of offset (since we don't have a full matrix lib used
        // here yet)
        float c = std::cos(torchAngleRad);
        float s = std::sin(torchAngleRad);
        vec2 worldOffset = {localOffset.x * c - localOffset.y * s,
                            localOffset.x * s + localOffset.y * c};

        vec2 torchPos = tTrans.position + worldOffset;
        vec2 torchDir = {std::cos(torchAngleRad), std::sin(torchAngleRad)};
        float halfFov = glm::radians(torch.angle) * 0.5f;
        float cosHalfFov = std::cos(halfFov);
        float rangeSq = torch.radius * torch.radius;

        // 3. Check against all LightExposure entities
        world.each<Components::Transform, Components::Collider,
                   Components::LightExposure>(
            [&](ECS::Entity targetEnt, Components::Transform &targetTrans,
                Components::Collider &targetCol,
                Components::LightExposure &light) {
              // Optimization: Skip if already lit (unless we support
              // multi-light intensity accumulation later)
              if (light.inLight)
                return;

              // Calculate Target AABB Center
              vec2 targetCenter = targetTrans.position + targetCol.offset;

              // We check 5 points: Center + 4 Corners of the AABB
              // If ANY point is in the cone, the object is lit.
              vec2 halfSize = targetCol.size * 0.5f;
              vec2 points[] = {
                  targetCenter,
                  targetCenter + vec2{-halfSize.x, -halfSize.y}, // Top-Left
                  targetCenter + vec2{halfSize.x, -halfSize.y},  // Top-Right
                  targetCenter + vec2{halfSize.x, halfSize.y},   // Bottom-Right
                  targetCenter + vec2{-halfSize.x, halfSize.y}   // Bottom-Left
              };

              bool hit = false;
              for (const auto &p : points) {
                vec2 toPoint = p - torchPos;
                float distSq = length_sq(toPoint);

                // A. Range Check
                if (distSq > rangeSq)
                  continue;

                // B. Angle Check
                // Avoid normalize if zero length
                if (distSq > 0.0001f) {
                  vec2 dirToPoint = normalize(toPoint);
                  float dotProd = dot(torchDir, dirToPoint);

                  if (dotProd < cosHalfFov) {
                    continue;
                  }
                }

                // C. Occlusion Check (Raycast)
                // Cast ray from torchPos to p
                // Check intersection with ALL solid colliders
                bool occluded = false;

                // Nested iteration over potential occluders
                // Note: deeply nested loops are expensive. In a real engine,
                // use spatial partition.
                world.each<Components::Transform, Components::Collider>(
                    [&](ECS::Entity obsEnt, Components::Transform &obsTrans,
                        Components::Collider &obsCol) {
                      if (occluded)
                        return; // Early exit
                      if (obsEnt.id == torchEnt.id || obsEnt.id == targetEnt.id)
                        return; // Ignore self and source
                      if (!obsCol.isSolid)
                        return; // Only solid objects block light

                      // Calculate Obstacle AABB
                      vec2 obsCenter = obsTrans.position + obsCol.offset;
                      vec2 obsHalf = obsCol.size * 0.5f;
                      vec2 obsMin = obsCenter - obsHalf;
                      vec2 obsMax = obsCenter + obsHalf;

                      if (LineIntersectsAABB(torchPos, p, obsMin, obsMax)) {
                        occluded = true;
                      }
                    });

                if (!occluded) {
                  hit = true;
                  // std::cout << "Light Hit Entity " << targetEnt.id << "\n";
                  break;
                }
              }

              if (hit) {
                if (!light.inLight) {
                  std::cout << "[LightSystem] Entity " << targetEnt.id
                            << " entered light.\n";
                }
                light.inLight = true;
              }
            });
      });

  // 4. Update Exposure Values based on state
  world.each<Components::LightExposure>(
      [&](ECS::Entity, Components::LightExposure &light) {
        float fDt = static_cast<float>(dt);

        if (light.inLight) {
          light.exposureTime += fDt;
        } else {
          light.exposureTime -= fDt;
        }

        // Clamp
        if (light.exposureTime < 0.0f)
          light.exposureTime = 0.0f;
        if (light.exposureTime > light.maxExposureTime)
          light.exposureTime = light.maxExposureTime;

        light.isDangerous = (light.exposureTime >= light.maxExposureTime);
      });
}

} // namespace Systems
