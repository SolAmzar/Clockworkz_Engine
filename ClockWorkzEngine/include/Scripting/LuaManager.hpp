/******************************************************************************/
/*!
\file           LuaManager.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Create functions to run the lua scripts

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once

#pragma warning(push, 0)

// Disable Code Analysis warnings
#pragma warning(disable : 26495) // type.6: variable uninitialized
#pragma warning(disable : 26439) // f.6: noexcept
#pragma warning(disable : 26496) // es.78: switch fallthrough (sometimes triggered in templated code)
#pragma warning(disable : 26488) // es

#include <sol/sol.hpp>
#pragma warning(pop)
#include <memory>
#include <string>
#include <unordered_map>

namespace Scripting {

class LuaManager {
public:
  LuaManager();
  ~LuaManager();

  // Initialize the Lua state and register engine bindings
  void Initialize();

  // Execute a Lua script file
  bool ExecuteScript(const std::string &filepath);

  // Execute a Lua string directly
  bool ExecuteString(const std::string &script);

  // Get the underlying Lua state for advanced operations
  sol::state &GetState() { return m_LuaState; }

  // Call a Lua function by name
  template <typename... Args>
  void CallFunction(const std::string &functionName, Args &&...args) {
    try {
      sol::function func = m_LuaState[functionName];
      if (func.valid()) {
        func(std::forward<Args>(args)...);
      } else {
        LogError("Lua function '" + functionName + "' not found");
      }
    } catch (const sol::error &e) {
      LogError(std::string("Lua error calling function: ") + e.what());
    }
  }

  // Get a global Lua variable (template)
  template <typename T> T GetGlobal(const std::string &variableName) {
    return m_LuaState.get_or<T>(variableName, T{});
  }

  //Set a global Lua variable
  template <typename T>
  void SetGlobal(const std::string &variableName, T &&value) {
    m_LuaState[variableName] = std::forward<T>(value);
  }

  // Create a sandboxed environment for script isolation
  // Scripts executed in this environment can access globals 
  // but cannot pollute the global namespace.
  sol::environment CreateSandboxedEnvironment();

  // Execute a script file in a sandboxed environment
  sol::protected_function_result ExecuteScriptInEnvironment(const std::string& filepath, sol::environment& env);

  private:

  // Register engine functions and types with Lua       
  void RegisterEngineFunctions();

  // Internal logging function
  void LogError(const std::string &message);

  sol::state m_LuaState;
  bool m_Initialized = false;
};

} // namespace Scripting