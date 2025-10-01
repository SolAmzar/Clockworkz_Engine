/******************************************************************************/
/*!
\file           Time.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Time, a utility for tracking frame timing and FPS.
				Responsibilities include:
				- beginFrame() to measure delta time between frames
				- endFrame() placeholder for per-frame finalization
				- Access to delta time and calculated frames-per-second (FPS)

				Uses GLFW’s high-resolution timer for accurate frame timing.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
namespace Core {
	class Time {
	public:
		void   beginFrame();
		void   endFrame();
		double delta() const { return m_dt; }
		double fps()   const { return m_fps; }
	private:
		double m_last = 0.0, m_dt = 0.0, m_accum = 0.0; int m_frames = 0; double m_fps = 0.0;
	};
}
