/*
 * @file LuaManager.h
 * @brief Lua scripting integration for ClockWorkz Engine
 * @author ClockWorkz Team
 */

#pragma once

#pragma warning(push, 0)
// Disable Code Analysis warnings
#pragma warning(disable : 26495) // type.6: variable uninitialized
#pragma warning(disable : 26439) // f.6: noexcept
#pragma warning(disable : 26496) // es.78: switch fallthrough (sometimes
                                 // triggered in templated code)
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

  /*
   * @brief Initialize the Lua state and register engine bindings
   */
  void Initialize();

  /**
   * @brief Execute a Lua script file
   * @param filepath Path to the .lua file
   * @return true if execution succeeded, false otherwise
   */
  bool ExecuteScript(const std::string &filepath);

  /**
   * @brief Execute a Lua string directly
   * @param script Lua code as a string
   * @return true if execution succeeded, false otherwise
   */
  bool ExecuteString(const std::string &script);

  /**
   * @brief Get the underlying Lua state for advanced operations
   * @return Reference to the Sol2 state
   */
  sol::state &GetState() { return m_LuaState; }

  /**
   * @brief Call a Lua function by name
   * @tparam Args Variable template for function arguments
   * @param functionName Name of the Lua function
   * @param args Arguments to pass to the function
   */
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

  /**
   * @brief Get a global Lua variable
   * @tparam T Type to retrieve
   * @param variableName Name of the global variable
   * @return The value, or default-constructed T if not found
   */
  template <typename T> T GetGlobal(const std::string &variableName) {
    return m_LuaState.get_or<T>(variableName, T{});
  }

  /**
   * @brief Set a global Lua variable
   * @tparam T Type to set
   * @param variableName Name of the global variable
   * @param value Value to set
   */
  template <typename T>
  void SetGlobal(const std::string &variableName, T &&value) {
    m_LuaState[variableName] = std::forward<T>(value);
  }

private:
  /**
   * @brief Register engine functions and types with Lua
   */
  void RegisterEngineFunctions();

  /**
   * @brief Internal logging function
   */
  void LogError(const std::string &message);

  sol::state m_LuaState;
  bool m_Initialized = false;
};

} // namespace Scripting