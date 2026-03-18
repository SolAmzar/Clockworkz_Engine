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

inline glm::vec2 rotateLightVec(const glm::vec2& v, float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

inline bool raycastBoxLight(const glm::vec2& origin,
                            const glm::vec2& dir,
                            const Components::Transform& t,
                            const Components::Collider& c,
                            float maxDist,
                            float& hitDist) {
    glm::vec2 center = glm::vec2(t.position.x, t.position.y) +
                       rotateLightVec(glm::vec2(c.offset.x - t.pivot.x,
                                                c.offset.y - t.pivot.y),
                                      t.rotation);
    glm::vec2 localOrigin = rotateLightVec(origin - center, -t.rotation);
    glm::vec2 localDir = rotateLightVec(dir, -t.rotation);
    glm::vec2 halfExtents = {c.size.x * t.scale.x * 0.5f,
                             c.size.y * t.scale.y * 0.5f};

    // If the light starts inside a blocker, ignore that blocker for this ray.
    if (localOrigin.x >= -halfExtents.x && localOrigin.x <= halfExtents.x &&
        localOrigin.y >= -halfExtents.y && localOrigin.y <= halfExtents.y) {
        return false;
    }

    float tmin = 0.0f;
    float tmax = maxDist;
    auto updateSlab = [&](float originAxis, float dirAxis, float minAxis,
                          float maxAxis) {
        if (std::abs(dirAxis) < 1e-6f) {
            return originAxis >= minAxis && originAxis <= maxAxis;
        }

        float invDir = 1.0f / dirAxis;
        float t1 = (minAxis - originAxis) * invDir;
        float t2 = (maxAxis - originAxis) * invDir;
        if (t1 > t2) std::swap(t1, t2);

        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        return tmax >= tmin;
    };

    if (!updateSlab(localOrigin.x, localDir.x, -halfExtents.x, halfExtents.x)) {
        return false;
    }
    if (!updateSlab(localOrigin.y, localDir.y, -halfExtents.y, halfExtents.y)) {
        return false;
    }

    hitDist = tmin;
    return hitDist >= 0.0f && hitDist < maxDist;
}

inline bool raycastCircleLight(const glm::vec2& origin,
                               const glm::vec2& dir,
                               const Components::Transform& t,
                               const Components::Collider& c,
                               float maxDist,
                               float& hitDist) {
    glm::vec2 center = glm::vec2(t.position.x, t.position.y) +
                       rotateLightVec(glm::vec2(c.offset.x - t.pivot.x,
                                                c.offset.y - t.pivot.y),
                                      t.rotation);
    float radius = c.radius * std::max(t.scale.x, t.scale.y);

    glm::vec2 oc = origin - center;
    float b = 2.0f * glm::dot(oc, dir);
    float cval = glm::dot(oc, oc) - radius * radius;
    float disc = b * b - 4.0f * cval;
    if (disc < 0.0f) {
        return false;
    }

    float sqrtDisc = std::sqrt(disc);
    float t0 = (-b - sqrtDisc) * 0.5f;
    float t1 = (-b + sqrtDisc) * 0.5f;
    float candidate = (t0 >= 0.0f) ? t0 : t1;
    if (candidate < 0.0f || candidate >= maxDist) {
        return false;
    }

    hitDist = candidate;
    return true;
}

inline std::vector<glm::vec2> buildLightPolygon(const glm::vec2& origin,
                                                float angleCenter,
                                                float fovRad,
                                                float radius,
                                                ECS::Entity sourceEntity,
                                                ECS::World& world) {
    std::vector<glm::vec2> points;
    points.push_back(origin);
    const int numRays = 100;
    float startAngle = angleCenter - fovRad * 0.5f;

    for (int i = 0; i < numRays; ++i) {
        float a = startAngle + fovRad * (static_cast<float>(i) / (numRays - 1));
        glm::vec2 dir = {std::cos(a), std::sin(a)};
        glm::vec2 hit = origin + dir * radius;
        float closestDist = radius;

        world.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity blockerEntity, Components::Transform& t, Components::Collider& c) {
                if (blockerEntity.id == sourceEntity.id &&
                    blockerEntity.generation == sourceEntity.generation) {
                    return;
                }
                if (!c.blocksLight) return;

                float hitDist = closestDist;
                bool didHit = false;

                if (c.shape == Components::Collider::Shape::Box) {
                    didHit = raycastBoxLight(origin, dir, t, c, closestDist, hitDist);
                } else if (c.shape == Components::Collider::Shape::Circle) {
                    didHit = raycastCircleLight(origin, dir, t, c, closestDist, hitDist);
                }

                if (didHit) {
                    closestDist = hitDist;
                    hit = origin + dir * hitDist;
                }
            });

        points.push_back(hit);
    }
    return points;
}

inline void renderLightPass(ECS::World& world,
                            Graphics::RenderingManager& renderer,
                            float batteryFactor = 1.0f) {
    auto& camera = Core::Engine::Get().camera();

    world.each<Components::Transform, Components::Light>(
        [&](ECS::Entity lightEntity, Components::Transform& trans, Components::Light& lt) {
            if (!lt.active) return;

            float angle = trans.rotation;
            glm::vec2 offset = {
                lt.offset.x * std::cos(angle) - lt.offset.y * std::sin(angle),
                lt.offset.x * std::sin(angle) + lt.offset.y * std::cos(angle)
            };
            glm::vec2 origin = glm::vec2(trans.position.x, trans.position.y) + offset;

            float fovRad = glm::radians(lt.angle);
            auto lightPoly = buildLightPolygon(origin, angle, fovRad, lt.radius, lightEntity, world);

            renderer.beginLightPass(camera.viewProj(), origin, lt.color,
                                    lt.radius, angle, lt.angle, batteryFactor, lt.intensity);
            renderer.drawLightCone(origin, lightPoly, lt.color);
            renderer.endLightPass();
        });
}

} // namespace Systems
