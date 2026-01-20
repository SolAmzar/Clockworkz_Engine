#pragma once
#include "ECS/World.hpp"
#include "Platform/InputManager.hpp"

namespace Systems {

class PlayerSystem {
public:
  void update(ECS::World &world, double dt, Platform::InputManager &input);
};

} // namespace Systems
