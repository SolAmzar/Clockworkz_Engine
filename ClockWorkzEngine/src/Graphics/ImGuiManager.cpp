
/******************************************************************************/
/*!
\file           ImGuiManager.cpp
\project        Lights Out
\author(s)      [Syafiq] 100%
\brief          Define ImGuiManager, debugging window for our engine.
                                Responsibilities include:
                                - Loading ImGui Window
                                - Intergrating all the external ImGui files

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
// #define IMGUI_IMPL_OPENGL_LOADER_CUSTOM

#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "IMGUI/imgui.h"
#include "Graphics/ImGuiManager.hpp"
#include "IMGUI/imgui_impl_glfw.h"
#include "IMGUI/imgui_impl_opengl3.h"
#include <iostream>

namespace Graphics {

bool ImGuiManager::init(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  // DISABLE imgui.ini file generation
  io.IniFilename = nullptr;

  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  // Increase font scale for better visibility (User Request)
  io.FontGlobalScale = 1.5f;

  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
    std::cerr << "[ImGuiManager] Failed to init GLFW backend\n";
    return false;
  }

  if (!ImGui_ImplOpenGL3_Init("#version 130")) {
    std::cerr << "[ImGuiManager] Failed to init OpenGL3 backend\n";
    return false;
  }

  m_initialised = true;
  return true;
}

void ImGuiManager::shutdown() {
  if (m_initialised) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialised = false;
  }
}

void ImGuiManager::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiManager::endFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  // If using multi-viewport, update platform windows
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow *backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

void ImGuiManager::beginDockspace() {
  // Create a fullscreen dockspace
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
  window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus |
                  ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DockSpace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  // Create dockspace
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                   ImGuiDockNodeFlags_PassthruCentralNode);
}

void ImGuiManager::endDockspace() {
  ImGui::End(); // End DockSpace window
}

ImGuiManager::~ImGuiManager() { shutdown(); }
} // namespace Graphics