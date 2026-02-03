/*!
\file           SceneSerializer.hpp
\project        Lights Out
\author(s)      [Lebon], 90%, [Amzar] 10%
\brief          Loads and saves entire scenes to/from JSON files.

                                Takes an ECS World and serializes all entities
with their components

All content ? 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/SceneSerializer.hpp"
#include "Components/ActivationZone.hpp"
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/TextRenderer.hpp"

#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Portal.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Runtime {

void SceneSerializer::Serialize(ECS::World &world,
                                const std::string &filepath) {
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
      obj["name"] = name;
      obj["components"] = json::object();

      // Transform
      obj["components"]["transform"] = {
          {"position", {{"x", tc.position.x}, {"y", tc.position.y}}},
          {"rotation", tc.rotation},
          {"scale", {{"x", tc.scale.x}, {"y", tc.scale.y}}},
          {"pivot", {{"x", tc.pivot.x}, {"y", tc.pivot.y}}}};

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
        obj["components"]["audioSource"] = {{"soundId", ac->soundId},
                                            {"volume", ac->volume},
                                            {"loop", ac->loop}};
      }

      // ActivationZone
      if (auto *zone = world.getComponent<Components::ActivationZone>(entity)) {
        obj["components"]["activationZone"] = {
            {"active", zone->active},
            {"actionType", (int)zone->actionType},
            {"payload", zone->payload},
            {"oneShot", zone->oneShot},
            {"triggered", zone->triggered}};
      }

      // EnemyAI
      if (auto *ai = world.getComponent<Components::EnemyAI>(entity)) {
        obj["components"]["enemyAI"] = {
            {"detectionRadius", ai->detectionRadius},
            {"chaseSpeed", ai->chaseSpeed},
            {"patrolSpeed", ai->patrolSpeed}};
      }

      // StateMachine
      if (auto *sm = world.getComponent<Components::StateMachine>(entity)) {
        obj["components"]["stateMachine"] = {
            {"currentState", sm->currentState}};
      }

      // PlayerController
      if (auto *pc = world.getComponent<Components::PlayerController>(entity)) {
        obj["components"]["playerController"] = {
            {"walkMultiplier", pc->walkMultiplier},
            {"sprintMultiplier", pc->sprintMultiplier}};
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

      // Door
      if (auto *door = world.getComponent<Components::Door>(entity)) {
        obj["components"]["door"] = {
            {"requiredFragments", door->requiredFragments},
            {"isOpen", door->isOpen},
            {"openTexture", door->openTexture},
            {"closedTexture", door->closedTexture}};
      }

      // DoorSwing
      if (auto *ds = world.getComponent<Components::DoorSwing>(entity)) {
        obj["components"]["doorSwing"] = {
            {"isOpen", ds->isOpen},
            {"swingAngle", ds->swingAngle},
            {"clockwise", ds->clockwise},
            {"speed", ds->speed},
            {"initialRotation", ds->initialRotation},
            {"initialized", ds->initialized}};
      }

      // Battery
      if (auto *bat = world.getComponent<Components::Battery>(entity)) {
        obj["components"]["battery"] = {{"charge", bat->charge},
                                        {"maxCharge", bat->maxCharge},
                                        {"isCharging", bat->isCharging}};
      }

      // Fragment
      if (auto *fc = world.getComponent<Components::Fragment>(entity)) {
        obj["components"]["fragment"] = {
            {"fragmentId",
             fc->fragmentId}, // Note: JSON uses int, ensure type match
            {"collected", fc->collected}
            // bob info usually not serialized if purely visual/runtime
        };
      }

      // FragmentCollector
      if (auto *fcc =
              world.getComponent<Components::FragmentCollector>(entity)) {
        obj["components"]["fragmentCollector"] = {
            {"requiredFragments", fcc->requiredFragments},
            {"collectedCount", fcc->collectedCount},
            {"pickupRadius", fcc->pickupRadius}};
      }

      // TorchComponent
      if (auto *torch =
              world.getComponent<Components::TorchComponent>(entity)) {
        obj["components"]["torch"] = {
            {"active", torch->active},
            {"offset", {{"x", torch->offset.x}, {"y", torch->offset.y}}},
            {"color",
             {{"r", torch->color.r},
              {"g", torch->color.g},
              {"b", torch->color.b}}},
            {"radius", torch->radius},
            {"angle", torch->angle},
            {"intensity", torch->intensity}};
      }

      // ParticleEmitter
      if (auto *pe = world.getComponent<Components::ParticleEmitter>(entity)) {
        auto &sys = pe->system();
        obj["components"]["particleEmitter"] = {
            {"active", pe->active()},
            {"rate", sys.rate},
            {"lifeMin", sys.lifeMin},
            {"lifeMax", sys.lifeMax},
            {"speedMin", sys.speedMin},
            {"speedMax", sys.speedMax},
            {"sizeStart", sys.sizeStart},
            {"sizeEnd", sys.sizeEnd},
            {"colorStart",
             {{"r", sys.colorStart.r},
              {"g", sys.colorStart.g},
              {"b", sys.colorStart.b},
              {"a", sys.colorStart.a}}},
            {"colorEnd",
             {{"r", sys.colorEnd.r},
              {"g", sys.colorEnd.g},
              {"b", sys.colorEnd.b},
              {"a", sys.colorEnd.a}}},
            {"spreadDeg", sys.spreadDeg},
            {"direction", {{"x", sys.dir.x}, {"y", sys.dir.y}}},
            {"acceleration",
             {{"x", sys.acceleration.x}, {"y", sys.acceleration.y}}},
            {"looping", sys.looping},
            {"worldSpace", sys.worldSpace},
            {"localOffset",
             {{"x", pe->localOffset.x}, {"y", pe->localOffset.y}}}};
      }

      // Health
      if (auto *h = world.getComponent<Components::Health>(entity)) {
        obj["components"]["health"] = {{"hp", h->hp},
                                       {"maxHp", h->maxHp},
                                       {"regenRate", h->regenRate},
                                       {"invulnerable", h->invulnerable},
                                       {"isDead", h->isDead}};
      }

      // LightExposure
      if (auto *le = world.getComponent<Components::LightExposure>(entity)) {
        obj["components"]["lightExposure"] = {
            {"exposureTime", le->exposureTime},
            {"maxExposureTime", le->maxExposureTime},
            {"inLight", le->inLight},
            {"isDangerous", le->isDangerous}};
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

      // Portal
      if (auto *portal = world.getComponent<Components::Portal>(entity)) {
        obj["components"]["portal"] = {{"levelName", portal->levelName}};
      }

      root["objects"].push_back(obj);
    }
  });

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

      // EnemyAI
      if (components.contains("enemyAI")) {
        auto &aic = components["enemyAI"];
        Components::EnemyAI ai;
        ai.detectionRadius = aic.value("detectionRadius", 300.f);
        ai.chaseSpeed = aic.value("chaseSpeed", 100.f);
        ai.patrolSpeed = aic.value("patrolSpeed", 50.f);
        world.addComponent(entity, ai);
      }

      // StateMachine
      if (components.contains("stateMachine")) {
        auto &smc = components["stateMachine"];
        Components::StateMachine sm;
        sm.currentState = smc.value("currentState", "idle");
        world.addComponent(entity, sm);
      }

      // PlayerController
      if (components.contains("playerController")) {
        auto &pcc = components["playerController"];
        Components::PlayerController pc;
        pc.walkMultiplier = pcc.value("walkMultiplier", 1.0f);
        pc.sprintMultiplier = pcc.value("sprintMultiplier", 2.0f);
        world.addComponent(entity, pc);
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

      // Door
      if (components.contains("door")) {
        auto &dc = components["door"];
        Components::Door door;
        door.requiredFragments = dc.value("requiredFragments", 1);
        door.isOpen = dc.value("isOpen", false);
        door.openTexture = dc.value("openTexture", "door_open");
        door.closedTexture = dc.value("closedTexture", "door_close");
        world.addComponent(entity, door);
      }

      // DoorSwing
      if (components.contains("doorSwing")) {
        auto &dsc = components["doorSwing"];
        Components::DoorSwing ds;
        ds.isOpen = dsc.value("isOpen", false);
        ds.swingAngle = dsc.value("swingAngle", 90.f);
        ds.clockwise = dsc.value("clockwise", true);
        ds.speed = dsc.value("speed", 90.f);
        ds.initialRotation = dsc.value("initialRotation", 0.f);
        ds.initialized = dsc.value("initialized", false);
        world.addComponent(entity, ds);
      }

      // Battery
      if (components.contains("battery")) {
        auto &bc = components["battery"];
        Components::Battery battery;
        battery.charge = bc.value("charge", 100.f);
        battery.maxCharge = bc.value("maxCharge", 100.f);
        battery.isCharging = bc.value("isCharging", false);
        world.addComponent(entity, battery);
      }

      // Fragment
      if (components.contains("fragment")) {
        auto &fc = components["fragment"];
        Components::Fragment fragment;
        // In JSON it might be "fragmentId": "fragment_1" (string) or int.
        // Legacy check: JSON says "fragmentId": "fragment_1"
        // Class definition says: int fragmentId
        // WE HAVE A MISMATCH.
        // I will use a simple hash or parsing for now, or change the
        // component to string. Let's check if json value is string or number.
        if (fc["fragmentId"].is_string()) {
          // quick hash or atoi if it contains number?
          // "fragment_1" -> 1?
          // Let's try to extract number.
          std::string fid = fc["fragmentId"];
          if (fid.find("_") != std::string::npos) {
            try {
              fragment.fragmentId = std::stoi(fid.substr(fid.find("_") + 1));
            } catch (...) {
              fragment.fragmentId = 0;
            }
          }
        } else {
          fragment.fragmentId = fc.value("fragmentId", 0);
        }

        fragment.collected = fc.value("collected", false);
        // value property in JSON? "value": 1.
        world.addComponent(entity, fragment);
      }

      // FragmentCollector
      if (components.contains("fragmentCollector")) {
        auto &fcc = components["fragmentCollector"];
        Components::FragmentCollector collector;
        collector.requiredFragments = fcc.value("requiredFragments", 1);
        collector.collectedCount = fcc.value("collectedCount", 0);
        collector.pickupRadius = fcc.value("pickupRadius", 50.f);
        world.addComponent(entity, collector);
      }

      // TorchComponent
      if (components.contains("torch")) {
        auto &tc = components["torch"];
        Components::TorchComponent torch;
        torch.active = tc.value("active", true);

        if (tc.contains("offset") && tc["offset"].is_object()) {
          torch.offset.x = tc["offset"].value("x", 35.0f);
          torch.offset.y = tc["offset"].value("y", -20.0f);
        }

        if (tc.contains("color") && tc["color"].is_object()) {
          torch.color.r = tc["color"].value("r", 1.0f);
          torch.color.g = tc["color"].value("g", 0.9f);
          torch.color.b = tc["color"].value("b", 0.6f);
        }

        torch.radius = tc.value("radius", 300.0f);
        torch.angle = tc.value("angle", 60.0f);
        torch.intensity = tc.value("intensity", 1.0f);

        world.addComponent(entity, torch);
      }

      // ParticleEmitter
      if (components.contains("particleEmitter")) {
        auto &pec = components["particleEmitter"];
        Components::ParticleEmitter pe;
        pe.setActive(pec.value("active", true));
        pe.localOffset.x = pec["localOffset"].value("x", 0.0f);
        pe.localOffset.y = pec["localOffset"].value("y", 0.0f);

        auto &sys = pe.system();
        sys.rate = pec.value("rate", 200.f);
        sys.lifeMin = pec.value("lifeMin", 0.5f);
        sys.lifeMax = pec.value("lifeMax", 1.2f);
        sys.speedMin = pec.value("speedMin", 20.f);
        sys.speedMax = pec.value("speedMax", 120.f);
        sys.sizeStart = pec.value("sizeStart", 2.25f);
        sys.sizeEnd = pec.value("sizeEnd", 0.f);
        sys.spreadDeg = pec.value("spreadDeg", 45.f);
        sys.looping = pec.value("looping", true);
        sys.worldSpace = pec.value("worldSpace", true);

        if (pec.contains("colorStart")) {
          sys.colorStart.r = pec["colorStart"].value("r", 1.f);
          sys.colorStart.g = pec["colorStart"].value("g", 1.f);
          sys.colorStart.b = pec["colorStart"].value("b", 1.f);
          sys.colorStart.a = pec["colorStart"].value("a", 1.f);
        }
        if (pec.contains("colorEnd")) {
          sys.colorEnd.r = pec["colorEnd"].value("r", 1.f);
          sys.colorEnd.g = pec["colorEnd"].value("g", 1.f);
          sys.colorEnd.b = pec["colorEnd"].value("b", 1.f);
          sys.colorEnd.a = pec["colorEnd"].value("a", 0.f);
        }
        if (pec.contains("direction")) {
          sys.dir.x = pec["direction"].value("x", 1.f);
          sys.dir.y = pec["direction"].value("y", 0.f);
        }
        if (pec.contains("acceleration")) {
          sys.acceleration.x = pec["acceleration"].value("x", 0.f);
          sys.acceleration.y = pec["acceleration"].value("y", -200.f);
        }

        world.addComponent(entity, pe);
      }

      // ActivationZone
      if (components.contains("activationZone")) {
        auto &azc = components["activationZone"];
        Components::ActivationZone zone;
        zone.active = azc.value("active", true);
        zone.actionType =
            (Components::ActivationZone::ActionType)azc.value("actionType", 0);
        zone.payload = azc.value("payload", "");
        zone.oneShot = azc.value("oneShot", false);
        zone.triggered = azc.value("triggered", false);
        world.addComponent(entity, zone);
      }

      // Health
      if (components.contains("health")) {
        auto &hc = components["health"];
        Components::Health health;
        health.hp = hc.value("hp", 10.f);
        health.maxHp = hc.value("maxHp", 10.f);
        health.regenRate = hc.value("regenRate", 0.f);
        health.invulnerable = hc.value("invulnerable", false);
        health.isDead = hc.value("isDead", false);
        world.addComponent(entity, health);
      }

      // LightExposure
      if (components.contains("lightExposure")) {
        auto &lec = components["lightExposure"];
        Components::LightExposure le;
        le.exposureTime = lec.value("exposureTime", 0.f);
        le.maxExposureTime = lec.value("maxExposureTime", 10.f);
        le.inLight = lec.value("inLight", false);
        le.isDangerous = lec.value("isDangerous", false);
        world.addComponent(entity, le);
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

      // Portal
      if (components.contains("portal")) {
        auto &pc = components["portal"];
        Components::Portal portal;
        portal.levelName = pc.value("levelName", "");
        world.addComponent(entity, portal);
      }
    }
  }
  return true;
}

} // namespace Runtime
