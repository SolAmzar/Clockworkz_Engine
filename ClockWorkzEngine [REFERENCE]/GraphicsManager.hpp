#pragma once
namespace Platform { class WindowManager; }
namespace Graphics {
	class GraphicsManager {
	public:
		bool init(Platform::WindowManager& window);
		void setViewport(int w, int h);
		void setClearColor(float r, float g, float b, float a);
	};
}
