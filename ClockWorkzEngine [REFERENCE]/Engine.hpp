#pragma once
#include <memory>
namespace Core {
	class Config; class Logger; class Time;
}
namespace Platform {
	class WindowManager; class InputManager;
}
namespace Graphics {
	class GraphicsManager; class RenderingManager; class Camera2D;
}

namespace Core {
	class Engine {
	public:
		Engine();
		~Engine();
		int Run();
	private:
		bool init();
		void shutdown();
		void update(double dt);
		void render();

		// Core
		std::unique_ptr<Config> m_config;
		std::unique_ptr<Logger> m_logger;
		std::unique_ptr<Time>   m_time;

		// Platform
		std::unique_ptr<Platform::WindowManager> m_window;
		std::unique_ptr<Platform::InputManager>  m_input;

		// Graphics
		std::unique_ptr<Graphics::GraphicsManager>   m_gfx;
		std::unique_ptr<Graphics::RenderingManager>  m_renderer;
		std::unique_ptr<Graphics::Camera2D>          m_camera;

		bool m_running{ false };
	};
}
