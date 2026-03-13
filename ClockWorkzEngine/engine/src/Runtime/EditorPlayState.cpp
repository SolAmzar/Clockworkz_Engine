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
#include "Runtime/LevelMusicManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Scripting/LuaManager.hpp"
#include "Components/Collider.hpp"
#include "Components/UIButton.hpp"
#include "Components/Transform.hpp"
#include "Components/Tag.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/TextRenderer.hpp"
#include "IMGUI/imgui.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>

namespace Runtime {

EditorPlayState::EditorPlayState(Platform::InputManager& input,
                                 GameStateManager& gsm,
                                 std::string scenePath, bool showColliders)
    : m_input(input), m_gsm(gsm), m_sceneFilePath(std::move(scenePath)),
      m_showColliders(showColliders) {}

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
    Runtime::LevelMusicManager::Stop(Core::Engine::Get().audio());
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

    // UIButton click detection — mirrors the game states so scripts execute in play mode.
    // State-transition GSM calls stop play mode (return to editor) since we can't
    // navigate to standalone game states from inside the editor.
    if (m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) {
        auto& engine   = Core::Engine::Get();
        auto& camera   = engine.camera();
        auto& window   = engine.window();
        vec2 mouseWorld = m_input.getMouseWorldPos(camera,
            static_cast<float>(window.width()),
            static_cast<float>(window.height()));

        m_scene.GetWorld().each<Components::Transform, Components::UIButton>(
            [&](ECS::Entity, Components::Transform& tf, Components::UIButton& btn) {
                if (!btn.interactable || btn.uiScript.empty()) return;

                float cx    = tf.position.x + btn.offset.x;
                float cy    = tf.position.y + btn.offset.y;
                float halfW = btn.size.x * 0.5f;
                float halfH = btn.size.y * 0.5f;
                bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                                mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
                if (!hovered) return;

                // Register GSM: state transitions stop play mode; overlay ops work on the scene.
                Scripting::LuaManager& lua = engine.lua();
                sol::state& ls = lua.GetState();
                auto setOverlayVisible = [&](const std::string& tag, bool visible) {
                    m_scene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
                        [&](ECS::Entity, Components::Tag& t, Components::SpriteRenderer& s) {
                            if (t.tag == tag) s.visible = visible;
                        });
                    m_scene.GetWorld().each<Components::Tag, Components::TextRenderer>(
                        [&](ECS::Entity, Components::Tag& t, Components::TextRenderer& tr) {
                            if (t.tag == tag) tr.visible = visible;
                        });
                };
                ls["GSM"] = ls.create_table_with(
                    "pop",                [&]() { m_pendingStop = true; },
                    "switch_to_gameplay", [&]() { m_pendingStop = true; },
                    "switch_to_menu",     [&]() { m_pendingStop = true; },
                    "push_settings",      [&]() { m_pendingStop = true; },
                    "open_options",       [&]() { m_pendingStop = true; },
                    "quit",               [&]() { m_pendingStop = true; },
                    "show_how_to_play", [&, setOverlayVisible]() {
                        m_scene.GetWorld().each<Components::Transform, Components::UIButton>(
                            [](ECS::Entity, Components::Transform&, Components::UIButton& btn) {
                                btn.interactable = false;
                            });
                        setOverlayVisible("howtoplay_overlay", true);
                    },
                    "show_overlay", [&, setOverlayVisible](const std::string& tag) {
                        setOverlayVisible(tag, true);
                    },
                    "hide_overlay", [&, setOverlayVisible](const std::string& tag) {
                        setOverlayVisible(tag, false);
                    }
                );
                if (lua.ExecuteScript("assets/scripts/" + btn.uiScript))
                    lua.CallFunction("on_click");
            });
    }

    m_scene.Update(dt, m_input);
    // Camera follow is handled by CameraSystem inside Scene::Update().
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

    m_scene.RenderAll(renderer, camera.viewProj());

    if (m_showColliders) {
        renderer.beginDebugPass(camera.viewProj());
        auto& world = m_scene.GetWorld();
        world.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity, Components::Transform& t, Components::Collider& c) {
                auto rotVec = [](glm::vec2 v, float a) {
                    return glm::vec2(v.x * std::cos(a) - v.y * std::sin(a),
                                    v.x * std::sin(a) + v.y * std::cos(a));
                };
                glm::vec2 offset = rotVec(
                    glm::vec2(c.offset.x - t.pivot.x, c.offset.y - t.pivot.y),
                    t.rotation);
                glm::vec2 pos(t.position.x + offset.x, t.position.y + offset.y);

                glm::vec3 col = c.isTrigger
                    ? glm::vec3(1.f, 1.f, 0.f)
                    : glm::vec3(0.f, 1.f, 0.f);

                if (c.shape == Components::Collider::Shape::Box) {
                    glm::vec2 half(c.size.x * t.scale.x * 0.5f,
                                   c.size.y * t.scale.y * 0.5f);
                    glm::vec2 p1 = rotVec({-half.x, -half.y}, t.rotation) + pos;
                    glm::vec2 p2 = rotVec({ half.x, -half.y}, t.rotation) + pos;
                    glm::vec2 p3 = rotVec({ half.x,  half.y}, t.rotation) + pos;
                    glm::vec2 p4 = rotVec({-half.x,  half.y}, t.rotation) + pos;
                    renderer.drawLine(p1.x, p1.y, p2.x, p2.y, col.r, col.g, col.b);
                    renderer.drawLine(p2.x, p2.y, p3.x, p3.y, col.r, col.g, col.b);
                    renderer.drawLine(p3.x, p3.y, p4.x, p4.y, col.r, col.g, col.b);
                    renderer.drawLine(p4.x, p4.y, p1.x, p1.y, col.r, col.g, col.b);
                } else {
                    float r = c.radius * std::max(t.scale.x, t.scale.y);
                    renderer.drawCircle(pos.x, pos.y, r, col.r, col.g, col.b);
                }
            });
        renderer.endDebugPass();
    }
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
