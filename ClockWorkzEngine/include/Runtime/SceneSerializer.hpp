#pragma once
#include "ECS/World.hpp"
#include <string>

namespace Runtime {

class SceneSerializer {
public:
  static void Serialize(ECS::World &world, const std::string &filepath);
  static bool Deserialize(ECS::World &world, const std::string &filepath);
};

} // namespace Runtime
