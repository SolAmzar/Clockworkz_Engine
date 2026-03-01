/******************************************************************************/
/*!
\file           Serializer.cpp
\project        Lights Out
\author(s)      [Yusuf], 50% [Lebon], 30%  [Wei An], 20%
\brief          Implements full JSON serialization/deserialization for
GameObjects, their components, and entire scenes, including prefab-based
                construction, transform/collider parsing, and animation/audio
loading.


All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Gameplay\Serializer.hpp"
#include "Gameplay\Collider.hpp"
#include "Gameplay\Components\DamageDealer.hpp"
#include "Gameplay\Components\Health.hpp"
#include "Gameplay\Components\Patrol.hpp"
#include "Gameplay\GameObject.hpp"
#include "Gameplay\Prefab.hpp"
#include "Math/Vec2.hpp"
#include <Gameplay/Components/AudioSource.hpp>
#include <Gameplay/Components/Fragment.hpp>
#include <Scripting/LuaScriptComponent.hpp>

namespace {
// JSON Keys
constexpr const char *KEY_COMPONENTS = "components";
constexpr const char *KEY_PREFAB = "prefab";
constexpr const char *KEY_NAME = "name";
constexpr const char *KEY_ACTIVE = "active";
constexpr const char *KEY_TYPE = "type";

// Component Keys
constexpr const char *KEY_TRANSFORM = "transform";
constexpr const char *KEY_COLLIDER = "collider";
constexpr const char *KEY_SPRITE_RENDERER = "spriteRenderer";
constexpr const char *KEY_HEALTH = "health";
constexpr const char *KEY_DAMAGE = "damage";
constexpr const char *KEY_PATROL = "patrol";
constexpr const char *KEY_SPRITE_ANIMATOR = "spriteAnimator";
constexpr const char *KEY_AUDIO = "audio";
constexpr const char *KEY_FRAGMENT = "fragment";
constexpr const char *KEY_FRAGMENT_COLLECTOR = "fragmentCollector";
constexpr const char *KEY_LUA_SCRIPT = "luaScript";
} // namespace
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <fstream>
#include <iostream>

namespace Gameplay {

// ========================================================================
// COMPONENT SERIALIZATION
// ========================================================================

json GameObjectSerializer::serializeFragment(const GameObject &obj) {
  auto *fragment = obj.getComponent<FragmentComponent>();
  if (!fragment)
    return json();

  json j;
  j["fragmentId"] = fragment->fragmentId();
  j["value"] = fragment->value();
  return j;
}

json GameObjectSerializer::serializeFragmentCollector(const GameObject &obj) {
  auto *collector = obj.getComponent<FragmentCollectorComponent>();
  if (!collector)
    return json();

  json j;
  j["requiredFragments"] = collector->requiredFragments();
  j["collectedCount"] = collector->collectedCount();
  return j;
}

json GameObjectSerializer::serializeTransform(const GameObject &obj) {
  json j;
  j["position"]["x"] = obj.transform().position.x;
  j["position"]["y"] = obj.transform().position.y;
  j["rotation"] = obj.transform().rotationRad;
  j["scale"]["x"] = obj.transform().scale.x;
  j["scale"]["y"] = obj.transform().scale.y;
  return j;
}

json GameObjectSerializer::serializeCollider(const GameObject &obj) {
  json j;

  // Common fields
  j["isSolid"] = obj.isSolid();
  j["layer"] = obj.layer();
  j["collisionMask"] = obj.collisionMask();
  j["moveable"] = obj.ismoveable();
  j["collisionCategory"] = obj.collisionCategory();
  j["blocksLight"] = obj.blocksLight();

  // Shape-specific: ask the colliders directly
  if (auto *circle = obj.getCollider<CircleCollider>()) {
    j[KEY_TYPE] = "Circle";
    j["radius"] = circle->radius();

    j["width"] = obj.size().x; // optional, for editor display
    j["height"] = obj.size().y;
  } else if (auto *aabb = obj.getCollider<AABBCollider>()) {
    j[KEY_TYPE] = "AABB";
    j["width"] = obj.size().x;
    j["height"] = obj.size().y;
  } else {
    j[KEY_TYPE] = "None";
  }

  return j;
}

json GameObjectSerializer::serializeHealth(const GameObject &obj) {
  auto *health = obj.getComponent<HealthComponent>();
  if (!health)
    return json();

  json j;
  j["hp"] = health->hp();
  return j;
}

json GameObjectSerializer::serializePatrol(const GameObject &obj) {
  if (auto *pat = obj.getComponent<PatrolComponent>()) {
    json j;
    j["a"]["x"] = pat->a().x;
    j["a"]["y"] = pat->a().y;
    j["b"]["x"] = pat->b().x;
    j["b"]["y"] = pat->b().y;
    j["speed"] = pat->speed();
    return j;
  }
  return json(); // empty if not present
}

json GameObjectSerializer::serializeDamage(const GameObject &obj) {
  auto *damage = obj.getComponent<DamageDealerComponent>();
  if (!damage)
    return json();

  json j;
  j[KEY_DAMAGE] = damage->damage();
  return j;
}

json GameObjectSerializer::serializeSpriteRenderer(const GameObject &obj) {
  json j;
  // j["textureId"] = obj.textureId();
  if (auto *sr = obj.getComponent<SpriteRendererComponent>()) {
    j["textureId"] =
        obj.textureId(); // or sr->textureAlias if you store it there

    auto uv = sr->uv();
    j["uv"] = {uv.x, uv.y, uv.z, uv.w};
    // (optionally tint too)
  }

  return j;
}

json GameObjectSerializer::serializeSpriteAnimator(const GameObject &obj) {
  if (auto *anim = obj.getComponent<SpriteAnimatorComponent>()) {
    json j;
    j["filename"] = anim->getFilename();
    return j;
  }
  return json();
}

json GameObjectSerializer::serializeAudioSource(const GameObject &obj) {
  json j;
  if (auto *audio = obj.getComponent<AudioSourceComponent>()) {
    j["soundId"] = audio->soundId;
    j["volume"] = audio->volume;
    j["loop"] = audio->loop;
    j["playOnStart"] = audio->playOnStart;
    j["group"] = static_cast<int>(audio->group);
  }
  return j;
}

json GameObjectSerializer::serializeLuaScript(const GameObject &obj) {
  auto *script = obj.getComponent<Scripting::LuaScriptComponent>();
  if (!script)
    return json();

  json j;
  j["scriptPath"] = script->scriptPath();
  j["enabled"] = script->isEnabled();
  return j;
}

// ========================================================================
// COMPONENT DESERIALIZATION
// ========================================================================

void GameObjectSerializer::deserializeFragment(GameObject &obj, const json &j) {
  std::string fragmentId = j.value("fragmentId", "fragment");
  int value = j.value("value", 1);

  obj.addComponent<FragmentComponent>(fragmentId, value);
}

// Deserialize FragmentCollectorComponent
void GameObjectSerializer::deserializeFragmentCollector(GameObject &obj,
                                                        const json &j) {
  int requiredFragments = j.value("requiredFragments", 3);
  int collectedCount = j.value("collectedCount", 0);
  float pickupRadius = j.value("pickupRadius", 80.0f); // Load pickup radius!

  auto *collector =
      obj.addComponent<FragmentCollectorComponent>(requiredFragments);
  collector->setCollectedCount(collectedCount);
  collector->setPickupRadius(pickupRadius);
}

void GameObjectSerializer::deserializeTransform(GameObject &obj,
                                                const json &j) {
  if (j.contains(KEY_PATROL)) {
    obj.transform().position.x = j["position"].value("x", 0.0f);
    obj.transform().position.y = j["position"].value("y", 0.0f);
  }
  if (j.contains("rotation")) {
    obj.transform().rotationRad = j["rotation"].get<float>();
  }
  if (j.contains(KEY_HEALTH)) {
    obj.transform().scale.x = j["scale"].value("x", 1.0f);
    obj.transform().scale.y = j["scale"].value("y", 1.0f);
  }
}

void GameObjectSerializer::deserializeCollider(GameObject &obj, const json &j) {
  if (j.contains(KEY_AUDIO) && j.contains("height")) {
    obj.setSize(vec2(j["width"].get<float>(), j["height"].get<float>()));
  }
  if (j.contains("isSolid")) {
    obj.setSolid(j["isSolid"].get<bool>());
  }
  if (j.contains("layer")) {
    obj.setLayer(j["layer"].get<uint32_t>());
  }
  if (j.contains("collisionMask")) {
    obj.setCollisionMask(j["collisionMask"].get<uint32_t>());
  }
  if (j.contains("collisionCategory")) {
    obj.setCollisionCategory(j["collisionCategory"].get<uint32_t>());
  }
  if (j.contains("blocksLight")) {
    obj.setBlocksLight(j["blocksLight"].get<bool>());
  }
  if (j.contains("layer")) {
    obj.setLayer(j["layer"].get<uint32_t>());
  }
  if (j.contains("type")) {
    std::string s = j[KEY_TYPE].get<std::string>();
    if (s == "AABB") {
      auto *collide = obj.addCollider<AABBCollider>();
      collide->type = ColliderType::AABB;
      if (j.contains("blocksLight")) {
        collide->blocksLight = ((j["blocksLight"].get<bool>()));
      }
      /*        if (obj.name() == "Enemy" || obj.name() == "Enemy1" ||
         obj.name() == "Enemy2") { std::cout << "deserialize enemy" <<
         std::endl;
              }*/

      if (obj.name() == "Wall") {
        std::cout << "deserialize wall" << std::endl;
      }
      if (j.contains("moveable")) {
        obj.setMoveable(j["moveable"].get<bool>());
      }
    } else if (s == "Circle") {
      auto *collide = obj.addCollider<CircleCollider>();
      collide->type = ColliderType::Circle;

      // Default: half of width
      float radius = obj.size().x * 0.5f;

      if (j.contains("radius")) {
        radius = j["radius"].get<float>();
      }

      collide->setRadius(radius);

      if (j.contains("blocksLight")) {
        collide->blocksLight = j["blocksLight"].get<bool>();
      }

      obj.setShape(Gameplay::GameObjectShape::Circle);
      obj.setSize(vec2(radius * 2.0f, radius * 2.0f));
      /*             if (obj.name() == "Enemy" || obj.name() == "Enemy1" ||
         obj.name() == "Enemy2") { std::cout << "deserialize enemy" <<
         std::endl;
                   }

                   if (obj.name() == "Wall") {
                       std::cout << "deserialize wall" << std::endl;
                   }*/
      if (j.contains("moveable")) {
        obj.setMoveable(j["moveable"].get<bool>());
      }
    } else if (s == "None") {
      auto *collide = obj.addCollider<Collider>();
      collide->type = ColliderType::None;
    }
  }
}

void GameObjectSerializer::deserializeHealth(GameObject &obj, const json &j) {
  if (j.contains("hp")) {
    obj.addComponent<HealthComponent>(j["hp"].get<int>());
  }
}

void GameObjectSerializer::deserializeDamage(GameObject &obj, const json &j) {
  if (j.contains(KEY_DAMAGE)) {
    obj.addComponent<DamageDealerComponent>(j[KEY_DAMAGE].get<int>());
  }
}

void GameObjectSerializer::deserializeSpriteRenderer(GameObject &obj,
                                                     const json &j) {
  if (j.contains("textureId")) {
    std::string texId = j["textureId"].get<std::string>();
    obj.setTextureId(texId);

    auto *sr = obj.addComponent<SpriteRendererComponent>(texId);

    if (j.contains("uv") && j["uv"].is_array() && j["uv"].size() == 4) {
      auto arr = j["uv"];
      glm::vec4 uv{arr[0].get<float>(), arr[1].get<float>(),
                   arr[2].get<float>(), arr[3].get<float>()};

      sr->setUV(uv);
    }
  }
}

void GameObjectSerializer::deserializePatrol(GameObject &obj, const json &j) {
  // Expected shape:
  if (!j.is_object())
    return;

  vec2 a{0.f, 0.f};
  vec2 b{0.f, 0.f};
  float speed = 100.f;

  if (j.contains("a")) {
    a.x = j["a"].value("x", 0.f);
    a.y = j["a"].value("y", 0.f);
  }
  if (j.contains("b")) {
    b.x = j["b"].value("x", 0.f);
    b.y = j["b"].value("y", 0.f);
  }
  if (j.contains(KEY_SPRITE_RENDERER)) {
    speed = j["speed"].get<float>();
  }

  obj.addComponent<PatrolComponent>(a, b, speed);
}

void GameObjectSerializer::deserializeSpriteAnimator(
    GameObject &obj, const std::string &jsonFileName) {
  std::string folder = "assets/animations/";
  std::string path = folder + std::string(jsonFileName);
  std::ifstream f(path);
  if (f.is_open()) {
    nlohmann::json j;
    f >> j;
    auto *comp = obj.addComponent<SpriteAnimatorComponent>();
    comp->loadFromJson(j);
    std::cout << "[SpriteState] Reloaded animation from: "
              << std::filesystem::absolute(path) << std::endl;
    if (auto *sr = obj.getComponent<SpriteRendererComponent>())
      comp->setSpriteRenderer(sr);
  }
}

void GameObjectSerializer::deserializeAudioSource(GameObject &obj,
                                                  const json &j) {
  if (!j.is_object())
    return;

  auto *audio = obj.addComponent<AudioSourceComponent>();
  audio->soundId = j.value("soundId", "");
  audio->volume = j.value("volume", 1.0f);
  audio->loop = j.value("loop", false);
  audio->playOnStart = j.value("playOnStart", false);
  audio->group = static_cast<Audio::Group>(
      j.value("group", static_cast<int>(Audio::Group::SFX)));
}

void GameObjectSerializer::deserializeLuaScript(GameObject &obj,
                                                const json &j) {
  if (!j.is_object())
    return;

  std::string scriptPath = j.value("scriptPath", "");
  if (scriptPath.empty()) {
    std::cerr << "[Serializer] LuaScript missing scriptPath for: " << obj.name()
              << "\n";
    return;
  }

  auto *script = obj.addComponent<Scripting::LuaScriptComponent>(scriptPath);
  if (script && j.contains("enabled")) {
    script->setEnabled(j["enabled"].get<bool>());
  }

  std::cout << "[Serializer] Attached Lua script '" << scriptPath << "' to "
            << obj.name() << "\n";
}

// ========================================================================
// GAMEOBJECT SERIALIZATION
// ========================================================================

json GameObjectSerializer::serializeGameObject(const GameObject &obj) {
  json j;

  // Basic properties
  j[KEY_NAME] = obj.name();
  j[KEY_ACTIVE] = obj.active();
  j[KEY_TYPE] = static_cast<int>(obj.type());

  // Save prefab name if the object was created from a prefab
  if (!obj.prefabName().empty()) {
    j[KEY_PREFAB] = obj.prefabName();
  }

  // Components object
  json components;

  // Always include transform
  components[KEY_TRANSFORM] = serializeTransform(obj);

  // Optional collider (only if size is not default)
  if (obj.size().x != 1.0f || obj.size().y != 1.0f || obj.isSolid()) {
    components[KEY_COLLIDER] = serializeCollider(obj);
  }

  // Optional health component
  if (obj.getComponent<HealthComponent>()) {
    components[KEY_HEALTH] = serializeHealth(obj);
  }

  // Optional damage component
  if (obj.getComponent<DamageDealerComponent>()) {
    components[KEY_DAMAGE] = serializeDamage(obj);
  }

  // Optional sprite renderer
  if (!obj.textureId().empty()) {
    components[KEY_SPRITE_RENDERER] = serializeSpriteRenderer(obj);
  }
  // Optional patrol component
  if (obj.getComponent<PatrolComponent>()) {
    components[KEY_PATROL] = serializePatrol(obj);
  }

  if (obj.getComponent<SpriteAnimatorComponent>()) {
    components[KEY_SPRITE_ANIMATOR] = serializeSpriteAnimator(obj);
  }

  if (obj.getComponent<AudioSourceComponent>()) {
    components[KEY_AUDIO] = serializeAudioSource(obj);
  }

  if (obj.getComponent<Scripting::LuaScriptComponent>()) {
    components[KEY_LUA_SCRIPT] = serializeLuaScript(obj);
  }

  if (auto *fragment = obj.getComponent<FragmentComponent>()) {
    j[KEY_COMPONENTS]["fragment"] = serializeFragment(obj);
  }

  if (auto *collector = obj.getComponent<FragmentCollectorComponent>()) {
    j[KEY_COMPONENTS]["fragmentCollector"] = serializeFragmentCollector(obj);
  }

  j[KEY_COMPONENTS] = components;

  return j;
}

// ========================================================================
// GAMEOBJECT DESERIALIZATION (WITH PREFAB SUPPORT)
// ========================================================================

std::unique_ptr<GameObject>
GameObjectSerializer::deserializePrefabObject(const json &j) {
  std::string prefabName = j[KEY_PREFAB].get<std::string>();
  std::cout << "[Serializer] Loading prefab: " << prefabName << "\n";

  // Extract some basic overrides early (position, scale, texture, hp, dmg)
  vec2 pos{0.f}, scale{100.f, 100.f}, patrola{0.f, 0.f}, patrolb{0.f, 0.f};
  std::string tex = "";
  std::string spriteAnimatorFileName;
  int hp = 3, dmg = 1;
  float speed = 100.f;

  if (j.contains(KEY_COMPONENTS)) {
    const auto &comps = j[KEY_COMPONENTS];

    if (comps.contains(KEY_TRANSFORM)) {
      const auto &t = comps[KEY_TRANSFORM];
      if (t.contains("position"))
        pos = {t["position"].value("x", 0.f), t["position"].value("y", 0.f)};
      if (t.contains("scale"))
        scale = {t["scale"].value("x", 100.f), t["scale"].value("y", 100.f)};
    }

    if (comps.contains(KEY_SPRITE_RENDERER))
      tex = comps[KEY_SPRITE_RENDERER].value("textureId", "");

    if (comps.contains(KEY_SPRITE_ANIMATOR)) {
      spriteAnimatorFileName = comps[KEY_SPRITE_ANIMATOR].value("filename", "");
    }

    if (comps.contains(KEY_HEALTH))
      hp = comps[KEY_HEALTH].value("hp", 3);

    if (comps.contains(KEY_DAMAGE))
      dmg = comps[KEY_DAMAGE].value("damage", 1);

    if (comps.contains(KEY_PATROL)) {
      const auto &p = comps[KEY_PATROL];
      if (p.contains("a")) {
        patrola.x = p["a"].value("x", 0.f);
        patrola.y = p["a"].value("y", 0.f);
      }
      if (p.contains("b")) {
        patrolb.x = p["b"].value("x", 0.f);
        patrolb.y = p["b"].value("y", 0.f);
      }
      speed = p.value("speed", speed);
    }
  }

  // 2️⃣ Create prefab with correct factory
  auto obj = PrefabFactory::get().createAt(prefabName, pos);

  if (!obj) {
    std::cerr << "[Serializer] Unknown prefab type: " << prefabName << "\n";
    obj = std::make_unique<GameObject>(GameObjectType::Unknown, prefabName);
  }

  // 3️⃣ Apply overrides AFTER prefab creation
  if (j.contains(KEY_COMPONENTS)) {
    const auto &comps = j[KEY_COMPONENTS];

    if (comps.contains(KEY_TRANSFORM))
      GameObjectSerializer::deserializeTransform(*obj, comps[KEY_TRANSFORM]);
    if (comps.contains(KEY_HEALTH))
      GameObjectSerializer::deserializeHealth(*obj, comps[KEY_HEALTH]);
    if (comps.contains(KEY_DAMAGE))
      GameObjectSerializer::deserializeDamage(*obj, comps[KEY_DAMAGE]);
    if (comps.contains(KEY_COLLIDER))
      GameObjectSerializer::deserializeCollider(*obj, comps[KEY_COLLIDER]);
    if (comps.contains(KEY_PATROL))
      GameObjectSerializer::deserializePatrol(*obj, comps[KEY_PATROL]);

    if (comps.contains(KEY_AUDIO))
      GameObjectSerializer::deserializeAudioSource(*obj, comps[KEY_AUDIO]);

    if (comps.contains(KEY_SPRITE_ANIMATOR)) {
      GameObjectSerializer::deserializeSpriteAnimator(
          *obj, comps[KEY_SPRITE_ANIMATOR].value("filename", ""));
    }

    if (comps.contains(KEY_LUA_SCRIPT)) {
      GameObjectSerializer::deserializeLuaScript(*obj, comps[KEY_LUA_SCRIPT]);
    }
  }

  // 4️⃣ Apply generic fields
  if (j.contains(KEY_NAME))
    obj->setName(j[KEY_NAME].get<std::string>());
  if (j.contains(KEY_ACTIVE))
    obj->setActive(j[KEY_ACTIVE].get<bool>());

  obj->autoUpdateCollisionBaseOnScale();
  return obj;
}

std::unique_ptr<GameObject>
GameObjectSerializer::deserializeLegacyObject(const json &j) {
  std::string name = j.value("name", "Unnamed");
  Gameplay::GameObjectType type = Gameplay::GameObjectType::Unknown;
  if (j.contains(KEY_TYPE)) {
    type = static_cast<Gameplay::GameObjectType>(j[KEY_TYPE].get<int>());
  }

  auto obj = std::make_unique<GameObject>(type, name);

  if (j.contains(KEY_ACTIVE))
    obj->setActive(j[KEY_ACTIVE].get<bool>());

  if (j.contains(KEY_COMPONENTS) && j[KEY_COMPONENTS].is_object()) {
    const json &comps = j[KEY_COMPONENTS];
    if (comps.contains(KEY_TRANSFORM))
      GameObjectSerializer::deserializeTransform(*obj, comps[KEY_TRANSFORM]);
    if (comps.contains(KEY_COLLIDER))
      GameObjectSerializer::deserializeCollider(*obj, comps[KEY_COLLIDER]);
    if (comps.contains(KEY_HEALTH))
      GameObjectSerializer::deserializeHealth(*obj, comps[KEY_HEALTH]);
    if (comps.contains(KEY_DAMAGE))
      GameObjectSerializer::deserializeDamage(*obj, comps[KEY_DAMAGE]);
    if (comps.contains(KEY_SPRITE_RENDERER))
      GameObjectSerializer::deserializeSpriteRenderer(
          *obj, comps[KEY_SPRITE_RENDERER]);
    if (comps.contains(KEY_SPRITE_ANIMATOR)) {
      std::string spriteAnimatorFilePath =
          comps[KEY_SPRITE_ANIMATOR].value("filename", "");
      GameObjectSerializer::deserializeSpriteAnimator(*obj,
                                                      spriteAnimatorFilePath);
    }
    if (comps.contains(KEY_AUDIO))
      GameObjectSerializer::deserializeAudioSource(*obj, comps[KEY_AUDIO]);

    if (comps.contains(KEY_FRAGMENT)) {
      GameObjectSerializer::deserializeFragment(*obj, comps[KEY_FRAGMENT]);
    }

    if (comps.contains(KEY_FRAGMENT_COLLECTOR)) {
      GameObjectSerializer::deserializeFragmentCollector(
          *obj, comps[KEY_FRAGMENT_COLLECTOR]);
    }

    if (comps.contains(KEY_LUA_SCRIPT)) {
      GameObjectSerializer::deserializeLuaScript(*obj, comps[KEY_LUA_SCRIPT]);
    }
  }
  return obj;
}

std::unique_ptr<GameObject>
GameObjectSerializer::deserializeGameObject(const json &j) {
  std::unique_ptr<GameObject> obj;

  if (j.contains(KEY_PREFAB)) {
    obj = deserializePrefabObject(j);
  } else {
    obj = deserializeLegacyObject(j);
  }

  // Hardcoded logic (Commented out)
  /*
  using GT = Gameplay::GameObjectType;
  switch (obj->type())
  {
  case GT::Wall:
      obj->setMoveable(false);
      break; // <-- walls are static
  case GT::Floor:
      obj->setMoveable(false);
      break; // optional: floors static too
  case GT::Trap:
      obj->setMoveable(false);
      break; // traps as triggers (no push)
  case GT::Box:
      obj->setMoveable(true);
      break; // boxes are movable
  case GT::Player:
      obj->setMoveable(true);
      break;
  case GT::Enemy:
      obj->setMoveable(true);
      break;
  case GT::Ball:
      obj->setMoveable(false);
      break; // dynamic bodies move
  default:
      obj->setMoveable(true);
  }
  */

  return obj;
}

// ========================================================================
// SCENE SERIALIZATION
// ========================================================================

json GameObjectSerializer::serializeScene(
    const std::string &sceneName,
    const std::vector<std::unique_ptr<GameObject>> &objects) {
  json j;
  j["sceneName"] = sceneName;

  json objectsArray = json::array();
  for (const auto &obj : objects) {
    if (obj) {
      objectsArray.push_back(serializeGameObject(*obj));
    }
  }
  j["objects"] = objectsArray;

  return j;
}

bool GameObjectSerializer::saveSceneToFile(
    const std::string &sceneName,
    const std::vector<std::unique_ptr<GameObject>> &objects,
    const std::string &filepath) {
  try {
    json sceneJson = serializeScene(sceneName, objects);

    std::ofstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[Serializer] Failed to open file for writing: " << filepath
                << "\n";
      return false;
    }

    // Write with pretty formatting (4 spaces indent)
    file << sceneJson.dump(4);
    file.close();

    std::cout << "[Serializer] Scene '" << sceneName
              << "' saved to: " << filepath << "\n";
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Serializer] Error saving scene: " << e.what() << "\n";
    return false;
  }
}

bool GameObjectSerializer::loadSceneFromFile(
    const std::string &filepath, std::string &outSceneName,
    std::vector<std::unique_ptr<GameObject>> &outObjects) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[Serializer] Failed to open file for reading: " << filepath
                << "\n";
      return false;
    }

    json j;
    file >> j;
    file.close();

    // Parse scene name
    outSceneName = j.value("sceneName", "Unnamed Scene");

    // Parse objects
    outObjects.clear();
    if (j.contains("objects") && j["objects"].is_array()) {
      for (const auto &objJson : j["objects"]) {
        auto obj = deserializeGameObject(objJson);
        if (obj) {
          outObjects.push_back(std::move(obj));
        }
      }
    }

    std::cout << "[Serializer] Scene '" << outSceneName
              << "' loaded from: " << filepath << " (" << outObjects.size()
              << " objects)\n";
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Serializer] Error loading scene: " << e.what() << "\n";
    return false;
  }
}

} // namespace Gameplay