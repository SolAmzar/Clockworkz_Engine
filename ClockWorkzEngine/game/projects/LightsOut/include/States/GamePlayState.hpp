/******************************************************************************/
/*!
\file           GamePlayState.hpp
\project        Lights Out
\author(s)      [Lebon]
\brief          ECS-based gameplay state. Loads level_save.json via the engine's
                ECS Scene system, wires all game callbacks via
invokeSceneSetup(), handles ESC→PauseState and portal-based level transitions.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include <string>

namespace Platform {
class InputManager;
class WindowManager;
} // namespace Platform
namespace Runtime {
class GameStateManager;
}
namespace Core {
class Time;
}
namespace Graphics {
class RenderingManager;
class Camera2D;
} // namespace Graphics

namespace Runtime {

class GamePlayState final : public GameState {
public:
  GamePlayState(GameStateManager &gsm, Platform::InputManager &input,
                Platform::WindowManager &window,
                std::string startLevel = std::string(k_firstLevel));
  ~GamePlayState() = default;

  const char *name() const override { return "GamePlayState"; }
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time * /*timer*/) override {}

private:
  GameStateManager &m_gsm;
  Platform::InputManager &m_input;
  Platform::WindowManager &m_window;
  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D *m_cam{nullptr};

  Runtime::Scene m_scene;
  std::string m_startLevel; // level loaded on enter (default: k_firstLevel)
  void  renderFragmentUI();
  float m_deathTimer{0.0f}; // counts up after Engine.triggerGameOver()
  std::string m_fragmentIconTexture{"fragment_texture"};

  // First level loaded on enter
  static constexpr const char *k_firstLevel = "assets/scenes/Level_Save.json";
  // Sentinel value used in a Portal's levelName to signal "return to main menu"
  static constexpr const char *k_menuSentinel = "MAIN_MENU";
};

} // namespace Runtime
