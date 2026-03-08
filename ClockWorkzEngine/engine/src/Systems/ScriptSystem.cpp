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
#include "Scripting/ScriptPropertyReader.hpp"
#include "Components/Battery.hpp"
#include "Components/Light.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/LightExposure.hpp"
#include <filesystem>
#include <iostream>

// Helper: push C++ ScriptPropValue into a sol table key
static void pushPropToLua(sol::table& tbl, const std::string& key,
                          const Components::ScriptPropValue& val) {
    std::visit([&](auto&& v) { tbl[key] = v; }, val);
}

// Helper: pull a sol table key back into a C++ ScriptPropValue
static void pullPropFromLua(const sol::table& tbl, const std::string& key,
                             Components::ScriptPropValue& val) {
    sol::object obj = tbl[key];
    if (!obj.valid()) return;
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        auto opt = obj.as<sol::optional<T>>();
        if (opt) val = *opt;
    }, val);
}

namespace Systems {


  // ---------------------------------------------------------------------------
  // Static Helper Functions for World table (avoid closures capturing world&)
  // ---------------------------------------------------------------------------
  static sol::optional<Scripting::EntityRef> LuaWorldFindByTag(ECS::World& world, const std::string& tag) {
      sol::optional<Scripting::EntityRef> result;
      world.each<Components::Tag>([&](ECS::Entity e, Components::Tag& t) {
          if (!result && t.tag == tag)
              result = Scripting::EntityRef(e, &world);
      });
      return result;
  }

  static void LuaWorldEachByTag(ECS::World& world, const std::string& tag, sol::function fn) {
      world.each<Components::Tag>([&](ECS::Entity e, Components::Tag& t) {
          if (t.tag == tag) {
              Scripting::EntityRef ref(e, &world);
              fn(ref);
          }
      });
  }

  static void LuaWorldEachFragment(ECS::World& world, sol::function fn) {
      world.each<Components::Fragment>([&](ECS::Entity e, Components::Fragment&) {
          Scripting::EntityRef ref(e, &world);
          fn(ref);
      });
  }

  static void LuaWorldEachLight(ECS::World& world, sol::function fn) {
      world.each<Components::Light>([&](ECS::Entity e, Components::Light&) {
          Scripting::EntityRef ref(e, &world);
          fn(ref);
      });
  }

  static void LuaWorldEachDoor(ECS::World& world, sol::function fn) {
      world.each<Components::Door>([&](ECS::Entity e, Components::Door&) {
          Scripting::EntityRef ref(e, &world);
          fn(ref);
      });
  }

  static void LuaWorldEachDoorSwing(ECS::World& world, sol::function fn) {
      world.each<Components::DoorSwing>([&](ECS::Entity e, Components::DoorSwing&) {
          Scripting::EntityRef ref(e, &world);
          fn(ref);
      });
  }

  static void LuaWorldEachLightExposure(ECS::World& world, sol::function fn) {
      world.each<Components::LightExposure>([&](ECS::Entity e, Components::LightExposure&) {
          Scripting::EntityRef ref(e, &world);
          fn(ref);
      });
  }

  static void LuaWorldEachWithScript(ECS::World& world, const std::string& scriptPath, sol::function fn) {
      world.each<Components::LuaScript>([&](ECS::Entity e, Components::LuaScript& ls) {
          for (auto& entry : ls.scripts) {
              if (entry.scriptPath == scriptPath && entry.enabled) {
                  Scripting::EntityRef ref(e, &world);
                  fn(ref);
                  break;
              }
          }
      });
  }

  static sol::object LuaWorldGetScriptProp(ECS::World& world, uint32_t entityId, const std::string& key) {
      sol::state& luaRef = Core::Engine::Get().lua().GetState();
      sol::object found = sol::make_object(luaRef, sol::nil);
      world.each<Components::LuaScript>([&](ECS::Entity e, Components::LuaScript& ls) {
          if (e.id != entityId) return;
          for (auto& entry : ls.scripts) {
              auto it = entry.properties.find(key);
              if (it != entry.properties.end()) {
                  found = std::visit([&luaRef](auto&& v) -> sol::object {
                      return sol::make_object(luaRef, v);
                  }, it->second);
                  return;
              }
          }
      });
      return found;
  }

  static void LuaWorldSetScriptProp(ECS::World& world, uint32_t entityId, const std::string& key, sol::object val) {
      world.each<Components::LuaScript>([&](ECS::Entity e, Components::LuaScript& ls) {
          if (e.id != entityId) return;
          for (auto& entry : ls.scripts) {
              auto it = entry.properties.find(key);
              if (it == entry.properties.end()) continue;
              std::visit([&](auto&& existing) {
                  using T = std::decay_t<decltype(existing)>;
                  auto opt = val.as<sol::optional<T>>();
                  if (opt) it->second = *opt;
              }, it->second);
              return;
          }
      });
  }

void ScriptSystem::update(ECS::World &world, double dt) {
  float fDt = static_cast<float>(dt);

  // Set up the World Lua global so any script can query cross-entity data.
  // Functions are rebuilt each frame so they capture the live world reference.
  {
    auto &luaMgr = Core::Engine::Get().lua();
    sol::state &lua = luaMgr.GetState();

    sol::table worldTable = lua.create_table();

    worldTable.set_function("findByTag", [&world](const std::string& tag) { return LuaWorldFindByTag(world, tag); });
    worldTable.set_function("eachByTag", [&world](const std::string& tag, sol::function fn) { LuaWorldEachByTag(world, tag, fn); });
    worldTable.set_function("eachFragment", [&world](sol::function fn) { LuaWorldEachFragment(world, fn); });
    worldTable.set_function("eachLight", [&world](sol::function fn) { LuaWorldEachLight(world, fn); });
    worldTable.set_function("eachDoor", [&world](sol::function fn) { LuaWorldEachDoor(world, fn); });
    worldTable.set_function("eachDoorSwing", [&world](sol::function fn) { LuaWorldEachDoorSwing(world, fn); });
    worldTable.set_function("eachLightExposure", [&world](sol::function fn) { LuaWorldEachLightExposure(world, fn); });
    worldTable.set_function("eachWithScript", [&world](const std::string& path, sol::function fn) { LuaWorldEachWithScript(world, path, fn); });
    worldTable.set_function("getScriptProp", [&world](uint32_t id, const std::string& key) { return LuaWorldGetScriptProp(world, id, key); });
    worldTable.set_function("setScriptProp", [&world](uint32_t id, const std::string& key, sol::object val) { LuaWorldSetScriptProp(world, id, key, val); });

    lua["World"] = worldTable;
  }

  world.each<Components::LuaScript>(
      [&](ECS::Entity entity, Components::LuaScript &luaScript) {
        uint32_t id = entity.id;
        auto& states = m_states[id];
        states.resize(luaScript.scripts.size());

        for (size_t i = 0; i < luaScript.scripts.size(); ++i) {
          Components::LuaScriptEntry& script = luaScript.scripts[i];
          if (!script.enabled || script.scriptPath.empty()) continue;

          ScriptState& state = states[i];

          // Load/reload if path changed
          if (state.loadedPath != script.scriptPath) {
            if (!loadScript(entity, i, script.scriptPath)) continue;

            // Seed missing properties from declared defaults
            const auto& propDefs = Scripting::ScriptPropertyReader::read(script.scriptPath);
            for (const auto& def : propDefs) {
              if (!script.properties.count(def.name))
                script.properties[def.name] = def.defaultValue;
            }
          }

          // Inject C++ property values → Lua self table
          for (auto& [key, val] : script.properties)
            pushPropToLua(state.scriptTable, key, val);

          // onStart
          if (!state.started) {
            if (state.onStart.valid()) {
              Scripting::EntityRef ref(entity, &world);
              try {
                state.onStart(state.scriptTable, ref);
              } catch (const sol::error &e) {
                std::cerr << "[ScriptSystem] onStart error (entity " << id
                          << " slot " << i << "): " << e.what() << std::endl;
              }
            }
            state.started = true;
            for (auto& [key, val] : script.properties)
              pullPropFromLua(state.scriptTable, key, val);
          }

          // onUpdate
          if (state.onUpdate.valid()) {
            Scripting::EntityRef ref(entity, &world);
            try {
              state.onUpdate(state.scriptTable, ref, fDt);
            } catch (const sol::error &e) {
              std::cerr << "[ScriptSystem] onUpdate error (entity " << id
                        << " slot " << i << "): " << e.what() << std::endl;
            }
            for (auto& [key, val] : script.properties)
              pullPropFromLua(state.scriptTable, key, val);
          }
        }
      });
}

void ScriptSystem::clear() {
  m_states.clear();
  // Release the World table so its lambdas (which capture world&) are freed
  // while the ECS world is still alive. Prevents dangling-reference UB and
  // the associated sol::function leak reported by the CRT leak detector.
  if (Core::Engine::IsInitialized()) {
    Core::Engine::Get().lua().GetState()["World"] = sol::nil;
  }
  std::cout << "[ScriptSystem] Cleared all script states" << std::endl;
}

// Private

bool ScriptSystem::loadScript(ECS::Entity entity, size_t idx,
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

  // Cache callbacks into the correct slot
  ScriptState &state = m_states[entity.id][idx];
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
