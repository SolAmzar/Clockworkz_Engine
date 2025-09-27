#include "Graphics/GraphicsManager.hpp"
#include "Platform/WindowManager.hpp"
#include <GLFW/glfw3.h>

using namespace Graphics;

bool GraphicsManager::init(Platform::WindowManager& window) {

	int w = window.width(), h = window.height();
	setViewport(w, h);
	setClearColor(0.07f, 0.07f, 0.1f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return true;
}
void GraphicsManager::setViewport(int w, int h) { glViewport(0, 0, w, h); }
void GraphicsManager::setClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
