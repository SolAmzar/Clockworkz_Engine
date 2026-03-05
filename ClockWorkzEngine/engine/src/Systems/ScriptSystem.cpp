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
#include "Components/Tag.hpp"
#include "Core/Engine.hpp"
#include "Scripting/EntityRef.hpp"
#include "Scripting/LuaManager.hpp"
// Game-specific components for World query helpers (known engine/game coupling)
#include "Components/Battery.hpp"
#include "Components/Torch.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/LightExposure.hpp"
#include <filesystem>
#include <iostream>

namespace Systems {


void ScriptSystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  // Set up the World Lua global so any script can query cross-entity data.
  // Functions are rebuilt each frame so they capture the live world reference.
  {
    auto &luaMgr = Core::Engine::Get().lua();
    sol::state &lua = luaMgr.GetState();

    sol::table worldTable = lua.create_table();

    // findByTag(tag) -> EntityRef or nil
    worldTable.set_function("findByTag",
        [&world](const std::string &tag) -> sol::optional<Scripting::EntityRef> {
          sol::optional<Scripting::EntityRef> result;
          world.each<Components::Tag>([&](ECS::Entity e, Components::Tag &t) {
            if (!result && t.tag == tag)
              result = Scripting::EntityRef(e, &world);
          });
          return result;
        });

    // eachByTag(tag, fn) - iterate entities matching a tag
    worldTable.set_function("eachByTag",
        [&world](const std::string &tag, sol::function fn) {
          world.each<Components::Tag>([&](ECS::Entity e, Components::Tag &t) {
            if (t.tag == tag) {
              Scripting::EntityRef ref(e, &world);
              fn(ref);
            }
          });
        });

    // eachFragment(fn)
    worldTable.set_function("eachFragment",
        [&world](sol::function fn) {
          world.each<Components::Fragment>([&](ECS::Entity e, Components::Fragment &) {
            Scripting::EntityRef ref(e, &world);
            fn(ref);
          });
        });

    // eachTorch(fn)
    worldTable.set_function("eachTorch",
        [&world](sol::function fn) {
          world.each<Components::TorchComponent>([&](ECS::Entity e, Components::TorchComponent &) {
            Scripting::EntityRef ref(e, &world);
            fn(ref);
          });
        });

    // eachDoor(fn)
    worldTable.set_function("eachDoor",
        [&world](sol::function fn) {
          world.each<Components::Door>([&](ECS::Entity e, Components::Door &) {
            Scripting::EntityRef ref(e, &world);
            fn(ref);
          });
        });

    // eachDoorSwing(fn)
    worldTable.set_function("eachDoorSwing",
        [&world](sol::function fn) {
          world.each<Components::DoorSwing>([&](ECS::Entity e, Components::DoorSwing &) {
            Scripting::EntityRef ref(e, &world);
            fn(ref);
          });
        });

    // eachLightExposure(fn)
    worldTable.set_function("eachLightExposure",
        [&world](sol::function fn) {
          world.each<Components::LightExposure>([&](ECS::Entity e, Components::LightExposure &) {
            Scripting::EntityRef ref(e, &world);
            fn(ref);
          });
        });

    lua["World"] = worldTable;
  }

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
