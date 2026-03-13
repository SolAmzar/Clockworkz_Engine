/******************************************************************************/
/*!
\file           Scene.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon] 57% [Syafiq] 25% [Sol-Amzar] 15% [Yusuf] 3%
\brief          Declares the runtime scene container, system orchestration, and
                scene-level settings used by the engine and game states.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ECS/World.hpp"
#include "Gameplay/GameObject.hpp" // Legacy Support
#include "Graphics/RenderingManager.hpp"
#include "Math/Vec2.hpp"
#include "Platform/InputManager.hpp"

// Engine System Includes (game-specific systems live in PlayState)
#include "Gameplay/Physics2D.hpp"
#include "Systems/AnimationSystem.hpp"
#include "Systems/AudioSystem.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Systems/PhysicsSystem.hpp"
#include "Systems/CameraSystem.hpp"
#include "Systems/RenderSystem.hpp"
#include "Systems/StateMachineSystem.hpp"
#include "Systems/ScriptSystem.hpp"
#include "Systems/TextRenderSystem.hpp"
#include "Systems/UIButtonSystem.hpp"
#include "Systems/UISliderSystem.hpp"

namespace Runtime {

class Scene {
public:
  Scene(const std::string &name = "Untitled") : m_name(name) {}
  ~Scene() = default;

  // "Runtime" Interface (My new API)
  void Init();
  void Update(double dt, Platform::InputManager &input);
  void Render(Graphics::RenderingManager &renderer);
  void RenderText(Graphics::RenderingManager &renderer); // Call AFTER endScenePass
  // Convenience: runs the full render pipeline in the correct order.
  // Prefer this over calling Render/RenderText manually in game states.
  void RenderAll(Graphics::RenderingManager &renderer, const glm::mat4 &viewProj,
                 const glm::vec4 &tint = glm::vec4(1.0f));
  void Clear();
  bool LoadFromFile(const std::string &path);
  ECS::World &GetWorld() { return m_world; }
  const ECS::World &GetWorld() const { return m_world; }

  // Death / Respawn Logic
  bool IsPlayerDead() const { return m_playerDeathTimer > 0.0f; }
  void RespawnPlayer();
  float GetDeathDuration() const { return m_deathDuration; }

  // Game callbacks: let game-side code hook into Scene lifecycle without the
  // engine needing to know about game-specific component types.
  using SceneUpdateFn  = std::function<void(ECS::World&, double,
                                            Systems::CollisionSystem&, Scene&)>;
  using SceneRenderFn  = std::function<void(ECS::World&,
                                            Graphics::RenderingManager&)>;
  using SceneLoadFn    = std::function<void(ECS::World&, const vec2& spawnPoint, Scene&)>;
  using SceneRespawnFn = std::function<void(ECS::World&, const vec2& spawnPoint)>;
  void setGameCallbacks(SceneUpdateFn  onUpdate,
                        SceneRenderFn  onRender,
                        SceneLoadFn    onLoad,
                        SceneRespawnFn onRespawn);

  // Delegate to ScriptSystem: lets game code register custom World.eachXxx()
  // functions without the engine needing game-specific includes.
  void setWorldTableExtension(Systems::ScriptSystem::WorldTableExtensionFn fn);

  // Pending Level Accessors for Editor
  std::string GetPendingLevel() const { return m_pendingLevel; }
  void ClearPendingLevel() { m_pendingLevel.clear(); }
  void SetPendingLevel(const std::string& level) { m_pendingLevel = level; }
  std::string GetCurrentLevelPath() const { return m_currentLevelPath; }

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
  bool m_audioSystemEnabled{true};
  bool m_animSystemEnabled{true};
  bool m_stateMachineSystemEnabled{true};
  bool m_textRenderSystemEnabled{true};
  bool m_scriptSystemEnabled{true};
  bool m_cameraSystemEnabled{true};

  // Layer System
  static constexpr int MAX_LAYERS = 32;
  std::vector<bool> m_layerEnabled;
  void setLayerEnabled(int layer, bool enabled);
  bool isLayerEnabled(int layer) const;
  const std::vector<bool>& getLayerEnabled() const { return m_layerEnabled; }

  // Public access to systems if needed for debug overlay (e.g. collision list)
  Systems::CollisionSystem &GetCollisionSystem() { return m_collisionSystem; }
  Systems::PhysicsSystem *getPhysics() { return &m_physicsSystem; }
  Gameplay::Physics2D *getLegacyPhysics() { return &m_legacyPhysics; }

private:
  ECS::World m_world;

  // --- Engine Systems ---
  Systems::RenderSystem m_renderSystem;
  Systems::PhysicsSystem m_physicsSystem;
  Gameplay::Physics2D m_legacyPhysics;
  Systems::CollisionSystem m_collisionSystem;
  Systems::AudioSystem m_audioSystem;
  Systems::AnimationSystem m_animSystem;
  Systems::StateMachineSystem m_stateMachineSystem;
  Systems::TextRenderSystem m_textRenderSystem;
  Systems::UIButtonSystem m_uiButtonSystem;
  Systems::UISliderSystem m_uiSliderSystem;
  Systems::ScriptSystem m_scriptSystem;
  Systems::CameraSystem m_cameraSystem;

  // --- Game Logic State (set/read by game states) ---
  float m_playerDeathTimer = -1.0f;
  float m_deathDuration = 0.0f;
  std::string m_pendingLevel;

  // --- Game callbacks (registered by game-side code) ---
  SceneUpdateFn  m_onUpdate;
  SceneRenderFn  m_onRender;
  SceneLoadFn    m_onLoad;
  SceneRespawnFn m_onRespawn;

  std::string m_name;
  std::string m_bgm;          // Legacy background music track name
  std::string m_musicContext; // Central music context id
  float m_sfxVolume{1.0f};    // SFX Volume multiplier
  vec2 m_spawnPoint{0.0f, 0.0f};
  std::unordered_map<std::string, float> m_soundVolumeOverrides;

  // Darkness Settings
  bool m_enableDarkness{false};
  float m_darknessIntensity{0.8f};
  glm::vec3 m_darknessColor{0.7f, 0.7f, 0.9f};

public:
  void setBGM(const std::string &bgm) { m_bgm = bgm; }
  std::string getBGM() const { return m_bgm; }
  void setMusicContext(const std::string &context) { m_musicContext = context; }
  std::string getMusicContext() const { return m_musicContext; }

  void setSFXVolume(float volume);
  float getSFXVolume() const { return m_sfxVolume; }
  void setSoundVolumeOverride(const std::string &name, float volume);
  void removeSoundVolumeOverride(const std::string &name);
  float getSoundVolumeOverride(const std::string &name, float fallback = 1.0f) const;
  const std::unordered_map<std::string, float> &soundVolumeOverrides() const {
    return m_soundVolumeOverrides;
  }

  void setSpawnPoint(const vec2 &p) { m_spawnPoint = p; }
  vec2 getSpawnPoint() const { return m_spawnPoint; }

  // Darkness / Global Tint Settings
  void setDarknessEnabled(bool enabled) { m_enableDarkness = enabled; }
  bool isDarknessEnabled() const { return m_enableDarkness; }

  void setDarknessIntensity(float intensity) {
    m_darknessIntensity = intensity;
  }
  float getDarknessIntensity() const { return m_darknessIntensity; }

  void setDarknessColor(const glm::vec3 &color) { m_darknessColor = color; }
  glm::vec3 getDarknessColor() const { return m_darknessColor; }

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

private:
  std::string m_currentLevelPath;
};

} // namespace Runtime

