#include "Gameplay/Core/Time.hpp"
#include <GLFW/glfw3.h>
void Core::Time::beginFrame() {
	double now = glfwGetTime();
	m_dt = (m_last == 0.0) ? 0.0 : (now - m_last);
	m_last = now;
	m_accum += m_dt; m_frames++;
	if (m_accum >= 1.0) { m_fps = m_frames / m_accum; m_accum = 0.0; m_frames = 0; }
}
void Core::Time::endFrame() { /* no-op for now */ }
