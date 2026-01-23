/******************************************************************************/
/*!
\file           StateMachineSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          StateMachineSystem - updates state timers.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

class StateMachineSystem {
public:
  void update(ECS::World &world, double dt);
};

} // namespace Systems
