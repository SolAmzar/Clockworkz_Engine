#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>


using namespace Graphics;

bool RenderingManager::init() { return true; }
void RenderingManager::beginFrame(const Camera2D&) { (void)0; glClear(GL_COLOR_BUFFER_BIT); }
void RenderingManager::endFrame() { /* submit/batch later */ }
