/******************************************************************************/
/*!
\file           PlayState.hpp
\project        Lights Out
\author(s)      [Syafiq] 100%
\brief          Declares ImGuiManager, debugging window for our engine.
				Responsibilities include:
				- Loading ImGui Window
				- Intergrating all the external ImGui files

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once


struct GLFWwindow;

namespace Graphics{
	class ImGuiManager{
	public:
		bool init(GLFWwindow* window);

		ImGuiManager() = default; //Ctor
		~ImGuiManager(); //Destructor

		bool init(void* glfwWindow);
		void shutdown();
		void beginFrame();
		void endFrame();
	private:
		bool m_initialised = false;
	};
}