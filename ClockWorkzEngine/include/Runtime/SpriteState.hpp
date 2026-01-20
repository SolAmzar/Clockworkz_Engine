#if 0
/******************************************************************************/
/*!
\file           SpriteState.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares PlayState, the main gameplay loop state.
                                Responsibilities include:
                                - Loading and managing the current Scene and its
GameObjects
                                - Updating gameplay logic such as movement,
collisions, and AI
                                - Rendering the game world each frame
                                - Transitioning back to other states (e.g.,
pause or menu)

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
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

namespace Graphics {
class RenderingManager;
}

namespace Runtime {

class SpriteState final : public GameState {
public:
  explicit SpriteState(std::string initialSceneId,
                       Platform::InputManager &input, GameStateManager &gsm);

  const char *name() const override;
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  bool m_shouldReturnToMenu{false};
  void rebindLightsAndPlayerAfterReload();
  void InitPlayerSpriteUVs();
  std::vector<std::string> GetTextureList(const std::string &folder);
  void renderImGui(Core::Time *timer) override; // Imgui for playstate

private:
  std::string m_initialSceneId;
  SceneManager m_scenes; // your existing scene manager
  Platform::InputManager &m_input;
  GameStateManager &m_gsm;
  bool m_loaded{false};
  bool m_debug{false};
  Graphics::RenderingManager *m_renderer{nullptr};
  void resetScene();
  ECS::Entity m_spriteEntity{};

  // IMGUI Variables
  std::string m_selectedTexture;
  std::string m_lastSelectedAnimation;
  std::string m_selectedAnimation;
  bool m_playLoop = true;
  bool m_playing = false;
  int m_selectedFrame = -1;

  // Grid generation helper
  int m_sheetWidth = 512;
  int m_sheetHeight = 512;
  int m_cellWidth = 64;
  int m_cellHeight = 64;
  int m_startFrame = 0;
  int m_endFrame = 0;
  char m_newAnimName[64] = "new_anim";
};

} // namespace Runtime
#endif
