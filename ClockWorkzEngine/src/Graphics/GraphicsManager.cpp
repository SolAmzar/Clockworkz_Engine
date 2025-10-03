/******************************************************************************/
/*!
\file           Engine.cpp
\project        Lights Out
\author(s)      [Wei An], 95%, [Syafiq] 5%
\brief          Declares Engine, the central orchestrator of the game loop.
				Responsibilities include:
				- Initializing and shutting down all subsystems
				  (Config, Logger, Time, Window, Input, Graphics, Renderer, Camera)
				- Managing the GameStateManager and bootstrapping the MenuState
				- Running the main loop (update, render, timing, input polling)
				- Providing Run() as the entry point to start the application

				Serves as the core runtime that coordinates gameplay, rendering,
				and platform-level services.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Graphics/GraphicsManager.hpp"
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
	return true;
}
void GraphicsManager::setViewport(int w, int h) { glViewport(0, 0, w, h); }
void GraphicsManager::setClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
