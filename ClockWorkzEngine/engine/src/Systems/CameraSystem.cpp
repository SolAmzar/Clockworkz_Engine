/******************************************************************************/
/*!
\file           CameraSystem.cpp
\project        ClockWorkz Engine
\brief          Implementation of CameraSystem.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/CameraSystem.hpp"

#include "Components/CameraComponent.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Core/Engine.hpp"
#include "ECS/World.hpp"
#include "Graphics/Camera2D.hpp"

#include <glm/vec2.hpp>

namespace Systems {

void CameraSystem::update(ECS::World& world) {
    auto& camera = Core::Engine::Get().camera();

    // --- Priority 1: explicit CameraComponent (isMain=true) ---
    bool found = false;
    world.each<Components::Transform, Components::CameraComponent>(
        [&](ECS::Entity, Components::Transform& t, Components::CameraComponent& cam) {
            if (found) return;
            if (!cam.isMain) return;

            camera.setPosition(glm::vec2(t.position.x, t.position.y) + cam.offset);
            camera.setZoom(cam.zoom);
            found = true;
        });

    if (found) return;

    // --- Priority 2: engine default – follow entity tagged "Player" ---
    world.each<Components::Tag, Components::Transform>(
        [&](ECS::Entity, Components::Tag& tag, Components::Transform& t) {
            if (found) return;
            if (tag.tag != "Player") return;

            camera.setPosition(glm::vec2(t.position.x, t.position.y));
            found = true;
        });

    // --- Priority 3: nothing found — camera stays as-is ---
}

} // namespace Systems
