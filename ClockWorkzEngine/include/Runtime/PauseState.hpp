/******************************************************************************/
/*!
\file           PauseState.hpp
\project        Lights Out
\author(s)      [Syafiq], 95%  [Lebon], 5%
\brief          Basic pause menu implementation header
*/
/******************************************************************************/
#pragma once
#include "Gameplay/GameObject.hpp"
#include "Runtime/GameState.hpp"
#include <string>
#include <vector>


namespace Platform {
class InputManager;
class WindowManager;
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
  void moveUp();
  void moveDown();
  void select();

  GameStateManager &m_gsm;
  Platform::InputManager &m_input;
  Audio::AudioManager &m_audio;

  Gameplay::GameObject m_background;
  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D *m_cam{nullptr};

  std::vector<std::string> m_items{"Resume Game", "Main Menu"};
  int m_selected{0};

  float m_bgmVolumeDuringPause{0.2f};
  float m_sfxVolumeDuringPause{0.2f};
  float m_bgmVolumeBeforePause{0.5f};
  float m_sfxVolumeBeforePause{0.5f};
  float m_sfx{1.0f};

  bool m_debug{false};
};
} // namespace Runtime
