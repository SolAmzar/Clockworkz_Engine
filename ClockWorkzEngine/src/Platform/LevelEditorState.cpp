/******************************************************************************/
/*!
\file           LevelEditorState.cpp
\project        Lights Out - Level Editor
\author(s)      [Yusuf], 100%
\brief          MODIFIED: Now supports loading scenes from PlayState

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Platform/LevelEditorState.hpp"
#include "Platform/LevelEditor.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneManager.hpp"
#include "Runtime/Scene.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Core/Engine.hpp"
#include <GLFW/glfw3.h>
#include "Assets/ResourceManager.hpp"
#include "Graphics/Shader.hpp"

namespace Runtime {

    EditorState::EditorState(Platform::InputManager& input,
        GameStateManager& gsm,
        Runtime::SceneManager* existingSceneManager)
        : m_editor(nullptr)
        , m_input(input)
        , m_gsm(gsm)
        , m_renderer(nullptr)
        , m_windowManager(nullptr)
        , m_imguiManager(nullptr)
        , m_initialized(false)
        , m_playStateSceneManager(existingSceneManager)
    {
    }

    EditorState::~EditorState() {
        onExit();
    }

    void EditorState::onEnter() {
        // Get system references from Engine singleton
        Core::Engine& engine = Core::Engine::Get();

        m_renderer = &engine.rendering();
        m_windowManager = &engine.window();

        auto textureShader = Assets::ResourceManager<Graphics::Shader>::Load(
            "texture",
            "assets/shaders/texture.vert",
            "assets/shaders/texture.frag");

        Assets::ResourceManager<Graphics::Shader>::Load(
            "light",
            "assets/shaders/light.vert",
            "assets/shaders/light.frag");

        engine.logger().info("Entering Editor State");

        // Create and initialize editor
        m_editor = std::make_unique<Platform::LevelEditor>();

        if (!m_editor->init(
            m_windowManager->handle(),
            *m_renderer,
            m_input,
            *m_windowManager)) {
            engine.logger().error("Failed to initialize level editor");
            return;
        }

        if (m_playStateSceneManager && m_playStateSceneManager->current()) {
            m_editor->loadSceneFromPlayState(m_playStateSceneManager->current());
            engine.logger().info("Loaded existing scene from PlayState into editor");
        }

        m_initialized = true;
        engine.logger().info("Level Editor initialized successfully");
    }

    void EditorState::onExit() {
        if (m_playStateSceneManager && m_playStateSceneManager->current() && m_editor) {
            m_editor->syncSceneBackToPlayState(m_playStateSceneManager->current());
            Core::Engine::Get().logger().info("Synced editor changes back to PlayState");
        }

        if (m_editor) {
            m_editor->shutdown();
            m_editor.reset();
        }

        m_initialized = false;

        if (Core::Engine::IsInitialized()) {
            Core::Engine::Get().logger().info("Exiting Editor State");
        }
    }

    void EditorState::update(double dt) {
        if (!m_editor || !m_initialized) return;

        // Allow ESC to exit editor
        if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE))
        {
            m_gsm.pop();
            return;
        }

        m_editor->update(dt);
    }

    void EditorState::render(Core::Time* timer) {
        if (!m_editor || !m_initialized) return;

        // Render editor scene
        m_editor->render();
        (void)timer;
    }

    void EditorState::renderImGui(Core::Time* timer) {
        if (!m_editor || !m_initialized) return;

        Core::Engine& engine = Core::Engine::Get();
        Graphics::ImGuiManager& imgui = engine.imguiManager();

        imgui.beginDockspace(); 

        m_editor->renderUI();

        imgui.endDockspace();

        (void)timer;
    }

} // namespace Runtime