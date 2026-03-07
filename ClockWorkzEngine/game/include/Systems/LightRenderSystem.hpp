#pragma once
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/Light.hpp"
#include "Components/Transform.hpp"
#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "ECS/World.hpp"
#include "Graphics/RenderingManager.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

namespace Systems {

inline std::optional<glm::vec2> raycastLight(const glm::vec2& origin,
                                              const glm::vec2& dir,
                                              float maxDist,
                                              ECS::World& world) {
    glm::vec2 closestHit = origin + dir * maxDist;
    float closestDist = maxDist;
    bool hitFound = false;

    world.each<Components::Transform, Components::Collider>(
        [&](ECS::Entity, Components::Transform& t, Components::Collider& c) {
            if (!c.blocksLight) return;
            float hx = c.size.x * 0.5f * t.scale.x;
            float hy = c.size.y * 0.5f * t.scale.y;
            glm::vec2 center = glm::vec2(t.position.x, t.position.y) + glm::vec2(c.offset.x, c.offset.y);
            glm::vec2 boxMin = center - glm::vec2(hx, hy);
            glm::vec2 boxMax = center + glm::vec2(hx, hy);
            glm::vec2 invDir;
            invDir.x = (std::abs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
            invDir.y = (std::abs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;
            glm::vec2 t1 = (boxMin - origin) * invDir;
            glm::vec2 t2 = (boxMax - origin) * invDir;
            float tmin = std::max(0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
            float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));
            if (tmax >= tmin && tmin < closestDist) {
                closestDist = tmin;
                closestHit  = origin + dir * tmin;
                hitFound    = true;
            }
        });

    if (hitFound || closestDist < maxDist) return closestHit;
    return std::nullopt;
}

inline std::vector<glm::vec2> computeVisibilityPolygon(const glm::vec2& origin,
                                                        float angleCenter,
                                                        float fov,
                                                        float radius,
                                                        ECS::World& world) {
    std::vector<glm::vec2> points;
    points.push_back(origin);
    const int numRays = 100;
    float halfFov    = fov * 0.5f;
    float startAngle = angleCenter - halfFov;
    for (int i = 0; i < numRays; ++i) {
        float angle = startAngle + fov * (float(i) / (numRays - 1));
        glm::vec2 dir = {std::cos(angle), std::sin(angle)};
        auto hit = raycastLight(origin, dir, radius, world);
        points.push_back(hit ? *hit : origin + dir * radius);
    }
    return points;
}

inline void renderLightPass(ECS::World& world, Graphics::RenderingManager& renderer) {
    auto& camera = Core::Engine::Get().camera();

    world.each<Components::Transform, Components::Light>(
        [&](ECS::Entity entity, Components::Transform& trans, Components::Light& lt) {
            if (!lt.active) return;

            float batteryFactor = 1.0f;
            if (auto* batt = world.getComponent<Components::Battery>(entity)) {
                if (batt->charge <= 0.0f) return;
                batteryFactor = batt->charge / batt->maxCharge;
            }

            float angle = trans.rotation;
            glm::vec2 offset = {
                lt.offset.x * std::cos(angle) - lt.offset.y * std::sin(angle),
                lt.offset.x * std::sin(angle) + lt.offset.y * std::cos(angle)
            };
            glm::vec2 origin = glm::vec2(trans.position.x, trans.position.y) + offset;

            float fovRad = glm::radians(lt.angle);
            auto lightPoly = computeVisibilityPolygon(origin, angle, fovRad, lt.radius, world);

            renderer.beginLightPass(camera.viewProj(), origin, lt.color,
                                    lt.radius, angle, lt.angle, batteryFactor, lt.intensity);
            renderer.drawLightCone(origin, lightPoly, lt.color);
            renderer.endLightPass();
        });
}

} // namespace Systems
