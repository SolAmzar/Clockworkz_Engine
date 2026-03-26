/******************************************************************************/
/*!
\file           Config.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Config, a container for engine settings.
                Responsibilities include:
                - loadFromFile() to parse configuration data json format
                - Providing default values if file parsing is not yet supported

                Designed for integration with JSON configuration files to
                control engine parameters such as window size and graphics.

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Core/Config.hpp"
#include <fstream>
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)

bool Core::Config::loadFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return true; // fallback to defaults
  }
  nlohmann::json j;
  file >> j;

  width = j.value("width", width);
  height = j.value("height", height);
  vsync = j.value("vsync", vsync);
  title = j.value("title", title);
  startState          = j.value("startState",          startState);
  gamepadDeadzone     = j.value("gamepad_deadzone",     gamepadDeadzone);
  gamepadSprintThresh = j.value("gamepad_sprint_thresh", gamepadSprintThresh);

  return true;
}
