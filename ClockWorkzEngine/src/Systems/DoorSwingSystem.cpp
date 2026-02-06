/******************************************************************************/
/*!
\file           DoorSwingSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of DoorSwingSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/DoorSwingSystem.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/Transform.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Systems {

void DoorSwingSystem::update(ECS::World &world, double dt) {
  world.each<Components::DoorSwing, Components::Transform>(
      [&](ECS::Entity, Components::DoorSwing &door,
          Components::Transform &trans) {
        // 1. Initialize if needed
        if (!door.initialized) {
          door.initialRotation = trans.rotation;
          door.initialized = true;
        }

        // 2. Calculate Target Rotation
        // Convert swingAngle to radians
        float swingRad = glm::radians(door.swingAngle);
        float targetRot = door.initialRotation;

        if (door.isOpen) {
          // Apply swing
          if (door.clockwise) {
            targetRot -= swingRad; // Clockwise decreases angle (usually)
          } else {
            targetRot += swingRad; // Counter-clockwise increases
          }
        }

        // 3. Interpolate
        float speedRad = glm::radians(door.speed) * static_cast<float>(dt);
        float diff = targetRot - trans.rotation;

        if (std::abs(diff) > 0.001f) {
          if (std::abs(diff) <= speedRad) {
            trans.rotation = targetRot; // Snap to target
          } else {
            // Move towards target
            trans.rotation += glm::sign(diff) * speedRad;
          }
        }
      });
}

} // namespace Systems
