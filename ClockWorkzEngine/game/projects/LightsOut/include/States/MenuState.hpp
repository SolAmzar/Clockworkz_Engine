/******************************************************************************/
/*!
\file           MenuState.hpp
\project        Lights Out
\author(s)      [Lebon], 70%, [Amirul Syafiq] 30%
\brief          Declares MenuState, the state representing the game's main menu.
                Responsibilities include:
                - Loading MainMenu.json scene (authored in level editor)
                - Handling keyboard and mouse input for menu navigation via UIButton components
                - Triggering state transitions (e.g., moving from menu to PlayState)
                - Scaling the view to any window size via camera zoom

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <memory>
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
} // namespace Core
namespace Graphics {
class RenderingManager;
class Camera2D;
} // namespace Graphics

namespace Runtime {

class MenuState final : public GameState {
public:
  MenuState(GameStateManager &gsm, Platform::InputManager &input,
            Platform::WindowManager &window, Audio::AudioManager &audio);

  const char *name() const override { return "MenuState"; }
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  void updateLayout(int w, int h);
  void handleAction(const std::string &action);
  void collectButtons();
  void applyHoverColors();
  void setOverlayVisible(const std::string &tag, bool visible);
  void setMenuButtonsVisible(bool visible);
  void openOptions();
  void closeOptions();
  void adjustOptionVolume(float delta);
  void setOptionVolume(int optionIndex, float value);
  void updateOptionsOverlayText();
  void renderOptionsOverlay();
  void renderVolumeBar(float centerX, float centerY, float width,
                       float height, float value, bool selected);

  GameStateManager         &m_gsm;
  Platform::InputManager   &m_input;
  Platform::WindowManager  &m_window;
  Audio::AudioManager      &m_audio;

  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D         *m_cam{nullptr};

  Runtime::Scene             m_menuScene;

  std::vector<ECS::Entity>   m_buttonEntities;
  int                        m_selected{0};
  int                        m_optionsSelected{0};
  bool                       m_showingHowToPlay{false};
  bool                       m_showingOptions{false};
  bool                       m_debug{false};

  int   m_bgmChannel{0};
  bool  m_bgmPaused{false};
};

} // namespace Runtime






