/******************************************************************************/
/*!
\file           MenuState.cpp
\project        Dark In 2
\author(s)      [Syafiq] 100%
\brief          Implements the Dark In 2 MenuState game state.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "States/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

MenuState::MenuState(GameStateManager& gsm, Platform::InputManager& input,
                     Platform::WindowManager& window, Audio::AudioManager& audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void MenuState::onEnter()
{
    // TODO: load MainMenu.json and collect buttons
    // m_menuScene.load("assets/scenes/MainMenu.json");
    // collectButtons();
}

void MenuState::onExit() {}

void MenuState::update(double /*dt*/)
{
    // TODO: keyboard nav, button hover, state transitions
}

void MenuState::render(Core::Time* /*timer*/)
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_menuScene
}

void MenuState::renderImGui(Core::Time* /*timer*/) {}

void MenuState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) to fill m_buttonEntities
}

void MenuState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void MenuState::applyHoverColors()
{
    // TODO: tint selected button's SpriteRenderer
}

} // namespace Runtime
