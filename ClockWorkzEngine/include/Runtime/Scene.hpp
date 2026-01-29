#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ECS/World.hpp"
#include "Gameplay/GameObject.hpp" // Legacy Support
#include "Graphics/RenderingManager.hpp"
#include "Platform/InputManager.hpp"

// System Includes
#include "Gameplay/Physics2D.hpp"
#include "Systems/AISystem.hpp"
#include "Systems/AnimationSystem.hpp"
#include "Systems/AudioSystem.hpp"
#include "Systems/BatterySystem.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Systems/DoorSwingSystem.hpp"
#include "Systems/DoorSystem.hpp"
#include "Systems/FragmentSystem.hpp"
#include "Systems/LightExposureSystem.hpp"
#include "Systems/PhysicsSystem.hpp"
#include "Systems/PlayerSystem.hpp"
#include "Systems/RenderSystem.hpp"
#include "Systems/StateMachineSystem.hpp"

namespace Runtime {

class Scene {
public:
  Scene(const std::string &name = "Untitled") : m_name(name) {}
  ~Scene() = default;

  // "Runtime" Interface (My new API)
  void Init();
  void Update(double dt, Platform::InputManager &input);
  void Render(Graphics::RenderingManager &renderer);
  void Clear();
  ECS::World &GetWorld() { return m_world; }

  // "SceneManager" Interface (Legacy/Manager expectation)
  bool load() {
    Init();
    return true;
  }
  void unload() { Clear(); }
  void update(double dt) {
    // Fallback update if input is missing, though this shouldn't be called if
    // we update SceneManager. For now, allow it but ideally we pass input.
    // However, SceneManager.cpp currently calls update(dt).
    // We will update SceneManager to pass input, so we can change this
    // signature or overload.
  }
  void update(double dt, Platform::InputManager &input) { Update(dt, input); }

  void render(Graphics::RenderingManager &renderer) { Render(renderer); }
  std::string name() const { return m_name; }

  // System Toggles (Public for Editor access)
  bool m_renderSystemEnabled{true};
  bool m_physicsSystemEnabled{true};
  bool m_collisionSystemEnabled{true};
  bool m_batterySystemEnabled{true};
  bool m_lightSystemEnabled{true};
  bool m_fragmentSystemEnabled{true};
  bool m_aiSystemEnabled{true};
  bool m_audioSystemEnabled{true};
  bool m_animSystemEnabled{true};
  bool m_stateMachineSystemEnabled{true};
  bool m_playerSystemEnabled{true};
  bool m_doorSystemEnabled{true};
  bool m_doorSwingSystemEnabled{true};

  // Public access to systems if needed for debug overlay (e.g. collision list)
  Systems::CollisionSystem &GetCollisionSystem() { return m_collisionSystem; }
  Systems::PhysicsSystem *getPhysics() { return &m_physicsSystem; }
  Gameplay::Physics2D *getLegacyPhysics() { return &m_legacyPhysics; }

private:
  ECS::World m_world;

  // Systems
  Systems::RenderSystem m_renderSystem;
  Systems::PhysicsSystem m_physicsSystem;
  Gameplay::Physics2D m_legacyPhysics;
  Systems::CollisionSystem m_collisionSystem;
  Systems::BatterySystem m_batterySystem;
  Systems::LightExposureSystem m_lightSystem;
  Systems::FragmentSystem m_fragmentSystem;
  Systems::AISystem m_aiSystem;
  Systems::AudioSystem m_audioSystem;
  Systems::AnimationSystem m_animSystem;
  Systems::StateMachineSystem m_stateMachineSystem;
  Systems::PlayerSystem m_playerSystem;
  Systems::DoorSystem m_doorSystem;
  Systems::DoorSwingSystem m_doorSwingSystem;

  std::string m_name;

  // Legacy GameObject Support
  std::vector<std::unique_ptr<Gameplay::GameObject>> m_gameObjects;

public:
  void addObject(std::unique_ptr<Gameplay::GameObject> obj) {
    m_gameObjects.push_back(std::move(obj));
  }
  std::vector<std::unique_ptr<Gameplay::GameObject>> &objects() {
    return m_gameObjects;
  }
  const std::vector<std::unique_ptr<Gameplay::GameObject>> &objects() const {
    return m_gameObjects;
  }
  void registerExistingObjectsToLayers() {}
};

} // namespace Runtime
