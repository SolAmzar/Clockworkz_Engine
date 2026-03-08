/*!
\file           SceneSerializer.hpp
\project        Lights Out
\author(s)      [Lebon], 80%, [Amzar] 19% , [Yusuf] 1%
\brief          Loads and saves entire scenes to/from JSON files.

                                Takes an ECS World and serializes all entities
with their components

All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/SceneSerializer.hpp"
#include "Runtime/Scene.hpp"

// Engine components only
#include "Components/AudioSource.hpp"
#include "Components/Collider.hpp"
#include "Components/Light.hpp"
#include "Components/LuaScript.hpp"
#include "Scripting/ScriptPropertyReader.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/Velocity.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Runtime {

// Static extension callbacks (null by default)
SceneSerializer::SerializeExtFn   SceneSerializer::s_serializeExt;
SceneSerializer::DeserializeExtFn SceneSerializer::s_deserializeExt;

void SceneSerializer::setExtensions(SerializeExtFn ser, DeserializeExtFn deser) {
  s_serializeExt   = std::move(ser);
  s_deserializeExt = std::move(deser);
}

// Helper to serialize world to JSON object
static json SerializeWorld(ECS::World &world,
                           const SceneSerializer::SerializeExtFn &serExt) {
  json root;
  root["objects"] = json::array();

  // Iterate over all entities with Transform
  world.each<Components::Transform>([&](ECS::Entity entity,
                                        Components::Transform &tc) {
    json obj;
    obj["active"] = true; // Assuming all serialized entities are active for now

    std::string name = "Entity_" + std::to_string(entity.id);

    if (auto *tag = world.getComponent<Components::Tag>(entity)) {
      name = tag->tag;
    }
    obj["name"] = name;
    obj["components"] = json::object();

    // Transform
    obj["components"]["transform"] = {
        {"position", {{"x", tc.position.x}, {"y", tc.position.y}}},
        {"rotation", tc.rotation},
        {"scale", {{"x", tc.scale.x}, {"y", tc.scale.y}}},
        {"pivot", {{"x", tc.pivot.x}, {"y", tc.pivot.y}}},
        {"snapToGrid", tc.snapToGrid}};

    // Velocity
    if (auto *vel = world.getComponent<Components::Velocity>(entity)) {
      obj["components"]["velocity"] = {{"x", vel->velocity.x},
                                       {"y", vel->velocity.y},
                                       {"moveSpeed", vel->moveSpeed}};
    }

    // SpriteRenderer
    if (auto *sc = world.getComponent<Components::SpriteRenderer>(entity)) {
      obj["components"]["spriteRenderer"] = {
          {"textureId", sc->textureId},
          {"uv", {sc->uv.x, sc->uv.y, sc->uv.z, sc->uv.w}},
          {"size", {{"x", sc->size.x}, {"y", sc->size.y}}},
          {"color",
           {{"r", sc->color.r},
            {"g", sc->color.g},
            {"b", sc->color.b},
            {"a", sc->color.a}}},
          {"visible", sc->visible},
          {"visible", sc->visible},
          {"unlit", sc->unlit},
          {"sortOrder", sc->sortOrder}};
    }

    // SpriteAnimator
    if (auto *sac = world.getComponent<Components::SpriteAnimator>(entity)) {
      if (!sac->filename.empty()) {
        std::string fname = sac->filename;
        std::string prefix = "assets/animations/";
        size_t pos = fname.find(prefix);
        if (pos != std::string::npos) {
          fname = fname.substr(pos + prefix.length());
        }
        obj["components"]["spriteAnimator"] = {{"filename", fname}};
      }
    }

    // AudioSource
    if (auto *ac = world.getComponent<Components::AudioSource>(entity)) {
      obj["components"]["audioSource"] = {
          {"soundId", ac->soundId}, {"volume", ac->volume}, {"loop", ac->loop}};
    }

    // StateMachine
    if (auto *sm = world.getComponent<Components::StateMachine>(entity)) {
      obj["components"]["stateMachine"] = {{"currentState", sm->currentState}};
    }

    // Collider
    if (auto *cc = world.getComponent<Components::Collider>(entity)) {
      obj["components"]["collider"] = {
          {"width", cc->size.x},
          {"height", cc->size.y},
          {"offset", {{"x", cc->offset.x}, {"y", cc->offset.y}}},
          {"shape", (int)cc->shape},
          {"radius", cc->radius},
          {"isSolid", cc->isSolid},
          {"blocksLight", cc->blocksLight},
          {"isHazard", cc->isHazard},
          {"isTrigger", cc->isTrigger},
          {"moveable", cc->moveable},
          {"collisionCategory", cc->collisionCategory},
          {"collisionMask", cc->collisionMask},
          {"mass", cc->mass}};
    }

    // TextRenderer
    if (auto *tr = world.getComponent<Components::TextRenderer>(entity)) {
      obj["components"]["textRenderer"] = {
          {"text", tr->text},
          {"fontId", tr->fontId},
          {"fontSize", tr->fontSize},
          {"color",
           {{"r", tr->color.r}, {"g", tr->color.g}, {"b", tr->color.b}}},
          {"visible", tr->visible},
          {"boxSize", {{"x", tr->boxSize.x}, {"y", tr->boxSize.y}}},
          {"alignment", (int)tr->alignment}};
    }

    // LuaScript — stored as a JSON array of script entries
    if (auto *ls = world.getComponent<Components::LuaScript>(entity)) {
      json arr = json::array();
      for (auto& entry : ls->scripts) {
        json lsj;
        lsj["scriptPath"] = entry.scriptPath;
        lsj["enabled"]    = entry.enabled;
        if (!entry.properties.empty()) {
          json propsJ = json::object();
          for (auto& [key, val] : entry.properties)
            std::visit([&](auto&& v) { propsJ[key] = v; }, val);
          lsj["properties"] = propsJ;
        }
        arr.push_back(lsj);
      }
      obj["components"]["luaScript"] = arr;
    }

    // Light
    if (auto* lt = world.getComponent<Components::Light>(entity)) {
      obj["components"]["light"] = {
          {"active", lt->active},
          {"offset", {{"x", lt->offset.x}, {"y", lt->offset.y}}},
          {"color",  {{"r", lt->color.r},  {"g", lt->color.g},  {"b", lt->color.b}}},
          {"radius", lt->radius}, {"angle", lt->angle}, {"intensity", lt->intensity}};
    }

    // UIButton
    if (auto *ub = world.getComponent<Components::UIButton>(entity)) {
      obj["components"]["uiButton"] = {
          {"action",       ub->action},
          {"interactable", ub->interactable},
          {"normalColor",  {{"r", ub->normalColor.r}, {"g", ub->normalColor.g}, {"b", ub->normalColor.b}}},
          {"hoverColor",   {{"r", ub->hoverColor.r},  {"g", ub->hoverColor.g},  {"b", ub->hoverColor.b}}}
      };
    }

    // Game-specific components (registered via SceneSerializer::setExtensions)
    if (serExt) serExt(entity, world, &obj["components"]);

    root["objects"].push_back(obj);
  });
  return root;
}

void SceneSerializer::Serialize(Scene &scene, const std::string &filepath) {
  // Use helper to get world objects
  json root = SerializeWorld(scene.GetWorld(), s_serializeExt);

  // Append Scene properties
  root["bgm"] = scene.getBGM();
  root["sfxVolume"] = scene.getSFXVolume();
  root["spawnPoint"] = {{"x", scene.getSpawnPoint().x},
                        {"y", scene.getSpawnPoint().y}};
  if (!scene.soundVolumeOverrides().empty()) {
    json vols = json::object();
    for (const auto &kv : scene.soundVolumeOverrides()) {
      vols[kv.first] = kv.second;
    }
    root["soundVolumes"] = vols;
  }

  // Darkness Settings
  root["enableDarkness"] = scene.isDarknessEnabled();
  root["darknessIntensity"] = scene.getDarknessIntensity();
  glm::vec3 dc = scene.getDarknessColor();
  root["darknessColor"] = {{"r", dc.r}, {"g", dc.g}, {"b", dc.b}};

  std::ofstream o(filepath);
  o << std::setw(4) << root << std::endl;
}

void SceneSerializer::Serialize(ECS::World &world,
                                const std::string &filepath) {
  json root = SerializeWorld(world, s_serializeExt);
  std::ofstream fout(filepath);
  fout << root.dump(4);
}

bool SceneSerializer::Deserialize(ECS::World &world,
                                  const std::string &filepath) {
  std::ifstream fin(filepath);
  if (!fin.is_open())
    return false;

  json root;
  fin >> root;

  if (root.contains("objects")) {
    for (auto &obj : root["objects"]) {
      ECS::Entity entity = world.createEntity();

      // Retrieve name if available (can store in a Tag component later)
      std::string name = obj.value("name", "Entity");
      world.addComponent(entity, Components::Tag{name});

      auto components = obj["components"];

      // Transform
      if (components.contains("transform")) {
        auto &tc = components["transform"];
        Components::Transform transform;
        if (tc["position"].is_object()) {
          transform.position.x = tc["position"].value("x", 0.0f);
          transform.position.y = tc["position"].value("y", 0.0f);
        }
        transform.rotation = tc.value("rotation", 0.0f);
        if (tc["scale"].is_object()) {
          transform.scale.x = tc["scale"].value("x", 1.0f);
          transform.scale.y = tc["scale"].value("y", 1.0f);
        }
        if (tc.contains("pivot") && tc["pivot"].is_object()) {
          transform.pivot.x = tc["pivot"].value("x", 0.0f);
          transform.pivot.y = tc["pivot"].value("y", 0.0f);
        }
        transform.snapToGrid = tc.value("snapToGrid", false);
        world.addComponent(entity, transform);
      }

      // Velocity
      if (components.contains("velocity")) {
        auto &vc = components["velocity"];
        Components::Velocity velocity;
        velocity.velocity.x = vc.value("x", 0.0f);
        velocity.velocity.y = vc.value("y", 0.0f);
        velocity.moveSpeed = vc.value("moveSpeed", 100.0f);
        world.addComponent(entity, velocity);
      }

      // SpriteRenderer
      if (components.contains("spriteRenderer")) {
        auto &sc = components["spriteRenderer"];
        Components::SpriteRenderer sprite;
        sprite.textureId = sc.value("textureId", "");
        if (sc.contains("uv")) {
          if (sc["uv"].is_array() && sc["uv"].size() == 4) {
            sprite.uv = {sc["uv"][0], sc["uv"][1], sc["uv"][2], sc["uv"][3]};
          }
        }
        if (sc.contains("size")) {
          sprite.size.x = sc["size"].value("x", 100.0f);
          sprite.size.y = sc["size"].value("y", 100.0f);
        }
        if (sc.contains("color")) {
          sprite.color.r = sc["color"].value("r", 1.0f);
          sprite.color.g = sc["color"].value("g", 1.0f);
          sprite.color.b = sc["color"].value("b", 1.0f);
          sprite.color.a = sc["color"].value("a", 1.0f);
        }
        sprite.visible = sc.value("visible", true);
        sprite.visible = sc.value("visible", true);
        sprite.unlit = sc.value("unlit", false);
        sprite.sortOrder = sc.value("sortOrder", 0);

        world.addComponent(entity, sprite);
      }

      // SpriteAnimator
      if (components.contains("spriteAnimator")) {
        auto &sac = components["spriteAnimator"];
        std::string filename = sac.value("filename", "");
        if (!filename.empty()) {
          Components::SpriteAnimator anim;
          anim.loadFromPath("assets/animations/" + filename);
          anim.filename = filename;
          world.addComponent(entity, anim);
        }
      }

      // AudioSource
      if (components.contains("audioSource")) {
        auto &ac = components["audioSource"];
        Components::AudioSource audio;
        audio.soundId = ac.value("soundId", "default");
        audio.volume = ac.value("volume", 1.0f);
        audio.loop = ac.value("loop", false);
        world.addComponent(entity, audio);
      }

      // StateMachine
      if (components.contains("stateMachine")) {
        auto &smc = components["stateMachine"];
        Components::StateMachine sm;
        sm.currentState = smc.value("currentState", "idle");
        world.addComponent(entity, sm);
      }

      // Collider
      if (components.contains("collider")) {
        auto &cc = components["collider"];
        Components::Collider collider;
        collider.size.x = cc.value("width", 50.f);
        collider.size.y = cc.value("height", 50.f);

        if (cc.contains("offset")) {
          collider.offset.x = cc["offset"].value("x", 0.0f);
          collider.offset.y = cc["offset"].value("y", 0.0f);
        }

        collider.shape = (Components::Collider::Shape)cc.value("shape", 0);
        collider.radius = cc.value("radius", 0.0f);
        collider.blocksLight = cc.value("blocksLight", false);
        collider.isSolid = cc.value("isSolid", true);
        collider.isHazard = cc.value("isHazard", false);
        collider.isTrigger = cc.value("isTrigger", false);
        collider.moveable = cc.value("moveable", false);
        collider.collisionCategory = cc.value("collisionCategory", 1);
        collider.collisionMask = cc.value("collisionMask", 0xFFFFFFFF);
        collider.mass = cc.value("mass", 1.0f);

        // Handle layer mapping from legacy json if present ("layer": 4)
        // For now, mapping directly to collisionCategory/Mask seems best if
        // JSON already has them.

        world.addComponent(entity, collider);
      }

      // TextRenderer
      if (components.contains("textRenderer")) {
        auto &trc = components["textRenderer"];
        Components::TextRenderer tr;
        tr.text = trc.value("text", "");
        tr.fontId = trc.value("fontId", "Debug");
        tr.fontSize = trc.value("fontSize", 1.0f);
        if (trc.contains("color")) {
          tr.color.r = trc["color"].value("r", 1.0f);
          tr.color.g = trc["color"].value("g", 1.0f);
          tr.color.b = trc["color"].value("b", 1.0f);
        }
        tr.visible = trc.value("visible", true);
        if (trc.contains("boxSize")) {
          tr.boxSize.x = trc["boxSize"].value("x", 200.0f);
          tr.boxSize.y = trc["boxSize"].value("y", 100.0f);
        }
        tr.alignment = (Components::TextAlignment)trc.value("alignment", 0);
        world.addComponent(entity, tr);
      }

      // LuaScript — supports new array format and old single-object format
      if (components.contains("luaScript")) {
        auto& lsData = components["luaScript"];
        Components::LuaScript ls;

        auto deserializeEntry = [](const json& lsj) {
          Components::LuaScriptEntry entry;
          entry.scriptPath = lsj.value("scriptPath", "");
          entry.enabled    = lsj.value("enabled", true);
          if (lsj.contains("properties") && !entry.scriptPath.empty()) {
            auto& propDefs = Scripting::ScriptPropertyReader::read(entry.scriptPath);
            for (auto& def : propDefs) {
              if (!lsj["properties"].contains(def.name)) continue;
              auto& jv = lsj["properties"][def.name];
              if      (def.type == "float")  entry.properties[def.name] = jv.get<float>();
              else if (def.type == "int")    entry.properties[def.name] = jv.get<int>();
              else if (def.type == "bool")   entry.properties[def.name] = jv.get<bool>();
              else if (def.type == "string") entry.properties[def.name] = jv.get<std::string>();
            }
          }
          return entry;
        };

        if (lsData.is_array()) {
          for (auto& item : lsData) ls.scripts.push_back(deserializeEntry(item));
        } else {
          ls.scripts.push_back(deserializeEntry(lsData)); // backward compat
        }
        world.addComponent(entity, ls);
      }

      // Light
      if (components.contains("light")) {
        auto& lc = components["light"];
        Components::Light lt;
        lt.active = lc.value("active", true);
        if (lc.contains("offset")) { lt.offset.x = lc["offset"].value("x", 35.f); lt.offset.y = lc["offset"].value("y", -20.f); }
        if (lc.contains("color"))  { lt.color.r  = lc["color"].value("r", 1.f);   lt.color.g  = lc["color"].value("g", 0.9f);  lt.color.b  = lc["color"].value("b", 0.6f); }
        lt.radius = lc.value("radius", 300.f); lt.angle = lc.value("angle", 60.f);
        lt.intensity = lc.value("intensity", 1.f);
        world.addComponent(entity, lt);
      }

      // UIButton
      if (components.contains("uiButton")) {
        auto &ub = components["uiButton"];
        Components::UIButton btn;
        btn.action      = ub.value("action", "");
        btn.interactable = ub.value("interactable", true);
        if (ub.contains("normalColor")) {
          btn.normalColor = {ub["normalColor"].value("r", 1.f),
                             ub["normalColor"].value("g", 1.f),
                             ub["normalColor"].value("b", 1.f)};
        }
        if (ub.contains("hoverColor")) {
          btn.hoverColor = {ub["hoverColor"].value("r", 1.f),
                            ub["hoverColor"].value("g", 0.8f),
                            ub["hoverColor"].value("b", 0.f)};
        }
        world.addComponent(entity, btn);
      }

      // Game-specific components (registered via SceneSerializer::setExtensions)
      if (s_deserializeExt) s_deserializeExt(entity, world, &components);
    }
  }
  return true;
}

bool SceneSerializer::Deserialize(Scene &scene, const std::string &filepath) {
  std::ifstream i(filepath);
  if (!i.is_open())
    return false;
  json root;
  i >> root;

  if (root.contains("bgm")) {
    scene.setBGM(root["bgm"]);
  }
  if (root.contains("sfxVolume")) {
    scene.setSFXVolume(root["sfxVolume"]);
  }
  if (root.contains("spawnPoint") && root["spawnPoint"].is_object()) {
    vec2 spawn;
    spawn.x = root["spawnPoint"].value("x", 0.0f);
    spawn.y = root["spawnPoint"].value("y", 0.0f);
    std::cout << "[SceneSerializer] Read SpawnPoint from JSON: (" << spawn.x
              << ", " << spawn.y << ")\n";
    scene.setSpawnPoint(spawn);
  } else {
    std::cout << "[SceneSerializer] Warning: 'spawnPoint' not found in JSON.\n";
  }
  if (root.contains("soundVolumes") && root["soundVolumes"].is_object()) {
    for (auto it = root["soundVolumes"].begin();
         it != root["soundVolumes"].end(); ++it) {
      scene.setSoundVolumeOverride(it.key(), it.value().get<float>());
    }
  }

  if (root.contains("enableDarkness")) {
    scene.setDarknessEnabled(root["enableDarkness"]);
  }
  if (root.contains("darknessIntensity")) {
    scene.setDarknessIntensity(root["darknessIntensity"]);
  }
  if (root.contains("darknessColor") && root["darknessColor"].is_object()) {
    glm::vec3 dc;
    dc.r = root["darknessColor"].value("r", 0.7f);
    dc.g = root["darknessColor"].value("g", 0.7f);
    dc.b = root["darknessColor"].value("b", 0.9f);
    scene.setDarknessColor(dc);
  }

  // Populate World as usual
  return Deserialize(scene.GetWorld(), filepath);
}

} // namespace Runtime
