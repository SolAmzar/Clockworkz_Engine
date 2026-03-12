#include "States/GameOverState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

GameOverState::GameOverState(GameStateManager& gsm, Platform::InputManager& input,
                             const std::string& currentLevelPath)
    : m_gsm(gsm), m_input(input), m_currentLevelPath(currentLevelPath) {}

void GameOverState::onEnter()
{
    // TODO: load LoseScreen.json
    // m_loseScene.load("assets/scenes/LoseScreen.json");
    // collectButtons();
}

void GameOverState::onExit() {}

void GameOverState::update(double /*dt*/)
{
    // TODO: R = retry level, ESC/M = return to main menu
    // Retry:  m_gsm.switchTo(std::make_shared<PlayState>(..., m_currentLevelPath));
    // Menu:   m_gsm.switchToAndClear(std::make_shared<MenuState>(...));
}

void GameOverState::render(Core::Time* /*timer*/)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_loseScene
}

void GameOverState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) from lose scene
}

void GameOverState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void GameOverState::applyHoverColors()
{
    // TODO: tint selected button
}

} // namespace Runtime
