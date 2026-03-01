/******************************************************************************/
/*!
\file           WindowManager.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares WindowManager, a wrapper around GLFW for creating
                                and managing the game window and OpenGL context.
                                Responsibilities include:
                                - Creating a window with version hints for
OpenGL 3.3 Core Profile
                                - Initializing GLEW for OpenGL function loading
                                - Providing poll() and swap() for event handling
and buffer swap
                                - Querying and requesting window close state
                                - Managing window dimensions and vsync
configuration

References:
- GLFW Window Guide: https://www.glfw.org/docs/latest/window_guide.html
- GLEW (OpenGL Extension Wrangler): http://glew.sourceforge.net/basic.html
- OpenGL Context Basics: https://www.khronos.org/opengl/wiki/OpenGL_Context

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <vector>
struct GLFWwindow;
namespace Core {
struct Config;
}
namespace Platform {
class WindowManager {
public:
  WindowManager();
  ~WindowManager();
  bool create(const Core::Config &cfg);
  void setTitle(const std::string &title);
  void poll();
  void swap();
  bool shouldClose() const;
  void requestClose();
  GLFWwindow *handle() const { return m_window; }
  int width() const { return m_width; }
  int height() const { return m_height; }
  GLFWwindow *getWindow() const { return m_window; }
  const std::vector<std::string> &droppedFiles() const {
    return m_droppedFiles;
  }
  std::vector<std::string> &droppedFilesMutable() { return m_droppedFiles; }
  void clearDroppedFiles() { m_droppedFiles.clear(); }
  std::vector<std::string> consumeDroppedFiles();
  bool hasFocus() const { return m_focused; }      // window is focused
  bool isMinimized() const { return m_iconified; } // window iconified

  // Optional hooks you can call from outside if you want
  void onFocusLost();
  void onFocusGained();
  void toggleFullscreen();
  void setFullscreen(bool enable);
  bool isFullscreen() const { return m_fullscreen; }

private:
  GLFWwindow *m_window = nullptr;
  bool m_fullscreen = false;
  // store windowed mode position before fullscreen
  int m_windowedX = 100;
  int m_windowedY = 100;
  int m_windowedWidth = 0;
  int m_windowedHeight = 0;
  int m_width = 0, m_height = 0;
  std::vector<std::string> m_droppedFiles;

  bool m_focused = true;
  bool m_iconified = false;
  static void GLFWFocusCallback(GLFWwindow *window, int focused);
  static void GLFWIconifyCallback(GLFWwindow *window, int iconified);
  static void GLFWResizeCallback(GLFWwindow *win, int w, int h);
};
} // namespace Platform
