/******************************************************************************/
/*!
\file           WindowManager.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares WindowManager, a wrapper around GLFW for creating
				and managing the game window and OpenGL context.
				Responsibilities include:
				- Creating a window with version hints for OpenGL 3.3 Core Profile
				- Initializing GLEW for OpenGL function loading
				- Providing poll() and swap() for event handling and buffer swap
				- Querying and requesting window close state
				- Managing window dimensions and vsync configuration

References:
- GLFW Window Guide: https://www.glfw.org/docs/latest/window_guide.html
- GLEW (OpenGL Extension Wrangler): http://glew.sourceforge.net/basic.html
- OpenGL Context Basics: https://www.khronos.org/opengl/wiki/OpenGL_Context

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

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

static void GLFWDropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* wm = static_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if (!wm) return;
	auto& files = wm->droppedFilesMutable();
	std::cout << "[Drop] Got " << count << " file(s):\n";
    for (int i = 0; i < count; ++i) {
		std::cout << "  " << paths[i] << "\n";
        files.emplace_back(paths[i]);
    }
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

	glfwSetWindowUserPointer(m_window, this);
	glfwSetDropCallback(m_window, GLFWDropCallback);;

	if (!m_window) return false;
	glfwMakeContextCurrent(m_window);
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW\n";
		return false;
	}
	glfwSwapInterval(cfg.vsync ? 1 : 0);
	return true;
}

std::vector<std::string> WindowManager::consumeDroppedFiles() {
	std::vector<std::string> out;
	out.swap(m_droppedFiles); // clear internal list
	return out;
}

void WindowManager::setTitle(const std::string& title) {
	if (m_window) glfwSetWindowTitle(m_window, title.c_str());
}

void WindowManager::poll() { glfwPollEvents(); }
void WindowManager::swap() { glfwSwapBuffers(m_window); }
bool WindowManager::shouldClose() const { return glfwWindowShouldClose(m_window); }
void WindowManager::requestClose() { glfwSetWindowShouldClose(m_window, 1); }
