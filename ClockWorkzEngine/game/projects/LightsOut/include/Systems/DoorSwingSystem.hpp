/******************************************************************************/
/*!
\file           DoorSwingSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          System to handle DoorSwing rotation logic.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "ECS/World.hpp"

namespace Systems {

class DoorSwingSystem {
public:
  void update(ECS::World &world, double dt);
};

} // namespace Systems
