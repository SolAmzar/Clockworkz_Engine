#pragma once
#include <unordered_map>
namespace Platform {
	enum class Action { Quit, Up, Down, Left, Right };
	class InputManager {
	public:
		void attachCallbacks(void* glfwWindow);
		bool isDown(Action a) const;
		// internal callbacks:
		void onKey(int key, int scancode, int action, int mods);
	private:
		std::unordered_map<Action, bool> m_state;
	};
}
