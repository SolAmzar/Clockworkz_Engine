/******************************************************************************/
/*!
\file           Engine.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
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

#pragma once
#include <memory>
#include "Graphics/ImGuiManager.hpp"

namespace Runtime { class GameStateManager; class MenuState; }

namespace Audio { class AudioManager; }

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
		Audio::AudioManager& audio();
		Audio::AudioManager const& audio() const;

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
		std::unique_ptr<Graphics::ImGuiManager>		 m_imgui;

		std::unique_ptr<Audio::AudioManager> m_audio;

		// Run Time
		std::unique_ptr<Runtime::GameStateManager> m_gsm;
		bool m_booted{ false }; // flag for Game state change


		// Engine
		bool m_running{ false };
	};
}
