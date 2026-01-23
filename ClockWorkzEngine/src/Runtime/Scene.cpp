#include "Runtime/Scene.hpp"

// Components
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
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
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"

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
}

void Scene::Render(Graphics::RenderingManager &renderer) {
  if (m_renderSystemEnabled) {
    m_renderSystem.update(m_world, renderer);
  }
}

} // namespace Runtime
