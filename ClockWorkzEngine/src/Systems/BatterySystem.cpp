/******************************************************************************/
/*!
\file           BatterySystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of BatterySystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/BatterySystem.hpp"
#include "Components/Battery.hpp"
#include <algorithm> // for min, max

namespace Systems {

void BatterySystem::update(ECS::World &world, double dt) {
  world.each<Components::Battery>(
      [&](ECS::Entity, Components::Battery &battery) {
        float fDt = static_cast<float>(dt);

        if (battery.isCharging) {
          // Charge logic
          battery.charge += battery.chargeRate * fDt;
        } else {
          // Drain logic
          battery.charge -= battery.drainRate * fDt;
        }

        // Clamp charge between 0 and maxCharge
        if (battery.charge < 0.0f) {
          battery.charge = 0.0f;
        }
        if (battery.charge > battery.maxCharge) {
          battery.charge = battery.maxCharge;
        }
      });
}

} // namespace Systems
