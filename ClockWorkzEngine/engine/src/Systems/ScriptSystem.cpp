/******************************************************************************/
/*!
\file           ScriptSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 97% [Amzar] 3%
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
#include "Components/Light.hpp"
#include "Components/Collider.hpp"
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

  static void LuaWorldEachLight(ECS::World& world, sol::function fn) {
      world.each<Components::Light>([&](ECS::Entity e, Components::Light&) {
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

void ScriptSystem::setWorldTableExtension(WorldTableExtensionFn fn) {
  m_worldTableExtensionFn = std::move(fn);
}

void ScriptSystem::setLoadLevelFn(LoadLevelFn fn) {
  m_loadLevelFn = std::move(fn);
}

void ScriptSystem::setCurrentLevelFn(CurrentLevelFn fn) {
  m_currentLevelFn = std::move(fn);
}

void ScriptSystem::update(ECS::World& world, double dt,
                          const std::vector<std::pair<ECS::Entity, ECS::Entity>>& collisions) {
  float fDt = static_cast<float>(dt);

  // Set up the World Lua global so any script can query cross-entity data.
  // Functions are rebuilt each frame so they capture the live world reference.
  {
    auto &luaMgr = Core::Engine::Get().lua();
    sol::state &lua = luaMgr.GetState();

    sol::table worldTable = lua.create_table();

    worldTable.set_function("findByTag", [&world](const std::string& tag) { return LuaWorldFindByTag(world, tag); });
    worldTable.set_function("eachByTag", [&world](const std::string& tag, sol::function fn) { LuaWorldEachByTag(world, tag, fn); });
    worldTable.set_function("eachLight", [&world](sol::function fn) { LuaWorldEachLight(world, fn); });
    worldTable.set_function("eachWithScript", [&world](const std::string& path, sol::function fn) { LuaWorldEachWithScript(world, path, fn); });
    worldTable.set_function("getScriptProp", [&world](uint32_t id, const std::string& key) { return LuaWorldGetScriptProp(world, id, key); });
    worldTable.set_function("setScriptProp", [&world](uint32_t id, const std::string& key, sol::object val) { LuaWorldSetScriptProp(world, id, key, val); });
    worldTable.set_function("loadLevel", [this](const std::string& name) {
        if (m_loadLevelFn) m_loadLevelFn(name);
    });
    worldTable.set_function("getCurrentLevel", [this]() {
        return m_currentLevelFn ? m_currentLevelFn() : std::string{};
    });
    worldTable.set_function("eachWall", [&world](sol::function fn) {
        world.each<Components::Collider>([&](ECS::Entity e, Components::Collider& col) {
            if (col.isSolid && col.blocksLight && !col.moveable) {
                Scripting::EntityRef ref(e, &world);
                fn(ref);
            }
        });
    });

    lua["World"] = worldTable;

    // Let game code inject additional World.eachXxx() functions without touching engine code.
    if (m_worldTableExtensionFn)
      m_worldTableExtensionFn(world, worldTable);
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

  // --- Collision event dispatch ---
  // Build current-frame pair map (canonical key → full entity pair).
  std::unordered_map<uint64_t, std::pair<ECS::Entity, ECS::Entity>> currPairs;
  currPairs.reserve(collisions.size());
  for (const auto& p : collisions)
    currPairs.emplace(collisionKey(p.first, p.second), p);

  // onTriggerEnter / onCollisionEnter: fire for pairs that are NEW this frame.
  for (const auto& [key, pair] : currPairs) {
    if (m_prevCollisionPairs.count(key)) continue;
    auto dispatchEnter = [&](ECS::Entity self, ECS::Entity other) {
      auto it = m_states.find(self.id);
      if (it == m_states.end()) return;
      auto* luaScript = world.getComponent<Components::LuaScript>(self);
      for (size_t stateIndex = 0; stateIndex < it->second.size(); ++stateIndex) {
        auto& state = it->second[stateIndex];
        if (!state.started) continue;
        Scripting::EntityRef selfRef(self, &world);
        Scripting::EntityRef otherRef(other, &world);
        if (state.onTriggerEnter.valid()) {
          try { state.onTriggerEnter(state.scriptTable, selfRef, otherRef); }
          catch (const sol::error& e) {
            std::cerr << "[ScriptSystem] onTriggerEnter error (entity "
                      << self.id << "): " << e.what() << std::endl;
          }
        }
        if (state.onCollisionEnter.valid()) {
          try { state.onCollisionEnter(state.scriptTable, selfRef, otherRef); }
          catch (const sol::error& e) {
            std::cerr << "[ScriptSystem] onCollisionEnter error (entity "
                      << self.id << "): " << e.what() << std::endl;
          }
        }
        if (luaScript && stateIndex < luaScript->scripts.size()) {
          for (auto& [propKey, propValue] : luaScript->scripts[stateIndex].properties)
            pullPropFromLua(state.scriptTable, propKey, propValue);
        }
      }
    };
    dispatchEnter(pair.first, pair.second);
    dispatchEnter(pair.second, pair.first);
  }

  // onTriggerExit: fire for pairs that existed last frame but are gone this frame.
  for (const auto& [key, pair] : m_prevCollisionPairs) {
    if (currPairs.count(key)) continue;
    auto dispatchExit = [&](ECS::Entity self, ECS::Entity other) {
      auto it = m_states.find(self.id);
      if (it == m_states.end()) return;
      auto* luaScript = world.getComponent<Components::LuaScript>(self);
      for (size_t stateIndex = 0; stateIndex < it->second.size(); ++stateIndex) {
        auto& state = it->second[stateIndex];
        if (!state.started) continue;
        Scripting::EntityRef selfRef(self, &world);
        Scripting::EntityRef otherRef(other, &world);
        if (state.onTriggerExit.valid()) {
          try { state.onTriggerExit(state.scriptTable, selfRef, otherRef); }
          catch (const sol::error& e) {
            std::cerr << "[ScriptSystem] onTriggerExit error (entity "
                      << self.id << "): " << e.what() << std::endl;
          }
        }
        if (state.onCollisionExit.valid()) {
          try { state.onCollisionExit(state.scriptTable, selfRef, otherRef); }
          catch (const sol::error& e) {
            std::cerr << "[ScriptSystem] onCollisionExit error (entity "
                      << self.id << "): " << e.what() << std::endl;
          }
        }
        if (luaScript && stateIndex < luaScript->scripts.size()) {
          for (auto& [propKey, propValue] : luaScript->scripts[stateIndex].properties)
            pullPropFromLua(state.scriptTable, propKey, propValue);
        }
      }
    };
    dispatchExit(pair.first, pair.second);
    dispatchExit(pair.second, pair.first);
  }

  m_prevCollisionPairs = std::move(currPairs);
}

void ScriptSystem::clear() {
  m_states.clear();
  m_prevCollisionPairs.clear();
  m_worldTableExtensionFn = nullptr;
  m_loadLevelFn = nullptr;
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
  state.onStart          = scriptTable["onStart"];
  state.onUpdate         = scriptTable["onUpdate"];
  state.onTriggerEnter   = scriptTable["onTriggerEnter"];
  state.onTriggerExit    = scriptTable["onTriggerExit"];
  state.onCollisionEnter = scriptTable["onCollisionEnter"];
  state.onCollisionExit  = scriptTable["onCollisionExit"];
  state.started = false;
  state.loadedPath = scriptPath;

  std::cout << "[ScriptSystem] Loaded " << fullPath << " for entity "
            << entity.id << std::endl;
  return true;
}

} // namespace Systems
