/******************************************************************************/
/*!
\file           GameStateManager.hpp
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

#pragma once
#include <memory>
#include <stack>

namespace Core {
	class Logger; class Time;
}

namespace Runtime {
	// Gamestate is forward declared to avoid circular dependency
	class GameState;
	class GameStateManager {
	public:
		void push(std::shared_ptr<GameState> s);
		void pop();
		void switchTo(std::shared_ptr<GameState> s);

		void update(double dt);
		void render(Core::Time* timer);

		std::shared_ptr<GameState> top() const { return m_states.empty() ? nullptr : m_states.top(); }

	private:
		void processPendingOps();

		std::stack<std::shared_ptr<GameState>> m_states;

		//deffered op
		enum class Operation { None, Pop, Switch };
		Operation m_pendingOp{ Operation::None };
		std::shared_ptr<GameState> m_pendingState{ nullptr };
		int m_popCount{ 0 };
	};

} // namespace Runtime
