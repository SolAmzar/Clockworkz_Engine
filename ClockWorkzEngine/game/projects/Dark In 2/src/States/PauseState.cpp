#include "States/PauseState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

PauseState::PauseState(GameStateManager& gsm, Platform::InputManager& input)
    : m_gsm(gsm), m_input(input) {}

void PauseState::onEnter()
{
    // TODO: save game camera, load PauseMenu.json
    // m_pauseScene.load("assets/scenes/PauseMenu.json");
    // collectButtons();
}

void PauseState::onExit()
{
    // TODO: restore game camera
}

void PauseState::update(double /*dt*/)
{
    // TODO: ESC resumes (m_gsm.pop()), button nav
}

void PauseState::render(Core::Time* /*timer*/)
{
    // No glClear — frozen game frame bleeds through as background
    // TODO: render m_pauseScene on top
}

void PauseState::renderImGui(Core::Time* /*timer*/) {}

void PauseState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) from pause scene
}

void PauseState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void PauseState::applyHoverColors()
{
    // TODO: tint selected button
}

} // namespace Runtime
