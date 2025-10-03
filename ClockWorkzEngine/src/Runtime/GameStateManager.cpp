/******************************************************************************/
/*!
\file           GameStateManager.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares GameStateManager, a stack-based controller for
				managing active GameStates.
				Responsibilities include:
				- Pushing and popping states dynamically
				- Updating and rendering the top-most state
				- Supporting smooth transitions between different states
				- Ensuring proper cleanup when states exit

Reference: GIT

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Runtime/GameStateManager.hpp"
#include "Runtime/GameState.hpp"
#include <Core/Time.hpp>

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

void GameStateManager::render(Core::Time* timer) {
	if (!m_states.empty()) {
		m_states.top()->render(timer);
	}
}
