#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include <GLFW/glfw3.h>

using namespace Platform;
static InputManager* s_active = nullptr;

static void key_cb(GLFWwindow*, int key, int scancode, int action, int mods) {
    if (s_active) s_active->onKey(key, scancode, action, mods);
}

void InputManager::attachCallbacks(void* glfwWindow) {
    s_active = this;
    glfwSetKeyCallback(static_cast<GLFWwindow*>(glfwWindow), key_cb);
    m_state[Action::Quit] = false;
}

bool InputManager::isDown(Action a) const {
    auto it = m_state.find(a); return it != m_state.end() && it->second;
}

void InputManager::onKey(int key, int, int action, int) {
    // Map ESC -> Quit
    if (key == GLFW_KEY_ESCAPE) {
        m_state[Action::Quit] = (action != GLFW_RELEASE);
    }
}
