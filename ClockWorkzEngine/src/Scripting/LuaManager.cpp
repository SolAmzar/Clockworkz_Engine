/******************************************************************************/
/*!
\file           LuaManager.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Create functions to run the lua scripts

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Scripting/LuaManager.hpp"
#include "Scripting/LuaBindings.hpp"
#include <iostream>
#include <fstream>

namespace Scripting {

    LuaManager::LuaManager() {
    }

    LuaManager::~LuaManager() {
    }

    void LuaManager::Initialize() {
        if (m_Initialized) {
            std::cout << "[LuaManager] Already initialized" << std::endl;
            return;
        }

        // Open standard Lua libraries
        m_LuaState.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::os,
            sol::lib::math,
            sol::lib::table,
            sol::lib::debug,
            sol::lib::bit32,
            sol::lib::io,
            sol::lib::utf8
        );

        // Register engine-specific bindings
        RegisterEngineFunctions();

        m_Initialized = true;
        std::cout << "[LuaManager] Initialized with Lua "
            << LUA_VERSION_MAJOR << "." << LUA_VERSION_MINOR << "." << LUA_VERSION_RELEASE
            << std::endl;
    }

    bool LuaManager::ExecuteScript(const std::string& filepath) {
        if (!m_Initialized) {
            LogError("LuaManager not initialized");
            return false;
        }

        // Check if file exists
        std::ifstream file(filepath);
        if (!file.good()) {
            LogError("Script file not found: " + filepath);
            return false;
        }
        file.close();

        try {
            m_LuaState.script_file(filepath);
            std::cout << "[LuaManager] Executed script: " << filepath << std::endl;
            return true;
        }
        catch (const sol::error& e) {
            LogError("Error executing script '" + filepath + "': " + e.what());
            return false;
        }
    }

    bool LuaManager::ExecuteString(const std::string& script) {
        if (!m_Initialized) {
            LogError("LuaManager not initialized");
            return false;
        }

        try {
            m_LuaState.script(script);
            return true;
        }
        catch (const sol::error& e) {
            LogError("Error executing Lua string: " + std::string(e.what()));
            return false;
        }
    }

    void LuaManager::RegisterEngineFunctions() {
        // Register basic logging functions
        m_LuaState.set_function("Log", [](const std::string& message) {
            std::cout << "[Lua] " << message << std::endl;
            });

        m_LuaState.set_function("LogWarning", [](const std::string& message) {
            std::cout << "[Lua Warning] " << message << std::endl;
            });

        m_LuaState.set_function("LogError", [](const std::string& message) {
            std::cerr << "[Lua Error] " << message << std::endl;
            });

        // Register math helper functions
        m_LuaState.set_function("Clamp", [](float value, float min, float max) {
            if (value < min) return min;
            if (value > max) return max;
            return value;
            });

        m_LuaState.set_function("Lerp", [](float a, float b, float t) {
            return a + (b - a) * t;
            });

        // Register all engine type bindings (vec2, Transform2D, GameObject, Components, Input)
        RegisterAllBindings(m_LuaState);

        std::cout << "[LuaManager] Engine functions registered" << std::endl;
    }

    sol::environment LuaManager::CreateSandboxedEnvironment() {
        // Create an environment that inherits from globals
        // Scripts can read globals but local assignments won't pollute global namespace
        return sol::environment(m_LuaState, sol::create, m_LuaState.globals());
    }

    sol::protected_function_result LuaManager::ExecuteScriptInEnvironment(
        const std::string& filepath, sol::environment& env) {

        if (!m_Initialized) {
            LogError("LuaManager not initialized");
            return sol::protected_function_result();
        }

        // Check if file exists
        std::ifstream file(filepath);
        if (!file.good()) {
            LogError("Script file not found: " + filepath);
            return sol::protected_function_result();
        }
        file.close();

        return m_LuaState.safe_script_file(filepath, env, sol::script_pass_on_error);
    }

    void LuaManager::LogError(const std::string& message) {
        std::cerr << "[LuaManager Error] " << message << std::endl;
    }

} // namespace ClockWorkz