/******************************************************************************/
/*!
\file           PauseState.cpp
\project        Lights Out
\author(s)      [Syafiq], 95%  [Lebon], 5%
\brief          Pause menu. Loads PauseMenu.json scene from the level editor
                and renders it as a semi-transparent overlay on top of the
                frozen game frame. Camera is saved on enter and restored on exit.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "States/PauseState.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "States/MenuState.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

using Assets::ResourceManager;
using Graphics::Shader;

namespace Runtime {

PauseState::PauseState(GameStateManager &gsm, Platform::InputManager &input)
    : m_gsm(gsm), m_input(input),
      m_audio(Core::Engine::Get().audio()) {}

void PauseState::onEnter() {
  m_selected = 0;
  m_buttonEntities.clear();
  std::cout << "[PauseState] Entered\n";

  Core::Engine &engine  = Core::Engine::Get();
  m_renderer = &engine.rendering();
  m_cam      = &engine.camera();

  // Save current game camera state so PlayState is unaffected on resume
  m_savedCamPos  = m_cam->position();
  m_savedCamZoom = m_cam->zoom();
  m_savedViewW   = engine.window().width();
  m_savedViewH   = engine.window().height();

  // Load font used by TextRenderer entities in the pause scene
  if (!ResourceManager<Assets::FontManager>::Get("PauseMenu")) {
    ResourceManager<Assets::FontManager>::Load("PauseMenu", "assets/fonts/MANIC1.TTF", 48);
  }
  ResourceManager<Shader>::Load("text", "assets/shaders/text.vert", "assets/shaders/text.frag");

  // Initialise and load the pause scene authored in the editor
  m_pauseScene.Init();
  m_pauseScene.LoadFromFile("assets/scenes/PauseMenu.json");

  collectButtons();

  // Set camera for 1920x1080 design space
  updateLayout(engine.window().width(), engine.window().height());

  // Resize camera when window changes
  engine.events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        if (e.width > 0 && e.height > 0)
          updateLayout(e.width, e.height);
      });

  m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeDuringPause);
  m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeDuringPause);

  std::cout << "[PauseState] Ready\n";
}

void PauseState::updateLayout(int w, int h) {
  float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
  m_cam->setViewport(w, h);
  m_cam->setPosition({0.f, 0.f});
  m_cam->setZoom(zoom);
  glViewport(0, 0, w, h);
}

void PauseState::collectButtons() {
  m_buttonEntities.clear();
  m_pauseScene.GetWorld().each<Components::UIButton>(
      [this](ECS::Entity e, Components::UIButton &) {
        m_buttonEntities.push_back(e);
      });
}

void PauseState::applyHoverColors() {
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e  = m_buttonEntities[i];
    auto *btn = m_pauseScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *sr  = m_pauseScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    auto *tr  = m_pauseScene.GetWorld().getComponent<Components::TextRenderer>(e);
    if (!btn) continue;
    bool selected = (i == m_selected);
    glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
    if (sr) sr->color = glm::vec4(c, 1.f);
    if (tr) tr->color = c;
  }
}

void PauseState::handleAction(const std::string &action) {
  if (action == "resume") {
    std::cout << "[PauseState] Resuming...\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.pop();
    return;
  }
  if (action == "main_menu") {
    std::cout << "[PauseState] Going to main menu...\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.switchToAndClear(
        std::make_shared<MenuState>(m_gsm, m_input,
                                    Core::Engine::Get().window(),
                                    Core::Engine::Get().audio()));
    return;
  }
  std::cout << "[PauseState] Unknown action: " << action << "\n";
}

void PauseState::update(double /*dt*/) {
  Core::Engine &engine = Core::Engine::Get();
  int w = engine.window().width();
  int h = engine.window().height();

  // ESC always resumes
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    std::cout << "[PauseState] ESC pressed - resuming\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.pop();
    return;
  }

  // Mouse world-space position (vec2 is the engine's custom type)
  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  bool clicked = m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);

  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_pauseScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *tf  = m_pauseScene.GetWorld().getComponent<Components::Transform>(e);
    auto *sr  = m_pauseScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    if (!btn || !tf || !sr || !btn->interactable) continue;

    float halfW = sr->size.x * 0.5f;
    float halfH = sr->size.y * 0.5f;
    bool hovered = (mouseWorld.x >= tf->position.x - halfW &&
                    mouseWorld.x <= tf->position.x + halfW &&
                    mouseWorld.y >= tf->position.y - halfH &&
                    mouseWorld.y <= tf->position.y + halfH);
    if (hovered) {
      if (m_selected != i) {
        m_selected = i;
        m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
      }
      if (clicked) {
        m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.f);
        handleAction(btn->action);
        return;
      }
    }
  }

  // Keyboard navigation
  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
    m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
    if (--m_selected < 0) m_selected = (int)m_buttonEntities.size() - 1;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) {
    m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
    if (++m_selected >= (int)m_buttonEntities.size()) m_selected = 0;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) {
    if (!m_buttonEntities.empty()) {
      m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.f);
      auto *btn = m_pauseScene.GetWorld().getComponent<Components::UIButton>(
          m_buttonEntities[m_selected]);
      if (btn) { handleAction(btn->action); return; }
    }
  }

  // Apply hover/normal colors every frame
  applyHoverColors();

#ifdef _DEBUG
  if (m_input.wasKeyPressed(GLFW_KEY_F1)) {
    m_debug = !m_debug;
  }
#endif
}

void PauseState::render(Core::Time * /*timer*/) {
  // NO glClear — framebuffer persistence keeps the frozen game frame visible behind the overlay
  glm::mat4 vp = m_cam->viewProj();

  m_renderer->beginScenePass(vp);
  m_pauseScene.Render(*m_renderer);
  m_renderer->endScenePass();

  m_renderer->beginFontPass(vp);
  m_textRenderSystem.update(m_pauseScene.GetWorld(), *m_renderer, vp);
  m_renderer->endFontPass();
}

void PauseState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  if (!m_debug) return;
  ImGui::Begin("Pause State Debug");
  ImGui::Text("Press ESC to Resume");
  ImGui::Text("Buttons: %d  Selected: %d", (int)m_buttonEntities.size(), m_selected);
  ImGui::End();
#endif
}

void PauseState::onExit() {
  // Remove event subscription to prevent dangling callback
  Core::Engine::Get().events().unsubscribeOwner(this);

  // Restore the game camera so PlayState resumes from where it was
  m_cam->setViewport(m_savedViewW, m_savedViewH);
  m_cam->setPosition(m_savedCamPos);
  m_cam->setZoom(m_savedCamZoom);
  glViewport(0, 0, m_savedViewW, m_savedViewH);

  std::cout << "[PauseState] Exiting\n";
}

} // namespace Runtime
