#include "States/IntroCutsceneState.hpp"

#include "Audio/AudioManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/VideoPlayer.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "States/MenuState.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

namespace Runtime {

using Assets::ResourceManager;
using Graphics::Shader;

IntroCutsceneState::IntroCutsceneState(GameStateManager &gsm,
                                       Platform::InputManager &input,
                                       Platform::WindowManager &window,
                                       Audio::AudioManager &audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void IntroCutsceneState::onEnter() {
  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();
  m_cam = &engine.camera();

  ResourceManager<Shader>::Load("texture", "assets/shaders/texture.vert",
                                "assets/shaders/texture.frag");

  m_world.registerComponent<Components::Transform>();
  m_world.registerComponent<Components::SpriteRenderer>();
  m_world.registerComponent<Components::VideoPlayer>();

  m_videoEntity = m_world.createEntity();

  Components::Transform transform;
  Components::SpriteRenderer sprite;
  sprite.visible = true;
  sprite.color = glm::vec4(1.f);

  Components::VideoPlayer player;
  player.filepath = m_videoPath;
  player.autoplay = true;
  player.isPlaying = true;
  player.loop = false;
  player.targetMode = Components::VideoTargetMode::SpriteRenderer;

  m_world.addComponent(m_videoEntity, transform);
  m_world.addComponent(m_videoEntity, sprite);
  m_world.addComponent(m_videoEntity, player);

  updateLayout(m_window.width(), m_window.height());

  engine.events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        if (e.width > 0 && e.height > 0) {
          updateLayout(e.width, e.height);
        }
      });

  if (!std::filesystem::exists(m_videoPath)) {
    std::cerr << "[IntroCutsceneState] Missing intro video: " << m_videoPath
              << "\n";
    transitionToMenu();
    return;
  }

  if (std::filesystem::exists(m_audioPath)) {
    if (!Assets::SoundManager::IsLoaded(m_audioSoundId)) {
      Assets::SoundManager::LoadSound(m_audioSoundId, m_audioPath, false, true,
                                      false);
      Audio::AudioManager::SoundProfile profile;
      profile.group = Audio::Group::BGM;
      profile.volume = 1.0f;
      m_audio.registerSoundProfile(m_audioSoundId, profile);
    }
    m_audioChannel = Assets::SoundManager::Play(m_audioSoundId);
  }

  m_videoReady = true;
}

void IntroCutsceneState::onExit() {
  Core::Engine::Get().events().unsubscribeOwner(this);
  if (m_audioChannel > 0) {
    m_audio.stop(m_audioChannel);
    m_audioChannel = 0;
  } else {
    m_audio.stop(m_audioSoundId);
  }
  m_videoSystem.cleanup(m_world);
  m_world.destroyAll();
}

void IntroCutsceneState::update(double /*dt*/) {
  if (m_transitioning || !m_videoReady) {
    return;
  }

  if (shouldSkip()) {
    transitionToMenu();
    return;
  }

  if (auto *player = m_world.getComponent<Components::VideoPlayer>(m_videoEntity)) {
    if (player->playbackState == "Finished" || player->playbackState == "Error") {
      transitionToMenu();
    }
  }
}

void IntroCutsceneState::render(Core::Time * /*timer*/) {
  int w = m_window.width();
  int h = m_window.height();
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  m_renderer->beginScenePass(m_cam->viewProj());
  if (!m_transitioning && m_videoReady) {
    m_videoSystem.update(m_world, *m_renderer);
  }
  m_renderer->endScenePass();
}

void IntroCutsceneState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  ImGui::Begin("Intro Cutscene");
  ImGui::Text("Press Enter, Space, Escape, or click to skip.");
  if (auto *player = m_world.getComponent<Components::VideoPlayer>(m_videoEntity)) {
    ImGui::Text("Playback: %s", player->playbackState.c_str());
    ImGui::Text("Time: %.2f / %.2f", player->currentTime, player->duration);
  }
  ImGui::End();
#endif
}

void IntroCutsceneState::updateLayout(int w, int h) {
  m_cam->setViewport(w, h);
  m_cam->setPosition({0.f, 0.f});
  m_cam->setZoom(1.0f);

  if (auto *sprite = m_world.getComponent<Components::SpriteRenderer>(m_videoEntity)) {
    sprite->size = vec2(static_cast<float>(w), static_cast<float>(h));
  }
}

void IntroCutsceneState::transitionToMenu() {
  if (m_transitioning) {
    return;
  }

  m_transitioning = true;
  m_gsm.switchTo(
      std::make_shared<MenuState>(m_gsm, m_input, m_window, m_audio));
}

bool IntroCutsceneState::shouldSkip() const {
  return m_input.wasKeyPressed(GLFW_KEY_ESCAPE) ||
         m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
         m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) ||
         m_input.wasKeyPressed(GLFW_KEY_SPACE) ||
         m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);
}

} // namespace Runtime
