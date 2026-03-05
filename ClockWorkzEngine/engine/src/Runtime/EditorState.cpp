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
#include "Assets/SoundManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Gameplay/Prefab.hpp"
#include "Assets/FontManager.hpp"
#include "Graphics/Shader.hpp"
#include "Memory/MemoryDebugRegistry.hpp"
#include "Memory/IPoolStats.hpp"
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
#include <fstream>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <commdlg.h>
#endif
#include <iostream>
#include <cmath>
#include <algorithm>

// Engine components
#include "Components/Velocity.hpp"
#include "Components/AudioSource.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/LuaScript.hpp"

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

template<typename T>
static void addCompEntry(const char* label, ECS::Entity entity, ECS::World& world) {
    bool has = world.hasComponent<T>(entity);
    if (ImGui::MenuItem(label, nullptr, false, !has))
        world.addComponent(entity, T{});
    if (has) { ImGui::SameLine(); ImGui::TextDisabled("(present)"); }
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

    // Derive the project root and engine assets root from the scene file path.
    // Scene path structure: <projectRoot>/assets/scenes/<name>.json
    // Engine assets (shaders, fonts) live in <projectRoot>/../../assets/
    {
        namespace fs = std::filesystem;
        fs::path sceneFs(m_sceneFilePath);
        m_projectRoot = sceneFs.parent_path()  // .../assets/scenes/
                               .parent_path()  // .../assets/
                               .parent_path()  // <projectRoot>/
                               .generic_string();
        m_engineAssetsRoot = (fs::path(m_projectRoot).parent_path()   // projects/
                                                      .parent_path()   // game/
                              / "assets").generic_string();

        // Change CWD to project root so ALL "assets/..." relative paths
        // (scripts, textures, audio, etc.) resolve to this project's folder.
        m_originalCwd = fs::current_path().generic_string();
        std::error_code cwdEc;
        fs::current_path(m_projectRoot, cwdEc);
    }

    // Load shaders required by the rendering passes — use absolute paths since
    // shaders live in game/assets/, not inside the project folder.
    Assets::ResourceManager<Graphics::Shader>::Load(
        "texture",
        m_engineAssetsRoot + "/shaders/texture.vert",
        m_engineAssetsRoot + "/shaders/texture.frag");
    Assets::ResourceManager<Graphics::Shader>::Load(
        "light",
        m_engineAssetsRoot + "/shaders/light.vert",
        m_engineAssetsRoot + "/shaders/light.frag");
    Assets::ResourceManager<Graphics::Shader>::Load(
        "text",
        m_engineAssetsRoot + "/shaders/text.vert",
        m_engineAssetsRoot + "/shaders/text.frag");
    Assets::ResourceManager<Assets::FontManager>::Load(
        "Debug", m_engineAssetsRoot + "/fonts/MANIC1.ttf", 24);

    // Load project-specific assets (CWD is now the project root, so all
    // "assets/..." paths resolve to this project's assets/ folder).
    Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");
    Assets::SoundManager::LoadFromConfig("assets/configs/sounds.json");
    Gameplay::PrefabFactory::get().loadPrefabFromFile("assets/configs/Prefab.json");

    m_scene.Init();

    // Defer the initial scene load to the first update() tick, so all
    // engine systems (audio, etc.) are running before Clear()/Deserialize.
    if (!m_sceneFilePath.empty())
        m_pendingLoadPath = m_sceneFilePath;

    auto& camera = Core::Engine::Get().camera();
    m_editorCamPos  = {0.0f, 0.0f};
    m_editorCamZoom = 1.0f;
    camera.setPosition(m_editorCamPos);
    camera.setZoom(m_editorCamZoom);
}

void EditorState::onExit() {
    // Unload project-specific assets before switching projects.
    Assets::TextureManager::Clear();
    Assets::SoundManager::Clear();

    // Restore CWD so EditorHubState can find its registry at the original path.
    if (!m_originalCwd.empty()) {
        std::error_code ec;
        std::filesystem::current_path(m_originalCwd, ec);
        m_originalCwd.clear();
    }

    m_framebuffer.destroy();
    m_scene.Clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedEntity        = {0, 0};
    m_gizmoActiveAxis       = -1;
    m_isRenaming            = false;
    m_dockLayoutInitialized = false;
    m_pendingLoadPath.clear();
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

    // Deferred: load scene
    if (!m_pendingLoadPath.empty()) {
        m_sceneFilePath = m_pendingLoadPath;
        m_pendingLoadPath.clear();
        loadScene(m_sceneFilePath);
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

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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

        ImGuiID main      = dockId;
        ImGuiID left      = 0, center    = 0;
        ImGuiID centerTop = 0, centerBot = 0;
        ImGuiID right     = 0, rightTop  = 0;

        ImGui::DockBuilderSplitNode(main,   ImGuiDir_Left,  0.20f, &left,      &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, &right,     &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.28f, &centerBot, &centerTop);
        // right column is a single node (Inspector + Stats tabs — no split needed)
        rightTop = right;

        ImGui::DockBuilderDockWindow("Hierarchy",              left);
        ImGui::DockBuilderDockWindow("Systems",                left);
        ImGui::DockBuilderDockWindow("Scenes",                 left);
        ImGui::DockBuilderDockWindow("Scene##EditorViewport",  centerTop);
        ImGui::DockBuilderDockWindow("Assets",                 centerBot);
        ImGui::DockBuilderDockWindow("Scripts",                centerBot);
        ImGui::DockBuilderDockWindow("Audio",                  centerBot);
        ImGui::DockBuilderDockWindow("Inspector",              rightTop);
        ImGui::DockBuilderDockWindow("Stats",                  rightTop);

        ImGui::DockBuilderFinish(dockId);
    }

    renderSystemsWindow();
    renderHierarchy();
    renderLoadSceneWindow();
    renderInspector();
    renderViewport();
    renderAssetBrowser();
    renderStatsWindow();
    renderScriptsBrowser();
    renderAudioBrowser();

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
// renderStatsWindow  (ECS info + memory management, merged)
// ---------------------------------------------------------------------------

void EditorState::renderStatsWindow() {
    ImGui::Begin("Stats");

    auto& world = m_scene.GetWorld();

    // ── ECS ─────────────────────────────────────────────────────────────────
    ImGui::SeparatorText("ECS");
    ImGui::Text("Entities : %u", world.entityCount());
    ImGui::Spacing();
    ImGui::Text("Component Counts:");
    ImGui::Text("  Transform      : %zu", world.componentCount<Components::Transform>());
    ImGui::Text("  Velocity       : %zu", world.componentCount<Components::Velocity>());
    ImGui::Text("  SpriteRenderer : %zu", world.componentCount<Components::SpriteRenderer>());
    ImGui::Text("  SpriteAnimator : %zu", world.componentCount<Components::SpriteAnimator>());
    ImGui::Text("  Collider       : %zu", world.componentCount<Components::Collider>());
    ImGui::Text("  LuaScript      : %zu", world.componentCount<Components::LuaScript>());
    ImGui::Text("  AudioSource    : %zu", world.componentCount<Components::AudioSource>());

    ImGui::Spacing();
    ImGui::Text("FPS        : %.1f",      ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time : %.3f ms",   1000.0f / ImGui::GetIO().Framerate);

    // ── Memory ───────────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Memory");

    auto& reg = Memory::MemoryDebugRegistry::Get();
    auto  agg = reg.aggregate();

    ImGui::Text("Used     : %zu B  (%.2f KB)", agg.usedBytes,  agg.usedBytes  / 1024.0f);
    ImGui::Text("Capacity : %zu B  (%.2f KB)", agg.totalBytes, agg.totalBytes / 1024.0f);
    ImGui::Text("Peak     : %zu B  (%.2f KB)", agg.peakBytes,  agg.peakBytes  / 1024.0f);
    ImGui::Text("Blocks   : Used %zu  |  Free %zu  |  Total %zu",
        agg.usedBlocks, agg.freeBlocks, agg.usedBlocks + agg.freeBlocks);
    ImGui::Text("Pools registered : %zu", reg.poolCount());

    if (reg.leakDetected()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        ImGui::TextWrapped("!! LEAK DETECTED - at least one pool was destroyed "
                           "with unfreed blocks. Check console.");
        ImGui::PopStyleColor();
    }

    // Usage bar
    ImGui::Spacing();
    {
        float ratio = (agg.totalBytes > 0)
            ? static_cast<float>(agg.usedBytes) / static_cast<float>(agg.totalBytes)
            : 0.0f;
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.1f%%", ratio * 100.0f);
        ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), overlay);
    }

    // History graph
    static float s_history[128] = {};
    static int   s_offset       = 0;
    s_history[s_offset] = static_cast<float>(agg.usedBytes) / 1024.0f;
    s_offset = (s_offset + 1) % 128;

    ImGui::Spacing();
    float graphMax = (agg.totalBytes > 0)
        ? static_cast<float>(agg.totalBytes) / 1024.0f : 1.0f;
    ImGui::PlotLines("##memhist", s_history, 128, s_offset,
                     nullptr, 0.0f, graphMax, ImVec2(-1.0f, 60.0f));

    // Per-pool table
    ImGui::Spacing();
    ImGui::SeparatorText("Pools");

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##pools", 6, kTableFlags, ImVec2(0.0f, 140.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Pool",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("BlkSz", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Used",  ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("Free",  ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("Peak",  ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < reg.poolCount(); ++i) {
            const Memory::IPoolStats* p = reg.getPool(i);
            if (!p) continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (reg.leakDetected() && p->usedBlocks() > 0)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", p->poolName());
            else
                ImGui::TextUnformatted(p->poolName());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%zu B", p->blockSize());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%zu",   p->usedBlocks());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%zu",   p->freeBlocks());
            ImGui::TableSetColumnIndex(4); ImGui::Text("%zu",   p->totalBlocks());
            ImGui::TableSetColumnIndex(5); ImGui::Text("%zu",   p->peakUsedBlocks());
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderLoadSceneWindow
// ---------------------------------------------------------------------------

void EditorState::renderLoadSceneWindow() {
    ImGui::Begin("Scenes");

    if (m_sceneFilePath.empty()) {
        ImGui::TextDisabled("No scene loaded.");
        ImGui::End();
        return;
    }

    namespace fs = std::filesystem;
    fs::path sceneDir  = fs::path(m_sceneFilePath).parent_path();
    std::string curFile = fs::path(m_sceneFilePath).filename().string();

    // New scene button
    if (ImGui::Button("+ New Scene"))
        ImGui::OpenPopup("NewScenePopup##ns");

    // New scene modal
    if (ImGui::BeginPopupModal("NewScenePopup##ns", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Scene name:");
        ImGui::SetNextItemWidth(220.0f);
        bool hitEnter = ImGui::InputText("##newscenename", m_newSceneBuf,
                                         sizeof(m_newSceneBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        bool doCreate = false;
        if (ImGui::Button("Create") || hitEnter) doCreate = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_newSceneBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        if (doCreate && m_newSceneBuf[0] != '\0') {
            std::string name = m_newSceneBuf;
            // Append .json if missing
            if (name.size() < 5 || name.substr(name.size() - 5) != ".json")
                name += ".json";

            fs::path newPath = sceneDir / name;
            if (!fs::exists(newPath)) {
                // Write minimal empty scene JSON
                std::ofstream f(newPath.string());
                f << "{\n"
                  << "    \"entities\": [],\n"
                  << "    \"bgm\": \"\",\n"
                  << "    \"sfxVolume\": 1.0,\n"
                  << "    \"spawnPoint\": {\"x\": 0, \"y\": 0},\n"
                  << "    \"enableDarkness\": false,\n"
                  << "    \"darknessIntensity\": 0.0,\n"
                  << "    \"darknessColor\": {\"r\": 0, \"g\": 0, \"b\": 0}\n"
                  << "}\n";
                f.close();
                m_pendingLoadPath = newPath.string();
            }
            m_newSceneBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##scenefilter", m_loadSceneFilter, sizeof(m_loadSceneFilter));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter scenes");

    ImGui::Separator();

    std::string filterStr = m_loadSceneFilter;
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(sceneDir, ec)) {
        if (entry.path().extension() != ".json") continue;

        std::string fname   = entry.path().filename().string();
        std::string fnameLow = fname;
        std::transform(fnameLow.begin(), fnameLow.end(), fnameLow.begin(), ::tolower);
        if (!filterStr.empty() && fnameLow.find(filterStr) == std::string::npos)
            continue;

        bool isCurrent = (fname == curFile);
        if (isCurrent)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));

        if (ImGui::Selectable(fname.c_str(), isCurrent))
            m_pendingLoadPath = entry.path().string();

        if (isCurrent)
            ImGui::PopStyleColor();
    }

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

    // ── Add Component ───────────────────────────────────────────────────────
    if (ImGui::Button("Add Component", ImVec2(-1, 0)))
        ImGui::OpenPopup("##AddComp");

    if (ImGui::BeginPopup("##AddComp")) {
        if (ImGui::BeginMenu("Core")) {
            addCompEntry<Components::Velocity>("Velocity", m_selectedEntity, world);
            addCompEntry<Components::Tag>("Tag", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            addCompEntry<Components::SpriteRenderer>("SpriteRenderer", m_selectedEntity, world);
            addCompEntry<Components::SpriteAnimator>("SpriteAnimator", m_selectedEntity, world);
            addCompEntry<Components::TextRenderer>("TextRenderer", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics")) {
            addCompEntry<Components::Collider>("Collider", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio")) {
            addCompEntry<Components::AudioSource>("AudioSource", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scripting")) {
            addCompEntry<Components::LuaScript>("LuaScript", m_selectedEntity, world);
            addCompEntry<Components::StateMachine>("StateMachine", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

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
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_ASSET"))
                    sr->textureId = static_cast<const char*>(pl->Data);
                ImGui::EndDragDropTarget();
            }

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

    // ── Velocity ────────────────────────────────────────────────────────────
    if (world.hasComponent<Components::Velocity>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("Velocity##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rv")) rm = true;
        if (open) {
            auto* vel = world.getComponent<Components::Velocity>(m_selectedEntity);
            float vv[2] = {vel->velocity.x, vel->velocity.y};
            if (ImGui::DragFloat2("Velocity##vel", vv, 0.5f))
                vel->velocity = {vv[0], vv[1]};
            ImGui::DragFloat("MoveSpeed##vel", &vel->moveSpeed, 1.0f);
        }
        if (rm) world.removeComponent<Components::Velocity>(m_selectedEntity);
    }

    // ── AudioSource ─────────────────────────────────────────────────────────
    if (world.hasComponent<Components::AudioSource>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("AudioSource##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##ras")) rm = true;
        if (open) {
            auto* au = world.getComponent<Components::AudioSource>(m_selectedEntity);
            char sndbuf[256]; snprintf(sndbuf, sizeof(sndbuf), "%s", au->soundId.c_str());
            if (ImGui::InputText("Sound ID##au", sndbuf, sizeof(sndbuf))) au->soundId = sndbuf;
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("AUDIO_ASSET"))
                    au->soundId = static_cast<const char*>(pl->Data);
                ImGui::EndDragDropTarget();
            }
            ImGui::SliderFloat("Volume##au", &au->volume, 0.0f, 1.0f);
            ImGui::Checkbox("Loop##au", &au->loop);
            ImGui::SameLine();
            ImGui::Checkbox("PlayOnStart##au", &au->playOnStart);
        }
        if (rm) world.removeComponent<Components::AudioSource>(m_selectedEntity);
    }

    // ── SpriteAnimator ──────────────────────────────────────────────────────
    if (world.hasComponent<Components::SpriteAnimator>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("SpriteAnimator##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rsa")) rm = true;
        if (open) {
            auto* sa = world.getComponent<Components::SpriteAnimator>(m_selectedEntity);
            char fbuf[256]; snprintf(fbuf, sizeof(fbuf), "%s", sa->filename.c_str());
            if (ImGui::InputText("File##sa", fbuf, sizeof(fbuf))) sa->filename = fbuf;
            char cbuf[128]; snprintf(cbuf, sizeof(cbuf), "%s", sa->currentClip.c_str());
            if (ImGui::InputText("Current Clip##sa", cbuf, sizeof(cbuf))) sa->currentClip = cbuf;
            ImGui::Checkbox("Playing##sa", &sa->playing);
            ImGui::SameLine();
            ImGui::Checkbox("Paused##sa", &sa->paused);
        }
        if (rm) world.removeComponent<Components::SpriteAnimator>(m_selectedEntity);
    }

    // ── StateMachine ────────────────────────────────────────────────────────
    if (world.hasComponent<Components::StateMachine>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("StateMachine##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rsm")) rm = true;
        if (open) {
            auto* sm = world.getComponent<Components::StateMachine>(m_selectedEntity);
            char sbuf[128]; snprintf(sbuf, sizeof(sbuf), "%s", sm->currentState.c_str());
            if (ImGui::InputText("Current State##sm", sbuf, sizeof(sbuf))) sm->currentState = sbuf;
            ImGui::BeginDisabled(true);
            ImGui::DragFloat("State Timer##sm", &sm->stateTimer, 0.0f);
            ImGui::EndDisabled();
        }
        if (rm) world.removeComponent<Components::StateMachine>(m_selectedEntity);
    }

    // ── LuaScript ───────────────────────────────────────────────────────────
    if (world.hasComponent<Components::LuaScript>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("LuaScript##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rls")) rm = true;
        if (open) {
            auto* ls = world.getComponent<Components::LuaScript>(m_selectedEntity);
            char spbuf[256]; snprintf(spbuf, sizeof(spbuf), "%s", ls->scriptPath.c_str());
            if (ImGui::InputText("Script Path##ls", spbuf, sizeof(spbuf))) ls->scriptPath = spbuf;
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SCRIPT_ASSET"))
                    ls->scriptPath = static_cast<const char*>(pl->Data);
                ImGui::EndDragDropTarget();
            }
            ImGui::Checkbox("Enabled##ls", &ls->enabled);
        }
        if (rm) world.removeComponent<Components::LuaScript>(m_selectedEntity);
    }

    // ── TextRenderer ────────────────────────────────────────────────────────
    if (world.hasComponent<Components::TextRenderer>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("TextRenderer##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rtr")) rm = true;
        if (open) {
            auto* tr = world.getComponent<Components::TextRenderer>(m_selectedEntity);
            char tbuf[512]; snprintf(tbuf, sizeof(tbuf), "%s", tr->text.c_str());
            if (ImGui::InputTextMultiline("Text##tr", tbuf, sizeof(tbuf), ImVec2(-1, 60)))
                tr->text = tbuf;
            char trfbuf[128]; snprintf(trfbuf, sizeof(trfbuf), "%s", tr->fontId.c_str());
            if (ImGui::InputText("Font ID##tr", trfbuf, sizeof(trfbuf))) tr->fontId = trfbuf;
            ImGui::DragFloat("Font Size##tr", &tr->fontSize, 0.01f);
            ImGui::ColorEdit3("Color##tr", glm::value_ptr(tr->color));
            ImGui::Checkbox("Visible##tr", &tr->visible);
            const char* alignItems[] = {"Left", "Center", "Right"};
            int alignIdx = static_cast<int>(tr->alignment);
            if (ImGui::Combo("Alignment##tr", &alignIdx, alignItems, 3))
                tr->alignment = static_cast<Components::TextAlignment>(alignIdx);
        }
        if (rm) world.removeComponent<Components::TextRenderer>(m_selectedEntity);
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
        ImGui::Text("Import Texture");
        ImGui::Separator();

        // Source path row — editable + Browse button
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##srcpath", m_newAssetPathBuf, sizeof(m_newAssetPathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
#ifdef _WIN32
            OPENFILENAMEA ofn{};
            char szFile[512]{};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile   = szFile;
            ofn.nMaxFile    = sizeof(szFile);
            ofn.lpstrFilter =
                "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) {
                strncpy_s(m_newAssetPathBuf, szFile, sizeof(m_newAssetPathBuf) - 1);
                // Auto-fill asset ID from filename stem if not already set
                if (!m_newAssetNameBuf[0]) {
                    namespace fs = std::filesystem;
                    std::string stem = fs::path(szFile).stem().string();
                    strncpy_s(m_newAssetNameBuf, stem.c_str(),
                              sizeof(m_newAssetNameBuf) - 1);
                }
            }
#endif
        }
        ImGui::TextDisabled("Source file");

        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("Asset ID", m_newAssetNameBuf, sizeof(m_newAssetNameBuf));
        ImGui::TextDisabled("Copied to assets/textures/ on import");

        ImGui::Separator();
        bool canImport = m_newAssetNameBuf[0] && m_newAssetPathBuf[0];
        if (!canImport) ImGui::BeginDisabled();
        if (ImGui::Button("Import", ImVec2(100, 0))) {
            Assets::TextureManager::ImportTexture(m_newAssetPathBuf,
                                                  m_newAssetNameBuf);
            ImGui::CloseCurrentPopup();
        }
        if (!canImport) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Rename modal ─────────────────────────────────────────────────────────
    if (!m_renamingAsset.empty())
        ImGui::OpenPopup("##RenameAsset");

    if (ImGui::BeginPopupModal("##RenameAsset", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename asset: %s", m_renamingAsset.c_str());
        ImGui::Separator();
        ImGui::InputText("New ID", m_renameAssetBuf, sizeof(m_renameAssetBuf));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (m_renameAssetBuf[0] && m_renameAssetBuf != m_renamingAsset) {
                std::string oldPath = Assets::TextureManager::GetFilePath(m_renamingAsset);
                if (!oldPath.empty()) {
                    Assets::TextureManager::LoadTexture(m_renameAssetBuf, oldPath);
                    Assets::TextureManager::RemoveFromConfig(m_renamingAsset);
                    Assets::TextureManager::Unload(m_renamingAsset);
                }
            }
            m_renamingAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_renamingAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Delete confirmation ───────────────────────────────────────────────────
    if (!m_deletingAsset.empty())
        ImGui::OpenPopup("##DeleteAsset");

    if (ImGui::BeginPopupModal("##DeleteAsset", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' from disk?", m_deletingAsset.c_str());
        ImGui::TextDisabled("This permanently removes the file.");
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            Assets::TextureManager::DeleteFromDisk(m_deletingAsset);
            m_deletingAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_deletingAsset.clear();
            ImGui::CloseCurrentPopup();
        }
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

                // Right-click context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Apply to selected")) {
                        if (auto* sr = m_scene.GetWorld()
                                .getComponent<Components::SpriteRenderer>(m_selectedEntity))
                            sr->textureId = name;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename")) {
                        m_renamingAsset = name;
                        strncpy_s(m_renameAssetBuf, name.c_str(),
                                  sizeof(m_renameAssetBuf) - 1);
                    }
                    if (ImGui::MenuItem("Unload"))
                        Assets::TextureManager::Unload(name);
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    if (ImGui::MenuItem("Delete from disk"))
                        m_deletingAsset = name;
                    ImGui::PopStyleColor();
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
// renderScriptsBrowser
// ---------------------------------------------------------------------------

void EditorState::renderScriptsBrowser() {
    ImGui::Begin("Scripts");

    namespace fs = std::filesystem;

    // ── Add Script button ────────────────────────────────────────────────────
    if (ImGui::Button("Add Script")) {
        m_newScriptBuf[0] = '\0';
        ImGui::OpenPopup("##AddScript");
    }

    if (ImGui::BeginPopupModal("##AddScript", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create New Script");
        ImGui::Separator();
        ImGui::InputText("Name", m_newScriptBuf, sizeof(m_newScriptBuf));
        ImGui::TextDisabled("'.lua' extension added automatically");

        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptBuf[0]) {
                std::string fname = m_newScriptBuf;
                if (fname.size() < 4 ||
                    fname.substr(fname.size() - 4) != ".lua")
                    fname += ".lua";

                fs::create_directories("assets/scripts");
                std::string fullPath = "assets/scripts/" + fname;
                if (!fs::exists(fullPath)) {
                    std::ofstream f(fullPath);
                    f << "-- " << fname << "\n";
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // ── Rename modal ─────────────────────────────────────────────────────────
    if (!m_renamingScriptPath.empty())
        ImGui::OpenPopup("##RenameScript");

    if (ImGui::BeginPopupModal("##RenameScript", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path renamePath(m_renamingScriptPath);
        ImGui::Text("Rename: %s", renamePath.filename().string().c_str());
        ImGui::Separator();
        ImGui::InputText("New name", m_renameScriptBuf, sizeof(m_renameScriptBuf));
        ImGui::TextDisabled("'.lua' extension added automatically");
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (m_renameScriptBuf[0]) {
                std::string newName = m_renameScriptBuf;
                if (newName.size() < 4 || newName.substr(newName.size() - 4) != ".lua")
                    newName += ".lua";
                fs::path newPath = renamePath.parent_path() / newName;
                std::error_code ec2;
                fs::rename(renamePath, newPath, ec2);
            }
            m_renamingScriptPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_renamingScriptPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Delete confirmation ───────────────────────────────────────────────────
    if (!m_deletingScriptPath.empty())
        ImGui::OpenPopup("##DeleteScript");

    if (ImGui::BeginPopupModal("##DeleteScript", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path delPath(m_deletingScriptPath);
        ImGui::Text("Delete '%s'?", delPath.filename().string().c_str());
        ImGui::TextDisabled("This permanently removes the file.");
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            std::error_code ec2;
            fs::remove(delPath, ec2);
            m_deletingScriptPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_deletingScriptPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ── Script file grid ─────────────────────────────────────────────────────
    std::error_code ec;
    std::vector<fs::path> scripts;
    for (const auto& entry : fs::directory_iterator("assets/scripts", ec)) {
        if (entry.path().extension() == ".lua")
            scripts.push_back(entry.path());
    }

    if (scripts.empty()) {
        ImGui::TextDisabled("No scripts found in assets/scripts/");
        ImGui::End();
        return;
    }

    constexpr float kIconW   = 64.0f;
    constexpr float kIconH   = 72.0f;
    constexpr float kMinCell = kIconW + 20.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int   cols   = std::max(1, static_cast<int>(panelW / kMinCell));

    // SizingStretchSame distributes all columns equally across the panel width
    if (ImGui::BeginTable("##ScriptsTbl", cols,
                          ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& path : scripts) {
            ImGui::TableNextColumn();
            std::string fname = path.filename().string();
            std::string stem  = path.stem().string();
            ImGui::PushID(fname.c_str());

            // Centre the icon within the (equally-wide) column cell
            float colW   = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            // ── File icon button ─────────────────────────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.14f, 0.16f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.22f, 0.28f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.10f, 0.12f, 0.22f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            ImGui::Button("##icon", ImVec2(kIconW, kIconH));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // Drag-drop source → LuaScript inspector accepts it
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("SCRIPT_ASSET", fname.c_str(), fname.size() + 1);
                ImGui::TextUnformatted(fname.c_str());
                ImGui::EndDragDropSource();
            }

            // Double-click → open in VS Code (no terminal window)
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                std::string absPath = fs::absolute(path, ec).string();
#ifdef _WIN32
                std::string cmdLine = "cmd /c code \"" + absPath + "\"";
                STARTUPINFOA si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                CreateProcessA(nullptr, &cmdLine[0],
                               nullptr, nullptr, FALSE,
                               CREATE_NO_WINDOW,
                               nullptr, nullptr, &si, &pi);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
#else
                system(("code \"" + absPath + "\"").c_str());
#endif
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Double-click to open: %s", fname.c_str());

            // ── Right-click context menu ─────────────────────────────────────
            if (ImGui::BeginPopupContextItem("##ScriptCtx")) {
                if (ImGui::MenuItem("Open in VS Code")) {
                    std::string absPath = fs::absolute(path, ec).string();
#ifdef _WIN32
                    std::string cmdLine = "cmd /c code \"" + absPath + "\"";
                    STARTUPINFOA si2{};
                    si2.cb = sizeof(si2);
                    PROCESS_INFORMATION pi2{};
                    CreateProcessA(nullptr, &cmdLine[0],
                                   nullptr, nullptr, FALSE,
                                   CREATE_NO_WINDOW,
                                   nullptr, nullptr, &si2, &pi2);
                    CloseHandle(pi2.hProcess);
                    CloseHandle(pi2.hThread);
#else
                    system(("code \"" + absPath + "\"").c_str());
#endif
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename")) {
                    m_renamingScriptPath = path.string();
                    strncpy_s(m_renameScriptBuf, stem.c_str(),
                              sizeof(m_renameScriptBuf) - 1);
                }
                if (ImGui::MenuItem("Duplicate")) {
                    std::string newName = stem + "_copy.lua";
                    fs::path    newPath = path.parent_path() / newName;
                    // Avoid collision
                    int n = 2;
                    while (fs::exists(newPath)) {
                        newName = stem + "_copy" + std::to_string(n++) + ".lua";
                        newPath = path.parent_path() / newName;
                    }
                    std::error_code ec2;
                    fs::copy(path, newPath, ec2);
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                if (ImGui::MenuItem("Delete"))
                    m_deletingScriptPath = path.string();
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            // ── Custom draw: page + fold corner + "LUA" label ───────────────
            {
                ImVec2 btnMin = ImGui::GetItemRectMin();
                ImVec2 btnMax = ImGui::GetItemRectMax();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                constexpr float fold = 12.0f;
                ImVec2 p0 = { btnMin.x + 6.0f,          btnMin.y + 6.0f };
                ImVec2 p1 = { btnMax.x - 6.0f - fold,   btnMin.y + 6.0f };
                ImVec2 p3 = { btnMax.x - 6.0f,          btnMax.y - 6.0f };
                ImVec2 p4 = { btnMin.x + 6.0f,          btnMax.y - 6.0f };
                ImVec2 c1 = { btnMax.x - 6.0f - fold,   btnMin.y + 6.0f };
                dl->AddQuadFilled(p0, p1, p3, p4, IM_COL32(60, 80, 180, 120));
                dl->AddTriangleFilled(c1,
                    { c1.x + fold, c1.y },
                    { c1.x, c1.y + fold },
                    IM_COL32(30, 40, 100, 200));

                const char* lbl  = "LUA";
                ImVec2      tsz  = ImGui::CalcTextSize(lbl);
                ImVec2      tpos = {
                    btnMin.x + (btnMax.x - btnMin.x - tsz.x) * 0.5f,
                    btnMin.y + (btnMax.y - btnMin.y - tsz.y) * 0.5f + 6.0f
                };
                dl->AddText(tpos, IM_COL32(160, 200, 255, 255), lbl);
            }

            // ── Filename label centred below the icon ────────────────────────
            std::string label = stem;
            if (label.size() > 9)
                label = label.substr(0, 7) + "..";

            float textW  = ImGui::CalcTextSize(label.c_str()).x;
            float textOX = (colW - textW) * 0.5f;
            if (textOX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOX);
            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", fname.c_str());

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderAudioBrowser
// ---------------------------------------------------------------------------

void EditorState::renderAudioBrowser() {
    ImGui::Begin("Audio");

    namespace fs = std::filesystem;

    // ── Import button ────────────────────────────────────────────────────────
    if (ImGui::Button("Import Audio")) {
        m_newAudioNameBuf[0] = '\0';
        m_newAudioPathBuf[0] = '\0';
        m_newAudioLoop   = false;
        m_newAudioStream = false;
        m_newAudioIs3D   = false;
        m_newAudioVolume = 1.0f;
        m_newAudioPitch  = 1.0f;
        m_newAudioIsBGM  = false;
        ImGui::OpenPopup("##ImportAudio");
    }

    // ── Import modal ─────────────────────────────────────────────────────────
    if (ImGui::BeginPopupModal("##ImportAudio", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import Audio File");
        ImGui::Separator();

        ImGui::InputText("Name##audio",  m_newAudioNameBuf, sizeof(m_newAudioNameBuf));
        ImGui::InputText("Source path",  m_newAudioPathBuf, sizeof(m_newAudioPathBuf));

#ifdef _WIN32
        ImGui::SameLine();
        if (ImGui::Button("Browse##audio")) {
            OPENFILENAMEA ofn{};
            char fileBuf[MAX_PATH]{};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Audio Files\0*.wav;*.mp3;*.ogg;*.flac\0All Files\0*.*\0";
            ofn.lpstrFile   = fileBuf;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn))
                strncpy_s(m_newAudioPathBuf, fileBuf, sizeof(m_newAudioPathBuf) - 1);
        }
#endif

        ImGui::Separator();
        ImGui::Checkbox("Loop",   &m_newAudioLoop);
        ImGui::SameLine();
        ImGui::Checkbox("Stream", &m_newAudioStream);
        ImGui::SameLine();
        ImGui::Checkbox("3D",     &m_newAudioIs3D);
        ImGui::Checkbox("BGM (unchecked = SFX)", &m_newAudioIsBGM);
        ImGui::SliderFloat("Volume", &m_newAudioVolume, 0.0f, 1.0f);
        ImGui::SliderFloat("Pitch",  &m_newAudioPitch,  0.1f, 3.0f);

        ImGui::Separator();
        bool canImport = m_newAudioNameBuf[0] != '\0' && m_newAudioPathBuf[0] != '\0';
        if (!canImport) ImGui::BeginDisabled();
        if (ImGui::Button("Import", ImVec2(100, 0))) {
            Assets::SoundConfig cfg;
            cfg.name   = m_newAudioNameBuf;
            cfg.path   = m_newAudioPathBuf;
            cfg.loop   = m_newAudioLoop;
            cfg.stream = m_newAudioStream;
            cfg.is3D   = m_newAudioIs3D;
            cfg.volume = m_newAudioVolume;
            cfg.pitch  = m_newAudioPitch;
            cfg.group  = m_newAudioIsBGM ? "BGM" : "SFX";
            Assets::SoundManager::ImportSound(m_newAudioPathBuf, cfg.name, cfg);
            ImGui::CloseCurrentPopup();
        }
        if (!canImport) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // ── Delete confirmation ───────────────────────────────────────────────────
    if (!m_deletingAudio.empty())
        ImGui::OpenPopup("##DeleteAudio");

    if (ImGui::BeginPopupModal("##DeleteAudio", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s'?", m_deletingAudio.c_str());
        ImGui::TextDisabled("Removes from memory, config, and disk.");
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            Assets::SoundManager::DeleteFromDisk(m_deletingAudio);
            m_deletingAudio.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_deletingAudio.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ── Sound grid ───────────────────────────────────────────────────────────
    auto names = Assets::SoundManager::GetLoadedSoundNames();
    if (names.empty()) {
        ImGui::TextDisabled("No audio loaded. Use 'Import Audio' or check sounds.json.");
        ImGui::End();
        return;
    }

    constexpr float kIconW   = 64.0f;
    constexpr float kIconH   = 72.0f;
    constexpr float kMinCell = kIconW + 20.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int   cols   = std::max(1, static_cast<int>(panelW / kMinCell));

    if (ImGui::BeginTable("##AudioTbl", cols, ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& soundName : names) {
            ImGui::TableNextColumn();
            ImGui::PushID(soundName.c_str());

            float colW   = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            // ── Icon button ───────────────────────────────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.12f, 0.25f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.20f, 0.42f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.08f, 0.18f, 0.12f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            ImGui::Button("##icon", ImVec2(kIconW, kIconH));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // Drag-drop source → AudioSource inspector accepts it
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("AUDIO_ASSET", soundName.c_str(), soundName.size() + 1);
                ImGui::TextUnformatted(soundName.c_str());
                ImGui::EndDragDropSource();
            }

            // Double-click → play
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                Assets::SoundManager::Play(soundName);
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Double-click to play: %s", soundName.c_str());

            // ── Right-click context menu ──────────────────────────────────────
            if (ImGui::BeginPopupContextItem("##AudioCtx")) {
                if (ImGui::MenuItem("Play"))
                    Assets::SoundManager::Play(soundName);
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                if (ImGui::MenuItem("Delete"))
                    m_deletingAudio = soundName;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            // ── Draw speaker icon ─────────────────────────────────────────────
            {
                ImVec2 btnMin = ImGui::GetItemRectMin();
                ImVec2 btnMax = ImGui::GetItemRectMax();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                float cx = (btnMin.x + btnMax.x) * 0.5f;
                float cy = (btnMin.y + btnMax.y) * 0.5f - 4.0f;
                // Speaker body
                dl->AddRectFilled({ cx - 10.0f, cy - 6.0f },
                                  { cx - 2.0f,  cy + 6.0f },
                                  IM_COL32(100, 220, 140, 200));
                // Cone
                dl->AddTriangleFilled(
                    { cx - 2.0f,  cy - 6.0f },
                    { cx - 2.0f,  cy + 6.0f },
                    { cx + 10.0f, cy + 12.0f },
                    IM_COL32(100, 220, 140, 200));
                dl->AddTriangleFilled(
                    { cx - 2.0f,  cy - 6.0f },
                    { cx + 10.0f, cy - 12.0f },
                    { cx + 10.0f, cy + 12.0f },
                    IM_COL32(100, 220, 140, 200));

                // "SFX" label inside icon
                const char* lbl = "SFX";
                ImVec2 tsz  = ImGui::CalcTextSize(lbl);
                ImVec2 tpos = {
                    btnMin.x + (btnMax.x - btnMin.x - tsz.x) * 0.5f,
                    btnMin.y + (btnMax.y - btnMin.y - tsz.y) * 0.5f + 8.0f
                };
                dl->AddText(tpos, IM_COL32(160, 255, 180, 255), lbl);
            }

            // ── Name label centred below icon ─────────────────────────────────
            std::string label = soundName;
            if (label.size() > 9)
                label = label.substr(0, 7) + "..";

            float textW  = ImGui::CalcTextSize(label.c_str()).x;
            float textOX = (colW - textW) * 0.5f;
            if (textOX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOX);
            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", soundName.c_str());

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
