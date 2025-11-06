/******************************************************************************/
/*!
\file           Time.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Time, a utility for tracking frame timing and FPS.
				Responsibilities include:
				- beginFrame() to measure delta time between frames
				- endFrame() placeholder for per-frame finalization
				- Access to delta time and calculated frames-per-second (FPS)

				Uses GLFW�s high-resolution timer for accurate frame timing.
				Can be use to track ms on fucntions performance later.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Core/Time.hpp"
#include <GLFW/glfw3.h>

/** Function header comment
 * @brief
 *
 * XXX
 *
 * @param XXX
 *
 * @return XXX
 */
void Core::Time::beginFrame() {
	double now = glfwGetTime();
	m_dt = (m_last == 0.0) ? 0.0 : (now - m_last);
	m_last = now;
	m_accum += m_dt;
	m_frames++;
	if (m_accum >= 1.0) { m_fps = m_frames / m_accum; m_accum = 0.0; m_frames = 0; }
}

double Core::Time::fps() const { return (m_fps > 0.0) ? m_fps : (m_dt > 0.0 ? 1.0 / m_dt : 0.0); }
double Core::Time::frameTimeMs() const { return m_dt * 1000.0; }

void Core::Time::endFrame() {
	// Compute instantaneous fps from last frame's delta
	double instFps = (m_dt > 0.0) ? 1.0 / m_dt : 0.0;

	// Smooth FPS over a few frames (EMA filter)
	static double smoothFps = 0.0;
	const double alpha = 0.15; // 0.1–0.2 = gentle smoothing
	if (smoothFps == 0.0)
		smoothFps = instFps;
	else
		smoothFps = alpha * instFps + (1.0 - alpha) * smoothFps;

	m_fps = smoothFps;
}
