/******************************************************************************/
/*!
\file           StateMachine.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          StateMachine component - generic state data.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <unordered_map>

namespace Components {

struct StateMachine {
  std::string currentState = "Idle";
  float stateTimer = 0.0f;

  // We can add simple variables/parameters here if needed
  // or just rely on other components.
  // For now, it's a tag holding the current state name.

  void changeState(const std::string &newState) {
    if (currentState != newState) {
      currentState = newState;
      stateTimer = 0.0f;
    }
  }
};

} // namespace Components
