/******************************************************************************/
/*!
\file           ActivationSystem.cpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Implementation of the Scene class.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include "Systems/CollisionSystem.hpp"
#include <vector>

namespace Systems {

class ActivationSystem {
public:
  void update(ECS::World &world, const CollisionSystem &collisionSystem);

private:
  void executeAction(ECS::World &world, ECS::Entity zoneEntity);
};

} // namespace Systems
