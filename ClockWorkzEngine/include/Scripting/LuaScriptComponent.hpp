/******************************************************************************/
/*!
\file           LuaScriptComponent.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Declares LuaScriptComponent, a component that attaches Lua scripts
                to GameObjects for custom behavior.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/GameObject.hpp"
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace Scripting {

    // Forward declaration
    class LuaManager;

    //Component that executes Lua scripts attached to a GameObject.
    // Scripts should return a table with optional callback functions:
    //  - onStart(self, gameObject): Called once when the component initializes
    //  - onUpdate(self, gameObject, dt): Called every frame
    //  - onCollision(self, gameObject, other): Called when collision occurs
    //  - onDestroy(self, gameObject): Called when the component is destroyed
    class LuaScriptComponent final : public Gameplay::Component {
    public:
        // Construct a LuaScriptComponent with a script path.
        explicit LuaScriptComponent(const std::string& scriptPath);

        // Destructor
        ~LuaScriptComponent();

        // Disable copy
        LuaScriptComponent(const LuaScriptComponent&) = delete;
        LuaScriptComponent& operator=(const LuaScriptComponent&) = delete;

        // Allow move
        LuaScriptComponent(LuaScriptComponent&&) noexcept = default;
        LuaScriptComponent& operator=(LuaScriptComponent&&) noexcept = default;

        //---------------------------------------------------------------------
        // Component Lifecycle Hooks
        //---------------------------------------------------------------------

        // Called before physics step each frame (future plans atm)
        void onBeforePhysics(Gameplay::GameObject& owner, double dt) override;

        // Called every frame. Invokes the script's onUpdate function.
        void onUpdate(Gameplay::GameObject& owner, double dt) override;

        // Called when rendering (Currently unused for scripts)
        void onRender(Gameplay::GameObject& owner) override;

        //---------------------------------------------------------------------
        // Custom Event Hooks
        //---------------------------------------------------------------------

        // Call the script's onCollision function when a collision occurs
        void onCollision(Gameplay::GameObject& owner, Gameplay::GameObject& other);

        // Call the script's onTriggerEnter function
        void onTriggerEnter(Gameplay::GameObject& owner, Gameplay::GameObject& other);

        // Call the script's onTriggerExit function
        void onTriggerExit(Gameplay::GameObject& owner, Gameplay::GameObject& other);

        //---------------------------------------------------------------------
        // Script Management
        //---------------------------------------------------------------------

        // Reload the script from disk (hot reload)

        bool reload();

        //Get the script file path
        const std::string& scriptPath() const { return m_scriptPath; }

        // Check if the script is loaded and valid
        bool isLoaded() const { return m_loaded; }

        // Enable or disable script execution
        void setEnabled(bool enabled) { m_enabled = enabled; }

        // Check if script execution is enabled
        bool isEnabled() const { return m_enabled; }

        //---------------------------------------------------------------------
        // Script Property Access
        //---------------------------------------------------------------------

        // Set a property in the script's table from C++
        void setProperty(const std::string& key, sol::object value);

        // Get a property from the script's table
        sol::object getProperty(const std::string& key) const;

        // Get the script's self table for direct manipulation
        sol::table& scriptTable() { return m_scriptTable; }

        // Call a custom function in the script by name
        template<typename... Args>
        void callFunction(const std::string& funcName, Args&&... args);

    private:

        // Load and execute the script file, caching function reference
        void loadScript();

        // Call onStart if the script has it defined
        void initializeScript(Gameplay::GameObject& owner);

        // Helper  func to safely call a script function
        void safeCall(sol::function& func, const char* funcName,
                      Gameplay::GameObject& owner);

        // Helper  func to safely call a script function with extra args
        template<typename... Args>
        void safeCallWith(sol::function& func, const char* funcName,
                          Gameplay::GameObject& owner, Args&&... args);

        // Script identity
        std::string m_scriptPath;

        // Lua state
        sol::environment m_env;
        sol::table m_scriptTable;

        // Cached function references
        sol::function m_onStart;
        sol::function m_onUpdate;
        sol::function m_onCollision;
        sol::function m_onTriggerEnter;
        sol::function m_onTriggerExit;
        sol::function m_onDestroy;

        // State flags
        bool m_loaded = false;
        bool m_initialized = false;
        bool m_enabled = true;
    };

    //-------------------------------------------------------------------------
    // Template Implementations
    //-------------------------------------------------------------------------

    template<typename... Args>
    void LuaScriptComponent::callFunction(const std::string& funcName, Args&&... args) {
        if (!m_loaded || !m_scriptTable.valid()) return;

        sol::function func = m_scriptTable[funcName];
        if (func.valid() && func.get_type() == sol::type::function) {
            try {
                func(m_scriptTable, std::forward<Args>(args)...);
            }
            catch (const sol::error& e) {
                std::cerr << "[LuaScript] Error calling " << funcName
                          << " in " << m_scriptPath << ": " << e.what() << std::endl;
            }
        }
    }

    template<typename... Args>
    void LuaScriptComponent::safeCallWith(sol::function& func, const char* funcName,
                                          Gameplay::GameObject& owner, Args&&... args) {
        if (!func.valid() || func.get_type() != sol::type::function) return;

        try {
            func(m_scriptTable, &owner, std::forward<Args>(args)...);
        }
        catch (const sol::error& e) {
            std::cerr << "[LuaScript] Error in " << funcName
                      << " (" << m_scriptPath << "): " << e.what() << std::endl;
        }
    }

} // namespace Scripting
