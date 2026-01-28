/******************************************************************************/
/*!
\file           DoorSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          System to handle door logic (unlocking).

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "ECS/World.hpp"

namespace Systems {

class DoorSystem {
public:
  void update(ECS::World &world, double dt);
};

} // namespace Systems
