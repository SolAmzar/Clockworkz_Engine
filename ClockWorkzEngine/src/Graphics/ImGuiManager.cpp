
/******************************************************************************/
/*!
\file           ImGuiManager.cpp
\project        Lights Out
\author(s)      [Syafiq] 100%
\brief          Define ImGuiManager, debugging window for our engine.
				Responsibilities include:
				- Loading ImGui Window
				- Intergrating all the external ImGui files

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM 

#include "Graphics/ImGuiManager.hpp"
#include <GLFW/glfw3.h>
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_glfw.h"
#include "IMGUI/imgui_impl_opengl3.h"
#include <iostream>


namespace Graphics {

	bool ImGuiManager::init(GLFWwindow* window) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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

	void ImGuiManager::endFrame(){
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	ImGuiManager::~ImGuiManager() {
		shutdown();
	}
}