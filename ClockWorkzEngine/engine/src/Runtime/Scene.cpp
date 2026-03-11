/******************************************************************************/
/*!
\file           Scene.cpp
\project        Lights Out
\author(s)      [Lebon] 90% [Amzar] 10% 
\brief          Implementation of the Scene class.
*/
/******************************************************************************/

#include "Runtime/Scene.hpp"

// Engine components
#include "Components/AudioSource.hpp"
#include "Components/Collider.hpp"
#include "Components/Layer.hpp"
#include "Components/Light.hpp"
#include "Components/LuaScript.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Components/Velocity.hpp"
#include "Graphics/Camera2D.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>

#include "Components/CameraComponent.hpp"
#include "Audio/AudioManager.hpp"
#include "Core/Engine.hpp"
#include "Memory/MemoryManager.hpp"

namespace Runtime {

void Scene::setGameCallbacks(SceneUpdateFn  onUpdate,
                             SceneRenderFn  onRender,
                             SceneLoadFn    onLoad,
                             SceneRespawnFn onRespawn) {
  m_onUpdate  = std::move(onUpdate);
  m_onRender  = std::move(onRender);
  m_onLoad    = std::move(onLoad);
  m_onRespawn = std::move(onRespawn);
}

void Scene::setWorldTableExtension(Systems::ScriptSystem::WorldTableExtensionFn fn) {
  m_scriptSystem.setWorldTableExtension(std::move(fn));
}

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
  // Pre-allocate memory pool registry entries at load time (25 component types).
  Memory::MemoryManager::Get().preallocateForScene(25);

  // Register engine-level component types.
  m_world.registerComponent<Components::Transform>();
  m_world.registerComponent<Components::Velocity>();
  m_world.registerComponent<Components::SpriteRenderer>();
  m_world.registerComponent<Components::Collider>();
  m_world.registerComponent<Components::AudioSource>();
  m_world.registerComponent<Components::SpriteAnimator>();
  m_world.registerComponent<Components::StateMachine>();
  m_world.registerComponent<Components::TextRenderer>();
  m_world.registerComponent<Components::Tag>();
  m_world.registerComponent<Components::LuaScript>();
  m_world.registerComponent<Components::Layer>();
  m_world.registerComponent<Components::Light>();
  m_world.registerComponent<Components::UIButton>();
  m_world.registerComponent<Components::UISlider>();
  m_world.registerComponent<Components::CameraComponent>();

  // Let the game register its own component types (set via Engine::setComponentRegistrar).
  Core::Engine::Get().invokeComponentRegistrar(m_world);

  // Wire level-loading so scripts can call World.loadLevel(name)
  m_scriptSystem.setLoadLevelFn([this](const std::string& name) { SetPendingLevel(name); });

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
  Core::Engine::Get().audio().stopGroup(Audio::Group::BGM);

  m_scriptSystem.clear();

  // Destroy all entities in the world
  m_world.destroyAll();

  // Clear legacy GameObjects
  m_gameObjects.clear();

  // Reset Scene settings
  m_soundVolumeOverrides.clear();
  std::fill(m_layerEnabled.begin(), m_layerEnabled.end(), true);
  m_enableDarkness = false;
  m_darknessIntensity = 0.8f;
  m_darknessColor = glm::vec3(0.7f, 0.7f, 0.9f);
  m_bgm.clear();
}

bool Scene::LoadFromFile(const std::string &path) {
  Clear();
  if (std::filesystem::exists(path)) {
    if (SceneSerializer::Deserialize(*this, path)) {
      m_currentLevelPath = path;

      std::cout << "[Scene] Loaded Level: " << path << " SpawnPoint: ("
                << m_spawnPoint.x << ", " << m_spawnPoint.y << ")\n";

      // Let game-side code reset game entities (e.g. player spawn point)
      if (m_onLoad) m_onLoad(m_world, m_spawnPoint);

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

  // Engine systems
  if (m_physicsSystemEnabled)
    m_physicsSystem.update(m_world, dt, layerFlags);
  if (m_collisionSystemEnabled)
    m_collisionSystem.update(m_world, layerFlags);
  static const std::vector<std::pair<ECS::Entity, ECS::Entity>> s_emptyCollisions;
  if (m_scriptSystemEnabled)
    m_scriptSystem.update(m_world, dt,
        m_collisionSystemEnabled ? m_collisionSystem.getCollisions() : s_emptyCollisions);
  if (m_audioSystemEnabled)
    m_audioSystem.update(m_world);
  if (m_animSystemEnabled)
    m_animSystem.update(m_world, dt);
  if (m_stateMachineSystemEnabled)
    m_stateMachineSystem.update(m_world, dt);
  // UI hover visual feedback (works in every state, including EditorPlayState)
  m_uiButtonSystem.update(m_world, input);

  // Note: game-specific systems (PlayerSystem, AISystem, BatterySystem, etc.)
  // are called by the game state (PlayState) after this update.

  // --- Update Legacy GameObjects ---
  for (auto &obj : m_gameObjects) {
    if (obj && obj->active()) {
      obj->update(dt);
    }
  }

  // --- Handle pending level transition (can be set by game callback) ---
  if (!m_pendingLevel.empty()) {
    std::string levelPath = m_pendingLevel;
    if (levelPath.find("assets/scenes/") == std::string::npos) {
      levelPath = "assets/scenes/" + levelPath;
    }
    LoadFromFile(levelPath);
    m_pendingLevel.clear();
    return;
  }

  // --- Handle death timer ---
  if (m_playerDeathTimer > 0.0f) {
    m_deathDuration += static_cast<float>(dt);
  }

  // --- Game-specific update (particles, collision reactions, portal, etc.) ---
  if (m_onUpdate) m_onUpdate(m_world, dt, m_collisionSystem, *this);

  // --- Camera system: must run last so it sees final entity positions ---
  if (m_cameraSystemEnabled)
    m_cameraSystem.update(m_world);
}

void Scene::Render(Graphics::RenderingManager &renderer) {
  // Get layer enabled flags pointer for render system
  const std::vector<bool> *layerFlags =
      m_layerEnabled.empty() ? nullptr : &m_layerEnabled;

  if (m_renderSystemEnabled) {
    m_renderSystem.update(m_world, renderer, layerFlags);
    m_uiSliderSystem.render(m_world, renderer);
  }

  // Flush the sprite batch before the light pass so light cones are drawn
  // on top of sprites (not underneath them).
  renderer.flush();

  // --- Render Legacy GameObjects ---
  for (auto &obj : m_gameObjects) {
    if (obj && obj->active()) {
      obj->debugRender(renderer); // Using debugRender for now primarily
      // obj->render(renderer); // If you have real rendering logic
    }
  }

  // --- Game-specific render (particles, etc.) ---
  if (m_onRender) m_onRender(m_world, renderer);
}

void Scene::RenderText(Graphics::RenderingManager &renderer) {
  if (m_textRenderSystemEnabled) {
    m_textRenderSystem.update(m_world, renderer,
                              Core::Engine::Get().camera().viewProj());
  }
}

void Scene::RenderAll(Graphics::RenderingManager &renderer,
                      const glm::mat4 &viewProj, const glm::vec4 &tint) {
  renderer.beginScenePass(viewProj, tint);
  Render(renderer);
  renderer.endScenePass();
  RenderText(renderer);
}

void Scene::RespawnPlayer() {
  m_playerDeathTimer = -1.0f;
  m_deathDuration = 0.0f;

  std::cout << "[Scene] Manual Respawn Triggered\n";

  // Let game-side code restore player state
  if (m_onRespawn) m_onRespawn(m_world, m_spawnPoint);
}

} // namespace Runtime
