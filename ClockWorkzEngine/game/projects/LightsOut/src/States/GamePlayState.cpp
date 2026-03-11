/******************************************************************************/
/*!
\file           GamePlayState.cpp
\project        Lights Out
\author(s)      [Lebon]
\brief          ECS-based gameplay state implementation.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "States/GamePlayState.hpp"
#include "States/MenuState.hpp"
#include "States/PauseState.hpp"

#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
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
                             Platform::WindowManager& window)
    : m_gsm(gsm), m_input(input), m_window(window) {}

void GamePlayState::onEnter() {
  auto& engine = Core::Engine::Get();
  m_renderer   = &engine.rendering();
  m_cam        = &engine.camera();

  // Init ECS world + register components + wire all game callbacks
  // (PlayerSystem, AISystem, particles, portal transitions, respawn, etc.)
  m_scene.Init();
  engine.invokeSceneSetup(m_scene);

  // Load the first level
  if (!m_scene.LoadFromFile(k_firstLevel)) {
    std::cerr << "[GamePlayState] Failed to load level: " << k_firstLevel << "\n";
  }

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
  if (m_scene.GetPendingLevel() == k_menuSentinel) {
    m_scene.ClearPendingLevel();
    auto& e = Core::Engine::Get();
    m_gsm.switchToAndClear(
        std::make_shared<MenuState>(m_gsm, m_input, e.window(), e.audio()));
    return;
  }

  // Normal gameplay update — scene handles level transitions (level_save →
  // level_save_02) automatically when a portal sets a non-sentinel levelName.
  m_scene.Update(dt, m_input);

  // ESC → push pause overlay (GamePlayState stays on the stack, frozen)
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
  // registered in main_game.cpp's setSceneSetup — no extra code needed here.
}

void GamePlayState::onExit() {
  m_scene.Clear();
}

} // namespace Runtime
