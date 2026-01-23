/******************************************************************************/
/*!
\file           SpriteEditorState.hpp
\project        Lights Out
\author(s)      [User/Refactor], 100%
\brief          Declares SpriteEditorState, a clean replacement for SpriteState.
                Designed to handle sprite and animation editing tasks within the
ECS framwork.
*/
/******************************************************************************/

#pragma once

#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneManager.hpp"
#include <memory>
#include <string>
#include <vector>


namespace Graphics {
class RenderingManager;
}

namespace Runtime {

class SpriteEditorState final : public GameState {
public:
  explicit SpriteEditorState(std::string initialSceneId,
                             Platform::InputManager &input,
                             GameStateManager &gsm);
  ~SpriteEditorState() override = default;

  const char *name() const override { return "SpriteEditorState"; }

  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  std::string m_initialSceneId;
  Platform::InputManager &m_input;
  GameStateManager &m_gsm;

  // Core members
  SceneManager m_scenes;
  Graphics::RenderingManager *m_renderer{nullptr};
  bool m_loaded{false};
  bool m_shouldReturnToMenu{false};
  bool m_debug{true};

  // Editor Entity
  ECS::Entity m_spriteEntity{};

  // ImGui / Editor State
  std::string m_selectedTexture;
  std::string m_lastSelectedAnimation;
  std::string m_selectedAnimation;
  bool m_playLoop{true};
  bool m_playing{false};
  int m_selectedFrame{-1};

  // Grid generation helper vars
  int m_sheetWidth{512};
  int m_sheetHeight{512};
  int m_cellWidth{64};
  int m_cellHeight{64};
  int m_startFrame{0};
  int m_endFrame{0};
  char m_newAnimName[64] = "new_anim";

  // Helper functionality
  // std::vector<std::string> GetTextureList(const std::string& folder); // Can
  // use TextureManager directly?
};

} // namespace Runtime
