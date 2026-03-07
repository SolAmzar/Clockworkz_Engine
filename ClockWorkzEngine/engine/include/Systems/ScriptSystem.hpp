/******************************************************************************/
/*!
\file           ScriptSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          ScriptSystem — executes per-entity Lua scripts.

                Manages script loading, sandboxed environments, and lifecycle
                callbacks (onStart, onUpdate).  Heavy Lua state is stored here
                in ScriptState, NOT in the LuaScript component.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

#pragma warning(push, 0)
#include <sol/sol.hpp>
#pragma warning(pop)

#include <string>
#include <unordered_map>

namespace Systems {

/**
 * @brief ScriptSystem — manages per-entity Lua script execution.
 *
 * For every entity that carries a Components::LuaScript, the system:
 *  1. Loads the referenced .lua file into a sandboxed sol::environment (once).
 *  2. Calls onStart(self, entityRef)  the first frame.
 *  3. Calls onUpdate(self, entityRef, dt) every subsequent frame.
 */
class ScriptSystem {
public:
  /**
   * @brief Update all entities that have a LuaScript component.
   * @param world The ECS world.
   * @param dt    Delta time in seconds.
   */
  void update(ECS::World &world, double dt);

  /**
   * @brief Clear all cached script state (call on scene transitions).
   */
  void clear();

private:
  // @brief Per-entity runtime data for a loaded script.
  struct ScriptState {
    sol::environment env;    // Sandboxed Lua environment.
    sol::table scriptTable;  // The table returned by the script.
    sol::function onStart;   // Cached onStart callback.
    sol::function onUpdate;  // Cached onUpdate callback.
    bool started{false};     // Has onStart been called?
    std::string loadedPath;  // Last loaded script path (for change detection).
  };

  // @brief Map from entity-id to its per-slot script states.
  std::unordered_map<uint32_t, std::vector<ScriptState>> m_states;

  /**
   * @brief Load (or reload) a script file into slot [idx] for the given entity.
   */
  bool loadScript(ECS::Entity entity, size_t idx, const std::string &scriptPath);
};

} // namespace Systems
