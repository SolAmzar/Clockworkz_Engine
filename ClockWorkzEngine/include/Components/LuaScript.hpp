/******************************************************************************/
/*!
\file           LuaScript.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          LuaScript component — assigns a Lua script to an entity.

                Pure data component. Script path only; heavy Lua objects
                (environments, cached callbacks) are managed by ScriptSystem.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>

namespace Components {

/**
 * @brief LuaScript component — attaches a Lua script to an ECS entity.
 *
 * The ScriptSystem looks for this component, loads the script file,
 * and calls its onStart / onUpdate callbacks each frame.
 */
struct LuaScript {
  // Path relative to assets/scripts/ (e.g. "example_behavior.lua")
  std::string scriptPath;
  
  // Enable / disable script execution at runtimee
  bool enabled{true};     
};

} // namespace Components
