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
#include "Components/Portal.hpp"
#include "Components/Velocity.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>

namespace Runtime {

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
}

void Scene::Clear() {
  // Destroy all entities in the world
  m_world.destroyAll();
}

bool Scene::LoadFromFile(const std::string &path) {
  Clear();
  if (std::filesystem::exists(path)) {
    if (SceneSerializer::Deserialize(m_world, path)) {
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
  if (m_physicsSystemEnabled)
    m_physicsSystem.update(m_world, dt);
  if (m_collisionSystemEnabled)
    m_collisionSystem.update(m_world);
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
  if (m_audioSystemEnabled)
    m_audioSystem.update(m_world);
  if (m_animSystemEnabled)
    m_animSystem.update(m_world, dt);
  if (m_stateMachineSystemEnabled)
    m_stateMachineSystem.update(m_world, dt);

  // Player System needs input
  if (m_playerSystemEnabled)
    m_playerSystem.update(m_world, dt, input);

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
    LoadFromFile(m_pendingLevel);
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
    m_world.each<Components::PlayerController, Components::Transform,
                 Components::Collider>(
        [&](ECS::Entity playerEntity, Components::PlayerController &,
            Components::Transform &pTrans, Components::Collider &pCol) {
          // Skip if already dead
          auto *pAnim =
              m_world.getComponent<Components::SpriteAnimator>(playerEntity);
          if (pAnim && pAnim->currentClip == "death")
            return;

          m_world.each<Components::EnemyAI, Components::Transform,
                       Components::Collider>(
              [&](ECS::Entity, Components::EnemyAI &,
                  Components::Transform &eTrans, Components::Collider &eCol) {
                // Calculate AABB
                float pW = pCol.size.x * pTrans.scale.x;
                float pH = pCol.size.y * pTrans.scale.y;
                float eW = eCol.size.x * eTrans.scale.x;
                float eH = eCol.size.y * eTrans.scale.y;

                float dx = std::abs(pTrans.position.x - eTrans.position.x);
                float dy = std::abs(pTrans.position.y - eTrans.position.y);

                // Check overlap
                float sumHalfW = (pW + eW) * 0.5f;
                float sumHalfH = (pH + eH) * 0.5f;

                if (dx < sumHalfW && dy < sumHalfH) {
                  // COLLISION!
                  std::cout << "[Scene] Player died!\n";
                  if (pAnim) {
                    pAnim->play("death");
                  }
                  // Disable physics
                  pCol.isSolid = false;
                  m_playerDeathTimer = 1.0f; // Set > 0 to indicate dead state
                  m_deathDuration = 0.0f;
                }
              });
        });
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
  if (m_renderSystemEnabled) {
    m_renderSystem.update(m_world, renderer);
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
        t.position = {0, 0}; // Reset to origin or spawn point... consider
                             // saving spawn point?
        anim.play("idle");
        if (auto *col = m_world.getComponent<Components::Collider>(entity)) {
          col->isSolid = true; // Re-enable physics
        }
      });
}

} // namespace Runtime
