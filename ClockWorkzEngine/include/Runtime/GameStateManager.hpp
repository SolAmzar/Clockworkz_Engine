#pragma once
#include <memory>
#include <stack>

namespace Runtime {

	class GameState;

	class GameStateManager {
	public:
		void push(std::shared_ptr<GameState> s);
		void pop();
		void switchTo(std::shared_ptr<GameState> s);

		void update(double dt);
		void render();

		std::shared_ptr<GameState> top() const { return m_states.empty() ? nullptr : m_states.top(); }

	private:
		std::stack<std::shared_ptr<GameState>> m_states;
	};

} // namespace Runtime
