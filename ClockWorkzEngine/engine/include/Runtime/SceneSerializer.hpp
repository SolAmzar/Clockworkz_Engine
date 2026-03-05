#pragma once
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include <functional>
#include <string>

namespace Runtime {

class SceneSerializer {
public:
  static void Serialize(ECS::World &world, const std::string &filepath);
  static void Serialize(class Scene &scene, const std::string &filepath);
  static bool Deserialize(ECS::World &world, const std::string &filepath);
  static bool Deserialize(class Scene &scene, const std::string &filepath);

  // Extension points so game code can serialize/deserialize its own components
  // without the engine serializer needing to know about them.
  // void* points to nlohmann::json& (serialize) or const nlohmann::json& (deserialize)
  // of the per-entity "components" object.
  using SerializeExtFn   = std::function<void(ECS::Entity, ECS::World&, void*)>;
  using DeserializeExtFn = std::function<void(ECS::Entity, ECS::World&, const void*)>;
  static void setExtensions(SerializeExtFn ser, DeserializeExtFn deser);

private:
  static SerializeExtFn   s_serializeExt;
  static DeserializeExtFn s_deserializeExt;
};

} // namespace Runtime
