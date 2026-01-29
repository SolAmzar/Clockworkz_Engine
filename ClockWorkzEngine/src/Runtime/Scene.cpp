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
#include "Components/Torch.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"

#include "Components/ActivationZone.hpp"
#include "Components/Velocity.hpp"
#include "Gameplay/Components/ParticleEmitter.hpp"


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
}

void Scene::Clear() {
  // Destroy all entities in the world
  m_world.destroyAll();
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

} // namespace Runtime
