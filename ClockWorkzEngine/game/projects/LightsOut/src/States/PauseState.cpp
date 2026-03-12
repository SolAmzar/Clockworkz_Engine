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
#include "Scripting/LuaManager.hpp"
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
#include "GlobalAudioSettings.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "States/MenuState.hpp"
#include "States/OptionsState.hpp"
#include "States/SettingsState.hpp"
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

  GlobalAudioSettings::Load();
  m_bgmVolumeBeforePause = GlobalAudioSettings::GetBGMVolume();
  m_sfxVolumeBeforePause = GlobalAudioSettings::GetSFXVolume();
  m_bgmVolumeDuringPause = m_bgmVolumeBeforePause * 0.4f;
  m_sfxVolumeDuringPause = m_sfxVolumeBeforePause * 0.4f;
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

  std::sort(m_buttonEntities.begin(), m_buttonEntities.end(),
            [this](ECS::Entity lhs, ECS::Entity rhs) {
              const auto *lhsTransform =
                  m_pauseScene.GetWorld().getComponent<Components::Transform>(lhs);
              const auto *rhsTransform =
                  m_pauseScene.GetWorld().getComponent<Components::Transform>(rhs);

              const float lhsY = lhsTransform ? lhsTransform->position.y : 0.0f;
              const float rhsY = rhsTransform ? rhsTransform->position.y : 0.0f;
              return lhsY > rhsY;
            });
}

void PauseState::applyHoverColors() {
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e      = m_buttonEntities[i];
    auto *btn    = m_pauseScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *sprite = m_pauseScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    auto *text   = m_pauseScene.GetWorld().getComponent<Components::TextRenderer>(e);
    if (!btn) continue;
    bool selected = (m_selected >= 0 && i == m_selected);
    glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
    if (text) text->color = c;
    if (sprite && !btn->showBackground) sprite->visible = false;
  }
}

void PauseState::handleButton(const Components::UIButton &btn) {
  if (!btn.uiScript.empty()) {
    Scripting::LuaManager &lua = Core::Engine::Get().lua();
    sol::state &ls = lua.GetState();
    ls["GSM"] = ls.create_table_with(
      "pop", [&]() {
        GlobalAudioSettings::Apply(m_audio);
        m_gsm.pop();
      },
      "switch_to_menu", [&]() {
        GlobalAudioSettings::Apply(m_audio);
        m_gsm.switchToAndClear(std::make_shared<MenuState>(
          m_gsm, m_input,
          Core::Engine::Get().window(),
          Core::Engine::Get().audio()));
      },
      "push_settings", [&]() {
           std::cout << "[PauseState] Opening options...\n";
          GlobalAudioSettings::Apply(m_audio);
        m_gsm.push(std::make_shared<SettingsState>(
          m_gsm, m_input, Core::Engine::Get().window(), m_audio));
      }
    );
    if (lua.ExecuteScript("assets/scripts/" + btn.uiScript)) {
      lua.CallFunction("on_click");
    }
  }
}

void PauseState::update(double /*dt*/) {
  Core::Engine &engine = Core::Engine::Get();
  int w = engine.window().width();
  int h = engine.window().height();

  // ESC always resumes
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    std::cout << "[PauseState] ESC pressed - resuming\n";
    GlobalAudioSettings::Apply(m_audio);
    m_gsm.pop();
    return;
  }

  // Mouse world-space position (vec2 is the engine's custom type)
  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  bool clicked    = m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);
  bool mouseHover = false;

  int hoveredIndex = -1;
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_pauseScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *tf  = m_pauseScene.GetWorld().getComponent<Components::Transform>(e);
    if (!btn || !tf || !btn->interactable) continue;

    float cx = tf->position.x + btn->offset.x;
    float cy = tf->position.y + btn->offset.y;
    float halfW = btn->size.x * 0.5f;
    float halfH = btn->size.y * 0.5f;
    bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                    mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
    if (hovered) {
      hoveredIndex = i;
      break;
    }
  }

  if (hoveredIndex >= 0) {
    if (m_selected != hoveredIndex) {
      m_selected = hoveredIndex;
      m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
    }
    if (clicked) {
      m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.f);
      auto *btn = m_pauseScene.GetWorld().getComponent<Components::UIButton>(
          m_buttonEntities[hoveredIndex]);
      if (btn) {
        handleButton(*btn);
        return;
      }
    }
  }

  // Deselect when mouse is not over any button
  mouseHover = (hoveredIndex >= 0);
  if (!mouseHover) m_selected = -1;

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
      if (btn) { handleButton(*btn); return; }
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
  // NO glClear - framebuffer persistence keeps the frozen game frame visible behind the overlay
  glm::mat4 vp = m_cam->viewProj();

  m_pauseScene.RenderAll(*m_renderer, vp);
  m_pauseScene.RenderText(*m_renderer);
}

void PauseState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  if (m_debug) {
    ImGui::Begin("Pause State Debug");
    ImGui::Text("Press ESC to Resume");
    ImGui::Text("Buttons: %d  Selected: %d", (int)m_buttonEntities.size(), m_selected);
    ImGui::End();
  }
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





