#pragma once
namespace Graphics { class Camera2D; }
namespace Graphics {
	class RenderingManager {
	public:
		bool init();
		void beginFrame(const Camera2D& cam);
		void endFrame();
	};
}
