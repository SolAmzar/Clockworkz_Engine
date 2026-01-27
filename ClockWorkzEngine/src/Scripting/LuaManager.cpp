/**
 * @file LuaManager.cpp
 * @brief Implementation of Lua scripting integration
 * @author ClockWorkz Team
 */

#include "Scripting/LuaManager.hpp"
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
        // Register a basic logging function
        m_LuaState.set_function("Log", [](const std::string& message) {
            std::cout << "[Lua] " << message << std::endl;
            });

        // Register a warning function
        m_LuaState.set_function("LogWarning", [](const std::string& message) {
            std::cout << "[Lua Warning] " << message << std::endl;
            });

        // Register an error function
        m_LuaState.set_function("LogError", [](const std::string& message) {
            std::cerr << "[Lua Error] " << message << std::endl;
            });

        // Example: Register a simple math helper
        m_LuaState.set_function("Clamp", [](float value, float min, float max) {
            if (value < min) return min;
            if (value > max) return max;
            return value;
            });

        // TODO: Register your engine-specific types and functions here
        // Examples:
        // - Entity management
        // - Transform component
        // - Input system
        // - Audio system
        // - Scene management

        std::cout << "[LuaManager] Engine functions registered" << std::endl;
    }

    void LuaManager::LogError(const std::string& message) {
        std::cerr << "[LuaManager Error] " << message << std::endl;
    }

} // namespace ClockWorkz