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
  std::string m_audioSoundId{"intro_cutscene"};
  std::string m_audioPath{"assets/audio/IntroCutscene.wav"};
  int m_audioChannel{0};

  bool m_transitioning{false};
  bool m_videoReady{false};
};

} // namespace Runtime
