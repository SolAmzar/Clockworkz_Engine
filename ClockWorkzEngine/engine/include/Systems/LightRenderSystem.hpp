/******************************************************************************/
/*!
\file           LightRenderSystem.hpp
\project        ClockWorkz Engine
\brief          Engine-level light rendering system.
                Renders all active Light components using visibility-polygon
                raycasting. Battery and other game-specific logic should be
                handled by the game layer (e.g. toggle lt.active when battery
                runs out) rather than here.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Components/Collider.hpp"
#include "Components/Light.hpp"
#include "Components/Transform.hpp"
#include "Core/Engine.hpp"
#include "ECS/World.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

namespace Systems {

// ---------------------------------------------------------------------------
// If lightOrigin falls inside a blocking box collider (e.g. because the light
// offset on the player entity pushes it into an adjacent wall), push it out to
// the nearest face of that box.  This prevents the "skip-if-inside" guard in
// buildLightPolygon from treating the wall as transparent.
// ---------------------------------------------------------------------------
inline glm::vec2 clampLightOriginOutsideBlockers(glm::vec2 origin, ECS::World& world) {
    static constexpr float kSurfaceBias = 1.0f; // push 1 world-unit outside the face

    world.each<Components::Transform, Components::Collider>(
        [&](ECS::Entity, Components::Transform& transform, Components::Collider& collider) {
            if (!collider.blocksLight) return;
            if (collider.shape != Components::Collider::Shape::Box) return;

            float halfWidth  = collider.size.x * 0.5f * transform.scale.x;
            float halfHeight = collider.size.y * 0.5f * transform.scale.y;

            float cosA = std::cos(transform.rotation);
            float sinA = std::sin(transform.rotation);

            glm::vec2 pivotAdjustedOffset = {
                collider.offset.x - transform.pivot.x,
                collider.offset.y - transform.pivot.y
            };
            glm::vec2 boxCenter = glm::vec2(transform.position.x, transform.position.y)
                + glm::vec2(pivotAdjustedOffset.x * cosA - pivotAdjustedOffset.y * sinA,
                            pivotAdjustedOffset.x * sinA + pivotAdjustedOffset.y * cosA);

            // Transform origin into box local space.
            glm::vec2 delta      = origin - boxCenter;
            glm::vec2 localOrigin = {
                 delta.x * cosA + delta.y * sinA,
                -delta.x * sinA + delta.y * cosA
            };

            // Not inside — nothing to do.
            if (localOrigin.x < -halfWidth  || localOrigin.x > halfWidth)  return;
            if (localOrigin.y < -halfHeight || localOrigin.y > halfHeight) return;

            // Inside the box: find which face is nearest and push out through it.
            float overlapRight  = halfWidth  - localOrigin.x;  // dist to +X face
            float overlapLeft   = halfWidth  + localOrigin.x;  // dist to -X face
            float overlapBottom = halfHeight - localOrigin.y;  // dist to +Y face
            float overlapTop    = halfHeight + localOrigin.y;  // dist to -Y face

            float minOverlap = std::min({overlapRight, overlapLeft, overlapBottom, overlapTop});

            glm::vec2 localPushed = localOrigin;
            if      (minOverlap == overlapRight)  localPushed.x =  halfWidth  + kSurfaceBias;
            else if (minOverlap == overlapLeft)   localPushed.x = -halfWidth  - kSurfaceBias;
            else if (minOverlap == overlapBottom) localPushed.y =  halfHeight + kSurfaceBias;
            else                                  localPushed.y = -halfHeight - kSurfaceBias;

            // Transform the pushed position back to world space.
            origin = boxCenter + glm::vec2(
                localPushed.x * cosA - localPushed.y * sinA,
                localPushed.x * sinA + localPushed.y * cosA);
        });

    return origin;
}

inline std::vector<glm::vec2> buildLightPolygon(const glm::vec2& origin,
                                                float angleCenter,
                                                float fovRad,
                                                float radius,
                                                ECS::Entity sourceEntity,
                                                ECS::World& world) {
    std::vector<glm::vec2> points;
    points.push_back(origin);
    const int numRays   = 100;
    float startAngle    = angleCenter - fovRad * 0.5f;

    for (int rayIndex = 0; rayIndex < numRays; ++rayIndex) {
        float rayAngle      = startAngle + fovRad * (float(rayIndex) / (numRays - 1));
        glm::vec2 rayDir    = { std::cos(rayAngle), std::sin(rayAngle) };
        glm::vec2 hitPoint  = origin + rayDir * radius;
        float closestDist   = radius;

        world.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity blockerEntity, Components::Transform& transform, Components::Collider& collider) {
                if (blockerEntity.id == sourceEntity.id &&
                    blockerEntity.generation == sourceEntity.generation) {
                    return;
                }
                if (!collider.blocksLight) return;

                // Compute world-space collider center — matches CollisionSystem exactly:
                // rotate (offset - pivot) by entity rotation, then add to position.
                float cosAngle = std::cos(transform.rotation);
                float sinAngle = std::sin(transform.rotation);
                auto rotateByEntityAngle = [&](glm::vec2 vec) -> glm::vec2 {
                    return {
                        vec.x * cosAngle - vec.y * sinAngle,
                        vec.x * sinAngle + vec.y * cosAngle
                    };
                };
                glm::vec2 pivotAdjustedOffset = glm::vec2(
                    collider.offset.x - transform.pivot.x,
                    collider.offset.y - transform.pivot.y);
                glm::vec2 colliderCenter = glm::vec2(transform.position.x, transform.position.y)
                                         + rotateByEntityAngle(pivotAdjustedOffset);

                if (collider.shape == Components::Collider::Shape::Box) {
                    float halfWidth  = collider.size.x * 0.5f * transform.scale.x;
                    float halfHeight = collider.size.y * 0.5f * transform.scale.y;

                    // Transform ray into box local space using the INVERSE rotation
                    // (rotate by -angle). Both origin and direction use the same transform.
                    glm::vec2 originToCenter = origin - colliderCenter;
                    glm::vec2 localRayOrigin = {
                         originToCenter.x * cosAngle + originToCenter.y * sinAngle,
                        -originToCenter.x * sinAngle + originToCenter.y * cosAngle
                    };
                    glm::vec2 localRayDir = {
                         rayDir.x * cosAngle + rayDir.y * sinAngle,
                        -rayDir.x * sinAngle + rayDir.y * cosAngle
                    };

                    // Skip if the light origin is inside the box.
                    if (localRayOrigin.x >= -halfWidth  && localRayOrigin.x <= halfWidth &&
                        localRayOrigin.y >= -halfHeight && localRayOrigin.y <= halfHeight) return;

                    // Slab method: find entry/exit distances along X and Y axes.
                    float invDirX = (std::abs(localRayDir.x) < 1e-6f) ? 1e6f : 1.0f / localRayDir.x;
                    float invDirY = (std::abs(localRayDir.y) < 1e-6f) ? 1e6f : 1.0f / localRayDir.y;
                    float slabXEntry = (-halfWidth  - localRayOrigin.x) * invDirX;
                    float slabXExit  = ( halfWidth  - localRayOrigin.x) * invDirX;
                    float slabYEntry = (-halfHeight - localRayOrigin.y) * invDirY;
                    float slabYExit  = ( halfHeight - localRayOrigin.y) * invDirY;
                    float entryDist  = std::max(0.0f, std::max(std::min(slabXEntry, slabXExit),
                                                               std::min(slabYEntry, slabYExit)));
                    float exitDist   = std::min(std::max(slabXEntry, slabXExit),
                                                std::max(slabYEntry, slabYExit));
                    if (exitDist >= entryDist && entryDist < closestDist) {
                        closestDist = entryDist;
                        hitPoint    = origin + rayDir * entryDist;
                    }
                }
                else if (collider.shape == Components::Collider::Shape::Circle) {
                    float scaledRadius = collider.radius * std::max(transform.scale.x, transform.scale.y);

                    // Quadratic ray-circle intersection.
                    // Solving: |origin + t*rayDir - colliderCenter|^2 = scaledRadius^2
                    glm::vec2 originToCenter  = origin - colliderCenter;
                    float     linearCoeff     = 2.0f * glm::dot(originToCenter, rayDir);
                    float     constantCoeff   = glm::dot(originToCenter, originToCenter) - scaledRadius * scaledRadius;
                    float     discriminant    = linearCoeff * linearCoeff - 4.0f * constantCoeff;
                    if (discriminant < 0.0f) return;

                    float entryDist = (-linearCoeff - std::sqrt(discriminant)) * 0.5f;
                    if (entryDist > 0.0f && entryDist < closestDist) {
                        closestDist = entryDist;
                        hitPoint    = origin + rayDir * entryDist;
                    }
                }
            });

        points.push_back(hitPoint);
    }
    return points;
}

inline void renderLightPass(ECS::World& world,
                            Graphics::RenderingManager& renderer,
                            float batteryFactor = 1.0f) {
    auto& camera = Core::Engine::Get().camera();

    world.each<Components::Transform, Components::Light>(
        [&](ECS::Entity lightEntity, Components::Transform& transform, Components::Light& light) {
            if (!light.active) return;

            float lightAngle = transform.rotation;
            glm::vec2 lightOffset = {
                light.offset.x * std::cos(lightAngle) - light.offset.y * std::sin(lightAngle),
                light.offset.x * std::sin(lightAngle) + light.offset.y * std::cos(lightAngle)
            };
            glm::vec2 lightOrigin = glm::vec2(transform.position.x, transform.position.y) + lightOffset;
            // If the light offset pushed the origin inside a wall, pull it back to the surface.
            lightOrigin = clampLightOriginOutsideBlockers(lightOrigin, world);

            float fovRad    = glm::radians(light.angle);
            auto lightPoly  = buildLightPolygon(lightOrigin, lightAngle, fovRad, light.radius, lightEntity, world);

            renderer.beginLightPass(camera.viewProj(), lightOrigin, light.color,
                                    light.radius, lightAngle, light.angle, batteryFactor, light.intensity);
            renderer.drawLightCone(lightOrigin, lightPoly, light.color);
            renderer.endLightPass();
        });
}

} // namespace Systems
