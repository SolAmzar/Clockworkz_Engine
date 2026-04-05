/******************************************************************************/
/*!
\file           IntroCutsceneState.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the startup game state that plays the intro cutscene
                before the main menu.

                Owns the temporary scene, skip handling, video playback system,
                and cutscene-to-menu transition logic.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "ECS/World.hpp"
#include "Runtime/GameState.hpp"
#include "Systems/VideoPlaybackSystem.hpp"
#include <string>

namespace Platform {
class InputManager;
class WindowManager;
} // namespace Platform

namespace Audio {
class AudioManager;
} // namespace Audio

namespace Core {
class Time;
} // namespace Core

namespace Graphics {
class Camera2D;
class RenderingManager;
} // namespace Graphics

namespace Runtime {

class GameStateManager;

class IntroCutsceneState final : public GameState {
public:
  IntroCutsceneState(GameStateManager &gsm, Platform::InputManager &input,
                     Platform::WindowManager &window, Audio::AudioManager &audio);

  const char *name() const override { return "IntroCutsceneState"; }
  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  void updateLayout(int w, int h);
  void transitionToMenu();
  bool shouldSkip() const;

  GameStateManager &m_gsm;
  Platform::InputManager &m_input;
  Platform::WindowManager &m_window;
  Audio::AudioManager &m_audio;

  Graphics::RenderingManager *m_renderer{nullptr};
  Graphics::Camera2D *m_cam{nullptr};

  ECS::World m_world;
  Systems::VideoPlaybackSystem m_videoSystem;
  ECS::Entity m_videoEntity{};
  std::string m_videoPath{"assets/videos/IntroCutscene.mov"};

  bool m_transitioning{false};
  bool m_videoReady{false};
};

} // namespace Runtime
