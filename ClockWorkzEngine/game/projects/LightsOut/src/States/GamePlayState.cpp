/******************************************************************************/
/*!
\file           GamePlayState.cpp
\project        Lights Out
\author(s)      [Syafiq] 92% [Sol-Amzar] 8%
\brief          ECS-based gameplay state implementation.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "States/GamePlayState.hpp"
#include "States/MenuState.hpp"
#include "States/PauseState.hpp"
#include "States/GameOverState.hpp"

#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Core/Engine.hpp"
#include "GlobalAudioSettings.hpp"
#include "Runtime/LevelMusicManager.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

namespace Runtime {

GamePlayState::GamePlayState(GameStateManager& gsm,
                             Platform::InputManager& input,
                             Platform::WindowManager& window,
                             std::string startLevel)
    : m_gsm(gsm), m_input(input), m_window(window),
      m_startLevel(std::move(startLevel)) {}

void GamePlayState::onEnter() {
  auto& engine = Core::Engine::Get();
  m_renderer   = &engine.rendering();
  m_cam        = &engine.camera();

  // Ensure gameplay has the same text and texture resources available even if
  // we reach this state without relying on another state's one-time loads.
  Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");
  Assets::ResourceManager<Graphics::Shader>::Load(
      "text", "assets/shaders/text.vert", "assets/shaders/text.frag");
  Assets::ResourceManager<Graphics::Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
  Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");

  // Init ECS world + register components + wire all game callbacks
  // (PlayerSystem, AISystem, particles, portal transitions, respawn, etc.)
  m_scene.Init();
  engine.invokeSceneSetup(m_scene);

  // Load the starting level (default: k_firstLevel; overridden on retry)
  if (!m_scene.LoadFromFile(m_startLevel)) {
    std::cerr << "[GamePlayState] Failed to load level: " << m_startLevel << "\n";
  }
  GlobalAudioSettings::Apply(engine.audio());

  // Scale camera to 1920x1080 design space (letterbox)
  float zoom = std::min(static_cast<float>(m_window.width())  / 1920.f,
                        static_cast<float>(m_window.height()) / 1080.f);
  m_cam->setViewport(m_window.width(), m_window.height());
  m_cam->setZoom(zoom);
}

void GamePlayState::update(double dt) {
  // Intercept the "MAIN_MENU" sentinel set by a portal's levelName BEFORE
  // Scene::Update() can try to load it as a file next frame.
  // The portal's onUpdate callback (wired in main_game.cpp's setSceneSetup)
  // calls scene.SetPendingLevel("MAIN_MENU") at end of the previous frame;
  // we catch it at the top of this frame, clear it, and transition cleanly.
  // Death: Engine.triggerGameOver() was called from Lua — wait 2s then push lose screen
  if (Core::Engine::Get().isGameOverPending()) {
    m_deathTimer += static_cast<float>(dt);
    m_scene.Update(dt, m_input);   // keep scene alive so death animation plays
    if (m_deathTimer >= 2.0f) {
      Core::Engine::Get().clearGameOverPending();
      m_deathTimer = 0.0f;
      m_gsm.push(std::make_shared<GameOverState>(
          m_gsm, m_input, m_scene.GetCurrentLevelPath()));
    }
    return;  // block ESC/pause during death countdown
  }

  if (m_scene.GetPendingLevel() == k_menuSentinel) {
    m_scene.ClearPendingLevel();
    auto& e = Core::Engine::Get();
    m_gsm.switchToAndClear(
        std::make_shared<MenuState>(m_gsm, m_input, e.window(), e.audio()));
    return;
  }

  // Normal gameplay update ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â scene handles level transitions (level_save ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  // level_save_02) automatically when a portal sets a non-sentinel levelName.
  m_scene.Update(dt, m_input);

  // ESC ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ push pause overlay (GamePlayState stays on the stack, frozen)
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE))
    m_gsm.push(std::make_shared<PauseState>(m_gsm, m_input));
}

void GamePlayState::render(Core::Time* /*timer*/) {
  int w = m_window.width(), h = m_window.height();
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glm::mat4 vp = m_cam->viewProj();
  m_scene.RenderAll(*m_renderer, vp);
  // Particle emitter + light pass are handled by the onRender callback
  // registered in main_game.cpp's setSceneSetup ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no extra code needed here.
}

void GamePlayState::onExit() {
  Runtime::LevelMusicManager::Stop(Core::Engine::Get().audio());
  m_scene.Clear();
}

} // namespace Runtime



