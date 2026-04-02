/******************************************************************************/
/*!
\file           SceneSerializer.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon] 47% [Syafiq] 24% [Sol-Amzar] 26% [Yusuf] 3%
\brief          Implements helpers for serializing and deserializing scenes.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/


#include "Runtime/SceneSerializer.hpp"
#include "Runtime/Scene.hpp"

// Engine components only
#include "Components/AudioSource.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/Collider.hpp"
#include "Components/Light.hpp"
#include "Components/LuaScript.hpp"
#include "Components/ParticleEmitter.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Components/Velocity.hpp"
#include "Scripting/ScriptPropertyReader.hpp"


#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Runtime {

// Static extension callbacks (null by default)
SceneSerializer::SerializeExtFn SceneSerializer::s_serializeExt;
SceneSerializer::DeserializeExtFn SceneSerializer::s_deserializeExt;

void SceneSerializer::setExtensions(SerializeExtFn ser,
                                    DeserializeExtFn deser) {
  s_serializeExt = std::move(ser);
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
    if (auto *textComp = world.getComponent<Components::TextRenderer>(entity)) {
      obj["components"]["textRenderer"] = {
          {"text", textComp->text},
          {"fontId", textComp->fontId},
          {"fontSize", textComp->fontSize},
          {"color",
           {{"r", textComp->color.r},
            {"g", textComp->color.g},
            {"b", textComp->color.b}}},
          {"visible", textComp->visible},
          {"boxSize", {{"x", textComp->boxSize.x}, {"y", textComp->boxSize.y}}},
          {"alignment", static_cast<int>(textComp->alignment)},
          {"verticalAlignment", static_cast<int>(textComp->verticalAlign)},
          {"wordWrap", textComp->wordWrap},
          {"lineSpacing", textComp->lineSpacing},
          {"padding",
           {{"x", textComp->padding.x}, {"y", textComp->padding.y}}}};
    }

    // LuaScript ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â stored as a JSON array of script
    // entries
    if (auto *ls = world.getComponent<Components::LuaScript>(entity)) {
      json arr = json::array();
      for (auto &entry : ls->scripts) {
        json lsj;
        lsj["scriptPath"] = entry.scriptPath;
        lsj["enabled"] = entry.enabled;
        if (!entry.properties.empty()) {
          json propsJ = json::object();
          for (auto &[key, val] : entry.properties)
            std::visit([&](auto &&v) { propsJ[key] = v; }, val);
          lsj["properties"] = propsJ;
        }
        arr.push_back(lsj);
      }
      obj["components"]["luaScript"] = arr;
    }

    // Light
    if (auto *lt = world.getComponent<Components::Light>(entity)) {
      obj["components"]["light"] = {
          {"active", lt->active},
          {"offset", {{"x", lt->offset.x}, {"y", lt->offset.y}}},
          {"color",
           {{"r", lt->color.r}, {"g", lt->color.g}, {"b", lt->color.b}}},
          {"radius", lt->radius},
          {"angle", lt->angle},
          {"intensity", lt->intensity}};
    }

    if (auto *pe = world.getComponent<Components::ParticleEmitter>(entity)) {
      const auto &sys = pe->system();
      obj["components"]["particleEmitter"] = {
          {"active", pe->active()},
          {"rate", sys.rate},
          {"lifeMin", sys.lifeMin},
          {"lifeMax", sys.lifeMax},
          {"speedMin", sys.speedMin},
          {"speedMax", sys.speedMax},
          {"sizeStart", sys.sizeStart},
          {"sizeEnd", sys.sizeEnd},
          {"spreadDeg", sys.spreadDeg},
          {"spawnRadius", sys.spawnRadius},
          {"looping", sys.looping},
          {"worldSpace", sys.worldSpace},
          {"direction", {{"x", sys.dir.x}, {"y", sys.dir.y}}},
          {"acceleration",
           {{"x", sys.acceleration.x}, {"y", sys.acceleration.y}}},
          {"localOffset",
           {{"x", pe->localOffset.x}, {"y", pe->localOffset.y}}},
          {"colorStart",
           {{"r", sys.colorStart.r},
            {"g", sys.colorStart.g},
            {"b", sys.colorStart.b},
            {"a", sys.colorStart.a}}},
          {"colorEnd",
           {{"r", sys.colorEnd.r},
            {"g", sys.colorEnd.g},
            {"b", sys.colorEnd.b},
            {"a", sys.colorEnd.a}}}};
    }

    // UIButton
    if (auto *ub = world.getComponent<Components::UIButton>(entity)) {
      obj["components"]["uiButton"] = {
          {"action", ub->action},
          {"uiScript", ub->uiScript},
          {"interactable", ub->interactable},
          {"showBackground", ub->showBackground},
          {"size", {{"x", ub->size.x}, {"y", ub->size.y}}},
          {"offset", {{"x", ub->offset.x}, {"y", ub->offset.y}}},
          {"normalColor",
           {{"r", ub->normalColor.r},
            {"g", ub->normalColor.g},
            {"b", ub->normalColor.b}}},
          {"hoverColor",
           {{"r", ub->hoverColor.r},
            {"g", ub->hoverColor.g},
            {"b", ub->hoverColor.b}}}};
    }

    // UISlider
    if (auto *us = world.getComponent<Components::UISlider>(entity)) {
      obj["components"]["uiSlider"] = {{"settingKey", us->settingKey},
                                       {"minValue", us->minValue},
                                       {"maxValue", us->maxValue},
                                       {"value", us->value},
                                       {"trackWidth", us->trackWidth},
                                       {"trackHeight", us->trackHeight},
                                       {"handleSize", us->handleSize},
                                       {"trackColor",
                                        {{"r", us->trackColor.r},
                                         {"g", us->trackColor.g},
                                         {"b", us->trackColor.b},
                                         {"a", us->trackColor.a}}},
                                       {"fillColor",
                                        {{"r", us->fillColor.r},
                                         {"g", us->fillColor.g},
                                         {"b", us->fillColor.b},
                                         {"a", us->fillColor.a}}},
                                       {"handleColor",
                                        {{"r", us->handleColor.r},
                                         {"g", us->handleColor.g},
                                         {"b", us->handleColor.b},
                                         {"a", us->handleColor.a}}}};
    }

    // CameraComponent
    if (auto *cc = world.getComponent<Components::CameraComponent>(entity)) {
      obj["components"]["cameraComponent"] = {
          {"isMain", cc->isMain},
          {"zoom", cc->zoom},
          {"offset", {{"x", cc->offset.x}, {"y", cc->offset.y}}}};
    }

    // Game-specific components (registered via SceneSerializer::setExtensions)
    if (serExt)
      serExt(entity, world, &obj["components"]);

    root["objects"].push_back(obj);
  });
  return root;
}

void SceneSerializer::Serialize(Scene &scene, const std::string &filepath) {
  // Use helper to get world objects
  json root = SerializeWorld(scene.GetWorld(), s_serializeExt);

  // Append Scene properties
  root["bgm"] = scene.getBGM();
  root["musicContext"] = scene.getMusicContext();
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
        auto &textRendererJson = components["textRenderer"];
        Components::TextRenderer textComp;
        textComp.text = textRendererJson.value("text", "");
        textComp.fontId = textRendererJson.value("fontId", "Debug");
        textComp.fontSize = textRendererJson.value("fontSize", 1.0f);
        if (textRendererJson.contains("color")) {
          textComp.color.r = textRendererJson["color"].value("r", 1.0f);
          textComp.color.g = textRendererJson["color"].value("g", 1.0f);
          textComp.color.b = textRendererJson["color"].value("b", 1.0f);
        }
        textComp.visible = textRendererJson.value("visible", true);
        if (textRendererJson.contains("boxSize")) {
          textComp.boxSize.x = textRendererJson["boxSize"].value("x", 200.0f);
          textComp.boxSize.y = textRendererJson["boxSize"].value("y", 100.0f);
        }
        textComp.alignment = static_cast<Components::TextAlignment>(
            textRendererJson.value("alignment", 0));
        textComp.verticalAlign = static_cast<Components::VerticalAlignment>(
            textRendererJson.value("verticalAlignment", 0));
        textComp.wordWrap = textRendererJson.value("wordWrap", true);
        textComp.lineSpacing = textRendererJson.value("lineSpacing", 1.0f);
        if (textRendererJson.contains("padding")) {
          textComp.padding.x = textRendererJson["padding"].value("x", 0.0f);
          textComp.padding.y = textRendererJson["padding"].value("y", 0.0f);
        }
        world.addComponent(entity, textComp);
      }

      // LuaScript ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â supports new array format and old
      // single-object format
      if (components.contains("luaScript")) {
        auto &lsData = components["luaScript"];
        Components::LuaScript ls;

        auto deserializeEntry = [](const json &lsj) {
          Components::LuaScriptEntry entry;
          entry.scriptPath = lsj.value("scriptPath", "");
          entry.enabled = lsj.value("enabled", true);
          if (lsj.contains("properties") && !entry.scriptPath.empty()) {
            auto& propDefs = Scripting::ScriptPropertyReader::read(entry.scriptPath);
            for (auto& def : propDefs) {
              if (!lsj["properties"].contains(def.name)) continue;
              auto& jv = lsj["properties"][def.name];
              if      (def.type == "float")  entry.properties[def.name] = jv.get<float>();
              else if (def.type == "int")    entry.properties[def.name] = jv.get<int>();
              else if (def.type == "bool")   entry.properties[def.name] = jv.get<bool>();
              else if (def.type == "string"   ||
                       def.type == "texture"  ||
                       def.type == "animation") entry.properties[def.name] = jv.get<std::string>();
            }
          }
          return entry;
        };

        if (lsData.is_array()) {
          for (auto &item : lsData)
            ls.scripts.push_back(deserializeEntry(item));
        } else {
          ls.scripts.push_back(deserializeEntry(lsData)); // backward compat
        }
        world.addComponent(entity, ls);
      }

      // Light
      if (components.contains("light")) {
        auto &lc = components["light"];
        Components::Light lt;
        lt.active = lc.value("active", true);
        if (lc.contains("offset")) {
          lt.offset.x = lc["offset"].value("x", 35.f);
          lt.offset.y = lc["offset"].value("y", -20.f);
        }
        if (lc.contains("color")) {
          lt.color.r = lc["color"].value("r", 1.f);
          lt.color.g = lc["color"].value("g", 0.9f);
          lt.color.b = lc["color"].value("b", 0.6f);
        }
        lt.radius = lc.value("radius", 300.f);
        lt.angle = lc.value("angle", 60.f);
        lt.intensity = lc.value("intensity", 1.f);
        world.addComponent(entity, lt);
      }

      if (components.contains("particleEmitter")) {
        auto &pec = components["particleEmitter"];
        Components::ParticleEmitter pe;
        pe.setActive(pec.value("active", true));
        pe.localOffset.x =
            pec.contains("localOffset") ? pec["localOffset"].value("x", 0.f)
                                        : 0.f;
        pe.localOffset.y =
            pec.contains("localOffset") ? pec["localOffset"].value("y", 0.f)
                                        : 0.f;

        auto &sys = pe.system();
        sys.rate = pec.value("rate", sys.rate);
        sys.lifeMin = pec.value("lifeMin", sys.lifeMin);
        sys.lifeMax = pec.value("lifeMax", sys.lifeMax);
        sys.speedMin = pec.value("speedMin", sys.speedMin);
        sys.speedMax = pec.value("speedMax", sys.speedMax);
        sys.sizeStart = pec.value("sizeStart", sys.sizeStart);
        sys.sizeEnd = pec.value("sizeEnd", sys.sizeEnd);
        sys.spreadDeg = pec.value("spreadDeg", sys.spreadDeg);
        sys.spawnRadius = pec.value("spawnRadius", sys.spawnRadius);
        sys.looping = pec.value("looping", sys.looping);
        sys.worldSpace = pec.value("worldSpace", sys.worldSpace);
        if (pec.contains("direction")) {
          sys.dir.x = pec["direction"].value("x", sys.dir.x);
          sys.dir.y = pec["direction"].value("y", sys.dir.y);
        }
        if (pec.contains("acceleration")) {
          sys.acceleration.x =
              pec["acceleration"].value("x", sys.acceleration.x);
          sys.acceleration.y =
              pec["acceleration"].value("y", sys.acceleration.y);
        }
        if (pec.contains("colorStart")) {
          sys.colorStart.r = pec["colorStart"].value("r", sys.colorStart.r);
          sys.colorStart.g = pec["colorStart"].value("g", sys.colorStart.g);
          sys.colorStart.b = pec["colorStart"].value("b", sys.colorStart.b);
          sys.colorStart.a = pec["colorStart"].value("a", sys.colorStart.a);
        }
        if (pec.contains("colorEnd")) {
          sys.colorEnd.r = pec["colorEnd"].value("r", sys.colorEnd.r);
          sys.colorEnd.g = pec["colorEnd"].value("g", sys.colorEnd.g);
          sys.colorEnd.b = pec["colorEnd"].value("b", sys.colorEnd.b);
          sys.colorEnd.a = pec["colorEnd"].value("a", sys.colorEnd.a);
        }
        world.addComponent(entity, pe);
      }

      // UIButton
      if (components.contains("uiButton")) {
        auto &ub = components["uiButton"];
        Components::UIButton btn;
        btn.action = ub.value("action", "");
        btn.uiScript = ub.value("uiScript", "");
        btn.interactable = ub.value("interactable", true);
        btn.showBackground = ub.value("showBackground", true);
        if (ub.contains("size")) {
          btn.size = {ub["size"].value("x", 0.f), ub["size"].value("y", 0.f)};
        }
        if (ub.contains("offset")) {
          btn.offset = {ub["offset"].value("x", 0.f),
                        ub["offset"].value("y", 0.f)};
        }
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

      // UISlider
      if (components.contains("uiSlider")) {
        auto &us = components["uiSlider"];
        Components::UISlider sl;
        sl.settingKey = us.value("settingKey", "");
        sl.minValue = us.value("minValue", 0.f);
        sl.maxValue = us.value("maxValue", 1.f);
        sl.value = us.value("value", 1.f);
        sl.trackWidth = us.value("trackWidth", 400.f);
        sl.trackHeight = us.value("trackHeight", 18.f);
        sl.handleSize = us.value("handleSize", 22.f);
        if (us.contains("trackColor")) {
          sl.trackColor = {us["trackColor"].value("r", 0.25f),
                           us["trackColor"].value("g", 0.25f),
                           us["trackColor"].value("b", 0.25f),
                           us["trackColor"].value("a", 1.f)};
        }
        if (us.contains("fillColor")) {
          sl.fillColor = {us["fillColor"].value("r", 0.85f),
                          us["fillColor"].value("g", 0.55f),
                          us["fillColor"].value("b", 0.10f),
                          us["fillColor"].value("a", 1.f)};
        }
        if (us.contains("handleColor")) {
          sl.handleColor = {us["handleColor"].value("r", 1.f),
                            us["handleColor"].value("g", 1.f),
                            us["handleColor"].value("b", 1.f),
                            us["handleColor"].value("a", 1.f)};
        }
        world.addComponent(entity, sl);
      }

      // CameraComponent
      if (components.contains("cameraComponent")) {
        auto &cc = components["cameraComponent"];
        Components::CameraComponent cam;
        cam.isMain = cc.value("isMain", true);
        cam.zoom = cc.value("zoom", 1.0f);
        if (cc.contains("offset")) {
          cam.offset.x = cc["offset"].value("x", 0.0f);
          cam.offset.y = cc["offset"].value("y", 0.0f);
        }
        world.addComponent(entity, cam);
      }

      // Game-specific components (registered via
      // SceneSerializer::setExtensions)
      if (s_deserializeExt)
        s_deserializeExt(entity, world, &components);
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
  if (root.contains("musicContext")) {
    scene.setMusicContext(root["musicContext"]);
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
