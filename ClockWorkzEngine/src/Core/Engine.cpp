/******************************************************************************/
/*!
\file           Engine.cpp
\project        Lights Out
\author(s)      [Lebon], 80% [Wei An], 5% [Amzar] 15%
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

#include "Core/Engine.hpp"
#include "Core/Config.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/SurfaceAudioSystem.hpp"
#include "Platform/WindowManager.hpp"
#include "Platform/InputManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>
#include "Runtime/GameStateManager.hpp"
#include "Runtime/MenuState.hpp"
#include "Runtime/SpriteState.hpp"
#include "Graphics/ImGuiManager.hpp"
#include <iostream>
#include <exception>
#include <string>
#include "Core/Profiler.hpp"
#include "Assets/TextureManager.hpp"
#include "Gameplay/Prefab.hpp"
#include "Platform/LevelEditor.hpp"

using namespace Core;

namespace Core
{
	Engine* Engine::s_instance = nullptr;
	Engine::Engine() {
		s_instance = this;  // register singleton instance
		m_accuml = 0.0;
	}

	Engine::~Engine() {
		s_instance = nullptr;  // clear singleton instance
		shutdown();
	}

	Engine& Engine::Get() {
		assert(s_instance && "Engine::Get() called before engine instance was created!");
		return *s_instance;
	}

	Audio::AudioManager& Engine::audio()
	{
		assert(m_audio && "Engine::audio() called before AudioManager is initialized");
		return *m_audio;
	}

	Audio::AudioManager const& Engine::audio() const
	{
		assert(m_audio && "Engine::audio() called before AudioManager is initialized");
		return *m_audio;
	}

    /** Function header comment
     * @brief Engine class private function
     *
     * XXX
     *
     * @param XXX
     *
     * @return XXX
     */
    bool Engine::init()
    {
        m_config = std::make_unique<Config>();
        m_logger = std::make_unique<Logger>();
        m_time = std::make_unique<Time>();
        m_window = std::make_unique<Platform::WindowManager>();
        m_input = std::make_unique<Platform::InputManager>();
        m_editor = std::make_unique<Platform::LevelEditor>();
        m_gfx = std::make_unique<Graphics::GraphicsManager>();
        m_renderer = std::make_unique<Graphics::RenderingManager>();
        m_camera = std::make_unique<Graphics::Camera2D>();
        m_imgui = std::make_unique<Graphics::ImGuiManager>();

		m_config->loadFromFile("assets/configs/engine_config.json");
		m_logger->init();
		m_logger->info("Engine init...");

		m_audio = std::make_unique<Audio::AudioManager>();
		m_sfAudio = std::make_unique<Audio::SurfaceAudioSystem>();

		if (!m_audio->init())
		{
			return false;
		}
		m_sfAudio->init(*m_audio);

		if (!m_window->create(*m_config))
			return false;
		m_input->init(*m_window);
		if (!m_gfx->init(*m_window))
			return false;

		if (!Assets::TextureManager::LoadFromConfig("assets/configs/textures.json")) {
			m_logger->fatal("Failed to load textures from config");
			return false;
		}

		// 1️ Register all prefab blueprints
		const std::string prefabFile = "assets/configs/Prefab.json";
		if (!Gameplay::PrefabFactory::get().loadPrefabFromFile(prefabFile))
		{
			std::cerr << "Failed to load prefab from file: " << prefabFile << "\n";
			return false;
		}

		/* */
		if (!m_renderer->init())
			return false;

		if (!m_imgui->init(m_window->handle()))
			return false;

		//m_camera->setViewport((float)m_window->width(), (float)m_window->height());
		m_camera->setViewport((int)m_window->width(), (int)m_window->height());
		m_camera->setPosition(glm::vec2{ 0,0 });
		m_camera->setZoom(1.0f);

		m_gsm = std::make_unique<Runtime::GameStateManager>();


		m_audio->load("ui_move", { "assets/audio/ui_move.wav", false, false, false });
		m_audio->load("ui_select", { "assets/audio/ui_select.wav", false, false, false });

		// BGM Test but it's just random royalty-free music atm
		m_audio->load("menu_bgm", { "assets/audio/menu_bgm.mp3", true, true, false });

		m_running = true;
		return true;
	}

	void Engine::shutdown()
	{
		std::cout << "Shutdown started" << std::endl;
		if (m_logger)
			m_logger->info("Engine shutdown.");

		// ADD THIS: Clean up game state manager (which will unload scenes)
		if (m_gsm)
		{
			// Pop all states, which triggers their onExit() methods
			while (m_gsm->top())
			{
				m_gsm->pop();
				m_gsm->update(0.0);
			}
			m_gsm.reset();
		}

		if (m_imgui)
		{
			m_imgui->shutdown();
			m_imgui.reset();
		}

		m_renderer.reset();
		m_gfx.reset();
		m_input.reset();
		m_window.reset();
		m_time.reset();
		m_logger.reset();
		m_config.reset();
		m_camera.reset();
		m_audio.reset();
		m_sfAudio.reset();

		Assets::TextureManager::Clear();

		std::cout << "Shutdown done" << std::endl;
	}

	/** Function header comment
	 * @brief Engine::update(double dt) is called once per frame in the game loop.
	 *
	 * It updates all the engine systems and ensures the initial game state is set up.
	 *
	 * @param XXX
	 *
	 * @return XXX
	 */
	void Engine::update(double dt)
	{
		//Check if Fixed Delta time (Uncomment if you want to check)
		/*std::cout << m_time->fixedDelta() << std::endl;*/

		if (!m_booted)
		{
			// Initial State when Run Game, start off from Menu State
			// Push MenuState once
			m_gsm->push(std::make_shared<Runtime::MenuState>(
				*m_gsm,
				*m_input,
				*m_window,
				this->audio(),
				[this]()
				{ m_running = false; }));

			m_booted = true;
		}
		m_audio->update(); // Update the audio manager class
		m_input->update(); // Update the input manager class

		m_gsm->update(dt);
		//int steps = m_time->currentSteps();
		//for (int i = 0; i < steps; ++i) {
		//	m_gsm->update(m_time->fixedDelta());
		//}
		
	}

	/** Function header comment
	 * @brief
	 *
	 * XXX
	 *
	 * @param XXX
	 *
	 * @return XXX
	 */
	void Engine::render()
	{
		// clear & present
		// (Clear happens inside RenderingManager::beginFrame)
		m_gsm->render(m_time.get());

		if (m_renderer && m_camera)
		{
			/*
			m_renderer->drawCircle(400.0f, 300.0f, 100.0f, 0.0f, 0.0f, 1.0f, 64);

			// Draw a red rectangle at (200, 150) with width 300 and height 200
			m_renderer->drawRect(200.0f, -300.0f, 300.0f, 200.0f, 1.0f, 0.0f, 0.0f);
			*/
		}
	}

	/** Function header comment
	 * @brief Engine class public function
	 *
	 * XXX
	 *
	 * @param XXX
	 *
	 * @return XXX
	 */
	int Engine::Run()
	{

		// Call the engine private function `init()`
		try {
			if (!init())
				return -1;

			// `m_running` is a private data member of type bool, and was set to true by `init()`.

			// `m_window` is a WindowManager class, and `shouldClose()` seems to be a wrapper? not sure if it is the right term, i dont really understand
			// wrapper. [Done]
			// Q: What is a wrapper?
			// A: A wrapper is a function or class that “wraps around” another function, library, or system to make it easier, safer, or cleaner to use. It
			// hides complexity of the underlying system. It can change the interface to be more convenient or type - safe. The rest of your code doesn’t
			// need to know about the internal details.

			// `m_` seems to be a prefix for manager, not sure what it is suppose to mean? [Done]
			// Q: What does the m_ prefix mean ?
			// A : In C++, m_ is a convention for member variables. m_ stands for member. It tells you the variable belongs to the class instance, not a
			// local variable or parameter. It has nothing to do with “manager”, even if it looks like that.

			// Q: Is m_window->shouldClose() calling itself recursively?
			// A: No.Engine::m_window is a WindowManager*, and WindowManager::shouldClose() calls glfwWindowShouldClose() on its GLFW window member.
			// They just happen to share the name m_window.
			while (m_running && !m_window->shouldClose()) {


				//PROFILE_SCOPE("Frame");

				{
					PROFILE_SCOPE("FrameSetup");
					m_time->beginFrame();
					m_window->poll();
				}

				{
					PROFILE_SCOPE("Update");
					update(m_time->delta());
				}

	/*
				m_renderer->beginFrame(*m_camera);
				m_imgui->beginFrame();*/
				{
					PROFILE_SCOPE("Full Render");
					m_renderer->beginFrame();
					/*std::cout << "Core Render Begin: " << std::endl;*/
					render();
					/*std::cout << "Core Render End " << std::endl;*/
					m_renderer->endFrame();
				}
	/*			m_imgui->endFrame();
				m_renderer->endFrame();*/
				

				{
					PROFILE_SCOPE("SwapBuffers");
					m_window->swap();
				}

				m_time->endFrame();

				m_accuml += m_time->delta();
				if (m_accuml >= 0.5)
				{
					m_accuml = 0.0;
					double fps = m_time->fps();
					double frameTime = m_time->frameTimeMs();

					std::string title = "Lights Out  |  FPS: " + std::to_string((int)fps) +
						"  (" + std::to_string(frameTime).substr(0, 5) + " ms)";
					m_window->setTitle(title);
				}
			}
			std::cout << "Exiting main loop" << std::endl;
			shutdown();
			return 0;
		}
		catch (const std::exception& e)
		{
			// Log fatal exceptions with crash report
			if (m_logger) {
				m_logger->fatal("Fatal exception caught: " + std::string(e.what()));
			}
			else {
				std::cerr << "[FATAL] Exception before logger initialized: " << e.what() << std::endl;
			}

			shutdown();
			return -1;
		}
		catch (...) {
			// Log unknown fatal exceptions
			if (m_logger) {
				m_logger->fatal("Unknown fatal exception caught");
			}
			else {
				std::cerr << "[FATAL] Unknown exception before logger initialized" << std::endl;
			}

			shutdown();
			return -1;
		}
	}
}
