/******************************************************************************/
/*!
\file           WindowManager.hpp
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

#pragma once
#include <string>
struct GLFWwindow;
namespace Core { struct Config; }
namespace Platform {
	class WindowManager {
	public:
		WindowManager(); ~WindowManager();
		bool create(const Core::Config& cfg);
		void setTitle(const std::string& title);
		void poll();
		void swap();
		bool shouldClose() const;
		void requestClose();
		GLFWwindow* handle() const { return m_window; }
		int width() const { return m_width; }
		int height() const { return m_height; }
	private:
		GLFWwindow* m_window = nullptr;
		int m_width = 0, m_height = 0;
	};
}
