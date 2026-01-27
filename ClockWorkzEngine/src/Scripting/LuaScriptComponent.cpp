/******************************************************************************/
/*!
\file           LuaScriptComponent.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Implements LuaScriptComponent for attaching Lua scripts to GameObjects.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Scripting/LuaScriptComponent.hpp"
#include "Scripting/LuaManager.hpp"
#include "Core/Engine.hpp"
#include <iostream>
#include <fstream>

namespace Scripting {

    //-------------------------------------------------------------------------
    // Constructor / Destructor
    //-------------------------------------------------------------------------

    LuaScriptComponent::LuaScriptComponent(const std::string& scriptPath)
        : m_scriptPath(scriptPath)
    {
        loadScript();
    }

    LuaScriptComponent::~LuaScriptComponent() {
        // Call onDestroy if the script has it and we have an owner
        if (m_loaded && m_onDestroy.valid() && owner()) {
            try {
                m_onDestroy(m_scriptTable, owner());
            }
            catch (const sol::error& e) {
                std::cerr << "[LuaScript] Error in onDestroy (" << m_scriptPath
                          << "): " << e.what() << std::endl;
            }
        }
    }

    //-------------------------------------------------------------------------
    // Script Loading
    //-------------------------------------------------------------------------

    void LuaScriptComponent::loadScript() {
        // Check if file exists
        std::ifstream file(m_scriptPath);
        if (!file.good()) {
            std::cerr << "[LuaScript] Script file not found: " << m_scriptPath << std::endl;
            m_loaded = false;
            return;
        }
        file.close();

        try {
            sol::state& lua = Core::Engine::Get().lua().GetState();

            // Create a sandboxed environment for this script
            // The environment inherits from globals but scripts can't pollute global namespace
            m_env = sol::environment(lua, sol::create, lua.globals());

            // Execute the script file in the sandboxed environment
            sol::protected_function_result result = lua.safe_script_file(
                m_scriptPath,
                m_env,
                sol::script_pass_on_error
            );

            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[LuaScript] Load error in " << m_scriptPath
                          << ": " << err.what() << std::endl;
                m_loaded = false;
                return;
            }

            // The script should return a table
            if (result.get_type() != sol::type::table) {
                std::cerr << "[LuaScript] Script must return a table: " << m_scriptPath << std::endl;
                m_loaded = false;
                return;
            }

            m_scriptTable = result.get<sol::table>();

            // Cache function references for performance
            m_onStart = m_scriptTable["onStart"];
            m_onUpdate = m_scriptTable["onUpdate"];
            m_onCollision = m_scriptTable["onCollision"];
            m_onTriggerEnter = m_scriptTable["onTriggerEnter"];
            m_onTriggerExit = m_scriptTable["onTriggerExit"];
            m_onDestroy = m_scriptTable["onDestroy"];

            m_loaded = true;
            m_initialized = false; // Will call onStart on first update

            std::cout << "[LuaScript] Loaded script: " << m_scriptPath << std::endl;
        }
        catch (const sol::error& e) {
            std::cerr << "[LuaScript] Exception loading " << m_scriptPath
                      << ": " << e.what() << std::endl;
            m_loaded = false;
        }
    }

    bool LuaScriptComponent::reload() {
        // Reset state
        m_loaded = false;
        m_initialized = false;
        m_scriptTable = sol::table();
        m_env = sol::environment();
        m_onStart = sol::function();
        m_onUpdate = sol::function();
        m_onCollision = sol::function();
        m_onTriggerEnter = sol::function();
        m_onTriggerExit = sol::function();
        m_onDestroy = sol::function();

        // Reload
        loadScript();
        return m_loaded;
    }

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    void LuaScriptComponent::initializeScript(Gameplay::GameObject& owner) {
        if (m_initialized) return;

        if (m_onStart.valid() && m_onStart.get_type() == sol::type::function) {
            safeCall(m_onStart, "onStart", owner);
        }

        m_initialized = true;
    }

    //-------------------------------------------------------------------------
    // Component Lifecycle Hooks
    //-------------------------------------------------------------------------

    void LuaScriptComponent::onBeforePhysics(Gameplay::GameObject& owner, double dt) {
        // Reserved for future use (e.g., onBeforePhysics callback)
        (void)owner;
        (void)dt;
    }

    void LuaScriptComponent::onUpdate(Gameplay::GameObject& owner, double dt) {
        if (!m_loaded || !m_enabled) return;

        // Initialize on first update
        if (!m_initialized) {
            initializeScript(owner);
        }

        // Call onUpdate
        if (m_onUpdate.valid() && m_onUpdate.get_type() == sol::type::function) {
            safeCallWith(m_onUpdate, "onUpdate", owner, dt);
        }
    }

    void LuaScriptComponent::onRender(Gameplay::GameObject& owner) {
        // Scripts don't typically handle rendering directly
        // Could add onRender callback if needed
        (void)owner;
    }

    //-------------------------------------------------------------------------
    // Custom Event Hooks
    //-------------------------------------------------------------------------

    void LuaScriptComponent::onCollision(Gameplay::GameObject& owner, Gameplay::GameObject& other) {
        if (!m_loaded || !m_enabled) return;

        if (m_onCollision.valid() && m_onCollision.get_type() == sol::type::function) {
            safeCallWith(m_onCollision, "onCollision", owner, &other);
        }
    }

    void LuaScriptComponent::onTriggerEnter(Gameplay::GameObject& owner, Gameplay::GameObject& other) {
        if (!m_loaded || !m_enabled) return;

        if (m_onTriggerEnter.valid() && m_onTriggerEnter.get_type() == sol::type::function) {
            safeCallWith(m_onTriggerEnter, "onTriggerEnter", owner, &other);
        }
    }

    void LuaScriptComponent::onTriggerExit(Gameplay::GameObject& owner, Gameplay::GameObject& other) {
        if (!m_loaded || !m_enabled) return;

        if (m_onTriggerExit.valid() && m_onTriggerExit.get_type() == sol::type::function) {
            safeCallWith(m_onTriggerExit, "onTriggerExit", owner, &other);
        }
    }

    //-------------------------------------------------------------------------
    // Property Access
    //-------------------------------------------------------------------------

    void LuaScriptComponent::setProperty(const std::string& key, sol::object value) {
        if (!m_loaded || !m_scriptTable.valid()) return;
        m_scriptTable[key] = value;
    }

    sol::object LuaScriptComponent::getProperty(const std::string& key) const {
        if (!m_loaded || !m_scriptTable.valid()) return sol::nil;
        return m_scriptTable[key];
    }

    //-------------------------------------------------------------------------
    // Safe Call Helpers
    //-------------------------------------------------------------------------

    void LuaScriptComponent::safeCall(sol::function& func, const char* funcName,
                                       Gameplay::GameObject& owner) {
        if (!func.valid() || func.get_type() != sol::type::function) return;

        try {
            func(m_scriptTable, &owner);
        }
        catch (const sol::error& e) {
            std::cerr << "[LuaScript] Error in " << funcName
                      << " (" << m_scriptPath << "): " << e.what() << std::endl;
        }
    }

} // namespace Scripting
