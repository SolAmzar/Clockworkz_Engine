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
#include "Core/Logger.hpp"
#include "Core/EventBus.hpp"
#include "Core/Event.hpp"
namespace Runtime { class GameStateManager; class MenuState; }

namespace Audio { class AudioManager; class SurfaceAudioSystem; }

namespace Core {
	class Config; class Logger; class Time;
}
namespace Platform {
	class WindowManager; class InputManager; class LevelEditor;
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

		// --- Accessors ---
		static Engine& Get();  // Singleton accessor
		static bool IsInitialized() { return s_instance != nullptr; }

		// Subsystem accessors
		EventBus& events() { return m_events; }
		Graphics::RenderingManager& rendering() { return *m_renderer; }
		Graphics::GraphicsManager& graphics() { return *m_gfx; }
		Graphics::Camera2D& camera() { return *m_camera; }
		Graphics::ImGuiManager& imguiManager() { return *m_imgui; }

		Platform::WindowManager& window() { return *m_window; }
		Platform::InputManager& input() { return *m_input; }
		Platform::LevelEditor& editor() { return *m_editor; }
		Core::Logger& logger() { return *m_logger; }
		Audio::SurfaceAudioSystem& surfaceAudio() { return *m_sfAudio; }
		Audio::SurfaceAudioSystem const& surfaceAudio() const { return *m_sfAudio; }
		//Audio::AudioManager& audio() { return *m_audio; }
	private:
		bool init();
		void shutdown();
		void update(double dt);
		void render();

	private: // engine seems to be made up of unique_ptr to class, like an instance?
		// Inside namespace Core
		std::unique_ptr<Config> m_config;
		std::unique_ptr<Logger> m_logger;
		std::unique_ptr<Time>   m_time;

		// Inside namespace Platform
		std::unique_ptr<Platform::WindowManager> m_window;
		std::unique_ptr<Platform::InputManager>  m_input;
		std::unique_ptr<Platform::LevelEditor>   m_editor;

		// Inside namespace Graphics
		std::unique_ptr<Graphics::GraphicsManager>   m_gfx;
		std::unique_ptr<Graphics::RenderingManager>  m_renderer;
		std::unique_ptr<Graphics::Camera2D>          m_camera;
		std::unique_ptr<Graphics::ImGuiManager>		 m_imgui;


		// Inside namespace Audio
		std::unique_ptr<Audio::AudioManager>         m_audio;
		std::unique_ptr<Audio::SurfaceAudioSystem>   m_sfAudio; // separate SFX audio manager for surface audio system

		// Inside namespace Runtime
		std::unique_ptr<Runtime::GameStateManager>   m_gsm;
		EventBus m_events;
	private: // simple data member
		bool m_booted{ false }; // flag for Game state change {i dont think this is working as intented? game state change or boot once only?}
		bool m_running{ false }; // Is the engine running?
		static Engine* s_instance;
		double m_accuml;
	};
}
