#include "States/PlayState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include <GL/glew.h>

namespace Runtime {

PlayState::PlayState(GameStateManager& gsm, Platform::InputManager& input,
                     Audio::AudioManager& audio, const std::string& scenePath)
    : m_gsm(gsm), m_input(input), m_audio(audio), m_scenePath(scenePath) {}

void PlayState::onEnter()
{
    // TODO: load level scene
    // m_scene.load(m_scenePath);
}

void PlayState::onExit() {}

void PlayState::update(double /*dt*/)
{
    // TODO: gameplay logic, pause/game-over checks
    // Push PauseState on ESC:   m_gsm.push(std::make_shared<PauseState>(...));
    // Switch to GameOverState:  m_gsm.switchTo(std::make_shared<GameOverState>(..., m_scenePath));
}

void PlayState::render(Core::Time* /*timer*/)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_scene
}

void PlayState::renderImGui(Core::Time* /*timer*/) {}

} // namespace Runtime
