/******************************************************************************/
/*!
\file           ActivationSystem.cpp
\project        Lights Out
\author(s)      [Sol-Amzar] 100%
\brief          Implementation of the Scene class.
*/
/******************************************************************************/

#include "Systems/ActivationSystem.hpp"
#include "Assets/SoundManager.hpp"
#include "Components/ActivationZone.hpp"
#include "Components/AudioSource.hpp"
#include "Components/PlayerController.hpp"
#include <iostream>

namespace Systems {

void ActivationSystem::update(ECS::World &world,
                              const CollisionSystem &collisionSystem) {
  const auto &collisions = collisionSystem.getCollisions();

  for (const auto &pair : collisions) {
    ECS::Entity e1 = pair.first;
    ECS::Entity e2 = pair.second;

    auto *zone1 = world.getComponent<Components::ActivationZone>(e1);
    auto *zone2 = world.getComponent<Components::ActivationZone>(e2);

    // Helper to check trigger
    auto check = [&](ECS::Entity zoneEnt, Components::ActivationZone *zone,
                     ECS::Entity otherEnt) {
      if (!zone || !zone->active)
        return;
      if (zone->oneShot && zone->triggered)
        return;

      // Check if other is Player (Presence of PlayerController implies Player)
      if (world.getComponent<Components::PlayerController>(otherEnt)) {
        zone->triggered = true;
        executeAction(world, zoneEnt);
      }
    };

    if (zone1)
      check(e1, zone1, e2);
    if (zone2)
      check(e2, zone2, e1);
  }
}

void ActivationSystem::executeAction(ECS::World &world,
                                     ECS::Entity zoneEntity) {
  auto *zone = world.getComponent<Components::ActivationZone>(zoneEntity);
  if (!zone)
    return;

  switch (zone->actionType) {
  case Components::ActivationZone::ActionType::PlayAudio: {
    std::string soundToPlay = zone->payload;

    // If AudioSource exists, use its soundId if payload is empty OR just prefer
    // it? User requested "Setting a sound in audio source component will set it
    // for the activation system too" So let's fallback to it if payload is
    // empty, or maybe just look for it.
    auto *audioSource = world.getComponent<Components::AudioSource>(zoneEntity);
    if (audioSource && !audioSource->soundId.empty()) {
      soundToPlay = audioSource->soundId;
    }

    if (!soundToPlay.empty()) {
      Assets::SoundManager::Play(soundToPlay);
      std::cout << "[ActivationSystem] Played Sound: " << soundToPlay << "\n";
    }
    break;
  }

  case Components::ActivationZone::ActionType::SpawnEnemy:
    std::cout << "[ActivationSystem] Spawn Request: " << zone->payload
              << " (Not fully implemented)\n";
    break;

  case Components::ActivationZone::ActionType::ChangeVariable:
    std::cout << "[ActivationSystem] Variable Change: " << zone->payload
              << "\n";
    break;

  default:
    break;
  }
  (void)world; // suppress unused warning if only logging
}

} // namespace Systems
