/******************************************************************************/
/*!
\file           Engine.cpp
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

#include "Core/Engine.hpp"
#include "Core/Config.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Core/AudioManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Platform/InputManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>
#include "Runtime/GameStateManager.hpp"
#include "Runtime/MenuState.hpp"
#include "Graphics/ImGuiManager.hpp"

using namespace Core;

namespace Core {
    Engine::Engine() = default;
    Engine::~Engine() { shutdown(); }

    Audio::AudioManager& Engine::audio() {
        assert(m_audio && "Engine::audio() called before AudioManager is initialized");
        return *m_audio;
    }

    Audio::AudioManager const& Engine::audio() const {
        assert(m_audio && "Engine::audio() called before AudioManager is initialized");
        return *m_audio;
    }

    bool Engine::init() {
        m_config = std::make_unique<Config>();
        m_logger = std::make_unique<Logger>();
        m_time = std::make_unique<Time>();
        m_window = std::make_unique<Platform::WindowManager>();
        m_input = std::make_unique<Platform::InputManager>();
        m_gfx = std::make_unique<Graphics::GraphicsManager>();
        m_renderer = std::make_unique<Graphics::RenderingManager>();
        m_camera = std::make_unique<Graphics::Camera2D>();
        m_imgui = std::make_unique<Graphics::ImGuiManager>();

        m_config->loadFromFile("assets/config.json");
        m_logger->init();
        m_logger->info("Engine init...");

		m_audio = std::make_unique<Audio::AudioManager>();

        if (!m_audio->init()) {  return false;  }

        if (!m_window->create(*m_config)) return false;
        m_input->init(*m_window);
        if (!m_gfx->init(*m_window)) return false;
        if (!m_renderer->init()) return false;

        if (!m_imgui->init(m_window->handle())) return false;


        m_camera->setOrtho((float)m_window->width(), (float)m_window->height(), 1.0f);
        m_gsm = std::make_unique<Runtime::GameStateManager>();

        m_audio->load("ui_move", { "assets/audio/ui_move.wav",   false, false, false });
        m_audio->load("ui_select", { "assets/audio/ui_select.wav", false, false, false });

        // Example BGM (stream + loop)
        // m_audio->load("menu_bgm", { "assets/audio/menu_bgm.ogg",  true,  true,  false });
      
        m_running = true;
        return true;
    }

    void Engine::shutdown() {
        if (m_logger) m_logger->info("Engine shutdown.");
        if (m_imgui) {
            m_imgui->shutdown();
            m_imgui.reset();
        }
        m_renderer.reset(); m_gfx.reset(); m_input.reset(); m_window.reset();
        m_time.reset(); m_logger.reset(); m_config.reset(); m_camera.reset();
        m_audio.reset();
    }

    void Engine::update(double dt) {
        if (!m_booted) {
            // Initial State when Run Game, start off from Menu State
            // Push MenuState once
            m_gsm->push(std::make_shared<Runtime::MenuState>(
                *m_gsm,
                *m_input,
                *m_window,
                this->audio(),
                [this]() { m_running = false; }
            ));
            m_booted = true;
        }
        m_audio->update();
        m_input->update();
        m_gsm->update(dt);
        
    }

    void Engine::render() {
        // clear & present
        // (Clear happens inside RenderingManager::beginFrame)
        m_gsm->render();

        if (m_renderer && m_camera) {
            m_renderer->drawCircle(400.0f, 300.0f, 100.0f, 0.0f, 0.0f, 1.0f, 64);

            // Draw a red rectangle at (200, 150) with width 300 and height 200
            m_renderer->drawRect(200.0f, 150.0f, 300.0f, 200.0f, 1.0f, 0.0f, 0.0f);
        }
    }

    int Engine::Run() {
        if (!init()) return -1;
        while (m_running && !m_window->shouldClose()) {
            m_time->beginFrame();
            m_window->poll();

            // game loop
            update(m_time->delta());

            m_renderer->beginFrame(*m_camera);
            m_imgui->beginFrame();

            // TODO: submit sprites/meshes here in M2
            render();
          
            m_imgui->endFrame();

            m_renderer->endFrame();

            m_window->swap();
            m_time->endFrame();
        }
        shutdown();
        return 0;
    }
}
