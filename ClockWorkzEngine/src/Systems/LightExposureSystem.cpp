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
#include "Components/LightExposure.hpp"
#include <algorithm> // for min, max

namespace Systems {

void LightExposureSystem::update(ECS::World &world, double dt) {
  world.each<Components::LightExposure>(
      [&](ECS::Entity, Components::LightExposure &light) {
        float fDt = static_cast<float>(dt);

        if (light.inLight) {
          // Increase exposure
          light.exposureTime += fDt;
        } else {
          // Recover (decrease exposure)
          light.exposureTime -= fDt;
        }

        // Clamp values
        if (light.exposureTime < 0.0f) {
          light.exposureTime = 0.0f;
        }
        if (light.exposureTime > light.maxExposureTime) {
          light.exposureTime = light.maxExposureTime;
        }

        // Determine danger state
        // If we hit max exposure, it becomes dangerous
        // You might want a buffer/hysteresis here in the future,
        // but for now, simple threshold.
        light.isDangerous = (light.exposureTime >= light.maxExposureTime);
      });
}

} // namespace Systems
