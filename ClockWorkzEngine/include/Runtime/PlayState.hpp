#if 0
/******************************************************************************/
/*!
\file           PlayState.hpp
\project        Lights Out
\author(s)      [Lebon], [Syafiq]
\brief          Declares PlayState, the main gameplay loop state.
                                Responsibilities include:
                                - Loading and managing the current Scene and its
GameObjects
                                - Updating gameplay logic such as movement,
collisions, and AI
                                - Rendering the game world each frame
                                - Transitioning back to other states (e.g.,
pause or menu)

All content   2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/Components/SurfaceComponent.hpp"
#include "Gameplay/GameObject.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneManager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Core {
class Logger;
class Time;
} // namespace Core

namespace Platform {
class LevelEditor;
}

namespace Graphics {
class RenderingManager;
}

namespace Runtime {

class PlayState final : public GameState {
public:
  explicit PlayState(std::string initialSceneId, Platform::InputManager &input,
                     GameStateManager &gsm);
  ~PlayState();

  const char *name() const override;

  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void updateLightExposure(double dt);
  void render(Core::Time *timer) override;
  bool m_spawning = false;
  int m_spawnedCount = 0;
  const int SPAWN_PER_FRAME = 100;
  bool m_shouldReturnToMenu{false};
  void rebindLightsAndPlayerAfterReload();
  void InitPlayerSpriteUVs();
  const char *toString(Gameplay::GameObjectType t);
  void renderImGui(Core::Time *timer) override; // Imgui for playstate
  Gameplay::SurfaceType getSurfaceUnder(Gameplay::GameObject *player);
  void requestReturnToMenu() { m_returnToMenuRequested = true; }

private:
  std::string m_initialSceneId;
  SceneManager m_scenes; // your existing scene manager
  Platform::InputManager &m_input;
  GameStateManager &m_gsm;
  Gameplay::GameObject *m_fx{nullptr};
  bool m_spawnonclick{false};
  bool m_loaded{false};
  bool m_debug{false};
  bool m_flashlightOn{true};
  Graphics::RenderingManager *m_renderer{nullptr};
  float m_leftBound{100.f};
  float m_rightBound{700.f};
  float m_ballSpeed{220.f};
  int m_ballDir{+1};
  float m_distanceSinceStep = 0.0f;
  float m_stepDistance = 60.0f;
  float m_timeSinceStep = 0.0f;
  float m_minStepInterval = 0.22f;
  vec2 m_lastFootPos{};
  bool m_haveLastFootPos = false;
  vec2 m_spawnPoint;
  void resetScene();
  Gameplay::GameObject *m_player;
  std::vector<std::unique_ptr<Gameplay::GameObject>> gameObjects;

  float currentBattery;
  float maxBattery;
  std::string currentBatteryTexture;

  void updateBatteryDisplay();

  bool m_pauseRequested{false};
  bool m_returnToMenuRequested{false};

  std::unique_ptr<Platform::LevelEditor> m_levelEditor;
  bool m_editorInitialized{false};

  void initializeEnemyAI(Runtime::Scene *scene);

  float m_playerRespawning{5.f};
  bool m_GameOver{false};
  bool m_playerDead{false};
  float m_deathTimer{0.0f};

  // Audio members
  int m_bgmChannel{-1};
  float m_bgmVol{0.5f};
  float m_sfxVol{0.5f};
  bool m_bgmPlaying{false};
  const float m_deathDelay = 2.0f;

  // Fragment system
  void updateFragmentCollection(double dt);
  void renderFragmentUI();
  void handleFragmentPickup(Gameplay::GameObject *player,
                            Gameplay::GameObject *fragment);
  std::string m_fragmentIconTexture = "fragment_texture";

  // Fragment tracking
  bool m_showFragmentNotification = false;
  float m_fragmentNotificationTimer = 0.0f;
  std::string m_lastCollectedFragmentId;
};

#endif