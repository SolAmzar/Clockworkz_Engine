/******************************************************************************/
/*!
\file           PauseState.cpp
\project        Lights Out
\author(s)      [Syafiq], 95%  [Lebon], 5%
\brief          Basic pause menu implementation

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/PauseState.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/MenuState.hpp"
#include "Runtime/PlayState.hpp"
#include <Assets/TextureManager.hpp>
#include <GLFW/glfw3.h>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>


using Assets::ResourceManager;
using Graphics::Shader;
using namespace Gameplay;

namespace Runtime {

PauseState::PauseState(GameStateManager &gsm, Platform::InputManager &input)
    : m_gsm(gsm), m_input(input),
      m_background(GameObjectType::Unknown, "PauseBG",
                   GameObjectShape::Rectangle),
      m_audio(Core::Engine::Get().audio()) {}

void PauseState::onEnter() {
  m_selected = 0;
  std::cout << "[PauseState] Entered\n";

  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();
  m_cam = &engine.camera();

  m_background.addComponent<SpriteRendererComponent>("pause_bg_texture");
  m_background.setSize({1.f, 1.f});                    // Proper dimensions
  m_background.transform().scale = {1920.0f, 1080.0f}; // Keep scale neutral

  m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeDuringPause);
  m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeDuringPause);

  std::cout << "[PauseState] Ready\n";
}

void PauseState::moveUp() {
  m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
  if (--m_selected < 0)
    m_selected = static_cast<int>(m_items.size()) - 1;
}

void PauseState::moveDown() {
  m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
  if (++m_selected >= static_cast<int>(m_items.size()))
    m_selected = 0;
}

void PauseState::select() {
  const std::string &choice = m_items[m_selected];
  m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.f);

  if (choice == "Resume Game") {
    std::cout << "[PauseState] Resuming...\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.pop();
    return;
  }

  if (choice == "Main Menu") {
    std::cout << "[PauseState] Exiting...\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.switchToAndClear(
        std::make_shared<MenuState>(m_gsm, m_input,
                                    Core::Engine::Get().window(),
                                    Core::Engine::Get().audio()));
  }

  std::cout << "[PauseState] Selected: " << choice << "\n";
}

void PauseState::update(double dt) {
  (void)dt;
  // Get window reference for mouse position
  Core::Engine &engine = Core::Engine::Get();
  Platform::WindowManager &window = engine.window();

  // Mouse hover detection
  double mouseX, mouseY;
  glfwGetCursorPos(window.getWindow(), &mouseX, &mouseY);

  int w = window.width();
  int h = window.height();
  float glMouseY =
      h - static_cast<float>(mouseY); // Convert to OpenGL coordinates

  // Check if mouse is hovering over any menu item
  bool isHover = false;
  float startY = h / 2.0f + 80.0f; // Same as render function

  for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
    float scale = 0.55f;
    float yPos = startY - (i * 55.0f); // Same as render function
    float textWidth = m_items[i].length() * 48.0f * scale *
                      0.55f; // Same calculation as render
    float xPos = (w / 2.0f) - (textWidth / 2.0f);

    float textHeight =
        48.0f * scale; // Approximate height based on font size and scale

    // Check if mouse is within this menu item's bounds
    if (mouseX >= xPos && mouseX <= (xPos + textWidth) && glMouseY >= yPos &&
        glMouseY <= (yPos + textHeight)) {
      isHover = true;
      if (m_selected != i) {
        m_selected = i;
        m_audio.playOneShot("ui_move", Audio::Group::SFX, m_sfx);
        // Optional: Add sound effect here if you have audio manager
        // m_audio.playOneShot("ui_move", Audio::Group::SFX, 0.9f);
      }
      break;
    }
  }

  // Mouse click to select
  if (isHover && m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) {
    // Optional: Add sound effect here
    // m_audio.playOneShot("ui_select", Audio::Group::SFX, 1.f);

    select();
  }

  // ESC to resume
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    std::cout << "[PauseState] ESC pressed - resuming\n";
    m_audio.setGroupVolume(Audio::Group::BGM, m_bgmVolumeBeforePause);
    m_audio.setGroupVolume(Audio::Group::SFX, m_sfxVolumeBeforePause);
    m_gsm.pop();
    return;
  }

  // Keyboard navigation
  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
    moveUp();
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) ||
      m_input.wasKeyPressed(GLFW_KEY_S)) {
    moveDown();
  }

  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
      m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) {

    select();
  }
#ifdef _DEBUG
  if (m_input.wasKeyPressed(GLFW_KEY_F1)) {
    m_debug = !m_debug;
  }
#endif
}

void PauseState::render(Core::Time *timer) {
  Core::Engine &engine = Core::Engine::Get();
  Platform::WindowManager &window = engine.window();
  Graphics::RenderingManager &renderer = engine.rendering();

  int w = window.width();
  int h = window.height();

  //  UI orthographic projection
  glm::mat4 uiProjection =
      glm::ortho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h));

  m_renderer->beginScenePass(uiProjection);

  // Center it on screen, size it to fill the window
  glm::vec2 bgPos(w * 0.5f, h * 0.5f); // Center of screen
  glm::vec2 bgSize(static_cast<float>(w),
                   static_cast<float>(h)); // Full window size

  // Get the texture ID for your pause background
  unsigned int bgTexId = Assets::TextureManager::Get("pause_bg_texture")->id();

  m_renderer->drawSprite(
      bgTexId, bgPos, bgSize,
      0.0f,                              // rotation
      glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // full UV rect
      glm::vec4(1.0f, 1.0f, 1.0f, 0.7f) // white tint, 70% opacity so text shows
  );

  m_renderer->endScenePass();

  glm::mat4 projection = glm::ortho(0.0f, (float)w, 0.0f, (float)h);
  m_renderer->beginFontPass(projection);

  auto shader = ResourceManager<Shader>::Get("text");
  if (!shader) {
    m_renderer->endFontPass();
    return;
  }
  auto pauseFont = ResourceManager<Assets::FontManager>::Get("PauseMenu");
  if (!pauseFont) {
    Assets::ResourceManager<Assets::FontManager>::Load(
        "PauseMenu", "assets/fonts/MANIC1.TTF", 48);
    pauseFont = ResourceManager<Assets::FontManager>::Get("PauseMenu");
  }

  // Colors matching the scribble/sticky note aesthetic
  glm::vec3 normalCol(0.1f, 0.1f, 0.1f);   // White
  glm::vec3 selectedCol(1.0f, 1.0f, 1.0f); // Black

  if (pauseFont) {
    // "PAUSED" title - brown color to match scribbles, smaller and centered
    std::string title = "PAUSED";
    float titleScale = 1.0f;
    float titleWidth =
        title.length() * 48.0f * titleScale * 0.55f; // Better estimation
    float titleX = (w / 2.0f) - (titleWidth / 2.0f) - 10.0f;
    pauseFont->RenderText(title, titleX, h - 150.0f, titleScale,
                          glm::vec3(0.5f, 0.0f, 0.0f), *shader);
  }

  // Render menu options - smaller, centered, matching aesthetic
  float startY = h / 2.0f + 80.0f;

  if (pauseFont) {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
      const std::string &item = m_items[i];
      glm::vec3 color = (i == m_selected) ? selectedCol : normalCol;

      float scale = 0.55f; // Smaller text
      float yPos = startY - (i * 55.0f);
      float textWidth =
          item.length() * 48.0f * scale * 0.55f; // Better width estimation
      float xPos = (w / 2.0f) - (textWidth / 2.0f);

      pauseFont->RenderText(item, xPos, yPos, scale, color, *shader);
    }
  }

  renderer.endFontPass();

  (void)timer;
}

void PauseState::renderImGui(Core::Time *timer) {
#ifdef _DEBUG
  if (!m_debug)
    return;

  ImGui::Begin("Pause State Debug");
  ImGui::Text("Press ESC to Resume");
  ImGui::Text("Selected: %d - %s", m_selected, m_items[m_selected].c_str());
  ImGui::End();

  (void)timer;
#endif
}

void PauseState::onExit() {
  std::cout << "[PauseState] Exiting\n";
  std::cout << "[PauseState] Exit complete\n";
}

} // namespace Runtime
