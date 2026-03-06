/******************************************************************************/
/*!
\file           EditorPlayState.cpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Editor play-mode state implementation.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Runtime/EditorPlayState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "IMGUI/imgui.h"

#include <GL/glew.h>
#include <filesystem>
#include <iostream>

namespace Runtime {

EditorPlayState::EditorPlayState(Platform::InputManager& input,
                                 GameStateManager& gsm,
                                 std::string scenePath)
    : m_input(input), m_gsm(gsm), m_sceneFilePath(std::move(scenePath)) {}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void EditorPlayState::onEnter() {
    // EditorState (below us on the stack) already set the CWD and loaded all
    // assets — do not reload or change CWD here.

    // Save editor camera state so we can restore it on stop
    auto& camera = Core::Engine::Get().camera();
    m_savedCamPos     = camera.position();
    m_savedCamZoom    = camera.zoom();
    auto& window      = Core::Engine::Get().window();
    m_savedViewportW  = window.width();
    m_savedViewportH  = window.height();

    m_scene.Init();
    Core::Engine::Get().invokeSceneSetup(m_scene);

    if (!m_sceneFilePath.empty()) {
        if (!m_scene.LoadFromFile(m_sceneFilePath))
            std::cerr << "[EditorPlayState] Failed to load: " << m_sceneFilePath << "\n";
    }

    // Set camera to full-screen viewport for play mode
    glViewport(0, 0, window.width(), window.height());
    camera.setViewport(window.width(), window.height());
    camera.setPosition({0.0f, 0.0f});
    camera.setZoom(1.0f);
}

void EditorPlayState::onExit() {
    m_scene.Clear();

    // Restore the editor camera to how it was before play
    auto& camera = Core::Engine::Get().camera();
    camera.setViewport(m_savedViewportW, m_savedViewportH);
    camera.setPosition(m_savedCamPos);
    camera.setZoom(m_savedCamZoom);

    // Delete the temp playtest file — it should never persist between sessions
    std::error_code ec;
    std::filesystem::remove(m_sceneFilePath, ec);

    // Assets and CWD are owned by EditorState (still on the stack) — do not
    // clear or restore them here.
}

// ---------------------------------------------------------------------------
// Update — run scene systems; deferred stop at top of frame
// ---------------------------------------------------------------------------

void EditorPlayState::update(double dt) {
    if (m_pendingStop) {
        m_pendingStop = false;
        m_gsm.pop();
        return;
    }

    m_scene.Update(dt, m_input);

    // Camera follow: track entity tagged "Player"
    auto& camera = Core::Engine::Get().camera();
    m_scene.GetWorld().each<Components::Tag, Components::Transform>(
        [&](ECS::Entity, Components::Tag& tag, Components::Transform& t) {
            if (tag.tag == "Player")
                camera.setPosition(glm::vec2(t.position.x, t.position.y));
        });
}

// ---------------------------------------------------------------------------
// Render — full-screen scene render (no framebuffer redirection)
// ---------------------------------------------------------------------------

void EditorPlayState::render(Core::Time* timer) {
    (void)timer;

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto& engine   = Core::Engine::Get();
    auto& renderer = engine.rendering();
    auto& camera   = engine.camera();

    renderer.beginScenePass(camera.viewProj(), glm::vec4(1.0f));
    m_scene.Render(renderer);
    renderer.endScenePass();
}

// ---------------------------------------------------------------------------
// renderImGui — minimal overlay: playing indicator + Stop button
// ---------------------------------------------------------------------------

void EditorPlayState::renderImGui(Core::Time* timer) {
    (void)timer;

    // Anchor a small toolbar to the top-centre of the viewport
    ImGuiIO& io = ImGui::GetIO();
    float overlayW = 200.0f;
    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - overlayW) * 0.5f, 4.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(overlayW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar     |
        ImGuiWindowFlags_NoResize       |
        ImGuiWindowFlags_NoScrollbar    |
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##PlayOverlay", nullptr, flags)) {
        // Green "playing" dot
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  PLAYING");
        ImGui::SameLine();

        // Red Stop button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button(" Stop "))
            m_pendingStop = true;
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

} // namespace Runtime
