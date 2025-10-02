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
