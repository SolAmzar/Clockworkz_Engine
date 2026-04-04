/******************************************************************************/
/*!
\file           PauseState.hpp
\project        Lights Out
\author(s)      [Syafiq] 53% [Amzar] 47%
\brief          Pause menu implementation header.
                Loads PauseMenu.json scene (authored in level editor) and
                renders it as a semi-transparent overlay on top of the frozen
                game frame. Saves/restores the game camera on enter/exit.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <glm/vec2.hpp>
#include <string>
#include <vector>

namespace Platform {
class InputManager;
} // namespace Platform
namespace Runtime {
class GameStateManager;
}
namespace Audio {
class AudioManager;
}
namespace Graphics {
class RenderingManager;
class Camera2D;
} // namespace Graphics

namespace Runtime {

class PauseState final : public GameState {
public:
  PauseState(GameStateManager &gsm, Platform::InputManager &input);
  ~PauseState() = default;

  const char *name() const override { return "PauseState"; }
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  void updateLayout(int w, int h);
  void handleButton(const Components::UIButton &btn);
  void collectButtons();
  void applyHoverColors();

  GameStateManager       &m_gsm;
  Platform::InputManager &m_input;
  Audio::AudioManager    &m_audio;

  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D         *m_cam{nullptr};

  Runtime::Scene            m_pauseScene;

  std::vector<ECS::Entity>  m_buttonEntities;
  int                       m_selected{0};

  // Camera save/restore (so PlayState camera is intact on resume)
  glm::vec2 m_savedCamPos{0.f, 0.f};
  float     m_savedCamZoom{1.f};
  int       m_savedViewW{800};
  int       m_savedViewH{600};

  float m_sfx{1.0f};

  float m_bgmVolumeBeforePause{1.0f};
  float m_sfxVolumeBeforePause{1.0f};
  float m_bgmVolumeDuringPause{0.4f};
  float m_sfxVolumeDuringPause{0.4f};

  bool m_debug{false};
};

} // namespace Runtime
