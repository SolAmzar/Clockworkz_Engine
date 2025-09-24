#include "Core/Engine.hpp"
#include "Core/Config.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Platform/WindowManager.hpp"
#include "Platform/InputManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GLFW/glfw3.h>

using namespace Core;

namespace Core {
    Engine::Engine() = default;
    Engine::~Engine() { shutdown(); }

    bool Engine::init() {
        m_config = std::make_unique<Config>();
        m_logger = std::make_unique<Logger>();
        m_time = std::make_unique<Time>();
        m_window = std::make_unique<Platform::WindowManager>();
        m_input = std::make_unique<Platform::InputManager>();
        m_gfx = std::make_unique<Graphics::GraphicsManager>();
        m_renderer = std::make_unique<Graphics::RenderingManager>();
        m_camera = std::make_unique<Graphics::Camera2D>();

        m_config->loadFromFile("assets/config.json");
        m_logger->init();
        m_logger->info("Engine init...");

        if (!m_window->create(*m_config)) return false;
        m_input->attachCallbacks((void*)m_window->handle());
        if (!m_gfx->init(*m_window)) return false;
        if (!m_renderer->init()) return false;

        m_camera->setOrtho((float)m_window->width(), (float)m_window->height(), 1.0f);
        m_running = true;
        return true;
    }

    void Engine::shutdown() {
        if (m_logger) m_logger->info("Engine shutdown.");
        m_renderer.reset(); m_gfx.reset(); m_input.reset(); m_window.reset();
        m_time.reset(); m_logger.reset(); m_config.reset(); m_camera.reset();
    }

    void Engine::update(double dt) {
        (void)dt;
        // Example: ESC quits
        // (Handled in Input callback as well)
    }

    void Engine::render() {
        // clear & present
        // (Clear happens inside RenderingManager::beginFrame)
    }

    int Engine::Run() {
        if (!init()) return -1;
        while (m_running && !m_window->shouldClose()) {
            m_time->beginFrame();
            m_window->poll();
            update(m_time->delta());

            m_renderer->beginFrame(*m_camera);
            // TODO: submit sprites/meshes here in M2
            m_renderer->endFrame();

            m_window->swap();
            m_time->endFrame();
        }
        shutdown();
        return 0;
    }
}
