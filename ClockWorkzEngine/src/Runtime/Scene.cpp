/******************************************************************************/
/*!
\file           Scene.cpp
\project        Lights Out
\author(s)      [Lebon] 100%
\brief          Implementation of the Scene class.
*/
/******************************************************************************/

#include "Runtime/Scene.hpp"

// Components
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Health.hpp"
#include "Components/Layer.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"

#include "Components/ActivationZone.hpp"
#include "Components/LuaScript.hpp"
#include "Components/Portal.hpp"
#include "Components/Velocity.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>

#include "Audio/AudioManager.hpp"
#include "Core/Engine.hpp"

namespace Runtime {

void Scene::setSFXVolume(float volume) {
  m_sfxVolume = volume;
  Core::Engine::Get().audio().setGroupVolume(Audio::Group::SFX, volume);
}

void Scene::setSoundVolumeOverride(const std::string &name, float volume) {
  m_soundVolumeOverrides[name] = volume;
  Core::Engine::Get().audio().setSoundProfileVolume(name, volume);
}

void Scene::removeSoundVolumeOverride(const std::string &name) {
  m_soundVolumeOverrides.erase(name);
}

float Scene::getSoundVolumeOverride(const std::string &name,
                                    float fallback) const {
  auto it = m_soundVolumeOverrides.find(name);
  if (it != m_soundVolumeOverrides.end()) {
    return it->second;
  }
  return fallback;
}

void Scene::Init() {
  // Register all component types
  m_world.registerComponent<Components::Transform>();
  m_world.registerComponent<Components::Velocity>();
  m_world.registerComponent<Components::SpriteRenderer>();
  m_world.registerComponent<Components::Collider>();
  m_world.registerComponent<Components::Health>();
  m_world.registerComponent<Components::Battery>();
  m_world.registerComponent<Components::LightExposure>();
  m_world.registerComponent<Components::Fragment>();
  m_world.registerComponent<Components::EnemyAI>();
  m_world.registerComponent<Components::AudioSource>();
  m_world.registerComponent<Components::SpriteAnimator>();
  m_world.registerComponent<Components::StateMachine>();
  m_world.registerComponent<Components::PlayerController>();
  m_world.registerComponent<Components::FragmentCollector>();
  m_world.registerComponent<Components::TorchComponent>();
  m_world.registerComponent<Components::Door>();
  m_world.registerComponent<Components::DoorSwing>();
  m_world.registerComponent<Components::ParticleEmitter>();
  m_world.registerComponent<Components::ActivationZone>();
  m_world.registerComponent<Components::TextRenderer>();
  m_world.registerComponent<Components::Tag>();
  m_world.registerComponent<Components::Portal>();
  m_world.registerComponent<Gameplay::SurfaceComponent>();
  m_world.registerComponent<Components::LuaScript>();
  m_world.registerComponent<Components::Layer>();

  // Initialize layer enabled flags (all enabled by default)
  m_layerEnabled.resize(MAX_LAYERS, true);
}

void Scene::setLayerEnabled(int layer, bool enabled) {
  if (layer >= 0 && layer < MAX_LAYERS) {
    if (layer >= static_cast<int>(m_layerEnabled.size())) {
      m_layerEnabled.resize(layer + 1, true);
    }
    m_layerEnabled[layer] = enabled;
  }
}

bool Scene::isLayerEnabled(int layer) const {
  if (layer >= 0 && layer < static_cast<int>(m_layerEnabled.size())) {
    return m_layerEnabled[layer];
  }
  return true; // Default to enabled if layer not in range
}

void Scene::Clear() {
  // Stop BGM when clearing scene
  std::cout << "[Scene] Clearing Scene... Stopping BGM Group.\n";
  Audio::AudioManager::get().stopGroup(Audio::Group::BGM);

  m_scriptSystem.clear();

  // Destroy all entities in the world
  m_world.destroyAll();
}

bool Scene::LoadFromFile(const std::string &path) {
  Clear();
  if (std::filesystem::exists(path)) {
    if (SceneSerializer::Deserialize(*this, path)) {
      m_currentLevelPath = path;

      std::cout << "[Scene] Loaded Level: " << path << " SpawnPoint: ("
                << m_spawnPoint.x << ", " << m_spawnPoint.y << ")\n";

      // Force Player to Spawn Point
      bool playerFound = false;
      m_world.each<Components::PlayerController, Components::Transform>(
          [&](ECS::Entity, Components::PlayerController &,
              Components::Transform &t) {
            std::cout << "[Scene] Resetting Player to SpawnPoint: ("
                      << m_spawnPoint.x << ", " << m_spawnPoint.y << ")\n";
            t.position = m_spawnPoint;
            playerFound = true;
          });

      if (!playerFound) {
        std::cout << "[Scene] Warning: No Player found to reset!\n";
      }

      return true;
    } else {
      std::cerr << "[Scene] Failed to load level: " << path << "\n";
    }
  } else {
    std::cerr << "[Scene] Error: " << path << " not found!\n";
  }
  return false;
}

void Scene::Update(double dt, Platform::InputManager &input) {
  // Get layer enabled flags pointer for systems
  const std::vector<bool> *layerFlags =
      m_layerEnabled.empty() ? nullptr : &m_layerEnabled;

  if (m_physicsSystemEnabled)
    m_physicsSystem.update(m_world, dt, layerFlags);
  if (m_collisionSystemEnabled)
    m_collisionSystem.update(m_world, layerFlags);
  if (m_activationSystemEnabled)
    m_activationSystem.update(m_world, m_collisionSystem);
  if (m_batterySystemEnabled)
    m_batterySystem.update(m_world, dt);
  if (m_lightSystemEnabled)
    m_lightSystem.update(m_world, dt);
  if (m_fragmentSystemEnabled)
    m_fragmentSystem.update(m_world, dt);
  if (m_aiSystemEnabled)
    m_aiSystem.update(m_world, dt);
  if (m_scriptSystemEnabled)
    m_scriptSystem.update(m_world, dt);
  if (m_audioSystemEnabled)
    m_audioSystem.update(m_world);
  if (m_animSystemEnabled)
    m_animSystem.update(m_world, dt);
  if (m_stateMachineSystemEnabled)
    m_stateMachineSystem.update(m_world, dt);

  // Player System needs input
  if (m_playerSystemEnabled)
    m_playerSystem.update(m_world, dt, input, layerFlags);

  if (m_doorSystemEnabled)
    m_doorSystem.update(m_world, dt);

  if (m_doorSwingSystemEnabled)
    m_doorSwingSystem.update(m_world, dt);

  // --- Update Legacy GameObjects ---
  for (auto &obj : m_gameObjects) {
    if (obj && obj->active()) {
      obj->update(dt);
    }
  }

  // --- Update ECS Particle Emitters ---
  m_world.each<Components::Transform, Components::ParticleEmitter>(
      [&](ECS::Entity, Components::Transform &t,
          Components::ParticleEmitter &pe) {
        if (pe.active()) {
          ps::v2 pos{t.position.x, t.position.y};
          pe.update(pos, dt);
        }
      });

  // --- Game Logic ---
  // 1. Handle Level Transition
  if (!m_pendingLevel.empty()) {
    std::string levelPath = m_pendingLevel;
    if (levelPath.find("assets/scenes/") == std::string::npos) {
      levelPath = "assets/scenes/" + levelPath;
    }
    LoadFromFile(levelPath);
    m_pendingLevel.clear();
    // Return early to avoid updating a just-cleared or fresh scene with old
    // input
    return;
  }

  // 2. Handle Death Timer
  // 2. Handle Death State
  if (m_playerDeathTimer > 0.0f) {
    // Player is dead.
    // Instead of auto-respawning, we just wait for manual respawn.
    // Ensure physics stays disabled and animation is correct.
    m_deathDuration += static_cast<float>(dt);
  } else {
    // 3. Check Collision (Player vs Enemy) only if alive
    // 3. Check Collision (Player vs Enemy) via CollisionSystem
    // Use the collisions detected by the physics engine to determine gameplay
    // effects
    auto &collisions = m_collisionSystem.getCollisions();
    for (const auto &pair : collisions) {
      if (m_playerDeathTimer > 0.0f)
        break; // Already handling death

      ECS::Entity eA = pair.first;
      ECS::Entity eB = pair.second;

      if (!m_world.isValid(eA) || !m_world.isValid(eB))
        continue;

      // Identify if one is Player and one is Enemy
      bool playerA = m_world.hasComponent<Components::PlayerController>(eA);
      bool playerB = m_world.hasComponent<Components::PlayerController>(eB);
      bool enemyA = m_world.hasComponent<Components::EnemyAI>(eA);
      bool enemyB = m_world.hasComponent<Components::EnemyAI>(eB);

      if ((playerA && enemyB) || (playerB && enemyA)) {
        ECS::Entity playerEntity = playerA ? eA : eB;
        // Skip if already dead
        auto *pAnim =
            m_world.getComponent<Components::SpriteAnimator>(playerEntity);
        if (pAnim && pAnim->currentClip == "death")
          continue;

        // COLLISION!
        std::cout << "[Scene] Player hit by enemy (via CollisionSystem)!\n";
        if (pAnim) {
          pAnim->play("death");
        }

        // Disable physics
        if (auto *pCol =
                m_world.getComponent<Components::Collider>(playerEntity)) {
          pCol->isSolid = false;
        }
        m_playerDeathTimer = 3.0f; // 3 seconds to respawn
      }
    }
  }

  // 4. Portal Logic
  auto &collisions = m_collisionSystem.getCollisions();
  for (const auto &pair : collisions) {
    auto entityA = pair.first;
    auto entityB = pair.second;

    if (!m_world.isValid(entityA) || !m_world.isValid(entityB))
      continue;

    // Use safe component check helper to avoid crashes if component not
    // registered
    bool portalA = m_world.hasComponent<Components::Portal>(entityA);
    bool portalB = m_world.hasComponent<Components::Portal>(entityB);
    bool playerA = m_world.hasComponent<Components::PlayerController>(entityA);
    bool playerB = m_world.hasComponent<Components::PlayerController>(entityB);

    ECS::Entity portalEntity = {0, 0};

    if (playerA && portalB) {
      portalEntity = entityB;
    } else if (playerB && portalA) {
      portalEntity = entityA;
    }

    if (portalEntity.isValid()) {
      auto *portal = m_world.getComponent<Components::Portal>(portalEntity);
      if (portal && !portal->levelName.empty()) {
        m_pendingLevel = portal->levelName;
        std::cout << "[Scene] Transitioning to level: " << m_pendingLevel
                  << "\n";
        break; // Only one transition per frame
      }
    }
  }
}

void Scene::Render(Graphics::RenderingManager &renderer) {
  // Get layer enabled flags pointer for render system
  const std::vector<bool> *layerFlags =
      m_layerEnabled.empty() ? nullptr : &m_layerEnabled;

  if (m_renderSystemEnabled) {
    m_renderSystem.update(m_world, renderer, layerFlags);
  }

  // --- Render Legacy GameObjects ---
  for (auto &obj : m_gameObjects) {
    if (obj && obj->active()) {
      obj->debugRender(renderer); // Using debugRender for now primarily
      // obj->render(renderer); // If you have real rendering logic
    }
  }

  // --- Render ECS Particle Emitters ---
  m_world.each<Components::ParticleEmitter>(
      [&](ECS::Entity, Components::ParticleEmitter &pe) {
        if (pe.active()) {
          // Ensure renderer is set if not already (safeguard)
          pe.setRenderer(&renderer);
          // We can just call onRender with a dummy or expose render method.
          // Logic is: if (m_renderer) m_system.render(*m_renderer);
          // so calling system().render(renderer) directly works if exposed,
          // OR better, we simply call pe.onRender(dummy) or pe.render(renderer)
          // if we added it. Since we didn't add explicit render(renderer) to
          // Emitter, let's use the fact that it has the pointer. But we can
          // just use the system directly!
          pe.system().render(renderer);
        }
      });
}

void Scene::RespawnPlayer() {
  m_playerDeathTimer = -1.0f;
  m_deathDuration = 0.0f;

  std::cout << "[Scene] Manual Respawn Triggered\n";

  // Reset Player
  m_world.each<Components::PlayerController, Components::Transform,
               Components::SpriteAnimator>(
      [&](ECS::Entity entity, Components::PlayerController &,
          Components::Transform &t, Components::SpriteAnimator &anim) {
        t.position = m_spawnPoint;
        anim.play("idle");
        if (auto *col = m_world.getComponent<Components::Collider>(entity)) {
          col->isSolid = true; // Re-enable physics
        }
      });
}

} // namespace Runtime
