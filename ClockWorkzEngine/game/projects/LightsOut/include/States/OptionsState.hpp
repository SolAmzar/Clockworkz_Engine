/******************************************************************************/
/*!
\file           OptionsState.hpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Declares the options/settings menu state for Lights Out, backed
                by OptionsMenu.json. Matches the MainMenu overlay style.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Systems/UISliderSystem.hpp"
#include <string>

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
namespace Core {
class Time;
}
namespace Graphics {
class RenderingManager;
class Camera2D;
} // namespace Graphics

namespace Runtime {

class OptionsState final : public GameState {
public:
  OptionsState(GameStateManager &gsm, Platform::InputManager &input,
               Platform::WindowManager &window, Audio::AudioManager &audio);

  const char *name() const override { return "OptionsState"; }
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  void updateLayout(int w, int h);
  void updateSceneText();
  void closeOptions();
  void syncSlidersFromSettings();
  void applySliderSettings();
  void adjustOptionVolume(float delta);
  void setOptionVolume(int optionIndex, float value);

  GameStateManager        &m_gsm;
  Platform::InputManager  &m_input;
  Platform::WindowManager &m_window;
  Audio::AudioManager     &m_audio;

  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D         *m_cam{nullptr};

  Runtime::Scene             m_scene;
  Systems::UISliderSystem    m_sliderSystem;

  int                        m_selected{0};
  bool                       m_debug{false};
};

} // namespace Runtime
