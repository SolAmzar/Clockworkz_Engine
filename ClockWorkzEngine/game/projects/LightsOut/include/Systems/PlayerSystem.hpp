/******************************************************************************/
/*!
\file           PlayerSystem.hpp
\project        Lights Out
\author(s)      [Syafiq] 100%
\brief          Declares player-specific gameplay updates for Lights Out.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
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
