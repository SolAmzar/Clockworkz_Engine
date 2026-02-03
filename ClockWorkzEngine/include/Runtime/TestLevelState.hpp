/******************************************************************************/
/*!
\file           TestLevelState.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          TestLevelState for playing Level_Save.json without editor.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"

namespace Platform {
class InputManager;
}

namespace Core {
class Time;
}

namespace Runtime {

class GameStateManager;

/**
 * @brief Playable state for Level_Save.json with no editor
 */
class TestLevelState final : public GameState {
public:
  explicit TestLevelState(Platform::InputManager &input, GameStateManager &gsm);
  ~TestLevelState();

  const char *name() const override { return "TestLevel"; }

  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  Platform::InputManager &m_input;
  GameStateManager &m_gsm;

  // Scene (Runtime Logic)
  Scene m_scene;

  // Render Torch Pass (copied from SandboxState logic)
  void renderTorchPass(Graphics::RenderingManager &renderer);

  bool m_showHUD{true};
  bool m_enableShadows{true};
  vec2 m_viewportSize{0.f, 0.f};

  // Portal / Level Loading
  // std::string m_pendingLevel;
  // void loadLevel(const std::string &levelName);
};

} // namespace Runtime
