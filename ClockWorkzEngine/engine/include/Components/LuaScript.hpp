/******************************************************************************/
/*!
\file           LuaScript.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          LuaScript component — assigns a Lua script to an entity.

                Pure data component. Script path only; heavy Lua objects
                (environments, cached callbacks) are managed by ScriptSystem.
                The properties map stores per-instance values declared in the
                script's Properties table; defaults are seeded from the script,
                then overridden by saved scene data and inspector edits.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Components {

using ScriptPropValue = std::variant<float, int, bool, std::string>;

// One script slot — an entity can have many of these.
struct LuaScriptEntry {
  std::string scriptPath;   // relative to assets/scripts/
  bool        enabled{true};
  std::unordered_map<std::string, ScriptPropValue> properties;
};

// Container component: holds all script slots for an entity.
struct LuaScript {
  std::vector<LuaScriptEntry> scripts;
};

} // namespace Components
