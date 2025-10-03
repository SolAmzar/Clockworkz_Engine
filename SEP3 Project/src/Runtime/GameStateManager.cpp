#include "Runtime/GameStateManager.hpp"
#include "Runtime/GameState.hpp"

using namespace Runtime;

void GameStateManager::push(std::shared_ptr<GameState> s) {
	if (!s) return;
	m_states.push(std::move(s));
	m_states.top()->onEnter();
}

void GameStateManager::pop() {
	if (m_states.empty()) return;
	m_states.top()->onExit();
	m_states.pop();
}

void GameStateManager::switchTo(std::shared_ptr<GameState> s) {
	if (!s) return;
	if (!m_states.empty()) {
		m_states.top()->onExit();
		m_states.pop();
	}
	m_states.push(std::move(s));
	m_states.top()->onEnter();
}

void GameStateManager::update(double dt) {
	if (!m_states.empty()) m_states.top()->update(dt);
}

void GameStateManager::render() {
	if (!m_states.empty()) m_states.top()->render();
}
