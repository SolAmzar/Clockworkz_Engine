/******************************************************************************/
/*!
\file           OptionsState.hpp
\project        Lights Out
\brief          Dedicated options menu state backed by OptionsMenu.json.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
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
  void collectButtons();
  void applyHoverColors();
  void handleAction(const std::string &action);
  void updateSceneText();
  void closeOptions();
  void adjustOptionVolume(float delta);
  void setOptionVolume(int optionIndex, float value);
  void renderVolumeBar(float centerX, float centerY, float width,
                       float height, float value, bool selected);
  void renderSceneBars();

  GameStateManager        &m_gsm;
  Platform::InputManager  &m_input;
  Platform::WindowManager &m_window;
  Audio::AudioManager     &m_audio;

  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D         *m_cam{nullptr};

  Runtime::Scene             m_scene;

  std::vector<ECS::Entity>   m_buttonEntities;
  int                        m_selected{0};
  bool                       m_debug{false};
};

} // namespace Runtime
