/******************************************************************************/
/*!
\file           EditorState.cpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Generic engine-level editor

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/EditorState.hpp"
#include "Runtime/EditorHubState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Assets/TextureManager.hpp"
#include "Components/Transform.hpp"
#include "Components/Tag.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Collider.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_internal.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Runtime {

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static vec2 snapToGrid(const vec2& pos, float gridSize) {
    return { std::round(pos.x / gridSize) * gridSize,
             std::round(pos.y / gridSize) * gridSize };
}

static vec2 worldToScreen(const vec2& worldPos, const vec2& camPos,
                           float zoom, const vec2& viewportMin,
                           const vec2& viewportSize) {
    vec2 delta = worldPos - camPos;
    vec2 center = viewportMin + viewportSize * 0.5f;
    return center + vec2(delta.x * zoom, -delta.y * zoom);
}

static vec2 screenToWorld(const vec2& screenPos, const vec2& camPos,
                           float zoom, const vec2& viewportMin,
                           const vec2& viewportSize) {
    vec2 local = screenPos - viewportMin;
    vec2 centered = local - viewportSize * 0.5f;
    return camPos + vec2(centered.x / zoom, -centered.y / zoom);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

EditorState::EditorState(Platform::InputManager& input,
                         GameStateManager& gsm,
                         std::string scenePath)
    : m_input(input), m_gsm(gsm), m_sceneFilePath(std::move(scenePath)) {}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void EditorState::onEnter() {
    m_framebuffer.create(800, 600);

    m_undoStack.clear();
    m_redoStack.clear();

    m_scene.Init();

    if (!m_sceneFilePath.empty())
        loadScene(m_sceneFilePath);

    auto& camera = Core::Engine::Get().camera();
    m_editorCamPos  = {0.0f, 0.0f};
    m_editorCamZoom = 1.0f;
    camera.setPosition(m_editorCamPos);
    camera.setZoom(m_editorCamZoom);
}

void EditorState::onExit() {
    m_framebuffer.destroy();
    m_scene.Clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedEntity        = {0, 0};
    m_gizmoActiveAxis       = -1;
    m_isRenaming            = false;
    m_dockLayoutInitialized = false;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void EditorState::update(double dt) {
    // Deferred: return to hub
    if (m_pendingReturnToHub) {
        m_pendingReturnToHub = false;
        m_gsm.switchToAndClear(
            std::make_shared<EditorHubState>(m_gsm, m_input));
        return;
    }

    // Deferred: launch play mode
    if (m_pendingPlay) {
        m_pendingPlay = false;
        auto& engine = Core::Engine::Get();
        if (engine.playFactory()) {
            saveScene(); // auto-save before play
            m_gsm.switchTo(engine.playFactory()(m_sceneFilePath));
        }
        return;
    }

    // Deferred: save
    if (m_pendingSave) {
        m_pendingSave = false;
        saveScene();
    }

    // Undo/Redo
    bool ctrl = m_input.isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
                m_input.isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    if (ctrl && m_input.wasKeyTriggered(GLFW_KEY_Z))
        undo();
    if (ctrl && m_input.wasKeyTriggered(GLFW_KEY_Y))
        redo();

    // Ctrl+S
    if (ctrl && m_input.wasKeyTriggered(GLFW_KEY_S))
        m_pendingSave = true;

    // Editor camera
    auto& camera = Core::Engine::Get().camera();

    float scroll = static_cast<float>(Platform::InputManager::GetScrollY());
    if (m_viewportHovered && scroll != 0.0f) {
        float newZoom = m_editorCamZoom + (scroll * 0.1f * m_editorCamZoom);
        m_editorCamZoom = glm::clamp(newZoom, 0.05f, 10.0f);
        camera.setZoom(m_editorCamZoom);
    }

    static double lastMouseX = 0.0, lastMouseY = 0.0;
    double cx = 0.0, cy = 0.0;
    m_input.getMousePosition(cx, cy);

    if (m_viewportHovered && m_input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
        float dx = static_cast<float>(cx - lastMouseX);
        float dy = static_cast<float>(cy - lastMouseY);
        float ps = 1.0f / m_editorCamZoom;
        m_editorCamPos.x -= dx * ps;
        m_editorCamPos.y += dy * ps;
        camera.setPosition(m_editorCamPos);
    }
    lastMouseX = cx;
    lastMouseY = cy;

    if (m_viewportFocused && m_input.isKeyDown(GLFW_KEY_SPACE)) {
        float sp = 500.0f / m_editorCamZoom * static_cast<float>(dt);
        if (m_input.isKeyDown(GLFW_KEY_W)) m_editorCamPos.y += sp;
        if (m_input.isKeyDown(GLFW_KEY_S)) m_editorCamPos.y -= sp;
        if (m_input.isKeyDown(GLFW_KEY_A)) m_editorCamPos.x -= sp;
        if (m_input.isKeyDown(GLFW_KEY_D)) m_editorCamPos.x += sp;
        camera.setPosition(m_editorCamPos);
    }

    // F: focus on selected entity
    if (m_viewportFocused && m_input.wasKeyTriggered(GLFW_KEY_F)) {
        auto* t = m_scene.GetWorld().getComponent<Components::Transform>(m_selectedEntity);
        if (t) {
            m_editorCamPos  = {t->position.x, t->position.y};
            m_editorCamZoom = 1.0f;
            camera.setPosition(m_editorCamPos);
            camera.setZoom(m_editorCamZoom);
        }
    }

    // Delete key: delete selected entity
    if (m_viewportFocused && m_input.wasKeyTriggered(GLFW_KEY_DELETE)) {
        auto& world = m_scene.GetWorld();
        if (world.isValid(m_selectedEntity)) {
            executeCommand(std::make_unique<DeleteEntityCmd>(world, m_selectedEntity));
            m_selectedEntity = {0, 0};
        }
    }
}

// ---------------------------------------------------------------------------
// Render (framebuffer)
// ---------------------------------------------------------------------------

void EditorState::render(Core::Time* timer) {
    (void)timer;

    m_framebuffer.bind();

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto& engine   = Core::Engine::Get();
    auto& renderer = engine.rendering();
    auto& camera   = engine.camera();

    renderer.beginScenePass(camera.viewProj(), glm::vec4(1.0f));
    m_scene.Render(renderer);
    renderer.endScenePass();

    // Grid (debug pass, drawn on top of scene)
    if (m_showGrid) {
        renderer.beginDebugPass(camera.viewProj());
        renderGrid();
        renderer.endDebugPass();
    }

    m_framebuffer.unbind();
}

// ---------------------------------------------------------------------------
// renderImGui
// ---------------------------------------------------------------------------

void EditorState::renderImGui(Core::Time* timer) {
    (void)timer;

    renderToolbar(); // must be before dockspace (BeginMainMenuBar is global)

    Core::Engine::Get().imguiManager().beginDockspace();

    // One-time layout — must use "MyDockSpace" to match ImGuiManager
    ImGuiID dockId = ImGui::GetID("MyDockSpace");
    if (!m_dockLayoutInitialized) {
        m_dockLayoutInitialized = true;

        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->Size);

        ImGuiID main  = dockId;
        ImGuiID left  = 0, center = 0, right = 0, rightTop = 0, rightBot = 0;

        ImGui::DockBuilderSplitNode(main,   ImGuiDir_Left,  0.20f, &left,  &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, &right, &center);
        ImGui::DockBuilderSplitNode(right,  ImGuiDir_Down,  0.40f, &rightBot, &rightTop);

        ImGui::DockBuilderDockWindow("Hierarchy",              left);
        ImGui::DockBuilderDockWindow("Systems",                left);
        ImGui::DockBuilderDockWindow("Scene##EditorViewport",  center);
        ImGui::DockBuilderDockWindow("Inspector",              rightTop);
        ImGui::DockBuilderDockWindow("Assets",                 rightBot);

        ImGui::DockBuilderFinish(dockId);
    }

    renderSystemsWindow();
    renderHierarchy();
    renderInspector();
    renderViewport();
    renderAssetBrowser();

    Core::Engine::Get().imguiManager().endDockspace();
}

// ---------------------------------------------------------------------------
// renderToolbar  (main menu bar)
// ---------------------------------------------------------------------------

void EditorState::renderToolbar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) m_pendingSave = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Return to Hub"))        m_pendingReturnToHub = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) redo();
        ImGui::EndMenu();
    }

    ImGui::Separator();

    // Quick buttons
    if (ImGui::Button(" Hub "))  m_pendingReturnToHub = true;
    if (ImGui::IsItemHovered())  ImGui::SetTooltip("Return to Editor Hub");
    ImGui::SameLine();
    if (ImGui::Button(" Save ")) m_pendingSave = true;
    if (ImGui::IsItemHovered())  ImGui::SetTooltip("Save scene (Ctrl+S)");

    ImGui::Separator(); ImGui::SameLine();

    // Play button
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.60f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.80f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.45f, 0.10f, 1.0f));
    if (ImGui::Button(" Play "))  m_pendingPlay = true;
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())  ImGui::SetTooltip("Save & play the scene");

    ImGui::Separator(); ImGui::SameLine();

    // Gizmo mode
    ImGui::Text("Gizmo:");
    ImGui::SameLine();
    if (ImGui::RadioButton("None",  m_gizmoMode == GizmoMode::None))
        m_gizmoMode = GizmoMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("Move",  m_gizmoMode == GizmoMode::Translation))
        m_gizmoMode = GizmoMode::Translation;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", m_gizmoMode == GizmoMode::Rotation))
        m_gizmoMode = GizmoMode::Rotation;

    ImGui::Separator(); ImGui::SameLine();

    // Grid controls
    ImGui::Checkbox("Grid", &m_showGrid);
    if (m_showGrid) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::DragFloat("##GridSize", &m_gridSize, 1.0f, 10.0f, 500.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size");
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_snapToGrid);
    }

    ImGui::Separator(); ImGui::SameLine();

    // Undo/redo buttons
    if (!m_undoStack.empty()) {
        if (ImGui::Button("Undo")) undo();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Undo: %s", m_undoStack.back()->label());
    }
    if (!m_redoStack.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Redo")) redo();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Redo: %s", m_redoStack.back()->label());
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextDisabled("%s", m_sceneFilePath.c_str());

    ImGui::EndMainMenuBar();
}

// ---------------------------------------------------------------------------
// renderSystemsWindow
// ---------------------------------------------------------------------------

void EditorState::renderSystemsWindow() {
    ImGui::Begin("Systems");

    ImGui::Text("Engine Systems");
    ImGui::Separator();
    ImGui::Checkbox("Render",       &m_scene.m_renderSystemEnabled);
    ImGui::Checkbox("Physics",      &m_scene.m_physicsSystemEnabled);
    ImGui::Checkbox("Collision",    &m_scene.m_collisionSystemEnabled);
    ImGui::Checkbox("Audio",        &m_scene.m_audioSystemEnabled);
    ImGui::Checkbox("Animation",    &m_scene.m_animSystemEnabled);
    ImGui::Checkbox("StateMachine", &m_scene.m_stateMachineSystemEnabled);
    ImGui::Checkbox("TextRender",   &m_scene.m_textRenderSystemEnabled);
    ImGui::Checkbox("Script",       &m_scene.m_scriptSystemEnabled);

    ImGui::Spacing();
    ImGui::Text("Entity count: %u", m_scene.GetWorld().entityCount());

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderHierarchy
// ---------------------------------------------------------------------------

void EditorState::renderHierarchy() {
    ImGui::Begin("Hierarchy");

    auto& world = m_scene.GetWorld();

    // Create entity
    if (ImGui::Button("+ Entity")) {
        EntitySnapshot snap;
        snap.tagName  = "Entity";
        snap.transform = Components::Transform{};
        auto cmd = std::make_unique<CreateEntityCmd>(world, snap);
        auto* rawCmd = cmd.get();
        executeCommand(std::move(cmd));
        m_selectedEntity = rawCmd->createdEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Sprite")) {
        EntitySnapshot snap;
        snap.tagName           = "Sprite";
        snap.transform         = Components::Transform{};
        snap.hasSpriteRenderer = true;
        snap.spriteRenderer    = Components::SpriteRenderer{};
        auto cmd = std::make_unique<CreateEntityCmd>(world, snap);
        auto* rawCmd = cmd.get();
        executeCommand(std::move(cmd));
        m_selectedEntity = rawCmd->createdEntity();
    }

    ImGui::Separator();

    // Collect entities by Tag
    struct EntityRow { ECS::Entity entity; std::string displayName; };
    std::vector<EntityRow> rows;

    world.each<Components::Tag>([&](ECS::Entity e, Components::Tag& tag) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s##%u", tag.tag.c_str(), e.id);
        rows.push_back({e, buf});
    });
    world.each<Components::Transform>([&](ECS::Entity e, Components::Transform&) {
        if (!world.hasComponent<Components::Tag>(e)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Entity %u##%u", e.id, e.id);
            rows.push_back({e, buf});
        }
    });

    for (auto& row : rows) {
        bool selected = (m_selectedEntity.id == row.entity.id &&
                         m_selectedEntity.generation == row.entity.generation);

        // Inline rename
        if (m_isRenaming &&
            m_renamingEntity.id == row.entity.id &&
            m_renamingEntity.generation == row.entity.generation) {
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
                if (auto* tag = world.getComponent<Components::Tag>(row.entity))
                    tag->tag = m_renameBuffer;
                m_isRenaming = false;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                m_isRenaming = false;
            continue;
        }

        if (ImGui::Selectable(row.displayName.c_str(), selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            m_selectedEntity = row.entity;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_isRenaming     = true;
                m_renamingEntity = row.entity;
                if (auto* tag = world.getComponent<Components::Tag>(row.entity))
                    strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_isRenaming     = true;
                m_renamingEntity = row.entity;
                if (auto* tag = world.getComponent<Components::Tag>(row.entity))
                    strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
            }
            if (ImGui::MenuItem("Clone")) {
                EntitySnapshot snap = EntitySnapshot::capture(world, row.entity);
                // Offset position slightly
                snap.transform.position.x += m_gridSize;
                auto cmd = std::make_unique<CreateEntityCmd>(world, snap);
                auto* rawCmd = cmd.get();
                executeCommand(std::move(cmd));
                m_selectedEntity = rawCmd->createdEntity();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                if (selected) m_selectedEntity = {0, 0};
                executeCommand(std::make_unique<DeleteEntityCmd>(world, row.entity));
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderInspector
// ---------------------------------------------------------------------------

void EditorState::renderInspector() {
    ImGui::Begin("Inspector");

    auto& world = m_scene.GetWorld();

    if (!world.isValid(m_selectedEntity)) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    // Tag
    if (auto* tag = world.getComponent<Components::Tag>(m_selectedEntity)) {
        if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
            char buf[256];
            strncpy_s(buf, tag->tag.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText("Name", buf, sizeof(buf)))
                tag->tag = buf;
        }
    }

    // Transform
    if (auto* t = world.getComponent<Components::Transform>(m_selectedEntity)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            float pos[2] = {t->position.x, t->position.y};
            if (ImGui::DragFloat2("Position", pos, 1.0f)) {
                t->position = {pos[0], pos[1]};
            }
            // Undo on release
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                vec2 newPos = {pos[0], pos[1]};
                vec2 oldPos = m_gizmoOriginalPos; // reuse for non-gizmo edits
                executeCommand(std::make_unique<MoveEntityCmd>(
                    world, m_selectedEntity, oldPos, newPos));
            }
            if (ImGui::IsItemActivated())
                m_gizmoOriginalPos = t->position;

            float rot = t->rotation;
            if (ImGui::DragFloat("Rotation (rad)", &rot, 0.01f)) {
                t->rotation = rot;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                executeCommand(std::make_unique<RotateEntityCmd>(
                    world, m_selectedEntity, m_gizmoOriginalRotation, rot));
            }
            if (ImGui::IsItemActivated())
                m_gizmoOriginalRotation = t->rotation;

            float scale[2] = {t->scale.x, t->scale.y};
            if (ImGui::DragFloat2("Scale", scale, 0.01f))
                t->scale = {scale[0], scale[1]};

            float pivot[2] = {t->pivot.x, t->pivot.y};
            if (ImGui::DragFloat2("Pivot", pivot, 0.5f))
                t->pivot = {pivot[0], pivot[1]};
        }
    }

    // SpriteRenderer
    if (world.hasComponent<Components::SpriteRenderer>(m_selectedEntity)) {
        auto* sr = world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
        if (ImGui::CollapsingHeader("SpriteRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            char texBuf[256];
            strncpy_s(texBuf, sr->textureId.c_str(), sizeof(texBuf) - 1);
            if (ImGui::InputText("Texture ID", texBuf, sizeof(texBuf)))
                sr->textureId = texBuf;

            // Show texture thumbnail if loaded
            auto tex = Assets::TextureManager::Get(sr->textureId);
            if (tex && tex->id() != 0) {
                ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(48, 48));
                ImGui::SameLine();
            }

            float sz[2] = {sr->size.x, sr->size.y};
            if (ImGui::DragFloat2("Size", sz, 1.0f))
                sr->size = {sz[0], sz[1]};

            ImGui::ColorEdit4("Color", glm::value_ptr(sr->color));
            ImGui::Checkbox("Visible", &sr->visible);
            ImGui::DragInt("Sort Order", &sr->sortOrder);
        }
    } else {
        if (ImGui::Button("+ Add SpriteRenderer"))
            world.addComponent(m_selectedEntity, Components::SpriteRenderer{});
    }

    // Collider (read-only)
    if (world.hasComponent<Components::Collider>(m_selectedEntity)) {
        auto* col = world.getComponent<Components::Collider>(m_selectedEntity);
        if (ImGui::CollapsingHeader("Collider")) {
            ImGui::Text("Shape: %s",
                col->shape == Components::Collider::Shape::Box ? "Box" : "Circle");
            float sz[2] = {col->size.x, col->size.y};
            ImGui::DragFloat2("Size##col", sz, 1.0f);
            col->size = {sz[0], sz[1]};
            ImGui::DragFloat("Radius", &col->radius, 1.0f);
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderViewport  (scene image + entity picking + gizmos)
// ---------------------------------------------------------------------------

void EditorState::renderViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scene##EditorViewport", nullptr,
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);

    m_viewportFocused = ImGui::IsWindowFocused();
    m_viewportHovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x != m_viewportSize.x || avail.y != m_viewportSize.y) {
        m_viewportSize = {avail.x, avail.y};
        m_framebuffer.resize(static_cast<int>(avail.x),
                             static_cast<int>(avail.y));
    }
    if (m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)
        Core::Engine::Get().camera().setViewport(
            static_cast<int>(m_viewportSize.x),
            static_cast<int>(m_viewportSize.y));

    ImGui::Image(reinterpret_cast<void*>(static_cast<uint64_t>(
                     m_framebuffer.getTextureID())),
                 avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    // Compute viewport bounds (for gizmos + picking)
    ImVec2 winPos    = ImGui::GetWindowPos();
    ImVec2 contMin   = ImGui::GetWindowContentRegionMin();
    vec2   vpMin     = {winPos.x + contMin.x, winPos.y + contMin.y};

    // ----- Entity picking (left-click, no active gizmo) -----
    if (m_viewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        m_gizmoActiveAxis == -1) {
        ImVec2 mp    = ImGui::GetMousePos();
        vec2   mPos  = {mp.x, mp.y};
        auto&  cam   = Core::Engine::Get().camera();
        vec2   camP  = {cam.position().x, cam.position().y};
        vec2   world = screenToWorld(mPos, camP, m_editorCamZoom, vpMin, m_viewportSize);

        auto& ecs    = m_scene.GetWorld();
        float best   = 60.0f; // pick radius in world units
        ECS::Entity bestEnt{0, 0};

        ecs.each<Components::Transform>([&](ECS::Entity e, Components::Transform& t) {
            float d = magnitude(vec2(t.position.x, t.position.y) - world);
            if (d < best) { best = d; bestEnt = e; }
        });

        m_selectedEntity = bestEnt;
    }

    // ----- Drag-drop: texture from Asset Browser → spawn entity -----
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
            const char* texName = static_cast<const char*>(pl->Data);
            ImVec2 mp = ImGui::GetMousePos();
            auto&  cam = Core::Engine::Get().camera();
            vec2   camP = {cam.position().x, cam.position().y};
            vec2   wPos = screenToWorld({mp.x, mp.y}, camP, m_editorCamZoom, vpMin, m_viewportSize);
            if (m_snapToGrid) wPos = snapToGrid(wPos, m_gridSize);

            EntitySnapshot snap;
            snap.tagName           = texName;
            snap.transform.position = wPos;
            snap.hasSpriteRenderer  = true;
            snap.spriteRenderer.textureId = texName;
            snap.spriteRenderer.size      = {100.0f, 100.0f};

            auto cmd = std::make_unique<CreateEntityCmd>(m_scene.GetWorld(), snap);
            auto* raw = cmd.get();
            executeCommand(std::move(cmd));
            m_selectedEntity = raw->createdEntity();
        }
        ImGui::EndDragDropTarget();
    }

    // ----- Gizmos -----
    renderGizmos(vpMin);

    ImGui::End();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// renderGrid  (called inside beginDebugPass / endDebugPass)
// ---------------------------------------------------------------------------

void EditorState::renderGrid() {
    auto&  cam     = Core::Engine::Get().camera();
    auto&  renderer = Core::Engine::Get().rendering();
    vec2   camPos  = {cam.position().x, cam.position().y};
    float  zoom    = cam.zoom();

    float worldW = m_viewportSize.x / zoom;
    float worldH = m_viewportSize.y / zoom;

    float left   = camPos.x - worldW * 0.6f;
    float right  = camPos.x + worldW * 0.6f;
    float bottom = camPos.y - worldH * 0.6f;
    float top    = camPos.y + worldH * 0.6f;

    float startX = std::floor(left   / m_gridSize) * m_gridSize;
    float endX   = std::ceil(right   / m_gridSize) * m_gridSize;
    float startY = std::floor(bottom / m_gridSize) * m_gridSize;
    float endY   = std::ceil(top     / m_gridSize) * m_gridSize;

    for (float x = startX; x <= endX; x += m_gridSize)
        renderer.drawLine(x, bottom, x, top,   0.25f, 0.25f, 0.25f);
    for (float y = startY; y <= endY; y += m_gridSize)
        renderer.drawLine(left, y, right, y,   0.25f, 0.25f, 0.25f);

    // Origin axes
    renderer.drawLine(0.0f, bottom, 0.0f, top,   0.0f, 0.8f, 0.0f); // Y - green
    renderer.drawLine(left, 0.0f,   right, 0.0f, 0.8f, 0.0f, 0.0f); // X - red
}

// ---------------------------------------------------------------------------
// renderGizmos  (ImDrawList overlay on the viewport)
// ---------------------------------------------------------------------------

void EditorState::renderGizmos(const vec2& viewportMin) {
    if (m_gizmoMode == GizmoMode::None) return;

    auto& world = m_scene.GetWorld();
    if (!world.isValid(m_selectedEntity)) return;

    auto* trans = world.getComponent<Components::Transform>(m_selectedEntity);
    if (!trans) return;

    auto&       cam    = Core::Engine::Get().camera();
    vec2        camPos = {cam.position().x, cam.position().y};
    float       zoom   = cam.zoom();

    vec2 objScreen = worldToScreen(
        {trans->position.x, trans->position.y}, camPos, zoom,
        viewportMin, m_viewportSize);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin  = {objScreen.x, objScreen.y};

    ImVec2 mousePos    = ImGui::GetMousePos();
    bool   mouseClick  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool   mouseRel    = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool   mouseDown   = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // Record which axis was released so we create an undo command
    int releasedAxis = -1;
    if (mouseRel && m_gizmoActiveAxis != -1) {
        releasedAxis    = m_gizmoActiveAxis;
        m_gizmoActiveAxis = -1;
    } else if (mouseRel) {
        m_gizmoActiveAxis = -1;
    }

    constexpr float kLen    = 60.0f;
    constexpr float kThick  = 5.0f;
    constexpr float kHitR   = 8.0f;

    ImU32 colX     = IM_COL32(200,  50,  50, 255);
    ImU32 colY     = IM_COL32( 50, 200,  50, 255);
    ImU32 colZ     = IM_COL32( 50,  50, 200, 255);
    ImU32 colHover = IM_COL32(255, 255,   0, 255);

    // Helper: closest point on segment
    auto distToSeg = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
        float dx = b.x - a.x, dy = b.y - a.y;
        float l2 = dx * dx + dy * dy;
        if (l2 == 0.0f)
            return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = std::max(0.0f, std::min(1.0f,
            ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2));
        float px = a.x + t * dx, py = a.y + t * dy;
        return std::sqrt((p.x - px) * (p.x - px) + (p.y - py) * (p.y - py));
    };

    if (m_gizmoMode == GizmoMode::Translation) {
        float angle = trans->rotation;
        float c = std::cos(angle), s = std::sin(angle);
        vec2 xAxisScr = {c, -s};
        vec2 yAxisScr = {-s, -c};

        ImVec2 xEnd = {origin.x + xAxisScr.x * kLen, origin.y + xAxisScr.y * kLen};
        ImVec2 yEnd = {origin.x + yAxisScr.x * kLen, origin.y + yAxisScr.y * kLen};

        bool hoverX = distToSeg(mousePos, origin, xEnd) < kHitR;
        bool hoverY = distToSeg(mousePos, origin, yEnd) < kHitR;
        if (m_gizmoActiveAxis == 0) hoverX = true;
        if (m_gizmoActiveAxis == 1) hoverY = true;

        dl->AddLine(origin, xEnd, hoverX ? colHover : colX, kThick);
        dl->AddLine(origin, yEnd, hoverY ? colHover : colY, kThick);

        // Arrow heads
        auto drawArrow = [&](ImVec2 tip, vec2 dir, ImU32 col) {
            vec2 perp = {-dir.y, dir.x};
            dl->AddTriangleFilled(
                tip,
                {tip.x - dir.x * 10 + perp.x * 5, tip.y - dir.y * 10 + perp.y * 5},
                {tip.x - dir.x * 10 - perp.x * 5, tip.y - dir.y * 10 - perp.y * 5},
                col);
        };
        drawArrow(xEnd, normalize(xAxisScr), hoverX ? colHover : colX);
        drawArrow(yEnd, normalize(yAxisScr), hoverY ? colHover : colY);

        // Start drag
        if (mouseClick && (hoverX || hoverY)) {
            m_gizmoActiveAxis   = hoverX ? 0 : 1;
            m_gizmoOriginalPos  = {trans->position.x, trans->position.y};
            m_gizmoClickOffset  = {mousePos.x, mousePos.y};
        }

        // Drag
        if (mouseDown && m_gizmoActiveAxis != -1) {
            float dx = mousePos.x - m_gizmoClickOffset.x;
            float dy = mousePos.y - m_gizmoClickOffset.y;
            vec2 screenMove(dx, dy);
            vec2 axisDir = normalize((m_gizmoActiveAxis == 0) ? xAxisScr : yAxisScr);
            float proj      = dot(screenMove, axisDir) / zoom;
            vec2  worldDir  = (m_gizmoActiveAxis == 0) ? vec2(c, s) : vec2(-s, c);
            vec2  newPos    = m_gizmoOriginalPos + worldDir * proj;
            if (m_snapToGrid) newPos = snapToGrid(newPos, m_gridSize);
            trans->position = newPos;
        }

        // Create undo command on release
        if (releasedAxis != -1) {
            vec2 finalPos = {trans->position.x, trans->position.y};
            if (finalPos.x != m_gizmoOriginalPos.x ||
                finalPos.y != m_gizmoOriginalPos.y) {
                executeCommand(std::make_unique<MoveEntityCmd>(
                    world, m_selectedEntity, m_gizmoOriginalPos, finalPos));
            }
        }
    }
    else if (m_gizmoMode == GizmoMode::Rotation) {
        float radius = kLen * 0.8f;
        dl->AddCircle(origin, radius, colZ, 64, kThick * 0.5f);

        float dist = std::sqrt((mousePos.x - origin.x) * (mousePos.x - origin.x) +
                               (mousePos.y - origin.y) * (mousePos.y - origin.y));
        bool hover = std::abs(dist - radius) < 10.0f;
        if (m_gizmoActiveAxis == 2) hover = true;
        if (hover) dl->AddCircle(origin, radius, colHover, 64, kThick);

        if (mouseClick && hover) {
            m_gizmoActiveAxis         = 2;
            m_gizmoOriginalRotation   = trans->rotation;
            m_gizmoClickOffset.x      = std::atan2(mousePos.y - origin.y,
                                                    mousePos.x - origin.x);
        }

        if (mouseDown && m_gizmoActiveAxis == 2) {
            float currentAngle = std::atan2(mousePos.y - origin.y,
                                             mousePos.x - origin.x);
            float delta = currentAngle - m_gizmoClickOffset.x;
            trans->rotation = m_gizmoOriginalRotation + delta;
        }

        if (releasedAxis == 2) {
            if (trans->rotation != m_gizmoOriginalRotation)
                executeCommand(std::make_unique<RotateEntityCmd>(
                    world, m_selectedEntity,
                    m_gizmoOriginalRotation, trans->rotation));
        }
    }
}

// ---------------------------------------------------------------------------
// renderAssetBrowser
// ---------------------------------------------------------------------------

void EditorState::renderAssetBrowser() {
    ImGui::Begin("Assets");

    if (ImGui::Button("Add Texture")) {
        m_newAssetNameBuf[0] = '\0';
        m_newAssetPathBuf[0] = '\0';
        ImGui::OpenPopup("##AddTexture");
    }

    if (ImGui::BeginPopupModal("##AddTexture", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Load Texture Asset");
        ImGui::Separator();
        ImGui::InputText("ID",   m_newAssetNameBuf, sizeof(m_newAssetNameBuf));
        ImGui::InputText("Path", m_newAssetPathBuf, sizeof(m_newAssetPathBuf));
        if (ImGui::Button("Load", ImVec2(100, 0))) {
            if (m_newAssetNameBuf[0] && m_newAssetPathBuf[0])
                Assets::TextureManager::LoadTexture(m_newAssetNameBuf,
                                                    m_newAssetPathBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();

    auto names = Assets::TextureManager::GetLoadedTextureNames();
    float thumbSize  = 64.0f;
    float padding    = 8.0f;
    float cellSize   = thumbSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int   cols       = std::max(1, static_cast<int>(panelWidth / cellSize));

    if (ImGui::BeginTable("##TextureTbl", cols)) {
        for (const auto& name : names) {
            ImGui::TableNextColumn();
            ImGui::PushID(name.c_str());

            auto tex = Assets::TextureManager::Get(name);
            if (tex && tex->id() != 0) {
                bool isSelected = (m_selectedAsset == name);
                if (isSelected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));

                if (ImGui::ImageButton("##tb",
                        (ImTextureID)(intptr_t)tex->id(),
                        ImVec2(thumbSize, thumbSize)))
                    m_selectedAsset = name;

                if (isSelected)
                    ImGui::PopStyleColor();

                // Drag-drop source → viewport accepts it
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("TEXTURE_ASSET",
                                             name.c_str(), name.size() + 1);
                    ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(32, 32));
                    ImGui::SameLine();
                    ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Right-click: apply to selected entity's SpriteRenderer
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Apply to selected")) {
                        if (auto* sr = m_scene.GetWorld()
                                .getComponent<Components::SpriteRenderer>(m_selectedEntity))
                            sr->textureId = name;
                    }
                    if (ImGui::MenuItem("Unload"))
                        Assets::TextureManager::Unload(name);
                    ImGui::EndPopup();
                }

                // Tooltip: which entities use this texture
                if (ImGui::IsItemHovered() &&
                    !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
                    int count = 0;
                    m_scene.GetWorld().each<Components::SpriteRenderer>(
                        [&](ECS::Entity, Components::SpriteRenderer& sr) {
                            if (sr.textureId == name) ++count;
                        });
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", name.c_str());
                    ImGui::TextDisabled("Used by %d entities", count);
                    ImGui::EndTooltip();
                }
            }
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Scene helpers
// ---------------------------------------------------------------------------

void EditorState::loadScene(const std::string& path) {
    if (path.empty()) return;
    std::cout << "[EditorState] Loading: " << path << "\n";
    if (!m_scene.LoadFromFile(path))
        std::cerr << "[EditorState] Failed to load: " << path << "\n";
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedEntity = {0, 0};
}

void EditorState::saveScene() {
    if (m_sceneFilePath.empty()) {
        std::cerr << "[EditorState] No file path — cannot save.\n";
        return;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(m_sceneFilePath).parent_path(), ec);
    SceneSerializer::Serialize(m_scene, m_sceneFilePath);
    std::cout << "[EditorState] Saved: " << m_sceneFilePath << "\n";
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

void EditorState::executeCommand(std::unique_ptr<IEditorCommand> cmd) {
    cmd->execute();
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear();
    if (m_undoStack.size() > MAX_UNDO)
        m_undoStack.erase(m_undoStack.begin());
}

void EditorState::undo() {
    if (m_undoStack.empty()) return;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->undo();
    m_redoStack.push_back(std::move(cmd));
}

void EditorState::redo() {
    if (m_redoStack.empty()) return;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->execute();
    m_undoStack.push_back(std::move(cmd));
}

} // namespace Runtime
