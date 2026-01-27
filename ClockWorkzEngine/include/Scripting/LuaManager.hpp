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

#include <sol/sol.hpp>
#include <string>
#include <memory>
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
        bool ExecuteScript(const std::string& filepath);

        /**
         * @brief Execute a Lua string directly
         * @param script Lua code as a string
         * @return true if execution succeeded, false otherwise
         */
        bool ExecuteString(const std::string& script);

        /**
         * @brief Get the underlying Lua state for advanced operations
         * @return Reference to the Sol2 state
         */
        sol::state& GetState() { return m_LuaState; }

        /**
         * @brief Call a Lua function by name
         * @tparam Args Variable template for function arguments
         * @param functionName Name of the Lua function
         * @param args Arguments to pass to the function
         */
        template<typename... Args>
        void CallFunction(const std::string& functionName, Args&&... args) {
            try {
                sol::function func = m_LuaState[functionName];
                if (func.valid()) {
                    func(std::forward<Args>(args)...);
                }
                else {
                    LogError("Lua function '" + functionName + "' not found");
                }
            }
            catch (const sol::error& e) {
                LogError(std::string("Lua error calling function: ") + e.what());
            }
        }

        /**
         * @brief Get a global Lua variable
         * @tparam T Type to retrieve
         * @param variableName Name of the global variable
         * @return The value, or default-constructed T if not found
         */
        template<typename T>
        T GetGlobal(const std::string& variableName) {
            return m_LuaState.get_or<T>(variableName, T{});
        }

        /**
         * @brief Set a global Lua variable
         * @tparam T Type to set
         * @param variableName Name of the global variable
         * @param value Value to set
         */
        template<typename T>
        void SetGlobal(const std::string& variableName, T&& value) {
            m_LuaState[variableName] = std::forward<T>(value);
        }

        /**
         * @brief Create a sandboxed environment for script isolation
         * Scripts executed in this environment can access globals but cannot
         * pollute the global namespace.
         * @return A new sandboxed environment
         */
        sol::environment CreateSandboxedEnvironment();

        /**
         * @brief Execute a script file in a sandboxed environment
         * @param filepath Path to the .lua file
         * @param env The sandboxed environment to use
         * @return The result of script execution (typically a table)
         */
        sol::protected_function_result ExecuteScriptInEnvironment(
            const std::string& filepath, sol::environment& env);

    private:
        /**
         * @brief Register engine functions and types with Lua
         */
        void RegisterEngineFunctions();

        /**
         * @brief Internal logging function
         */
        void LogError(const std::string& message);

        sol::state m_LuaState;
        bool m_Initialized = false;
    };

} // namespace Scripting