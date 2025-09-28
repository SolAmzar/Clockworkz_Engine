#include "Platform/WindowManager.hpp"
#include "Core/Config.hpp"
#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include <iostream>

using namespace Platform;

static void glfw_error_cb(int code, const char* desc) {
	std::cerr << "[GLFW] Error " << code << ": " << desc << "\n";
}

WindowManager::WindowManager() { }
WindowManager::~WindowManager() {
	if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
	glfwTerminate();
}

bool WindowManager::create(const Core::Config& cfg) {
	glfwSetErrorCallback(glfw_error_cb);
	if (!glfwInit()) return false;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	m_width = cfg.width; m_height = cfg.height;
	m_window = glfwCreateWindow(m_width, m_height, cfg.title.c_str(), nullptr, nullptr);
	if (!m_window) return false;
	glfwMakeContextCurrent(m_window);
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW\n";
		return false;
	}
	glfwSwapInterval(cfg.vsync ? 1 : 0);
	return true;
}

void WindowManager::poll() { glfwPollEvents(); }
void WindowManager::swap() { glfwSwapBuffers(m_window); }
bool WindowManager::shouldClose() const { return glfwWindowShouldClose(m_window); }
void WindowManager::requestClose() { glfwSetWindowShouldClose(m_window, 1); }
