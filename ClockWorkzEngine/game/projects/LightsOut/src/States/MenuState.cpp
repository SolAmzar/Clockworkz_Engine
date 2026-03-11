/******************************************************************************/
/*!
\file           MenuState.cpp
\project        Lights Out
\author(s)      [Lebon], 70%, [Amirul Syafiq] 30%
\brief          Main menu state. Loads MainMenu.json scene from the level
                editor. Button entities are identified by UIButton components;
                positions, sizes, and colors are all controlled in the editor.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "States/MenuState.hpp"
#include "States/GamePlayState.hpp"
#include "Scripting/LuaManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "States/PlayState.hpp"
#include "States/SettingsState.hpp"
#include "States/SpriteEditorState.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>

using Assets::ResourceManager;
using Graphics::Shader;

namespace Runtime {

MenuState::MenuState(GameStateManager &gsm, Platform::InputManager &input,
                     Platform::WindowManager &window, Audio::AudioManager &audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void MenuState::onEnter() {
  m_selected         = 0;
  m_showingHowToPlay = false;
  m_buttonEntities.clear();

  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();
  m_cam      = &engine.camera();

  // Load fonts (kept for compatibility with any TextRenderer entities)
  Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");
  ResourceManager<Shader>::Load("text",    "assets/shaders/text.vert",    "assets/shaders/text.frag");
  ResourceManager<Shader>::Load("texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");

  // Start BGM
  m_bgmChannel = Assets::SoundManager::Play("menu_bgm");

  // Load all textures from config (required for the background sprite to resolve its textureId)
  Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");

  // Initialise and load the menu scene authored in the editor
  m_menuScene.Init();
  m_menuScene.LoadFromFile("assets/scenes/MainMenu.json");

  // Collect all UIButton entities (world.each preserves JSON insertion order)
  collectButtons();

  // Set camera for 1920x1080 design space, scaled to current window
  updateLayout(m_window.width(), m_window.height());

  // Resize camera when window changes
  engine.events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        if (e.width > 0 && e.height > 0)
          updateLayout(e.width, e.height);
      });
}

void MenuState::updateLayout(int w, int h) {
  // Keep aspect ratio of 1920x1080 design space (letterbox)
  float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
  m_cam->setViewport(w, h);
  m_cam->setPosition({0.f, 0.f});
  m_cam->setZoom(zoom);
  glViewport(0, 0, w, h);
}

void MenuState::collectButtons() {
  m_buttonEntities.clear();
  m_menuScene.GetWorld().each<Components::UIButton>(
      [this](ECS::Entity e, Components::UIButton &) {
        m_buttonEntities.push_back(e);
      });
}

void MenuState::applyHoverColors() {
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e   = m_buttonEntities[i];
    auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *sprite = m_menuScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    auto *text   = m_menuScene.GetWorld().getComponent<Components::TextRenderer>(e);
    if (!btn) continue;
    bool selected = (m_selected >= 0 && i == m_selected);
    glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
    if (text) text->color = c;                                         // tint text only
    if (sprite && !btn->showBackground) sprite->visible = false;      // hide background if flagged off
  }
}

void MenuState::handleButton(const Components::UIButton &btn) {
  if (!btn.uiScript.empty()) {
    Scripting::LuaManager &lua = Core::Engine::Get().lua();
    sol::state &ls = lua.GetState();
    ls["GSM"] = ls.create_table_with(
      "switch_to_gameplay", [&]() {
        m_gsm.switchTo(std::make_shared<GamePlayState>(m_gsm, m_input, m_window));
      },
      "push_settings", [&]() {
        m_gsm.push(std::make_shared<SettingsState>(m_gsm, m_input, m_window, m_audio));
      },
      "quit", [&]() {
        Core::Engine::Get().events().publish(Core::QuitGameEvent{});
      },
      "show_overlay", [&](const std::string &tag) {
        m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
          [&tag](ECS::Entity, Components::Tag &t, Components::SpriteRenderer &s) {
            if (t.tag == tag) s.visible = true;
          });
      },
      "hide_overlay", [&](const std::string &tag) {
        m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
          [&tag](ECS::Entity, Components::Tag &t, Components::SpriteRenderer &s) {
            if (t.tag == tag) s.visible = false;
          });
      }
    );
    if (lua.ExecuteScript("assets/scripts/" + btn.uiScript)) {
      lua.CallFunction("on_click");
    }
  }
}

void MenuState::update(double /*dt*/) {
  int w = m_window.width();
  int h = m_window.height();

  // Dismiss How-to-Play overlay
  if (m_showingHowToPlay) {
    if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) ||
        m_input.wasKeyPressed(GLFW_KEY_ENTER)  ||
        m_input.wasKeyPressed(GLFW_KEY_SPACE)  ||
        m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) {
      m_showingHowToPlay = false;
      m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
          [](ECS::Entity, Components::Tag &tag, Components::SpriteRenderer &sprite) {
            if (tag.tag == "howtoplay_overlay") sprite.visible = false;
          });
    }
    return;
  }

  if (m_bgmChannel != 0 && m_bgmPaused) {
    m_audio.setPaused(m_bgmChannel, false);
    m_bgmPaused = false;
  }

  // Mouse world-space position (vec2 is the engine's custom type)
  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  bool clicked = m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);
  bool mouseHover = false;

  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *tf  = m_menuScene.GetWorld().getComponent<Components::Transform>(e);
    if (!btn || !tf || !btn->interactable) continue;

    float cx = tf->position.x + btn->offset.x;
    float cy = tf->position.y + btn->offset.y;
    float halfW = btn->size.x * 0.5f;
    float halfH = btn->size.y * 0.5f;
    bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                    mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
    if (hovered) {
      mouseHover = true;
      if (m_selected != i) {
        m_selected = i;
        Assets::SoundManager::Play("ui_move");
      }
      if (clicked) {
        Assets::SoundManager::Play("ui_select");
        handleButton(*btn);
        return;
      }
    }
  }

  // Deselect when mouse is not over any button
  if (!mouseHover) m_selected = -1;

  // Keyboard navigation
  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
    Assets::SoundManager::Play("ui_move");
    if (--m_selected < 0) m_selected = (int)m_buttonEntities.size() - 1;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) {
    Assets::SoundManager::Play("ui_move");
    if (++m_selected >= (int)m_buttonEntities.size()) m_selected = 0;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) {
    if (!m_buttonEntities.empty()) {
      Assets::SoundManager::Play("ui_select");
      auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(
          m_buttonEntities[m_selected]);
      if (btn) { handleButton(*btn); return; }
    }
  }

  // Apply hover/normal colors every frame
  applyHoverColors();

#ifdef _DEBUG
  if (m_input.wasKeyPressed(GLFW_KEY_P)) {
    m_debug = !m_debug;
  }
  if (m_input.wasKeyTriggered(GLFW_KEY_M)) {
    m_gsm.push(std::make_shared<Runtime::SpriteEditorState>("SpriteEditor", m_input, m_gsm));
  }
#endif

  if (m_input.wasKeyTriggered(GLFW_KEY_V)) {
    m_bgmPaused = !m_bgmPaused;
    if (m_bgmChannel) m_audio.setPaused(m_bgmChannel, m_bgmPaused);
  }
}

void MenuState::render(Core::Time * /*timer*/) {
  int w = m_window.width();
  int h = m_window.height();
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glm::mat4 vp = m_cam->viewProj();

  m_menuScene.RenderAll(*m_renderer, vp);
}

void MenuState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  if (m_debug) {
    ImGui::Begin("MenuState Debug");
    ImGui::Text("Buttons loaded: %d", (int)m_buttonEntities.size());
    ImGui::Text("Selected: %d", m_selected);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
  }
#endif
}

void MenuState::onExit() {
  // Remove event subscription to prevent dangling callback
  Core::Engine::Get().events().unsubscribeOwner(this);
  if (m_bgmChannel > 0) m_audio.fadeOutChannel(m_bgmChannel, 0.5f);
  m_bgmChannel = 0;
}

} // namespace Runtime
