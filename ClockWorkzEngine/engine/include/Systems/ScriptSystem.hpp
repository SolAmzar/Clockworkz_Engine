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

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
  ~ScriptSystem() { clear(); }

  // Callback type for game code to inject custom World.eachXxx() functions.
  // Called every frame after the built-in World table entries are registered.
  // Signature: void(ECS::World&, sol::table& worldTable)
  using WorldTableExtensionFn = std::function<void(ECS::World&, sol::table&)>;
  void setWorldTableExtension(WorldTableExtensionFn fn);

  /**
   * @brief Update all entities that have a LuaScript component.
   * @param world      The ECS world.
   * @param dt         Delta time in seconds.
   * @param collisions Current-frame collision pairs from CollisionSystem.
   */
  void update(ECS::World& world, double dt,
              const std::vector<std::pair<ECS::Entity, ECS::Entity>>& collisions);

  /**
   * @brief Clear all cached script state (call on scene transitions).
   * Also nils out the Lua World global to release world-capturing lambdas.
   */
  void clear();

private:
  // @brief Per-entity runtime data for a loaded script.
  struct ScriptState {
    sol::environment env;            // Sandboxed Lua environment.
    sol::table scriptTable;          // The table returned by the script.
    sol::function onStart;           // Cached onStart callback.
    sol::function onUpdate;          // Cached onUpdate callback.
    sol::function onTriggerEnter;    // Fires once when a collision pair starts.
    sol::function onTriggerExit;     // Fires once when a collision pair ends.
    sol::function onCollisionEnter;  // Same timing as onTriggerEnter (solid).
    bool started{false};             // Has onStart been called?
    std::string loadedPath;          // Last loaded script path (for change detection).
  };

  // @brief Map from entity-id to its per-slot script states.
  std::unordered_map<uint32_t, std::vector<ScriptState>> m_states;

  // @brief Map canonical pair-key → full entity pair for enter/exit detection.
  // Key encodes ordered (min_id, max_id) so (A,B) and (B,A) share the same key.
  std::unordered_map<uint64_t, std::pair<ECS::Entity, ECS::Entity>> m_prevCollisionPairs;

  // @brief Optional game-side extension injected into the World Lua table each frame.
  WorldTableExtensionFn m_worldTableExtensionFn;

  // @brief Canonical collision pair key: order-independent, id-based.
  static uint64_t collisionKey(ECS::Entity a, ECS::Entity b) noexcept {
    uint32_t lo = std::min(a.id, b.id);
    uint32_t hi = std::max(a.id, b.id);
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
  }

  /**
   * @brief Load (or reload) a script file into slot [idx] for the given entity.
   */
  bool loadScript(ECS::Entity entity, size_t idx, const std::string& scriptPath);
};

} // namespace Systems
