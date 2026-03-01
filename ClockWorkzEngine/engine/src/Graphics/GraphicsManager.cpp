/******************************************************************************/
/*!
\file           GraphicsManager.cpp
\project        Lights Out
\author(s)      [Lebon], 95%, [Syafiq], 5%
\brief          Implements GraphicsManager, the subsystem responsible for
				initializing and configuring global graphics state.
				Responsibilities include:
				- Initializing GLEW and OpenGL context
				- Setting viewport and clear color
				- Enabling blending and other global graphics options
				- Providing utility functions for graphics configuration
				- Used by Engine to prepare rendering environment

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Graphics/GraphicsManager.hpp"
#include "Core/Engine.hpp"
#include "Platform/WindowManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace Graphics;

bool GraphicsManager::init(Platform::WindowManager& window) {

	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cerr << "[GraphicsManager] GLEW Error: " << glewGetErrorString(err) << "\n";
		return false;
	}

	int w = window.width(), h = window.height();
	setViewport(w, h);
	setClearColor(0.07f, 0.07f, 0.1f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// I love to listen to WindowResizeEvent, when broadcasted, i will do this.
	Core::Engine::Get().events().subscribe<Core::WindowResizeEvent>(
		this,
		[this](const Core::WindowResizeEvent& e) {
			this->setViewport(e.width, e.height);
		}
	);

	return true;
}
void GraphicsManager::setViewport(int w, int h) {
	const float targetAspect = 16.0f / 9.0f;
	float windowAspect = static_cast<float>(w) / static_cast<float>(h);

	int vpWidth = w;
	int vpHeight = h;
	int vpX = 0;
	int vpY = 0;

	if (windowAspect > targetAspect) {
		// Window too wide pillarbox
		vpWidth = static_cast<int>(h * targetAspect);
		vpX = (w - vpWidth) / 2;
	}
	else if (windowAspect < targetAspect) {
		// Window too tall letterbox
		vpHeight = static_cast<int>(w / targetAspect);
		vpY = (h - vpHeight) / 2;
	}

	glViewport(vpX, vpY, vpWidth, vpHeight);

}
void GraphicsManager::setClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
