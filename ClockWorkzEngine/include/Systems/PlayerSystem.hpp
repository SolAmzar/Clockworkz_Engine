#pragma once
#include "ECS/World.hpp"
#include "Platform/InputManager.hpp"
#include <vector>

namespace Systems {

class PlayerSystem {
public:
  void update(ECS::World &world, double dt, Platform::InputManager &input,
              const std::vector<bool> *layerEnabled = nullptr);
};

} // namespace Systems
