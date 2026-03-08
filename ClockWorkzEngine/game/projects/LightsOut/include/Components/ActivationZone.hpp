#pragma once
#include <string>

namespace Components {

struct ActivationZone {
  enum class ActionType {
    None,
    PlayAudio,
    SpawnEnemy,    // "Spawn Prefab"
    ChangeVariable // "Log Message" or similar for now
  };

  ActionType actionType = ActionType::None;
  std::string payload; // "sfx_jump", "enemy_prefab", "MyVar"
  bool oneShot = false;
  bool triggered = false;
  bool active = true;
};

} // namespace Components
