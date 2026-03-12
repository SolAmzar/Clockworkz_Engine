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
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

namespace Systems {

inline std::vector<glm::vec2> buildLightPolygon(const glm::vec2& origin,
                                                  float angleCenter,
                                                  float fovRad,
                                                  float radius,
                                                  ECS::World& world) {
    std::vector<glm::vec2> points;
    points.push_back(origin);
    const int numRays  = 100;
    float startAngle   = angleCenter - fovRad * 0.5f;

    for (int i = 0; i < numRays; ++i) {
        float a = startAngle + fovRad * (float(i) / (numRays - 1));
        glm::vec2 dir = {std::cos(a), std::sin(a)};
        glm::vec2 hit = origin + dir * radius;
        float closestDist = radius;

        world.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity, Components::Transform& t, Components::Collider& c) {
                if (!c.blocksLight) return;

                glm::vec2 center = glm::vec2(t.position.x, t.position.y) +
                                   glm::vec2(c.offset.x, c.offset.y);

                if (c.shape == Components::Collider::Shape::Box) {
                    float hx = c.size.x * 0.5f * t.scale.x;
                    float hy = c.size.y * 0.5f * t.scale.y;

                    // Skip if light origin is inside this box — slab method returns
                    // tmin=0, collapsing the polygon to a point and hiding the light.
                    if (origin.x >= center.x - hx && origin.x <= center.x + hx &&
                        origin.y >= center.y - hy && origin.y <= center.y + hy) return;

                    glm::vec2 invDir;
                    invDir.x = (std::abs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
                    invDir.y = (std::abs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;
                    glm::vec2 t1 = (center - glm::vec2(hx, hy) - origin) * invDir;
                    glm::vec2 t2 = (center + glm::vec2(hx, hy) - origin) * invDir;
                    float tmin = std::max(0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
                    float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));
                    if (tmax >= tmin && tmin < closestDist) {
                        closestDist = tmin;
                        hit = origin + dir * tmin;
                    }
                }
                else if (c.shape == Components::Collider::Shape::Circle) {
                    float r = c.radius * std::max(t.scale.x, t.scale.y);

                    glm::vec2 oc = origin - center;
                    float b    = 2.0f * glm::dot(oc, dir);
                    float cval = glm::dot(oc, oc) - r * r;
                    float disc = b * b - 4.0f * cval;
                    if (disc < 0.0f) return;

                    float t1 = (-b - std::sqrt(disc)) * 0.5f;
                    if (t1 > 0.0f && t1 < closestDist) {
                        closestDist = t1;
                        hit = origin + dir * t1;
                    }
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
        [&](ECS::Entity, Components::Transform& trans, Components::Light& lt) {
            if (!lt.active) return;

            float angle = trans.rotation;
            glm::vec2 offset = {
                lt.offset.x * std::cos(angle) - lt.offset.y * std::sin(angle),
                lt.offset.x * std::sin(angle) + lt.offset.y * std::cos(angle)
            };
            glm::vec2 origin = glm::vec2(trans.position.x, trans.position.y) + offset;

            float fovRad   = glm::radians(lt.angle);
            auto lightPoly = buildLightPolygon(origin, angle, fovRad, lt.radius, world);

            renderer.beginLightPass(camera.viewProj(), origin, lt.color,
                                    lt.radius, angle, lt.angle, batteryFactor, lt.intensity);
            renderer.drawLightCone(origin, lightPoly, lt.color);
            renderer.endLightPass();
        });
}

} // namespace Systems
