/******************************************************************************/
/*!
\file           ScriptSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          ScriptSystem implementation — loads and runs per-entity Lua
                scripts with sandboxed environments.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/ScriptSystem.hpp"
#include "Components/LuaScript.hpp"
#include "Core/Engine.hpp"
#include "Scripting/EntityRef.hpp"
#include "Scripting/LuaManager.hpp"
#include <filesystem>
#include <iostream>

namespace Systems {


void ScriptSystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  world.each<Components::LuaScript>(
      [&](ECS::Entity entity, Components::LuaScript &script) {
        if (!script.enabled || script.scriptPath.empty())
          return;

        uint32_t id = entity.id;

        // Load script on first encounter or path change
        auto it = m_states.find(id);
        if (it == m_states.end() || it->second.loadedPath != script.scriptPath) {

          if (!loadScript(entity, script.scriptPath)) {
            return;
          }

          it = m_states.find(id);

        }

        ScriptState &state = it->second;

        // onStart
        if (!state.started) {
          if (state.onStart.valid()) {
            Scripting::EntityRef ref(entity, &world);
            try {
              state.onStart(state.scriptTable, ref);
            } catch (const sol::error &e) {
              std::cerr << "[ScriptSystem] onStart error (entity " << id
                        << "): " << e.what() << std::endl;
            }
          }
          state.started = true;
        }

        // onUpdate
        if (state.onUpdate.valid()) {
          Scripting::EntityRef ref(entity, &world);
          try {
            state.onUpdate(state.scriptTable, ref, fDt);
          } catch (const sol::error &e) {
            std::cerr << "[ScriptSystem] onUpdate error (entity " << id
                      << "): " << e.what() << std::endl;
          }
        }
      });
}

void ScriptSystem::clear() {
  m_states.clear();
  std::cout << "[ScriptSystem] Cleared all script states" << std::endl;
}

// Private

bool ScriptSystem::loadScript(ECS::Entity entity,
                              const std::string &scriptPath) {
  std::string fullPath = "assets/scripts/" + scriptPath;

  // Check file exists
  if (!std::filesystem::exists(fullPath)) {
    std::cerr << "[ScriptSystem] Script not found: " << fullPath << std::endl;
    return false;
  }

  // Create sandboxed environment and execute
  auto &luaMgr = Core::Engine::Get().lua();
  sol::environment env = luaMgr.CreateSandboxedEnvironment();
  auto result = luaMgr.ExecuteScriptInEnvironment(fullPath, env);

  if (!result.valid()) {
    sol::error err = result;
    std::cerr << "[ScriptSystem] Load error (" << fullPath
              << "): " << err.what() << std::endl;
    return false;
  }

  // Script must return a table
  sol::object retVal = result;
  if (retVal.get_type() != sol::type::table) {
    std::cerr << "[ScriptSystem] Script must return a table: " << fullPath
              << std::endl;
    return false;
  }

  sol::table scriptTable = retVal.as<sol::table>();

  // Cache callbacks
  ScriptState &state = m_states[entity.id];
  state.env = std::move(env);
  state.scriptTable = scriptTable;
  state.onStart = scriptTable["onStart"];
  state.onUpdate = scriptTable["onUpdate"];
  state.started = false;
  state.loadedPath = scriptPath;

  std::cout << "[ScriptSystem] Loaded " << fullPath << " for entity "
            << entity.id << std::endl;
  return true;
}

} // namespace Systems
