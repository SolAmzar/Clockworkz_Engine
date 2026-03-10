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
#include "Runtime/EditorPlayState.hpp"
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
#include "Components/Light.hpp"
#include "Components/Transform.hpp"
#include "Components/Tag.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Collider.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_internal.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <fstream>
#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
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
#include <limits>
#include <map>

// Engine components
#include "Components/Velocity.hpp"
#include "Components/AudioSource.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/UIButton.hpp"
#include "Components/LuaScript.hpp"
#include "Scripting/ScriptPropertyReader.hpp"
#include "Systems/LightRenderSystem.hpp"

namespace Runtime {

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static vec2 snapToGrid(const vec2& pos, float gridSize) {
    float half = gridSize * 0.5f;
    return { std::round((pos.x - half) / gridSize) * gridSize + half,
             std::round((pos.y - half) / gridSize) * gridSize + half };
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

// Helper: draw a speaker icon with sound waves
static void DrawAudioIcon() {
    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    
    float cx = (bMin.x + bMax.x) * 0.5f;
    float cy = (bMin.y + bMax.y) * 0.5f - 4.0f;
    ImU32 col = IM_COL32(120, 220, 160, 255);

    // Speaker body
    dl->AddRectFilled({ cx - 10, cy - 6 }, { cx - 2, cy + 6 }, col, 1.0f);
    dl->AddTriangleFilled({ cx - 2, cy - 6 }, { cx - 2, cy + 6 }, { cx + 8, cy + 12 }, col);
    dl->AddTriangleFilled({ cx - 2, cy - 6 }, { cx + 8, cy - 12 }, { cx + 8, cy + 12 }, col);

    // Sound waves (Path-based for compatibility)
    dl->PathArcTo({cx + 10, cy}, 6.0f, -0.8f, 0.8f, 10);
    dl->PathStroke(col, false, 2.0f);
    dl->PathArcTo({cx + 10, cy}, 12.0f, -0.8f, 0.8f, 10);
    dl->PathStroke(col, false, 2.0f);
}

// Helper: draw the LUA file icon inside an existing Button item rect
static void DrawScriptIcon(bool isComponent) {
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr float fold = 12.0f;
    ImVec2 p0 = { btnMin.x + 6.0f,        btnMin.y + 6.0f };
    ImVec2 p1 = { btnMax.x - 6.0f - fold, btnMin.y + 6.0f };
    ImVec2 p3 = { btnMax.x - 6.0f,        btnMax.y - 6.0f };
    ImVec2 p4 = { btnMin.x + 6.0f,        btnMax.y - 6.0f };
    ImVec2 c1 = { btnMax.x - 6.0f - fold, btnMin.y + 6.0f };
    ImU32 bodyCol = isComponent ? IM_COL32(60, 120, 80, 120) : IM_COL32(60, 80, 180, 120);
    ImU32 foldCol = isComponent ? IM_COL32(30, 70, 40, 200)  : IM_COL32(30, 40, 100, 200);
    ImU32 textCol = isComponent ? IM_COL32(140, 220, 160, 255) : IM_COL32(160, 200, 255, 255);
    dl->AddQuadFilled(p0, p1, p3, p4, bodyCol);
    dl->AddTriangleFilled(c1, { c1.x + fold, c1.y }, { c1.x, c1.y + fold }, foldCol);
    const char* lbl = "LUA";
    ImVec2 tsz  = ImGui::CalcTextSize(lbl);
    ImVec2 tpos = {
        btnMin.x + (btnMax.x - btnMin.x - tsz.x) * 0.5f,
        btnMin.y + (btnMax.y - btnMin.y - tsz.y) * 0.5f + 6.0f
    };
    dl->AddText(tpos, textCol, lbl);
}

// Helper: draw a folder icon inside an existing Button item rect
static void DrawFolderIcon() {
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cx = btnMin.x + (btnMax.x - btnMin.x) * 0.5f;
    float cy = btnMin.y + (btnMax.y - btnMin.y) * 0.5f;
    // Folder body
    ImVec2 bodyTL = { cx - 22.0f, cy - 10.0f };
    ImVec2 bodyBR = { cx + 22.0f, cy + 16.0f };
    dl->AddRectFilled(bodyTL, bodyBR, IM_COL32(200, 170, 60, 180), 3.0f);
    // Folder tab
    ImVec2 tabTL = { bodyTL.x, bodyTL.y - 8.0f };
    ImVec2 tabBR = { bodyTL.x + 18.0f, bodyTL.y + 1.0f };
    dl->AddRectFilled(tabTL, tabBR, IM_COL32(220, 190, 80, 180), 2.0f);
}

// Helper: open a file in VS Code silently
static void OpenInVSCode(const std::string& absPath) {
#ifdef _WIN32
    std::string cmdLine = "cmd /c code \"" + absPath + "\"";
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    CreateProcessA(nullptr, &cmdLine[0], nullptr, nullptr, FALSE,
                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
#else
    // Sanitize path: reject if it contains shell metacharacters
    static const std::string kBadChars = ";&|`$<>\n\r";
    if (absPath.find_first_of(kBadChars) == std::string::npos)
        system(("code \"" + absPath + "\"").c_str());
#endif
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
    m_animPreviewFB.create(192, 192);

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
    // Load engine-level fonts (Debug, etc.) from shared assets config
    Assets::FontManager::LoadFromConfig(m_engineAssetsRoot + "/configs/fonts.json");
    // Load project-specific fonts (Title, Selection, BodyText, etc.) from project config
    Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");

    // Load project-specific assets (CWD is now the project root, so all
    // "assets/..." paths resolve to this project's assets/ folder).
    Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");
    initTextureFilePaths();
    Assets::SoundManager::LoadFromConfig("assets/configs/sounds.json");
    Gameplay::PrefabFactory::get().loadPrefabFromFile("assets/configs/Prefab.json");

    m_scene.Init();

    m_currentAssetDir = "assets/textures";
    m_currentAnimDir = "assets/animations";
    m_currentScriptDir = "assets/scripts";
    m_currentAudioDir = "assets/audio";

    loadAvailablePrefabs();
    m_layerVisible.assign(m_numLayers, true);
    for (int i = 0; i < m_numLayers; ++i)
        m_scene.setLayerEnabled(i, true);

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
    m_selectedEntities.clear();
    m_gizmoActiveAxis       = -1;
    m_gizmoGroupOriginalPos.clear();
    m_gizmoGroupOriginalRot.clear();
    m_gizmoGroupOriginalScale.clear();
    m_pendingEntityDrag     = false;
    m_isDraggingEntity      = false;
    m_isBoxSelecting        = false;
    m_isRenaming            = false;
    m_dockLayoutInitialized = false;
    m_pendingLoadPath.clear();

    // Tilemap / prefab browser state
    m_tilemap.clear();
    m_entityGroups.clear();
    m_availablePrefabs.clear();
    m_layerVisible.clear();
    m_tileMapMode        = false;
    m_showTilePalette    = false;
    m_selectedTilePrefab.clear();
    m_paintedThisFrame   = false;
    m_currentLayer       = 0;
    m_numLayers          = 3;
    m_selectedPrefabIndex = -1;
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
        namespace fs = std::filesystem;
        fs::path tempDir = fs::path(m_sceneFilePath).parent_path()
                                                     .parent_path() / "temp";
        fs::create_directories(tempDir);
        std::string tempPath = (tempDir / "__playtest__.json").generic_string();
        SceneSerializer::Serialize(m_scene, tempPath);
        m_gsm.push(std::make_shared<EditorPlayState>(m_input, m_gsm, tempPath));
        return;
    }

    // Deferred: save
    if (m_pendingSave) {
        m_pendingSave = false;
        saveScene();
    }

    // Animation editor preview playback
    if (m_animEditorOpen && m_animEdPlaying && !m_animEdClip.empty()) {
        if (auto* clip = m_animEditorAnim.getClip(m_animEdClip)) {
            if (!clip->frames.empty()) {
                if (m_animEdFrame < 0) m_animEdFrame = 0;
                m_animEdTimer += static_cast<float>(dt);
                while (m_animEdTimer >= clip->frames[m_animEdFrame].duration) {
                    m_animEdTimer -= clip->frames[m_animEdFrame].duration;
                    ++m_animEdFrame;
                    if (m_animEdFrame >= static_cast<int>(clip->frames.size())) {
                        if (m_animEdLoop) m_animEdFrame = 0;
                        else { m_animEdFrame = static_cast<int>(clip->frames.size()) - 1; m_animEdPlaying = false; }
                    }
                }
            }
        }
    }

    // Texture hot-reload
    m_hotReloadTimer += static_cast<float>(dt);
    if (m_hotReloadTimer >= HOT_RELOAD_INTERVAL) {
        m_hotReloadTimer = 0.f;
        checkAssetHotReload();
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

    double cx = 0.0, cy = 0.0;
    m_input.getMousePosition(cx, cy);

    bool panning = m_viewportHovered &&
                   (m_input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE) ||
                    m_input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT));
    if (panning) {
        float dx = static_cast<float>(cx - m_lastMouseX);
        float dy = static_cast<float>(cy - m_lastMouseY);
        float ps = 1.0f / m_editorCamZoom;
        m_editorCamPos.x -= dx * ps;
        m_editorCamPos.y += dy * ps;
        camera.setPosition(m_editorCamPos);
    }
    m_lastMouseX = cx;
    m_lastMouseY = cy;

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

    // Delete key: delete all selected entities
    // Block only when a text field is active (e.g. renaming), allow everywhere else
    if (m_input.wasKeyTriggered(GLFW_KEY_DELETE) || m_input.wasKeyTriggered(GLFW_KEY_BACKSPACE)) {
        auto& log = Core::Engine::Get().logger();
        log.info("[Editor] DELETE/BACKSPACE key detected. WantTextInput=" +
                 std::string(ImGui::GetIO().WantTextInput ? "true" : "false") +
                 " | selectedCount=" + std::to_string(m_selectedEntities.size()) +
                 " | viewportFocused=" + std::string(m_viewportFocused ? "true" : "false"));
    }
    if (!ImGui::GetIO().WantTextInput &&
        (m_input.wasKeyTriggered(GLFW_KEY_DELETE) || m_input.wasKeyTriggered(GLFW_KEY_BACKSPACE))) {
        auto& world = m_scene.GetWorld();
        for (auto e : m_selectedEntities) {
            if (!world.isValid(e)) continue;
            // If this entity is a tilemap tile, clean up tile data too
            auto tileIt = std::find_if(m_tilemap.begin(), m_tilemap.end(),
                [&](const TileData& t) {
                    return t.entity.id == e.id && t.entity.generation == e.generation;
                });
            if (tileIt != m_tilemap.end()) {
                removeEntityFromGroups(e);
                world.destroyEntity(e);
                m_tilemap.erase(tileIt);
            } else {
                executeCommand(std::make_unique<DeleteEntityCmd>(world, e));
            }
        }
        m_selectedEntities.clear();
        m_selectedEntity = {0, 0};
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
    m_scene.RenderText(renderer);

    Systems::renderLightPass(m_scene.GetWorld(), renderer);

    // Grid + collider outlines (debug pass, drawn on top of scene)
    if (m_showGrid || m_showColliders) {
        renderer.beginDebugPass(camera.viewProj());

        if (m_showGrid)
            renderGrid();

        if (m_showColliders) {
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

                    // Yellow for triggers, green for solid
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
        }

        renderer.endDebugPass();
    }

    m_framebuffer.unbind();

    // Animation editor preview — render current frame into m_animPreviewFB
    if (m_animEditorOpen && !m_animEdTexture.empty() && !m_animEdClip.empty()) {
        auto* clip = m_animEditorAnim.getClip(m_animEdClip);
        if (clip && !clip->frames.empty()) {
            int fi = (m_animEdFrame >= 0 && m_animEdFrame < static_cast<int>(clip->frames.size()))
                     ? m_animEdFrame : 0;
            auto tex = Assets::TextureManager::Get(m_animEdTexture);
            if (tex) {
                m_animPreviewFB.bind();
                glClearColor(0.12f, 0.12f, 0.12f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
                // Simple square ortho: world unit [0,1]^2 fills the FB
                glm::mat4 vp = glm::ortho(0.f, 1.f, 0.f, 1.f, -1.f, 1.f);
                renderer.beginScenePass(vp, glm::vec4(1.f));
                renderer.drawSprite(tex->id(),
                                    {0.5f, 0.5f}, {1.f, 1.f},
                                    0.f, clip->frames[fi].uv,
                                    glm::vec4(1.f), false);
                renderer.endScenePass();
                m_animPreviewFB.unbind();
            }
        }
    }
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
        ImGui::DockBuilderDockWindow("Animations",             centerBot);
        ImGui::DockBuilderDockWindow("Prefabs",                centerBot);
        ImGui::DockBuilderDockWindow("Fonts",                  centerBot);
        ImGui::DockBuilderDockWindow("Tile Palette",           left);
        ImGui::DockBuilderDockWindow("Inspector",              rightTop);
        ImGui::DockBuilderDockWindow("Stats",                  rightTop);

        ImGui::DockBuilderFinish(dockId);
    }

    renderSystemsWindow();
    renderHierarchy();
    renderLoadSceneWindow();
    renderViewport();
    renderInspector();
    renderAssetBrowser();
    renderStatsWindow();
    renderScriptsBrowser();
    renderAnimationsBrowser();
    renderAnimationEditor();
    renderAudioBrowser();
    renderFontBrowser();
    renderPrefabBrowser();
    renderTilePalette();

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
        if (ImGui::BeginMenu("Tilemap")) {
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("##TilemapPath", m_tilemapPathBuf, sizeof(m_tilemapPathBuf));
            if (ImGui::Button("Save Tilemap"))  saveTilemap(m_tilemapPathBuf);
            ImGui::SameLine();
            if (ImGui::Button("Load Tilemap"))  loadTilemap(m_tilemapPathBuf);
            ImGui::SameLine();
            if (ImGui::Button("Clear"))         clearTilemap();
            ImGui::EndMenu();
        }
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
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale",  m_gizmoMode == GizmoMode::Scale))
        m_gizmoMode = GizmoMode::Scale;

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

    // Tilemap mode
    if (ImGui::Checkbox("Tilemap", &m_tileMapMode)) {
        if (m_tileMapMode) {
            m_selectedEntities.clear();
            m_selectedEntity = {0, 0};
            m_showTilePalette = true;
        } else {
            m_showTilePalette = false;
        }
    }
    if (m_tileMapMode) {
        ImGui::SameLine();
        ImGui::Separator(); ImGui::SameLine();
        if (ImGui::RadioButton("Paint",  m_paintMode == PaintMode::Paint))
            m_paintMode = PaintMode::Paint;
        ImGui::SameLine();
        if (ImGui::RadioButton("Erase",  m_paintMode == PaintMode::Erase))
            m_paintMode = PaintMode::Erase;
        ImGui::SameLine();
        if (ImGui::RadioButton("Select", m_paintMode == PaintMode::Select))
            m_paintMode = PaintMode::Select;

        ImGui::SameLine();
        ImGui::Separator(); ImGui::SameLine();
        ImGui::Text("Layer:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::SliderInt("##Layer", &m_currentLayer, 0, m_numLayers - 1);
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) {
            ++m_numLayers;
            m_layerVisible.push_back(true);
            m_scene.setLayerEnabled(m_numLayers - 1, true);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("-") && m_numLayers > 1) {
            --m_numLayers;
            m_layerVisible.resize(m_numLayers, true);
            if (m_currentLayer >= m_numLayers) m_currentLayer = m_numLayers - 1;
        }
        ImGui::SameLine();
        ImGui::Separator(); ImGui::SameLine();
        for (int i = 0; i < m_numLayers; ++i) {
            char lbl[16];
            std::snprintf(lbl, sizeof(lbl), "L%d", i);
            bool vis = (i < (int)m_layerVisible.size()) ? m_layerVisible[i] : true;
            if (ImGui::Checkbox(lbl, &vis)) {
                if (i < (int)m_layerVisible.size()) m_layerVisible[i] = vis;
                m_scene.setLayerEnabled(i, vis);
            }
            if (i + 1 < m_numLayers) ImGui::SameLine();
        }
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
    m_statsHistory[m_statsHistoryOffset] = static_cast<float>(agg.usedBytes) / 1024.0f;
    m_statsHistoryOffset = (m_statsHistoryOffset + 1) % 128;

    ImGui::Spacing();
    float graphMax = (agg.totalBytes > 0)
        ? static_cast<float>(agg.totalBytes) / 1024.0f : 1.0f;
    ImGui::PlotLines("##memhist", m_statsHistory, 128, m_statsHistoryOffset,
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
    auto selectNew = [&](ECS::Entity e) {
        m_selectedEntities.clear();
        m_selectedEntity = e;
        m_selectedEntities.push_back(e);
    };
    if (ImGui::Button("+ Entity")) {
        EntitySnapshot snap;
        snap.tagName  = "Entity";
        snap.transform = Components::Transform{};
        auto cmd = std::make_unique<CreateEntityCmd>(world, snap);
        auto* rawCmd = cmd.get();
        executeCommand(std::move(cmd));
        selectNew(rawCmd->createdEntity());
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
        selectNew(rawCmd->createdEntity());
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
        // Highlight if primary OR in secondary selection
        bool selected = (m_selectedEntity.id == row.entity.id &&
                         m_selectedEntity.generation == row.entity.generation);
        if (!selected) {
            for (auto& se : m_selectedEntities) {
                if (se.id == row.entity.id && se.generation == row.entity.generation) {
                    selected = true;
                    break;
                }
            }
        }

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
            // Clear any active text-field so Delete works immediately after selecting an entity
            ImGui::ClearActiveID();
            bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                         ImGui::IsKeyDown(ImGuiKey_RightShift);
            if (shift) {
                // Shift+click: toggle entity in selection
                auto it = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                    [&](ECS::Entity e){
                        return e.id == row.entity.id && e.generation == row.entity.generation;
                    });
                if (it != m_selectedEntities.end()) {
                    m_selectedEntities.erase(it);
                    // If we removed the primary, pick a new one
                    if (m_selectedEntity.id == row.entity.id &&
                        m_selectedEntity.generation == row.entity.generation)
                        m_selectedEntity = m_selectedEntities.empty()
                                           ? ECS::Entity{0, 0}
                                           : m_selectedEntities.front();
                } else {
                    m_selectedEntities.push_back(row.entity);
                    // First entity becomes primary if none set
                    if (!world.isValid(m_selectedEntity))
                        m_selectedEntity = row.entity;
                }
            } else {
                // Regular click: clear selection, make this entity primary
                m_selectedEntities.clear();
                m_selectedEntity = row.entity;
                m_selectedEntities.push_back(row.entity);
            }
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
                selectNew(rawCmd->createdEntity());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                if (selected) {
                    m_selectedEntities.erase(
                        std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                            [&](ECS::Entity e){
                                return e.id == row.entity.id &&
                                       e.generation == row.entity.generation;
                            }),
                        m_selectedEntities.end());
                    if (m_selectedEntity.id == row.entity.id &&
                        m_selectedEntity.generation == row.entity.generation)
                        m_selectedEntity = {0, 0};
                }
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
            addCompEntry<Components::Light>("Light", m_selectedEntity, world);
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
        if (ImGui::BeginMenu("UI")) {
            addCompEntry<Components::UIButton>("UIButton", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera")) {
            addCompEntry<Components::CameraComponent>("CameraComponent", m_selectedEntity, world);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scripting")) {
            // LuaScript: always available — adds a new script slot each click
            if (ImGui::MenuItem("LuaScript")) {
                if (!world.hasComponent<Components::LuaScript>(m_selectedEntity))
                    world.addComponent(m_selectedEntity, Components::LuaScript{});
                world.getComponent<Components::LuaScript>(m_selectedEntity)->scripts.push_back({});
            }
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

            ImGui::Checkbox("Snap to Grid##transform", &t->snapToGrid);
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
                ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(48, 48), ImVec2(0,1), ImVec2(1,0));
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

    // Collider
    if (ImGui::CollapsingHeader("Collider")) {
        auto* c = world.getComponent<Components::Collider>(m_selectedEntity);
        if (c) {
            ImGui::Checkbox("Show Outline", &m_showColliders);
            ImGui::Separator();

            // Shape selector — changing this affects collision logic immediately
            const char* shapeNames[] = {"Box", "Circle"};
            int currentShape = static_cast<int>(c->shape);
            if (ImGui::Combo("Shape##col", &currentShape, shapeNames, 2))
                c->shape = static_cast<Components::Collider::Shape>(currentShape);

            if (c->shape == Components::Collider::Shape::Box) {
                ImGui::DragFloat2("Size##col", &c->size.x, 1.0f);
            } else {
                ImGui::DragFloat("Radius##col", &c->radius, 1.0f);
            }

            ImGui::DragFloat2("Offset##col", &c->offset.x, 1.0f);
            ImGui::Checkbox("Solid##col",        &c->isSolid);
            ImGui::Checkbox("Trigger##col",      &c->isTrigger);
            ImGui::Checkbox("Blocks Light##col", &c->blocksLight);
            ImGui::Checkbox("Moveable##col",     &c->moveable);
            if (c->moveable)
                ImGui::DragFloat("Mass##col", &c->mass, 0.1f);

            if (ImGui::Button("Fit to Sprite##col")) {
                auto* spr = world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
                if (spr) {
                    if (c->shape == Components::Collider::Shape::Box)
                        c->size = spr->size;
                    else
                        c->radius = std::max(spr->size.x, spr->size.y) * 0.5f;
                    c->offset = {0.f, 0.f};
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##col"))
                world.removeComponent<Components::Collider>(m_selectedEntity);
        } else {
            if (ImGui::Button("Add Collider##col"))
                world.addComponent(m_selectedEntity, Components::Collider{});
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
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ANIMATION_ASSET")) {
                    sa->filename = static_cast<const char*>(pl->Data);
                    sa->loadFromPath("assets/animations/" + sa->filename);
                }
                ImGui::EndDragDropTarget();
            }
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

    // ── LuaScript (multi-slot) ───────────────────────────────────────────────
    if (world.hasComponent<Components::LuaScript>(m_selectedEntity)) {
        namespace fs = std::filesystem;
        auto* ls = world.getComponent<Components::LuaScript>(m_selectedEntity);
        bool removeComponent = false;

        for (int si = 0; si < (int)ls->scripts.size(); ++si) {
            auto& entry = ls->scripts[si];
            ImGui::PushID(si);

            // Header name: derive from script filename (e.g. "health.lua" → "Health")
            std::string headerName = "Script";
            if (!entry.scriptPath.empty()) {
                std::string stem = fs::path(entry.scriptPath).stem().string();
                if (!stem.empty()) stem[0] = (char)toupper((unsigned char)stem[0]);
                headerName = stem;
            }

            bool open = ImGui::CollapsingHeader(headerName.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
            if (ImGui::SmallButton("X##lse")) {
                ls->scripts.erase(ls->scripts.begin() + si);
                ImGui::PopID();
                --si;
                continue;
            }

            if (open) {
                char spbuf[256]; snprintf(spbuf, sizeof(spbuf), "%s", entry.scriptPath.c_str());
                if (ImGui::InputText("Script Path##lse", spbuf, sizeof(spbuf)))
                    entry.scriptPath = spbuf;
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SCRIPT_ASSET"))
                        entry.scriptPath = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::Checkbox("Enabled##lse", &entry.enabled);

                if (!entry.scriptPath.empty()) {
                    auto& propDefs = Scripting::ScriptPropertyReader::read(entry.scriptPath);
                    if (!propDefs.empty()) {
                        ImGui::Separator();
                        ImGui::TextDisabled("Script Properties");
                        for (auto& def : propDefs) {
                            if (def.label.empty()) continue;
                            if (!entry.properties.count(def.name))
                                entry.properties[def.name] = def.defaultValue;
                            auto& val = entry.properties[def.name];
                            if (def.type == "float") {
                                float v = std::get<float>(val);
                                if (ImGui::DragFloat(def.label.c_str(), &v, 0.1f)) val = v;
                            } else if (def.type == "int") {
                                int v = std::get<int>(val);
                                if (ImGui::DragInt(def.label.c_str(), &v)) val = v;
                            } else if (def.type == "bool") {
                                bool v = std::get<bool>(val);
                                if (ImGui::Checkbox(def.label.c_str(), &v)) val = v;
                            } else if (def.type == "string") {
                                std::string sv = std::get<std::string>(val);
                                char strbuf[256]; snprintf(strbuf, sizeof(strbuf), "%s", sv.c_str());
                                if (ImGui::InputText(def.label.c_str(), strbuf, sizeof(strbuf)))
                                    val = std::string(strbuf);
                            }
                        }
                    }
                }
            }
            ImGui::PopID();
        }

        // Remove entire component only if all slots removed
        if (ls->scripts.empty()) {
            if (ImGui::Button("Remove Script Component##lsrm"))
                removeComponent = true;
        }
        if (removeComponent) world.removeComponent<Components::LuaScript>(m_selectedEntity);
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
            {
                auto fontKeys = Assets::ResourceManager<Assets::FontManager>::GetKeys();
                std::sort(fontKeys.begin(), fontKeys.end());
                int curIdx = 0;
                for (int i = 0; i < (int)fontKeys.size(); ++i)
                    if (fontKeys[i] == tr->fontId) { curIdx = i; break; }
                std::vector<const char*> fontCStrs;
                fontCStrs.reserve(fontKeys.size());
                for (const auto& k : fontKeys) fontCStrs.push_back(k.c_str());
                if (ImGui::Combo("Font ID##tr", &curIdx, fontCStrs.data(), (int)fontCStrs.size()))
                    tr->fontId = fontKeys[curIdx];
            }
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

    // Light
    if (world.hasComponent<Components::Light>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("Light##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rlt")) rm = true;
        if (open) {
            auto* lt = world.getComponent<Components::Light>(m_selectedEntity);
            ImGui::Checkbox("Active##lt", &lt->active);
            ImGui::DragFloat2("Offset##lt", &lt->offset.x, 1.0f);
            ImGui::ColorEdit3("Color##lt", &lt->color.r);
            ImGui::DragFloat("Radius##lt", &lt->radius, 1.0f, 0.0f, 2000.0f);
            ImGui::DragFloat("Angle##lt", &lt->angle, 0.5f, 0.0f, 360.0f);
            ImGui::SliderFloat("Intensity##lt", &lt->intensity, 0.0f, 1.0f);
        }
        if (rm) world.removeComponent<Components::Light>(m_selectedEntity);
    }

    // ── UIButton ─────────────────────────────────────────────────────────────
    if (world.hasComponent<Components::UIButton>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("UIButton##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##ruib")) rm = true;
        if (open) {
            auto *btn = world.getComponent<Components::UIButton>(m_selectedEntity);
            char abuf[256]; snprintf(abuf, sizeof(abuf), "%s", btn->action.c_str());
            if (ImGui::InputText("Action##uib", abuf, sizeof(abuf)))
                btn->action = abuf;
            ImGui::Checkbox("Interactable##uib", &btn->interactable);
            ImGui::ColorEdit3("Normal Color##uib", glm::value_ptr(btn->normalColor));
            ImGui::ColorEdit3("Hover Color##uib",  glm::value_ptr(btn->hoverColor));
        }
        if (rm) world.removeComponent<Components::UIButton>(m_selectedEntity);
    }

    // ── CameraComponent ───────────────────────────────────────────────────────
    if (world.hasComponent<Components::CameraComponent>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("CameraComponent##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rcam")) rm = true;
        if (open) {
            auto* cam = world.getComponent<Components::CameraComponent>(m_selectedEntity);
            ImGui::Checkbox("Is Main##cam",  &cam->isMain);
            ImGui::DragFloat("Zoom##cam",    &cam->zoom, 0.01f, 0.01f, 20.0f);
            ImGui::DragFloat2("Offset##cam", &cam->offset.x, 1.0f);
        }
        if (rm) world.removeComponent<Components::CameraComponent>(m_selectedEntity);
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

    auto& cam   = Core::Engine::Get().camera();
    vec2  camP  = {cam.position().x, cam.position().y};
    auto& ecs   = m_scene.GetWorld();

    // ----- Tilemap input (when in tilemap mode) -----
    if (m_tileMapMode)
        handleTileMapInput(vpMin);

    // ----- Entity picking (left-click) -----
    // Skip if gizmo handle is active OR was hovered last frame (user is clicking handle)
    // Also skip when tilemap mode is active (tilemap mode handles its own input)
    if (m_viewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !m_tileMapMode &&
        m_gizmoActiveAxis == -1 &&
        !m_gizmoHoveredHandle) {
        // Clear any active text-field focus so Delete key works immediately after clicking an entity
        ImGui::ClearActiveID();
        ImVec2 mp       = ImGui::GetMousePos();
        vec2   worldPos = screenToWorld({mp.x, mp.y}, camP, m_editorCamZoom, vpMin, m_viewportSize);

        // Pick the topmost entity under the cursor.
        // Among entities whose bounds contain worldPos, prefer highest sortOrder
        // (higher sortOrder = rendered last = visually on top).
        // Fall back to closest-center distance for entities without a SpriteRenderer.
        ECS::Entity bestEnt{0, 0};
        int   bestSort  = std::numeric_limits<int>::min();
        float bestDist  = 60.0f;   // max pick radius for fallback
        bool  foundBounds = false;

        ecs.each<Components::Transform>([&](ECS::Entity e, Components::Transform& t) {
            vec2 center{t.position.x, t.position.y};
            auto* sr = ecs.getComponent<Components::SpriteRenderer>(e);
            if (sr) {
                // Axis-aligned bounds test (ignores rotation, good enough for picking)
                float hw = (sr->size.x * std::abs(t.scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t.scale.y)) * 0.5f;
                bool inside = (worldPos.x >= center.x - hw && worldPos.x <= center.x + hw &&
                               worldPos.y >= center.y - hh && worldPos.y <= center.y + hh);
                if (inside) {
                    // Higher sortOrder = rendered last = visually on top → prefer it
                    if (!foundBounds || sr->sortOrder > bestSort) {
                        bestSort    = sr->sortOrder;
                        bestEnt     = e;
                        foundBounds = true;
                    }
                }
            } else if (!foundBounds) {
                // No sprite — fall back to proximity
                float d = magnitude(center - worldPos);
                if (d < bestDist) { bestDist = d; bestEnt = e; }
            }
        });

        bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                     ImGui::IsKeyDown(ImGuiKey_RightShift);

        if (shift) {
            if (ecs.isValid(bestEnt)) {
                // Shift+click entity: toggle in multi-selection
                auto it = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                    [&](ECS::Entity e){ return e.id == bestEnt.id && e.generation == bestEnt.generation; });
                if (it != m_selectedEntities.end()) {
                    m_selectedEntities.erase(it);
                    if (m_selectedEntity.id == bestEnt.id && m_selectedEntity.generation == bestEnt.generation)
                        m_selectedEntity = m_selectedEntities.empty() ? ECS::Entity{0,0} : m_selectedEntities.front();
                } else {
                    m_selectedEntities.push_back(bestEnt);
                    if (!ecs.isValid(m_selectedEntity)) m_selectedEntity = bestEnt;
                }
            } else {
                // Shift+click empty space: start additive box selection
                m_isBoxSelecting = true;
                m_boxSelectStart = {mp.x, mp.y};
                m_boxSelectEnd   = {mp.x, mp.y};
            }
        } else {
            // Check if clicking an already-selected entity
            bool alreadySelected = ecs.isValid(bestEnt) &&
                std::any_of(m_selectedEntities.begin(), m_selectedEntities.end(),
                    [&](ECS::Entity e){ return e.id == bestEnt.id && e.generation == bestEnt.generation; });

            if (!alreadySelected) {
                // New entity — clear selection and select this one
                m_selectedEntities.clear();
                m_selectedEntity = bestEnt;
                if (ecs.isValid(bestEnt)) m_selectedEntities.push_back(bestEnt);
            }
            // Either way, start body-drag tracking for all selected entities
            if (ecs.isValid(bestEnt)) {
                m_pendingEntityDrag = true;
                m_isDraggingEntity  = false;
                m_dragStartMousePos = {mp.x, mp.y};
                m_dragClickWorldPos = worldPos;
                // Capture original positions for entire group
                m_gizmoGroupOriginalPos.clear();
                for (auto& se : m_selectedEntities) {
                    if (auto* t = ecs.getComponent<Components::Transform>(se))
                        m_gizmoGroupOriginalPos.push_back({se, {t->position.x, t->position.y}});
                }
            } else {
                // Clicked empty space — start box selection
                m_pendingEntityDrag = false;
                m_isDraggingEntity  = false;
                m_isBoxSelecting    = true;
                m_boxSelectStart    = {mp.x, mp.y};
                m_boxSelectEnd      = {mp.x, mp.y};
                // Clear selection immediately (entities deselect on empty-space click)
                if (!shift) {
                    m_selectedEntities.clear();
                    m_selectedEntity = {0, 0};
                }
            }
        }
    }

    // ----- Direct entity body drag -----
    if (m_gizmoActiveAxis != -1) {
        m_pendingEntityDrag = false;
        m_isDraggingEntity  = false;
        m_isBoxSelecting    = false;
    }

    if (m_pendingEntityDrag && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mp = ImGui::GetMousePos();
        float  dx = mp.x - m_dragStartMousePos.x;
        float  dy = mp.y - m_dragStartMousePos.y;
        if (!m_isDraggingEntity && std::sqrt(dx * dx + dy * dy) > kDragThreshold)
            m_isDraggingEntity = true;

        if (m_isDraggingEntity) {
            vec2 worldMouse = screenToWorld({mp.x, mp.y}, camP, m_editorCamZoom, vpMin, m_viewportSize);
            vec2 delta = worldMouse - m_dragClickWorldPos;
            // Move all selected entities by the same delta
            for (auto& [e, origPos] : m_gizmoGroupOriginalPos) {
                if (auto* t = ecs.getComponent<Components::Transform>(e)) {
                    vec2 newPos = origPos + delta;
                    if (m_snapToGrid || t->snapToGrid) newPos = snapToGrid(newPos, m_gridSize);
                    t->position = newPos;
                }
            }
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_isDraggingEntity) {
            // Push undo for every moved entity
            for (auto& [e, origPos] : m_gizmoGroupOriginalPos) {
                if (auto* t = ecs.getComponent<Components::Transform>(e)) {
                    vec2 finalPos = {t->position.x, t->position.y};
                    if (finalPos.x != origPos.x || finalPos.y != origPos.y)
                        executeCommand(std::make_unique<MoveEntityCmd>(ecs, e, origPos, finalPos));
                }
            }
        }
        m_pendingEntityDrag = false;
        m_isDraggingEntity  = false;
    }

    // ----- Box selection update / draw / finalize -----
    if (m_isBoxSelecting) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 mp   = ImGui::GetMousePos();
            m_boxSelectEnd = {mp.x, mp.y};

            // Draw selection rectangle overlay
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 a = {std::min(m_boxSelectStart.x, m_boxSelectEnd.x),
                        std::min(m_boxSelectStart.y, m_boxSelectEnd.y)};
            ImVec2 b = {std::max(m_boxSelectStart.x, m_boxSelectEnd.x),
                        std::max(m_boxSelectStart.y, m_boxSelectEnd.y)};
            dl->AddRectFilled(a, b, IM_COL32(100, 160, 255, 35));
            dl->AddRect      (a, b, IM_COL32(100, 160, 255, 220), 0.f, 0, 1.5f);
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_isBoxSelecting = false;

            float minSX = std::min(m_boxSelectStart.x, m_boxSelectEnd.x);
            float maxSX = std::max(m_boxSelectStart.x, m_boxSelectEnd.x);
            float minSY = std::min(m_boxSelectStart.y, m_boxSelectEnd.y);
            float maxSY = std::max(m_boxSelectStart.y, m_boxSelectEnd.y);

            // Only act if box is large enough to be intentional
            constexpr float kMinBox = 4.0f;
            if ((maxSX - minSX) > kMinBox || (maxSY - minSY) > kMinBox) {
                bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                             ImGui::IsKeyDown(ImGuiKey_RightShift);
                if (!shift) {
                    m_selectedEntities.clear();
                    m_selectedEntity = {0, 0};
                }

                ecs.each<Components::Transform>([&](ECS::Entity e, Components::Transform& t) {
                    vec2 sc = worldToScreen({t.position.x, t.position.y},
                                           camP, m_editorCamZoom, vpMin, m_viewportSize);
                    if (sc.x >= minSX && sc.x <= maxSX &&
                        sc.y >= minSY && sc.y <= maxSY) {
                        bool alreadyIn = std::any_of(
                            m_selectedEntities.begin(), m_selectedEntities.end(),
                            [&](ECS::Entity se){ return se.id == e.id && se.generation == e.generation; });
                        if (!alreadyIn) {
                            m_selectedEntities.push_back(e);
                            if (!ecs.isValid(m_selectedEntity))
                                m_selectedEntity = e;
                        }
                    }
                });

                // Ensure primary is one of the selected entities
                if (!m_selectedEntities.empty()) {
                    bool primaryOk = std::any_of(
                        m_selectedEntities.begin(), m_selectedEntities.end(),
                        [&](ECS::Entity e){ return e.id == m_selectedEntity.id && e.generation == m_selectedEntity.generation; });
                    if (!primaryOk)
                        m_selectedEntity = m_selectedEntities.front();
                }
            }
            // Box too small = just a click on empty space → selection stays cleared
        }
    }

    // ----- Drag-drop: texture / prefab from browsers → spawn entity -----
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
            const char* texName = static_cast<const char*>(pl->Data);
            ImVec2 mp = ImGui::GetMousePos();
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
            m_selectedEntities.clear();
            m_selectedEntity = raw->createdEntity();
            m_selectedEntities.push_back(m_selectedEntity);
        }
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("PREFAB_NAME")) {
            const char* prefabName = static_cast<const char*>(pl->Data);
            ImVec2 mp = ImGui::GetMousePos();
            vec2   wPos = screenToWorld({mp.x, mp.y}, camP, m_editorCamZoom, vpMin, m_viewportSize);
            if (m_snapToGrid) wPos = snapToGrid(wPos, m_gridSize);
            ECS::Entity e = spawnPrefab(prefabName, wPos);
            if (m_scene.GetWorld().isValid(e)) {
                // Record in tilemap data so it participates in save/load
                m_tilemap.push_back({prefabName, wPos, m_currentLayer, e});
                if (auto* spr = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(e))
                    spr->sortOrder = m_currentLayer;
                getOrCreateLayerGroup(m_currentLayer)->entities.push_back(e);
                m_selectedEntities.clear();
                m_selectedEntity = e;
                m_selectedEntities.push_back(e);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // ----- Gizmos (drawn after Image so overlays are visible) -----
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
    // Always clear hover flag each frame; set below if handle is hovered
    m_gizmoHoveredHandle = false;

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
        if (hoverX || hoverY) m_gizmoHoveredHandle = true;

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

        // Start drag — capture group original positions
        if (mouseClick && (hoverX || hoverY)) {
            m_gizmoActiveAxis   = hoverX ? 0 : 1;
            m_gizmoOriginalPos  = {trans->position.x, trans->position.y};
            m_gizmoClickOffset  = {mousePos.x, mousePos.y};
            m_gizmoGroupOriginalPos.clear();
            for (auto& e : m_selectedEntities) {
                if (auto* t = world.getComponent<Components::Transform>(e))
                    m_gizmoGroupOriginalPos.push_back({e, {t->position.x, t->position.y}});
            }
        }

        // Drag — move primary and apply same delta to group
        if (mouseDown && m_gizmoActiveAxis != -1) {
            float dx = mousePos.x - m_gizmoClickOffset.x;
            float dy = mousePos.y - m_gizmoClickOffset.y;
            vec2 screenMove(dx, dy);
            vec2 axisDir = normalize((m_gizmoActiveAxis == 0) ? xAxisScr : yAxisScr);
            float proj      = dot(screenMove, axisDir) / zoom;
            vec2  worldDir  = (m_gizmoActiveAxis == 0) ? vec2(c, s) : vec2(-s, c);
            vec2  newPos    = m_gizmoOriginalPos + worldDir * proj;
            if (m_snapToGrid || trans->snapToGrid) newPos = snapToGrid(newPos, m_gridSize);
            trans->position = newPos;
            vec2 delta = newPos - m_gizmoOriginalPos;
            // Apply same delta to secondary entities
            for (auto& [e, origPos] : m_gizmoGroupOriginalPos) {
                if (e.id == m_selectedEntity.id && e.generation == m_selectedEntity.generation)
                    continue; // primary already updated
                if (auto* t = world.getComponent<Components::Transform>(e))
                    t->position = origPos + delta;
            }
        }

        // Undo commands for all moved entities on release
        if (releasedAxis != -1) {
            for (auto& [e, origPos] : m_gizmoGroupOriginalPos) {
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    vec2 finalPos = {t->position.x, t->position.y};
                    if (finalPos.x != origPos.x || finalPos.y != origPos.y)
                        executeCommand(std::make_unique<MoveEntityCmd>(world, e, origPos, finalPos));
                }
            }
        }
    }
    else if (m_gizmoMode == GizmoMode::Rotation) {
        float radius = kLen * 0.8f;

        float dist = std::sqrt((mousePos.x - origin.x) * (mousePos.x - origin.x) +
                               (mousePos.y - origin.y) * (mousePos.y - origin.y));
        bool hover = std::abs(dist - radius) < 10.0f;
        if (m_gizmoActiveAxis == 2) hover = true;
        if (hover) { m_gizmoHoveredHandle = true; dl->AddCircle(origin, radius, colHover, 64, kThick); }
        else dl->AddCircle(origin, radius, colZ, 64, kThick * 0.5f);

        if (mouseClick && hover) {
            m_gizmoActiveAxis         = 2;
            m_gizmoOriginalRotation   = trans->rotation;
            m_gizmoClickOffset.x      = std::atan2(mousePos.y - origin.y,
                                                    mousePos.x - origin.x);
            m_gizmoGroupOriginalRot.clear();
            for (auto& e : m_selectedEntities) {
                if (auto* t = world.getComponent<Components::Transform>(e))
                    m_gizmoGroupOriginalRot.push_back({e, t->rotation});
            }
        }

        if (mouseDown && m_gizmoActiveAxis == 2) {
            float currentAngle = std::atan2(mousePos.y - origin.y,
                                             mousePos.x - origin.x);
            float delta = currentAngle - m_gizmoClickOffset.x;
            trans->rotation = m_gizmoOriginalRotation + delta;
            for (auto& [e, origRot] : m_gizmoGroupOriginalRot) {
                if (e.id == m_selectedEntity.id && e.generation == m_selectedEntity.generation)
                    continue;
                if (auto* t = world.getComponent<Components::Transform>(e))
                    t->rotation = origRot + delta;
            }
        }

        if (releasedAxis == 2) {
            for (auto& [e, origRot] : m_gizmoGroupOriginalRot) {
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    if (t->rotation != origRot)
                        executeCommand(std::make_unique<RotateEntityCmd>(
                            world, e, origRot, t->rotation));
                }
            }
        }
    }
    else if (m_gizmoMode == GizmoMode::Scale) {
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
        if (hoverX || hoverY) m_gizmoHoveredHandle = true;

        dl->AddLine(origin, xEnd, hoverX ? colHover : colX, kThick);
        dl->AddLine(origin, yEnd, hoverY ? colHover : colY, kThick);

        // Box heads (distinguishes scale from translate arrows)
        auto drawBox = [&](ImVec2 center, ImU32 col) {
            dl->AddRectFilled({center.x - 5, center.y - 5},
                              {center.x + 5, center.y + 5}, col);
        };
        drawBox(xEnd, hoverX ? colHover : colX);
        drawBox(yEnd, hoverY ? colHover : colY);

        // Start drag — capture group original scales
        if (mouseClick && (hoverX || hoverY)) {
            m_gizmoActiveAxis    = hoverX ? 0 : 1;
            m_gizmoOriginalScale = {trans->scale.x, trans->scale.y};
            m_gizmoClickOffset   = {mousePos.x, mousePos.y};
            m_gizmoGroupOriginalScale.clear();
            for (auto& e : m_selectedEntities) {
                if (auto* t = world.getComponent<Components::Transform>(e))
                    m_gizmoGroupOriginalScale.push_back({e, {t->scale.x, t->scale.y}});
            }
        }

        // Drag — project mouse movement onto axis, apply as scale delta
        if (mouseDown && m_gizmoActiveAxis != -1) {
            float dx = mousePos.x - m_gizmoClickOffset.x;
            float dy = mousePos.y - m_gizmoClickOffset.y;
            vec2 axisDir = normalize((m_gizmoActiveAxis == 0) ? xAxisScr : yAxisScr);
            float proj       = dot(vec2(dx, dy), axisDir);
            float scaleDelta = proj * 0.01f;
            if (m_gizmoActiveAxis == 0)
                trans->scale.x = m_gizmoOriginalScale.x + scaleDelta;
            else
                trans->scale.y = m_gizmoOriginalScale.y + scaleDelta;
            // Apply same relative scale delta to secondary entities
            for (auto& [e, origScale] : m_gizmoGroupOriginalScale) {
                if (e.id == m_selectedEntity.id && e.generation == m_selectedEntity.generation)
                    continue;
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    if (m_gizmoActiveAxis == 0)
                        t->scale.x = origScale.x + scaleDelta;
                    else
                        t->scale.y = origScale.y + scaleDelta;
                }
            }
        }

        // Undo commands for all scaled entities on release
        if (releasedAxis != -1) {
            for (auto& [e, origScale] : m_gizmoGroupOriginalScale) {
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    vec2 finalScale = {t->scale.x, t->scale.y};
                    if (finalScale.x != origScale.x || finalScale.y != origScale.y)
                        executeCommand(std::make_unique<ScaleEntityCmd>(
                            world, e, origScale, finalScale));
                }
            }
        }
    }

    // ----- Highlight secondary selected entities -----
    for (auto& e : m_selectedEntities) {
        if (e.id == m_selectedEntity.id && e.generation == m_selectedEntity.generation)
            continue; // primary — already shown by gizmo or green outline elsewhere
        auto* t = world.getComponent<Components::Transform>(e);
        if (!t) continue;
        vec2 sc = worldToScreen({t->position.x, t->position.y},
                                camPos, zoom, viewportMin, m_viewportSize);
        dl->AddCircle({sc.x, sc.y}, 8.0f, IM_COL32(255, 200, 0, 200), 16, 2.0f);
    }
}

// ---------------------------------------------------------------------------
// renderAssetBrowser
// ---------------------------------------------------------------------------

void EditorState::renderAssetBrowser() {
    ImGui::Begin("Assets");

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path assetsRoot("assets/textures");

    // Ensure directory exists
    if (!fs::exists(assetsRoot)) {
        fs::create_directories(assetsRoot, ec);
    }

    // ── Handle file drops from Windows Explorer ───────────────────────────────
    {
        auto dropped = Core::Engine::Get().window().consumeDroppedFiles();
        for (const auto& filePath : dropped) {
            std::string ext = fs::path(filePath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const char* kImgExts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
            for (auto e : kImgExts) {
                if (ext == e) {
                    std::string stem = fs::path(filePath).stem().string();
                    // Import into CURRENT directory
                    fs::path dest = fs::path(m_currentAssetDir) / fs::path(filePath).filename();
                    Assets::TextureManager::ImportTexture(filePath.c_str(), stem.c_str());
                    // If imported to a different location than the manager's default,
                    // we might need to handle the move, but ImportTexture handles the copy to assets/textures.
                    break;
                }
            }
            // Route font files to the font browser import modal
            if (ext == ".ttf" || ext == ".otf") {
                strncpy_s(m_newFontPathBuf, filePath.c_str(), sizeof(m_newFontPathBuf) - 1);
                if (!m_newFontNameBuf[0]) {
                    std::string stem = fs::path(filePath).stem().string();
                    strncpy_s(m_newFontNameBuf, stem.c_str(), sizeof(m_newFontNameBuf) - 1);
                }
                m_newFontSize = 32;
                m_pendingFontImportPopup = true;
            }
        }
    }

    // ── Navigation Bar ───────────────────────────────────────────────────────
    if (m_currentAssetDir != assetsRoot.generic_string()) {
        if (ImGui::Button("<- Back##assets")) {
            m_currentAssetDir = fs::path(m_currentAssetDir).parent_path().generic_string();
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentAssetDir, assetsRoot, ec).generic_string().c_str());
    ImGui::Separator();

    if (ImGui::Button("Add Texture")) {
        m_newAssetNameBuf[0] = '\0';
        m_newAssetPathBuf[0] = '\0';
        ImGui::OpenPopup("##AddTexture");
    }
    ImGui::SameLine();
    if (ImGui::Button("New Folder##assets")) {
        m_newScriptFolderBuf[0] = '\0'; // reuse buffer
        ImGui::OpenPopup("##NewAssetFolder");
    }

    if (ImGui::BeginPopupModal("##AddTexture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import Texture");
        ImGui::Separator();
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##srcpath", m_newAssetPathBuf, sizeof(m_newAssetPathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
#ifdef _WIN32
            OPENFILENAMEA ofn{}; char szFile[512]{};
            ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
            ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) {
                strncpy_s(m_newAssetPathBuf, szFile, sizeof(m_newAssetPathBuf) - 1);
                if (!m_newAssetNameBuf[0]) {
                    std::string stem = fs::path(szFile).stem().string();
                    strncpy_s(m_newAssetNameBuf, stem.c_str(), sizeof(m_newAssetNameBuf) - 1);
                }
            }
#endif
        }
        ImGui::InputText("Asset ID", m_newAssetNameBuf, sizeof(m_newAssetNameBuf));
        if (ImGui::Button("Import", ImVec2(100, 0))) {
            Assets::TextureManager::ImportTexture(m_newAssetPathBuf, m_newAssetNameBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("##NewAssetFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create Folder in: %s", m_currentAssetDir.c_str());
        ImGui::InputText("Folder name##nf", m_newScriptFolderBuf, sizeof(m_newScriptFolderBuf));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptFolderBuf[0]) fs::create_directories(fs::path(m_currentAssetDir) / m_newScriptFolderBuf, ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Rename/Delete Modals ────────────────────────────────────────────────
    if (!m_renamingAsset.empty()) ImGui::OpenPopup("##RenameAsset");
    if (ImGui::BeginPopupModal("##RenameAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename asset: %s", m_renamingAsset.c_str());
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
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAsset.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (!m_deletingAsset.empty()) ImGui::OpenPopup("##DeleteAsset");
    if (ImGui::BeginPopupModal("##DeleteAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s' from disk?", m_deletingAsset.c_str());
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            Assets::TextureManager::DeleteFromDisk(m_deletingAsset);
            m_deletingAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAsset.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Rename folder modal ───────────────────────────────────────────────────
    if (!m_renamingAssetDirPath.empty()) ImGui::OpenPopup("##RenameAssetDir");
    if (ImGui::BeginPopupModal("##RenameAssetDir", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename folder: %s", fs::path(m_renamingAssetDirPath).filename().string().c_str());
        ImGui::InputText("New name##adir", m_renameAssetDirBuf, sizeof(m_renameAssetDirBuf));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (m_renameAssetDirBuf[0]) {
                fs::path src(m_renamingAssetDirPath);
                fs::rename(src, src.parent_path() / m_renameAssetDirBuf, ec);
            }
            m_renamingAssetDirPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAssetDirPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Delete folder modal ───────────────────────────────────────────────────
    if (!m_deletingAssetDirPath.empty()) ImGui::OpenPopup("##DeleteAssetDir");
    if (ImGui::BeginPopupModal("##DeleteAssetDir", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete folder '%s' and all its contents?", fs::path(m_deletingAssetDirPath).filename().string().c_str());
        ImGui::TextDisabled("This cannot be undone.");
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            fs::remove_all(fs::path(m_deletingAssetDirPath), ec);
            if (m_currentAssetDir.find(m_deletingAssetDirPath) == 0)
                m_currentAssetDir = assetsRoot.generic_string();
            m_deletingAssetDirPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAssetDirPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Move to folder modal ──────────────────────────────────────────────────
    if (!m_movingAssetPath.empty()) ImGui::OpenPopup("##MoveAsset");
    if (ImGui::BeginPopupModal("##MoveAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Move '%s' to:", fs::path(m_movingAssetPath).filename().string().c_str());
        ImGui::Separator();
        std::vector<std::string> assetFolders;
        assetFolders.push_back("(root)");
        for (const auto& entry : fs::recursive_directory_iterator(assetsRoot, ec))
            if (entry.is_directory())
                assetFolders.push_back(fs::relative(entry.path(), assetsRoot, ec).generic_string());
        for (const auto& folder : assetFolders) {
            if (ImGui::Selectable(folder.c_str())) {
                fs::path destDir = (folder == "(root)") ? assetsRoot : assetsRoot / folder;
                fs::path destFile = destDir / fs::path(m_movingAssetPath).filename();
                if (!fs::exists(destFile)) fs::rename(fs::path(m_movingAssetPath), destFile, ec);
                m_movingAssetPath.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_movingAssetPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ── Grid ────────────────────────────────────────────────────────────────
    std::vector<fs::path> items;
    for (const auto& entry : fs::directory_iterator(m_currentAssetDir, ec)) {
        if (entry.is_directory()) {
            items.push_back(entry.path());
        } else {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
                items.push_back(entry.path());
        }
    }
    std::sort(items.begin(), items.end(), [](const fs::path& a, const fs::path& b) {
        bool aDir = fs::is_directory(a), bDir = fs::is_directory(b);
        if (aDir != bDir) return aDir;
        return a.filename().string() < b.filename().string();
    });

    float thumbSize = 64.0f;
    float padding = 8.0f;
    float cellSize = thumbSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelWidth / cellSize));

    if (ImGui::BeginTable("##TextureTbl", cols)) {
        for (const auto& path : items) {
            ImGui::TableNextColumn();
            std::string fname = path.filename().string();
            std::string stem = path.stem().string();
            bool isDir = fs::is_directory(path);
            ImGui::PushID(fname.c_str());

            if (isDir) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
                ImGui::Button("##icon", ImVec2(thumbSize, thumbSize));
                ImGui::PopStyleColor();
                DrawFolderIcon();
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    m_currentAssetDir = path.generic_string();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payloadFile = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_FILE")) {
                        fs::path src((const char*)payloadFile->Data);
                        fs::rename(src, path / src.filename(), ec);
                    } else if (const ImGuiPayload* payloadAsset = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
                        std::string srcPath = Assets::TextureManager::GetFilePath((const char*)payloadAsset->Data);
                        if (!srcPath.empty()) {
                            fs::path src(srcPath);
                            fs::rename(src, path / src.filename(), ec);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        m_renamingAssetDirPath = path.string();
                        strncpy_s(m_renameAssetDirBuf, fname.c_str(), sizeof(m_renameAssetDirBuf)-1);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) m_deletingAssetDirPath = path.string();
                    ImGui::EndPopup();
                }
            } else {
                auto tex = Assets::TextureManager::Get(stem);
                if (!tex || tex->id() == 0) {
                    Assets::TextureManager::LoadTexture(stem, path.generic_string());
                    tex = Assets::TextureManager::Get(stem);
                }

                bool isSelected = (m_selectedAsset == stem);
                if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                if (ImGui::ImageButton("##tb", (ImTextureID)(intptr_t)(tex?tex->id():0), ImVec2(thumbSize, thumbSize), ImVec2(0,1), ImVec2(1,0)))
                    m_selectedAsset = stem;
                if (isSelected) ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource()) {
                    std::string pstr = path.generic_string();
                    ImGui::SetDragDropPayload("TEXTURE_ASSET_FILE", pstr.c_str(), pstr.size() + 1);
                    ImGui::SetDragDropPayload("TEXTURE_ASSET", stem.c_str(), stem.size() + 1);
                    ImGui::Text("%s", stem.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Apply to selected")) {
                        if (auto* sr = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(m_selectedEntity))
                            sr->textureId = stem;
                    }
                    if (ImGui::MenuItem("Rename")) { m_renamingAsset = stem; strncpy_s(m_renameAssetBuf, stem.c_str(), sizeof(m_renameAssetBuf)-1); }
                    if (ImGui::MenuItem("Move to...")) { m_movingAssetPath = path.string(); }
                    if (ImGui::MenuItem("Duplicate")) {
                        fs::path src = path;
                        fs::path dst = src.parent_path() / (src.stem().string() + "_copy" + src.extension().string());
                        int n = 1;
                        while (fs::exists(dst))
                            dst = src.parent_path() / (src.stem().string() + "_copy" + std::to_string(n++) + src.extension().string());
                        fs::copy_file(src, dst, ec);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) m_deletingAsset = stem;
                    ImGui::EndPopup();
                }
            }
            ImGui::TextUnformatted(fname.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderAnimationsBrowser
// ---------------------------------------------------------------------------

void EditorState::renderAnimationsBrowser() {
    ImGui::Begin("Animations");

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path animRoot("assets/animations");

    if (!fs::exists(animRoot)) {
        fs::create_directories(animRoot, ec);
    }

    // ── Navigation Bar ───────────────────────────────────────────────────────
    if (m_currentAnimDir != animRoot.generic_string()) {
        if (ImGui::Button("<- Back##anim")) {
            m_currentAnimDir = fs::path(m_currentAnimDir).parent_path().generic_string();
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentAnimDir, animRoot, ec).generic_string().c_str());
    ImGui::Separator();

    if (ImGui::Button("New Folder##anim")) {
        m_newScriptFolderBuf[0] = '\0';
        ImGui::OpenPopup("##NewAnimFolder");
    }
    ImGui::SameLine();
    if (ImGui::Button("New Animation")) {
        memset(m_newAnimNameBuf, 0, sizeof(m_newAnimNameBuf));
        m_newAnimModalOpen = true;
    }

    if (m_newAnimModalOpen) ImGui::OpenPopup("##NewAnimFile");
    if (ImGui::BeginPopupModal("##NewAnimFile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Animation name:");
        ImGui::InputText("##animfilename", m_newAnimNameBuf, sizeof(m_newAnimNameBuf));
        if (ImGui::Button("Create", ImVec2(100, 0)) && m_newAnimNameBuf[0] != '\0') {
            namespace fs = std::filesystem;
            std::error_code ec2;
            std::string newPath = (fs::path(m_currentAnimDir) / (std::string(m_newAnimNameBuf) + ".json")).string();
            std::ofstream f(newPath);
            f << "{\"animations\":[]}";
            f.close();
            m_newAnimModalOpen = false;
            m_editingAnimPath    = newPath;
            m_editingAnimRelPath = fs::relative(newPath, "assets/animations", ec2).string();
            m_animEditorAnim.clearAll();
            m_animEdClip  = "";
            m_animEdFrame = -1;
            m_animEdPlaying = false;
            m_animEditorOpen = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_newAnimModalOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("##NewAnimFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create Folder in: %s", m_currentAnimDir.c_str());
        ImGui::InputText("Folder name##nfan", m_newScriptFolderBuf, sizeof(m_newScriptFolderBuf));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptFolderBuf[0]) fs::create_directories(fs::path(m_currentAnimDir) / m_newScriptFolderBuf, ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!m_renamingAnimPath.empty()) ImGui::OpenPopup("##RenameAnim");
    if (ImGui::BeginPopupModal("##RenameAnim", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path rPath(m_renamingAnimPath);
        ImGui::Text("Rename: %s", rPath.filename().string().c_str());
        ImGui::InputText("New name##rnan", m_renameAnimBuf, sizeof(m_renameAnimBuf));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (m_renameAnimBuf[0]) {
                std::string newName = m_renameAnimBuf;
                if (!fs::is_directory(rPath) && (newName.size() < 5 || newName.substr(newName.size()-5) != ".json"))
                    newName += ".json";
                fs::rename(rPath, rPath.parent_path() / newName, ec);
            }
            m_renamingAnimPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAnimPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (!m_deletingAnimPath.empty()) ImGui::OpenPopup("##DeleteAnim");
    if (ImGui::BeginPopupModal("##DeleteAnim", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path dPath(m_deletingAnimPath);
        ImGui::Text("Delete '%s'?", dPath.filename().string().c_str());
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            fs::remove_all(dPath, ec);
            m_deletingAnimPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAnimPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ── Grid ────────────────────────────────────────────────────────────────
    std::vector<fs::path> items;
    for (const auto& entry : fs::directory_iterator(m_currentAnimDir, ec)) {
        if (entry.is_directory() || entry.path().extension() == ".json")
            items.push_back(entry.path());
    }
    std::sort(items.begin(), items.end(), [](const fs::path& a, const fs::path& b) {
        bool aDir = fs::is_directory(a), bDir = fs::is_directory(b);
        if (aDir != bDir) return aDir;
        return a.filename().string() < b.filename().string();
    });

    constexpr float kIconW = 64.0f, kIconH = 72.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelW / (kIconW + 20.0f)));

    if (ImGui::BeginTable("##AnimTbl", cols)) {
        for (const auto& path : items) {
            ImGui::TableNextColumn();
            std::string fname = path.filename().string();
            std::string stem = path.stem().string();
            std::string relPath = fs::relative(path, animRoot, ec).generic_string();
            bool isDir = fs::is_directory(path);
            ImGui::PushID(fname.c_str());

            float colW = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            if (isDir) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
                if (ImGui::Button("##icon", ImVec2(kIconW, kIconH))) m_currentAnimDir = path.generic_string();
                ImGui::PopStyleColor();
                DrawFolderIcon();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ANIMATION_ASSET")) {
                        fs::path src(animRoot / (const char*)pl->Data);
                        fs::rename(src, path / src.filename(), ec);
                    }
                    ImGui::EndDragDropTarget();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.15f, 0.35f, 1.0f));
                ImGui::Button("##icon", ImVec2(kIconW, kIconH));
                ImGui::PopStyleColor();
                
                // Anim icon (simple reel shape)
                ImVec2 bMin = ImGui::GetItemRectMin(), bMax = ImGui::GetItemRectMax();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float cx = (bMin.x+bMax.x)*0.5f, cy = (bMin.y+bMax.y)*0.5f;
                dl->AddRectFilled({cx-15, cy-15}, {cx+15, cy+15}, IM_COL32(200, 150, 255, 200), 2.0f);
                dl->AddCircleFilled({cx-8, cy-8}, 3.0f, IM_COL32(50, 20, 80, 255));
                dl->AddCircleFilled({cx+8, cy-8}, 3.0f, IM_COL32(50, 20, 80, 255));
                dl->AddCircleFilled({cx-8, cy+8}, 3.0f, IM_COL32(50, 20, 80, 255));
                dl->AddCircleFilled({cx+8, cy+8}, 3.0f, IM_COL32(50, 20, 80, 255));

                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ANIMATION_ASSET", relPath.c_str(), relPath.size() + 1);
                    ImGui::Text("%s", fname.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Edit")) {
                        std::error_code ec2;
                        m_editingAnimPath    = path.string();
                        m_editingAnimRelPath = std::filesystem::relative(path, "assets/animations", ec2).string();
                        m_animEditorAnim.clearAll();
                        m_animEditorAnim.loadFromPath(m_editingAnimPath);
                        m_animEdClip  = m_animEditorAnim.clips.empty() ? "" : m_animEditorAnim.clips.begin()->first;
                        m_animEdFrame = -1;
                        m_animEdPlaying = false;
                        m_animEditorOpen = true;
                    }
                    if (ImGui::MenuItem("Rename")) { m_renamingAnimPath = path.string(); strncpy_s(m_renameAnimBuf, fname.c_str(), sizeof(m_renameAnimBuf)-1); }
                    if (ImGui::MenuItem("Delete")) m_deletingAnimPath = path.string();
                    ImGui::EndPopup();
                }
            }
            ImGui::TextUnformatted(fname.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderAnimationEditor
// ---------------------------------------------------------------------------

void EditorState::renderAnimationEditor() {
    if (!m_animEditorOpen) return;

    ImGui::SetNextWindowSize(ImVec2(500, 650), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animation Editor", &m_animEditorOpen)) { ImGui::End(); return; }

    // ── Top row: texture picker (left) + preview image (right) ──────────────
    ImGui::BeginGroup();
    ImGui::Text("File: %s", m_editingAnimRelPath.c_str());

    // Texture combo
    {
        auto texNames = Assets::TextureManager::GetLoadedTextureNames();
        if (ImGui::BeginCombo("Texture##aned", m_animEdTexture.empty() ? "(none)" : m_animEdTexture.c_str())) {
            for (auto& n : texNames) {
                if (ImGui::Selectable(n.c_str(), n == m_animEdTexture))
                    m_animEdTexture = n;
            }
            ImGui::EndCombo();
        }
    }
    ImGui::EndGroup();

    // Preview image — top-right corner, same line as the group above
    ImGui::SameLine(ImGui::GetWindowWidth() - 148.f);
    if (m_animPreviewFB.getTextureID() != 0) {
        ImGui::Image(reinterpret_cast<void*>(static_cast<uint64_t>(
                         m_animPreviewFB.getTextureID())),
                     ImVec2(140.f, 140.f), ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
    } else {
        ImGui::Dummy(ImVec2(140.f, 140.f));
    }

    ImGui::Separator();

    // ── Clip selection ───────────────────────────────────────────────────────
    {
        std::vector<std::string> clipNames;
        for (auto& [k, _] : m_animEditorAnim.clips) clipNames.push_back(k);
        std::sort(clipNames.begin(), clipNames.end());

        if (ImGui::BeginCombo("Clip##aned", m_animEdClip.c_str())) {
            for (auto& n : clipNames) {
                if (ImGui::Selectable(n.c_str(), n == m_animEdClip)) {
                    m_animEdClip = n;
                    m_animEdFrame = -1;
                    m_animEdPlaying = false;
                    m_animEdTimer = 0.f;
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::InputText("##newclipname", m_animEdNewClipName, sizeof(m_animEdNewClipName));
    ImGui::SameLine();
    if (ImGui::Button("Add Clip") && m_animEdNewClipName[0] != '\0') {
        m_animEditorAnim.createClip(std::string(m_animEdNewClipName), m_animEdLoop);
        m_animEdClip = std::string(m_animEdNewClipName);
        m_animEdNewClipName[0] = '\0';
        m_animEdFrame = -1;
        m_animEdPlaying = false;
    }
    if (!m_animEdClip.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Delete Clip")) {
            m_animEditorAnim.removeClip(m_animEdClip);
            m_animEdClip  = m_animEditorAnim.clips.empty() ? "" : m_animEditorAnim.clips.begin()->first;
            m_animEdFrame = -1;
            m_animEdPlaying = false;
        }
    }

    // ── Playback controls ────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::Checkbox("Loop##aned", &m_animEdLoop);
    ImGui::SameLine();
    if (ImGui::Button(m_animEdPlaying ? "Pause##aned" : "Play##aned")) m_animEdPlaying = !m_animEdPlaying;
    ImGui::SameLine();
    if (ImGui::Button("Reset##aned")) { m_animEdFrame = 0; m_animEdTimer = 0.f; m_animEdPlaying = false; }

    // Show current frame index
    if (!m_animEdClip.empty()) {
        if (auto* clip = m_animEditorAnim.getClip(m_animEdClip)) {
            int total = static_cast<int>(clip->frames.size());
            int disp  = (m_animEdFrame >= 0 && total > 0) ? m_animEdFrame : 0;
            ImGui::SameLine();
            ImGui::Text("Frame: %d / %d", disp, total);
        }
    }

    // ── Frame list ───────────────────────────────────────────────────────────
    ImGui::Separator();
    if (!m_animEdClip.empty()) {
        if (auto* clip = m_animEditorAnim.getClip(m_animEdClip)) {
            if (ImGui::Button("Add Frame##aned")) {
                Components::FrameData fd{};
                fd.duration = 0.1f;
                clip->frames.push_back(fd);
            }
            ImGui::SameLine();
            if (m_animEdFrame >= 0 && m_animEdFrame < static_cast<int>(clip->frames.size())) {
                if (ImGui::Button("Remove Frame##aned")) {
                    clip->frames.erase(clip->frames.begin() + m_animEdFrame);
                    m_animEdFrame = static_cast<int>(clip->frames.size()) > 0
                                    ? std::min(m_animEdFrame, static_cast<int>(clip->frames.size()) - 1)
                                    : -1;
                }
            }

            ImGui::BeginChild("FrameList##aned", ImVec2(0, 120), true);
            for (int i = 0; i < static_cast<int>(clip->frames.size()); ++i) {
                char label[32];
                snprintf(label, sizeof(label), "Frame %d (%.2fs)", i, clip->frames[i].duration);
                if (ImGui::Selectable(label, m_animEdFrame == i))
                    m_animEdFrame = i;
            }
            ImGui::EndChild();

            // ── Selected frame editor ────────────────────────────────────────
            if (m_animEdFrame >= 0 && m_animEdFrame < static_cast<int>(clip->frames.size())) {
                auto& fd = clip->frames[m_animEdFrame];
                ImGui::Separator();
                ImGui::Text("Frame %d", m_animEdFrame);
                ImGui::DragFloat("U##aned",        &fd.uv.x,  0.001f, 0.f, 1.f, "%.3f");
                ImGui::DragFloat("V##aned",        &fd.uv.y,  0.001f, 0.f, 1.f, "%.3f");
                ImGui::DragFloat("W##aned",        &fd.uv.z,  0.001f, 0.f, 1.f, "%.3f");
                ImGui::DragFloat("H##aned",        &fd.uv.w,  0.001f, 0.f, 1.f, "%.3f");
                ImGui::DragFloat("Duration##aned", &fd.duration, 0.01f, 0.01f, 5.f, "%.3f s");

                // Preview: show UV rect as a coloured indicator
                ImGui::Text("UV: (%.3f, %.3f)  Size: (%.3f, %.3f)", fd.uv.x, fd.uv.y, fd.uv.z, fd.uv.w);
            }
        }
    }

    // ── Grid generator ───────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Grid Generator##aned")) {
        ImGui::InputInt("Sheet W##aned", &m_animEdSheetW);
        ImGui::InputInt("Sheet H##aned", &m_animEdSheetH);
        ImGui::InputInt("Cell W##aned",  &m_animEdCellW);
        ImGui::InputInt("Cell H##aned",  &m_animEdCellH);
        ImGui::InputInt("Start Frame##aned", &m_animEdStartF);
        ImGui::InputInt("End Frame##aned",   &m_animEdEndF);

        // Guard against zero/negative values
        m_animEdSheetW = std::max(m_animEdSheetW, 1);
        m_animEdSheetH = std::max(m_animEdSheetH, 1);
        m_animEdCellW  = std::max(m_animEdCellW,  1);
        m_animEdCellH  = std::max(m_animEdCellH,  1);
        m_animEdStartF = std::max(m_animEdStartF, 0);
        m_animEdEndF   = std::max(m_animEdEndF,   m_animEdStartF);

        if (ImGui::Button("Generate Frames##aned") && !m_animEdClip.empty()) {
            auto* clip = m_animEditorAnim.getClip(m_animEdClip);
            if (!clip) {
                m_animEditorAnim.createClip(m_animEdClip, m_animEdLoop);
                clip = m_animEditorAnim.getClip(m_animEdClip);
            }
            clip->frames.clear();
            int   cols = m_animEdSheetW / m_animEdCellW;
            float uw   = static_cast<float>(m_animEdCellW) / static_cast<float>(m_animEdSheetW);
            float uh   = static_cast<float>(m_animEdCellH) / static_cast<float>(m_animEdSheetH);
            for (int fi = m_animEdStartF; fi <= m_animEdEndF; ++fi) {
                int col = fi % cols;
                int row = fi / cols;
                Components::FrameData fd{};
                fd.uv       = { col * uw, row * uh, uw, uh };
                fd.duration = 0.1f;
                clip->frames.push_back(fd);
            }
            m_animEdFrame = 0;
        }
    }

    // ── Bulk duration ────────────────────────────────────────────────────────
    if (!m_animEdClip.empty()) {
        if (auto* clip = m_animEditorAnim.getClip(m_animEdClip); clip && !clip->frames.empty()) {
            ImGui::Separator();
            static float bulkDuration = 0.1f;
            ImGui::DragFloat("Bulk Duration##aned", &bulkDuration, 0.01f, 0.01f, 5.f, "%.3f s");
            ImGui::SameLine();
            if (ImGui::Button("Apply to All##aned")) {
                for (auto& fd : clip->frames) fd.duration = bulkDuration;
            }
        }
    }

    // ── Save ─────────────────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::Button("Save##aned") && !m_editingAnimPath.empty()) {
        nlohmann::json j = m_animEditorAnim.serialize(m_editingAnimPath);
        std::ofstream outf(m_editingAnimPath);
        if (outf.is_open())
            outf << j.dump(2);
    }

    ImGui::End();
}

void EditorState::renderScriptsBrowser() {
    ImGui::Begin("Scripts");

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path scriptsRoot("assets/scripts");

    // ── Navigation Bar ───────────────────────────────────────────────────────
    if (m_currentScriptDir != scriptsRoot.generic_string()) {
        if (ImGui::Button("<- Back")) {
            m_currentScriptDir = fs::path(m_currentScriptDir).parent_path().generic_string();
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentScriptDir, scriptsRoot, ec).generic_string().c_str());
    ImGui::Separator();

    // ── Add Script button ────────────────────────────────────────────────────
    if (ImGui::Button("Add Script")) {
        m_newScriptBuf[0]       = '\0';
        m_newScriptFolderBuf[0] = '\0';
        ImGui::OpenPopup("##AddScript");
    }
    ImGui::SameLine();
    if (ImGui::Button("New Folder")) {
        m_newScriptFolderBuf[0] = '\0';
        ImGui::OpenPopup("##NewScriptFolder");
    }

    // Add Script popup
    if (ImGui::BeginPopupModal("##AddScript", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create New Script");
        ImGui::Separator();
        ImGui::InputText("Name##ns", m_newScriptBuf, sizeof(m_newScriptBuf));
        ImGui::InputText("Folder (optional)##nsf", m_newScriptFolderBuf, sizeof(m_newScriptFolderBuf));
        ImGui::TextDisabled("Leave folder blank for root.  '.lua' added automatically.");

        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptBuf[0]) {
                std::string fname = m_newScriptBuf;
                if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".lua")
                    fname += ".lua";
                std::string folder = m_newScriptFolderBuf;
                fs::path destDir = folder.empty()
                    ? scriptsRoot
                    : scriptsRoot / folder;
                fs::create_directories(destDir, ec);
                fs::path fullPath = destDir / fname;
                if (!fs::exists(fullPath)) {
                    std::ofstream f(fullPath.string());
                    if (!folder.empty()) {
                        // Scaffold a component-style script
                        f << "Properties = {\n";
                        f << "    -- { name = \"myField\", type = \"float\", default = 0.0, label = \"My Field\" },\n";
                        f << "}\n\n";
                        f << "function onStart(self, entity)\nend\n\n";
                        f << "function onUpdate(self, entity, dt)\nend\n\n";
                        f << "return { onStart = onStart, onUpdate = onUpdate }\n";
                    } else {
                        f << "-- " << fname << "\n\n";
                        f << "local M = {}\n\n";
                        f << "function M:onStart(entity)\nend\n\n";
                        f << "function M:onUpdate(entity, dt)\nend\n\n";
                        f << "return M\n";
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // New Folder popup
    if (ImGui::BeginPopupModal("##NewScriptFolder", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create Folder");
        ImGui::Separator();
        ImGui::InputText("Folder name##nf", m_newScriptFolderBuf, sizeof(m_newScriptFolderBuf));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptFolderBuf[0]) {
                fs::create_directories(scriptsRoot / m_newScriptFolderBuf, ec);
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
        ImGui::InputText("New name##rn", m_renameScriptBuf, sizeof(m_renameScriptBuf));
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

    // ── Move to folder modal ──────────────────────────────────────────────────
    if (!m_movingScriptPath.empty())
        ImGui::OpenPopup("##MoveScript");

    if (ImGui::BeginPopupModal("##MoveScript", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path movePath(m_movingScriptPath);
        ImGui::Text("Move '%s' to folder:", movePath.filename().string().c_str());
        ImGui::Separator();

        // Collect available subdirectories
        std::vector<std::string> folders;
        folders.push_back("(root)");
        for (const auto& entry : fs::recursive_directory_iterator(scriptsRoot, ec)) {
            if (entry.is_directory()) {
                auto rel = fs::relative(entry.path(), scriptsRoot, ec).generic_string();
                if (!rel.empty() && rel != ".")
                    folders.push_back(rel);
            }
        }

        for (const auto& folder : folders) {
            if (ImGui::Selectable(folder.c_str())) {
                fs::path destDir = (folder == "(root)")
                    ? scriptsRoot
                    : scriptsRoot / folder;
                fs::path destFile = destDir / movePath.filename();
                if (!fs::exists(destFile)) {
                    std::error_code ec2;
                    fs::rename(movePath, destFile, ec2);
                }
                m_movingScriptPath.clear();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_movingScriptPath.clear();
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

    // ── Collect items in current directory ──────────────────────────────────
    std::vector<fs::path> items;
    for (const auto& entry : fs::directory_iterator(m_currentScriptDir, ec)) {
        if (entry.is_directory() || entry.path().extension() == ".lua")
            items.push_back(entry.path());
    }

    // Sort: directories first, then files
    std::sort(items.begin(), items.end(), [](const fs::path& a, const fs::path& b) {
        bool aDir = fs::is_directory(a);
        bool bDir = fs::is_directory(b);
        if (aDir != bDir) return aDir;
        return a.filename().string() < b.filename().string();
    });

    constexpr float kIconW = 64.0f;
    constexpr float kIconH = 72.0f;
    constexpr float kMinCell = kIconW + 20.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelW / kMinCell));

    if (ImGui::BeginTable("##ScriptsTbl", cols, ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& path : items) {
            ImGui::TableNextColumn();
            bool isDir = fs::is_directory(path);
            std::string fname = path.filename().string();
            std::string relPath = fs::relative(path, scriptsRoot, ec).generic_string();
            
            ImGui::PushID(relPath.c_str());
            float colW = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            // ── Icon button ──────────────────────────────────────────────
            bool isComp = relPath.find("components") != std::string::npos;
            ImVec4 btnBase = isDir ? ImVec4(0.3f, 0.3f, 0.1f, 1.0f) : (isComp ? ImVec4(0.10f, 0.22f, 0.12f, 1.0f) : ImVec4(0.14f, 0.16f, 0.32f, 1.0f));
            ImVec4 btnHov  = isDir ? ImVec4(0.4f, 0.4f, 0.15f, 1.0f) : (isComp ? ImVec4(0.16f, 0.34f, 0.20f, 1.0f) : ImVec4(0.22f, 0.28f, 0.55f, 1.0f));

            ImGui::PushStyleColor(ImGuiCol_Button, btnBase);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            
            ImGui::Button("##icon", ImVec2(kIconW, kIconH));
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            if (isDir) {
                DrawFolderIcon();
            } else {
                DrawScriptIcon(isComp);
            }

            // Drag-drop
            if (!isDir) {
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SCRIPT_ASSET", relPath.c_str(), relPath.size() + 1);
                    ImGui::TextUnformatted(relPath.c_str());
                    ImGui::EndDragDropSource();
                }
            } else {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_ASSET")) {
                        std::string sourceRel = (const char*)payload->Data;
                        fs::path sourcePath = scriptsRoot / sourceRel;
                        fs::path destPath = path / sourcePath.filename();
                        if (!fs::exists(destPath)) {
                            fs::rename(sourcePath, destPath, ec);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            // Navigation / Opening
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (isDir) {
                    m_currentScriptDir = path.generic_string();
                } else {
                    OpenInVSCode(fs::absolute(path).string());
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", isDir ? "Double-click to open folder" : "Double-click to edit");

            // Label
            std::string label = fname;
            if (!isDir && label.size() > 4 && label.substr(label.size()-4) == ".lua")
                label = label.substr(0, label.size()-4);
            if (label.size() > 9) label = label.substr(0, 7) + "..";

            float textW = ImGui::CalcTextSize(label.c_str()).x;
            float textOX = (colW - textW) * 0.5f;
            if (textOX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOX);
            ImGui::TextUnformatted(label.c_str());

            // Context Menu
            if (ImGui::BeginPopupContextItem("##Ctx")) {
                if (!isDir && ImGui::MenuItem("Open in VS Code")) {
                    OpenInVSCode(fs::absolute(path).string());
                }
                if (ImGui::MenuItem("Rename")) {
                    m_renamingScriptPath = path.string();
                    strncpy_s(m_renameScriptBuf, fname.c_str(), sizeof(m_renameScriptBuf)-1);
                }
                if (!isDir && ImGui::MenuItem("Move to...")) {
                    m_movingScriptPath = path.string();
                    m_newScriptFolderBuf[0] = '\0';
                }
                if (!isDir && ImGui::MenuItem("Duplicate")) {
                    fs::path src = path;
                    fs::path dst = src.parent_path() / (src.stem().string() + "_copy" + src.extension().string());
                    int n = 1;
                    while (fs::exists(dst)) {
                        dst = src.parent_path() / (src.stem().string() + "_copy" + std::to_string(n++) + src.extension().string());
                    }
                    fs::copy_file(src, dst, ec);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete")) { m_deletingScriptPath = path.string(); }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderFontBrowser
// ---------------------------------------------------------------------------

void EditorState::renderFontBrowser() {
    ImGui::Begin("Fonts");

    namespace fs = std::filesystem;
    static constexpr const char* kFontConfig = "assets/configs/fonts.json";

    // ── Handle pending font import from dropped files ─────────────────────
    if (m_pendingFontImportPopup) {
        m_pendingFontImportPopup = false;
        ImGui::OpenPopup("##AddFont");
    }

    // ── Toolbar ───────────────────────────────────────────────────────────
    if (ImGui::Button("Add Font")) {
        m_newFontNameBuf[0] = '\0';
        m_newFontPathBuf[0] = '\0';
        m_newFontSize = 32;
        ImGui::OpenPopup("##AddFont");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(drag .ttf/.otf here)");
    ImGui::Separator();

    // ── Add Font modal ────────────────────────────────────────────────────
    if (ImGui::BeginPopupModal("##AddFont", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import Font");
        ImGui::Separator();

        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("File##fontpath", m_newFontPathBuf, sizeof(m_newFontPathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Browse...##font")) {
#ifdef _WIN32
            OPENFILENAMEA ofn{}; char szFile[512]{};
            ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Font Files\0*.ttf;*.otf\0All Files\0*.*\0";
            ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) {
                strncpy_s(m_newFontPathBuf, szFile, sizeof(m_newFontPathBuf) - 1);
                if (!m_newFontNameBuf[0]) {
                    std::string stem = fs::path(szFile).stem().string();
                    strncpy_s(m_newFontNameBuf, stem.c_str(), sizeof(m_newFontNameBuf) - 1);
                }
            }
#endif
        }

        ImGui::InputText("Name (ID)##fontname", m_newFontNameBuf, sizeof(m_newFontNameBuf));
        ImGui::InputInt("Size (pt)##fontsize", &m_newFontSize);
        if (m_newFontSize < 4) m_newFontSize = 4;

        ImGui::Spacing();
        if (ImGui::Button("Import##font", ImVec2(100, 0))) {
            if (m_newFontNameBuf[0] && m_newFontPathBuf[0] && m_newFontSize > 0) {
                Assets::FontManager::ImportFont(m_newFontPathBuf, m_newFontNameBuf,
                                                (unsigned int)m_newFontSize, kFontConfig);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##font", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Delete confirmation modal ─────────────────────────────────────────
    if (!m_deletingFont.empty()) ImGui::OpenPopup("##DeleteFont");
    if (ImGui::BeginPopupModal("##DeleteFont", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete font \"%s\"?", m_deletingFont.c_str());
        ImGui::Text("This will unload it, remove it from fonts.json,");
        ImGui::Text("and delete the file from assets/fonts/.");
        ImGui::Spacing();
        if (ImGui::Button("Delete##fontconfirm", ImVec2(100, 0))) {
            Assets::FontManager::DeleteFont(m_deletingFont, kFontConfig);
            m_deletingFont.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##fontcancel", ImVec2(100, 0))) {
            m_deletingFont.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Font list ─────────────────────────────────────────────────────────
    auto fontKeys = Assets::ResourceManager<Assets::FontManager>::GetKeys();
    std::sort(fontKeys.begin(), fontKeys.end());

    if (fontKeys.empty()) {
        ImGui::TextDisabled("No fonts loaded. Use 'Add Font' or drag a .ttf/.otf file here.");
    } else {
        if (ImGui::BeginTable("##FontTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Name",   ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Path",   ImGuiTableColumnFlags_WidthStretch, 4.0f);
            ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed,   60.0f);
            ImGui::TableHeadersRow();

            for (const auto& key : fontKeys) {
                std::string filePath = Assets::FontManager::GetFilePath(key, kFontConfig);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(key.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(filePath.empty() ? "(built-in)" : filePath.c_str());
                ImGui::TableSetColumnIndex(2);
                std::string delId = "X##fdel_" + key;
                if (ImGui::Button(delId.c_str())) {
                    m_deletingFont = key;
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderAudioBrowser
// ---------------------------------------------------------------------------

void EditorState::renderAudioBrowser() {
    ImGui::Begin("Audio");

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path audioRoot("assets/audio");

    if (!fs::exists(audioRoot)) fs::create_directories(audioRoot, ec);

    // ── Navigation Bar ───────────────────────────────────────────────────────
    if (m_currentAudioDir != audioRoot.generic_string()) {
        if (ImGui::Button("<- Back##audio")) {
            m_currentAudioDir = fs::path(m_currentAudioDir).parent_path().generic_string();
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentAudioDir, audioRoot, ec).generic_string().c_str());
    ImGui::Separator();

    if (ImGui::Button("Import Audio")) {
        m_newAudioNameBuf[0] = '\0'; m_newAudioPathBuf[0] = '\0';
        m_newAudioLoop = false; m_newAudioStream = false; m_newAudioIs3D = false;
        m_newAudioVolume = 1.0f; m_newAudioPitch = 1.0f; m_newAudioIsBGM = false;
        ImGui::OpenPopup("##ImportAudio");
    }
    ImGui::SameLine();
    if (ImGui::Button("New Folder##audio")) {
        m_newScriptFolderBuf[0] = '\0';
        ImGui::OpenPopup("##NewAudioFolder");
    }

    if (ImGui::BeginPopupModal("##ImportAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import Audio File");
        ImGui::InputText("Name##audio", m_newAudioNameBuf, sizeof(m_newAudioNameBuf));
        ImGui::InputText("Source path", m_newAudioPathBuf, sizeof(m_newAudioPathBuf));
#ifdef _WIN32
        ImGui::SameLine();
        if (ImGui::Button("Browse##audio")) {
            OPENFILENAMEA ofn{}; char fileBuf[MAX_PATH]{};
            ofn.lStructSize = sizeof(ofn); ofn.lpstrFilter = "Audio Files\0*.wav;*.mp3;*.ogg;*.flac\0All Files\0*.*\0";
            ofn.lpstrFile = fileBuf; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) strncpy_s(m_newAudioPathBuf, fileBuf, sizeof(m_newAudioPathBuf) - 1);
        }
#endif
        ImGui::Checkbox("Loop", &m_newAudioLoop); ImGui::SameLine();
        ImGui::Checkbox("Stream", &m_newAudioStream); ImGui::SameLine();
        ImGui::Checkbox("3D", &m_newAudioIs3D);
        ImGui::Checkbox("BGM", &m_newAudioIsBGM);
        ImGui::SliderFloat("Volume", &m_newAudioVolume, 0.0f, 1.0f);
        if (ImGui::Button("Import", ImVec2(100, 0))) {
            Assets::SoundConfig cfg; cfg.name = m_newAudioNameBuf; cfg.path = m_newAudioPathBuf;
            cfg.loop = m_newAudioLoop; cfg.stream = m_newAudioStream; cfg.is3D = m_newAudioIs3D;
            cfg.volume = m_newAudioVolume; cfg.group = m_newAudioIsBGM ? "BGM" : "SFX";
            Assets::SoundManager::ImportSound(m_newAudioPathBuf, cfg.name, cfg);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("##NewAudioFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create Folder in: %s", m_currentAudioDir.c_str());
        ImGui::InputText("Folder name##nf", m_newScriptFolderBuf, sizeof(m_newScriptFolderBuf));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (m_newScriptFolderBuf[0]) fs::create_directories(fs::path(m_currentAudioDir) / m_newScriptFolderBuf, ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!m_deletingAudio.empty()) ImGui::OpenPopup("##DeleteAudio");
    if (ImGui::BeginPopupModal("##DeleteAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete '%s'?", m_deletingAudio.c_str());
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            Assets::SoundManager::DeleteFromDisk(m_deletingAudio);
            m_deletingAudio.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAudio.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Rename modal ──────────────────────────────────────────────────────────
    if (!m_renamingAudioPath.empty()) ImGui::OpenPopup("##RenameAudio");
    if (ImGui::BeginPopupModal("##RenameAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path renamePath(m_renamingAudioPath);
        bool isAudioDir = fs::is_directory(renamePath);
        ImGui::Text("Rename %s: %s", isAudioDir ? "folder" : "file", renamePath.filename().string().c_str());
        ImGui::InputText("New name##ra", m_renameAudioBuf, sizeof(m_renameAudioBuf));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (m_renameAudioBuf[0]) {
                fs::path dst = renamePath.parent_path() / m_renameAudioBuf;
                if (!isAudioDir) dst.replace_extension(renamePath.extension());
                fs::rename(renamePath, dst, ec);
            }
            m_renamingAudioPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Delete folder modal ───────────────────────────────────────────────────
    if (!m_deletingAudioPath.empty()) ImGui::OpenPopup("##DeleteAudioDir");
    if (ImGui::BeginPopupModal("##DeleteAudioDir", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete folder '%s' and all its contents?", fs::path(m_deletingAudioPath).filename().string().c_str());
        ImGui::TextDisabled("This cannot be undone.");
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            fs::remove_all(fs::path(m_deletingAudioPath), ec);
            if (m_currentAudioDir.find(m_deletingAudioPath) == 0)
                m_currentAudioDir = audioRoot.generic_string();
            m_deletingAudioPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // ── Move to folder modal ──────────────────────────────────────────────────
    if (!m_movingAudioPath.empty()) ImGui::OpenPopup("##MoveAudio");
    if (ImGui::BeginPopupModal("##MoveAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Move '%s' to:", fs::path(m_movingAudioPath).filename().string().c_str());
        ImGui::Separator();
        std::vector<std::string> audioFolders;
        audioFolders.push_back("(root)");
        for (const auto& entry : fs::recursive_directory_iterator(audioRoot, ec))
            if (entry.is_directory())
                audioFolders.push_back(fs::relative(entry.path(), audioRoot, ec).generic_string());
        for (const auto& folder : audioFolders) {
            if (ImGui::Selectable(folder.c_str())) {
                fs::path destDir = (folder == "(root)") ? audioRoot : audioRoot / folder;
                fs::path destFile = destDir / fs::path(m_movingAudioPath).filename();
                if (!fs::exists(destFile)) fs::rename(fs::path(m_movingAudioPath), destFile, ec);
                m_movingAudioPath.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_movingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // ── Grid ────────────────────────────────────────────────────────────────
    std::vector<fs::path> items;
    for (const auto& entry : fs::directory_iterator(m_currentAudioDir, ec)) {
        if (entry.is_directory()) {
            items.push_back(entry.path());
        } else {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
                items.push_back(entry.path());
        }
    }
    std::sort(items.begin(), items.end(), [](const fs::path& a, const fs::path& b) {
        bool aDir = fs::is_directory(a), bDir = fs::is_directory(b);
        if (aDir != bDir) return aDir;
        return a.filename().string() < b.filename().string();
    });

    constexpr float kIconW = 64.0f, kIconH = 72.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelW / (kIconW + 20.0f)));

    if (ImGui::BeginTable("##AudioTbl", cols)) {
        for (const auto& path : items) {
            ImGui::TableNextColumn();
            std::string fname = path.filename().string();
            std::string stem = path.stem().string();
            bool isDir = fs::is_directory(path);
            ImGui::PushID(fname.c_str());

            float colW = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            if (isDir) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
                ImGui::Button("##icon", ImVec2(kIconW, kIconH));
                ImGui::PopStyleColor();
                DrawFolderIcon();
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    m_currentAudioDir = path.generic_string();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payloadFile = ImGui::AcceptDragDropPayload("AUDIO_ASSET_FILE")) {
                        fs::path src((const char*)payloadFile->Data);
                        fs::rename(src, path / src.filename(), ec);
                    } else if (const ImGuiPayload* payloadAsset = ImGui::AcceptDragDropPayload("AUDIO_ASSET")) {
                        std::string srcPath = Assets::SoundManager::GetFilePath((const char*)payloadAsset->Data);
                        if (!srcPath.empty()) {
                            fs::path src(srcPath);
                            fs::rename(src, path / src.filename(), ec);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        m_renamingAudioPath = path.string();
                        strncpy_s(m_renameAudioBuf, fname.c_str(), sizeof(m_renameAudioBuf)-1);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) m_deletingAudioPath = path.string();
                    ImGui::EndPopup();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.25f, 0.18f, 1.0f));
                ImGui::Button("##icon", ImVec2(kIconW, kIconH));
                ImGui::PopStyleColor();
                DrawAudioIcon();

                if (ImGui::BeginDragDropSource()) {
                    std::string pstr = path.generic_string();
                    ImGui::SetDragDropPayload("AUDIO_ASSET_FILE", pstr.c_str(), pstr.size() + 1);
                    ImGui::SetDragDropPayload("AUDIO_ASSET", stem.c_str(), stem.size() + 1);
                    ImGui::Text("%s", stem.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) Assets::SoundManager::Play(stem);

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Play")) Assets::SoundManager::Play(stem);
                    if (ImGui::MenuItem("Stop")) Assets::SoundManager::Stop(stem);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename")) {
                        m_renamingAudioPath = path.string();
                        strncpy_s(m_renameAudioBuf, stem.c_str(), sizeof(m_renameAudioBuf)-1);
                    }
                    if (ImGui::MenuItem("Move to...")) { m_movingAudioPath = path.string(); }
                    if (ImGui::MenuItem("Duplicate")) {
                        fs::path src = path;
                        fs::path dst = src.parent_path() / (src.stem().string() + "_copy" + src.extension().string());
                        int n = 1;
                        while (fs::exists(dst))
                            dst = src.parent_path() / (src.stem().string() + "_copy" + std::to_string(n++) + src.extension().string());
                        fs::copy_file(src, dst, ec);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) m_deletingAudio = stem;
                    ImGui::EndPopup();
                }
            }
            ImGui::TextUnformatted(fname.c_str());
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
    m_selectedEntities.clear();
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

// ---------------------------------------------------------------------------
// Prefab helpers
// ---------------------------------------------------------------------------

void EditorState::loadAvailablePrefabs() {
    namespace fs = std::filesystem;
    m_availablePrefabs.clear();
    std::string dir = "assets/prefabs/";
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                m_availablePrefabs.push_back(entry.path().stem().string());
        }
    }
}

void EditorState::savePrefabFromEntity(ECS::Entity entity,
                                        const std::string& name,
                                        const std::string& category) {
    using json = nlohmann::json;
    auto& world = m_scene.GetWorld();

    json prefabJson;
    prefabJson["name"]       = name;
    prefabJson["category"]   = category;
    prefabJson["components"] = json::object();
    auto& comps = prefabJson["components"];

    if (auto* t = world.getComponent<Components::Transform>(entity))
        comps["transform"] = {
            {"position", {{"x", t->position.x}, {"y", t->position.y}}},
            {"rotation", t->rotation},
            {"scale",    {{"x", t->scale.x},    {"y", t->scale.y}}}};

    if (auto* sr = world.getComponent<Components::SpriteRenderer>(entity))
        comps["spriteRenderer"] = {
            {"textureId", sr->textureId},
            {"size",  {{"x", sr->size.x},  {"y", sr->size.y}}},
            {"color", {{"r", sr->color.r}, {"g", sr->color.g},
                       {"b", sr->color.b}, {"a", sr->color.a}}},
            {"sortOrder", sr->sortOrder}};

    if (auto* c = world.getComponent<Components::Collider>(entity))
        comps["collider"] = {
            {"shape",    static_cast<int>(c->shape)},
            {"width",    c->size.x}, {"height",  c->size.y},
            {"radius",   c->radius},
            {"isSolid",  c->isSolid},  {"isTrigger",   c->isTrigger},
            {"blocksLight", c->blocksLight}, {"moveable", c->moveable},
            {"mass",     c->mass},
            {"collisionCategory", c->collisionCategory},
            {"collisionMask",     c->collisionMask}};

    if (auto* tag = world.getComponent<Components::Tag>(entity))
        comps["tag"] = {{"tag", tag->tag}};

    if (auto* vel = world.getComponent<Components::Velocity>(entity))
        comps["velocity"] = {{"x", vel->velocity.x}, {"y", vel->velocity.y},
                              {"moveSpeed", vel->moveSpeed}};

    if (auto* au = world.getComponent<Components::AudioSource>(entity))
        comps["audioSource"] = {{"soundId", au->soundId}, {"volume", au->volume},
                                 {"loop", au->loop}, {"playOnStart", au->playOnStart}};

    if (auto* ls = world.getComponent<Components::LuaScript>(entity)) {
        json arr = json::array();
        for (auto& entry : ls->scripts) {
            json lsj;
            lsj["scriptPath"] = entry.scriptPath;
            lsj["enabled"]    = entry.enabled;
            if (!entry.properties.empty()) {
                json propsJ = json::object();
                for (auto& [key, val] : entry.properties)
                    std::visit([&](auto&& v) { propsJ[key] = v; }, val);
                lsj["properties"] = propsJ;
            }
            arr.push_back(lsj);
        }
        comps["luaScript"] = arr;
    }

    if (auto* sa = world.getComponent<Components::SpriteAnimator>(entity))
        comps["spriteAnimator"] = {{"filename", sa->filename},
                                    {"currentClip", sa->currentClip}};

    if (auto* sm = world.getComponent<Components::StateMachine>(entity))
        comps["stateMachine"] = {{"currentState", sm->currentState}};

    if (auto* tr = world.getComponent<Components::TextRenderer>(entity))
        comps["textRenderer"] = {{"text", tr->text}, {"fontId", tr->fontId},
                                  {"fontSize", tr->fontSize},
                                  {"color", {{"r", tr->color.r}, {"g", tr->color.g}, {"b", tr->color.b}}},
                                  {"visible", tr->visible},
                                  {"alignment", static_cast<int>(tr->alignment)}};

    namespace fs = std::filesystem;
    std::string filepath = "assets/prefabs/" + name + ".json";
    fs::create_directories(fs::path(filepath).parent_path());
    std::ofstream out(filepath);
    if (out) {
        out << prefabJson.dump(4);
        out.close();
        std::cout << "[Editor] Saved prefab: " << filepath << "\n";
        loadAvailablePrefabs();
    } else {
        std::cerr << "[Editor] Failed to save prefab: " << filepath << "\n";
    }
}

ECS::Entity EditorState::spawnPrefab(const std::string& prefabName, vec2 position) {
    using json = nlohmann::json;
    std::string filepath = "assets/prefabs/" + prefabName + ".json";
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Editor] Failed to open prefab: " << filepath << "\n";
        return {0, 0};
    }

    json pj;
    file >> pj;
    file.close();

    auto& world  = m_scene.GetWorld();
    auto  entity = world.createEntity();

    world.addComponent(entity, Components::Tag{prefabName});

    if (!pj.contains("components")) return entity;
    const auto& c = pj["components"];

    if (c.contains("transform")) {
        Components::Transform t;
        t.position = position;
        t.rotation = c["transform"].value("rotation", 0.f);
        if (c["transform"].contains("scale")) {
            t.scale.x = c["transform"]["scale"].value("x", 1.f);
            t.scale.y = c["transform"]["scale"].value("y", 1.f);
        }
        world.addComponent(entity, t);
    } else {
        Components::Transform t; t.position = position;
        world.addComponent(entity, t);
    }

    if (c.contains("spriteRenderer")) {
        Components::SpriteRenderer sr;
        sr.textureId = c["spriteRenderer"].value("textureId", "");
        if (c["spriteRenderer"].contains("size")) {
            sr.size.x = c["spriteRenderer"]["size"].value("x", 50.f);
            sr.size.y = c["spriteRenderer"]["size"].value("y", 50.f);
        }
        if (c["spriteRenderer"].contains("color")) {
            sr.color.r = c["spriteRenderer"]["color"].value("r", 1.f);
            sr.color.g = c["spriteRenderer"]["color"].value("g", 1.f);
            sr.color.b = c["spriteRenderer"]["color"].value("b", 1.f);
            sr.color.a = c["spriteRenderer"]["color"].value("a", 1.f);
        }
        sr.sortOrder = c["spriteRenderer"].value("sortOrder", 0);
        world.addComponent(entity, sr);
    }

    if (c.contains("collider")) {
        Components::Collider col;
        col.shape   = static_cast<Components::Collider::Shape>(c["collider"].value("shape", 0));
        col.size.x  = c["collider"].value("width",  50.f);
        col.size.y  = c["collider"].value("height", 50.f);
        col.radius  = c["collider"].value("radius", 25.f);
        col.isSolid     = c["collider"].value("isSolid",     true);
        col.isTrigger   = c["collider"].value("isTrigger",   false);
        col.blocksLight = c["collider"].value("blocksLight",  false);
        col.moveable    = c["collider"].value("moveable",    false);
        col.mass        = c["collider"].value("mass",        1.f);
        col.collisionCategory = c["collider"].value("collisionCategory", 1);
        col.collisionMask     = c["collider"].value("collisionMask",     0xFFFFFFFF);
        world.addComponent(entity, col);
    }

    if (c.contains("velocity")) {
        Components::Velocity vel;
        vel.velocity.x = c["velocity"].value("x", 0.f);
        vel.velocity.y = c["velocity"].value("y", 0.f);
        vel.moveSpeed  = c["velocity"].value("moveSpeed", 100.f);
        world.addComponent(entity, vel);
    }

    if (c.contains("audioSource")) {
        Components::AudioSource au;
        au.soundId     = c["audioSource"].value("soundId", "");
        au.volume      = c["audioSource"].value("volume", 1.f);
        au.loop        = c["audioSource"].value("loop", false);
        au.playOnStart = c["audioSource"].value("playOnStart", false);
        world.addComponent(entity, au);
    }

    if (c.contains("luaScript")) {
        Components::LuaScript ls;
        auto& lsData = c["luaScript"];
        auto deserEntry = [](const json& lsj) {
            Components::LuaScriptEntry entry;
            entry.scriptPath = lsj.value("scriptPath", "");
            entry.enabled    = lsj.value("enabled", true);
            if (lsj.contains("properties") && !entry.scriptPath.empty()) {
                auto& propDefs = Scripting::ScriptPropertyReader::read(entry.scriptPath);
                for (auto& def : propDefs) {
                    if (!lsj["properties"].contains(def.name)) continue;
                    auto& jv = lsj["properties"][def.name];
                    if      (def.type == "float")  entry.properties[def.name] = jv.get<float>();
                    else if (def.type == "int")    entry.properties[def.name] = jv.get<int>();
                    else if (def.type == "bool")   entry.properties[def.name] = jv.get<bool>();
                    else if (def.type == "string") entry.properties[def.name] = jv.get<std::string>();
                }
            }
            return entry;
        };
        if (lsData.is_array()) {
            for (auto& item : lsData) ls.scripts.push_back(deserEntry(item));
        } else {
            ls.scripts.push_back(deserEntry(lsData)); // backward compat
        }
        world.addComponent(entity, ls);
    }

    if (c.contains("spriteAnimator")) {
        Components::SpriteAnimator sa;
        sa.filename    = c["spriteAnimator"].value("filename", "");
        sa.currentClip = c["spriteAnimator"].value("currentClip", "");
        if (!sa.filename.empty())
            sa.loadFromPath("assets/animations/" + sa.filename);
        world.addComponent(entity, sa);
    }

    if (c.contains("stateMachine")) {
        Components::StateMachine sm;
        sm.currentState = c["stateMachine"].value("currentState", "idle");
        world.addComponent(entity, sm);
    }

    if (c.contains("textRenderer")) {
        Components::TextRenderer tr;
        tr.text      = c["textRenderer"].value("text", "");
        tr.fontId    = c["textRenderer"].value("fontId", "Debug");
        tr.fontSize  = c["textRenderer"].value("fontSize", 1.f);
        tr.visible   = c["textRenderer"].value("visible", true);
        tr.alignment = static_cast<Components::TextAlignment>(
            c["textRenderer"].value("alignment", 0));
        if (c["textRenderer"].contains("color")) {
            tr.color.r = c["textRenderer"]["color"].value("r", 1.f);
            tr.color.g = c["textRenderer"]["color"].value("g", 1.f);
            tr.color.b = c["textRenderer"]["color"].value("b", 1.f);
        }
        world.addComponent(entity, tr);
    }

    return entity;
}

void EditorState::deletePrefab(const std::string& prefabName) {
    std::error_code ec;
    std::filesystem::remove("assets/prefabs/" + prefabName + ".json", ec);
    loadAvailablePrefabs();
}

// ---------------------------------------------------------------------------
// Tilemap helpers
// ---------------------------------------------------------------------------

bool EditorState::isCellOccupied(vec2 worldPos, int layer) const {
    auto& world = m_scene.GetWorld();
    for (const auto& tile : m_tilemap) {
        if (tile.layer == layer &&
            world.isValid(tile.entity) &&
            magnitude(tile.position - worldPos) < m_gridSize * 0.5f)
            return true;
    }
    return false;
}

EditorState::EntityGroup* EditorState::getOrCreateLayerGroup(int layer) {
    char name[32];
    snprintf(name, sizeof(name), "Layer %d", layer);
    for (auto& g : m_entityGroups)
        if (g.name == name) return &g;
    m_entityGroups.push_back({name, {}});
    return &m_entityGroups.back();
}

void EditorState::removeEntityFromGroups(ECS::Entity entity) {
    for (auto& g : m_entityGroups) {
        auto it = std::remove_if(g.entities.begin(), g.entities.end(),
            [&](ECS::Entity e) {
                return e.id == entity.id && e.generation == entity.generation;
            });
        if (it != g.entities.end()) {
            g.entities.erase(it, g.entities.end());
            return;
        }
    }
}

void EditorState::clearTilemap() {
    auto& world = m_scene.GetWorld();
    for (auto& tile : m_tilemap) {
        if (world.isValid(tile.entity))
            world.destroyEntity(tile.entity);
    }
    m_tilemap.clear();
    m_entityGroups.clear();
    m_selectedEntity     = {0, 0};
    m_selectedEntities.clear();
    m_gizmoActiveAxis    = -1;
    m_pendingEntityDrag  = false;
    m_isDraggingEntity   = false;
}

void EditorState::saveTilemap(const std::string& filepath) {
    using json = nlohmann::json;
    json j;
    j["gridSize"]  = m_gridSize;
    j["numLayers"] = m_numLayers;
    j["tiles"]     = json::array();
    for (const auto& tile : m_tilemap) {
        json t;
        t["prefab"]    = tile.prefabName;
        t["position"]  = {{"x", tile.position.x}, {"y", tile.position.y}};
        t["layer"]     = tile.layer;
        j["tiles"].push_back(t);
    }
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(filepath).parent_path());
    std::ofstream out(filepath);
    if (out) { out << j.dump(4); std::cout << "[Editor] Saved tilemap: " << filepath << "\n"; }
    else       std::cerr << "[Editor] Failed to save tilemap: " << filepath << "\n";
}

void EditorState::loadTilemap(const std::string& filepath) {
    using json = nlohmann::json;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Editor] Failed to open tilemap: " << filepath << "\n";
        return;
    }
    json j; file >> j; file.close();

    clearTilemap();
    m_gridSize  = j.value("gridSize",  m_gridSize);
    m_numLayers = j.value("numLayers", m_numLayers);
    m_layerVisible.assign(m_numLayers, true);
    for (int i = 0; i < m_numLayers; ++i)
        m_scene.setLayerEnabled(i, true);

    if (!j.contains("tiles")) return;
    auto& world = m_scene.GetWorld();
    for (const auto& t : j["tiles"]) {
        std::string name = t.value("prefab", "");
        vec2 pos{t["position"].value("x", 0.f), t["position"].value("y", 0.f)};
        int  layer = t.value("layer", 0);
        ECS::Entity entity = spawnPrefab(name, pos);
        if (world.isValid(entity)) {
            if (auto* spr = world.getComponent<Components::SpriteRenderer>(entity))
                spr->sortOrder = layer;
            m_tilemap.push_back({name, pos, layer, entity});
            getOrCreateLayerGroup(layer)->entities.push_back(entity);
        }
    }
    std::cout << "[Editor] Loaded tilemap: " << filepath << "\n";
}

void EditorState::handleTileMapInput(const vec2& viewportMin) {
    if (m_selectedTilePrefab.empty() && m_paintMode == PaintMode::Paint) return;

    auto& world  = m_scene.GetWorld();
    auto& cam    = Core::Engine::Get().camera();
    vec2  camP   = {cam.position().x, cam.position().y};

    ImVec2 mp = ImGui::GetMousePos();
    vec2   mouseScreen{mp.x, mp.y};
    vec2   worldPos = screenToWorld(mouseScreen, camP, m_editorCamZoom,
                                    viewportMin, m_viewportSize);
    worldPos = snapToGrid(worldPos, m_gridSize);

    m_paintedThisFrame = false;

    if (m_paintMode != PaintMode::Select && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;

    if (m_paintMode == PaintMode::Paint) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // If clicking an existing tile, select it instead of painting over
            for (auto& tile : m_tilemap) {
                if (tile.layer == m_currentLayer &&
                    world.isValid(tile.entity) &&
                    magnitude(tile.position - worldPos) < m_gridSize * 0.5f) {
                    m_selectedEntities.clear();
                    m_selectedEntity = tile.entity;
                    m_selectedEntities.push_back(tile.entity);
                    m_paintedThisFrame = true;
                    return;
                }
            }
        }
        // Paint on held mouse (allows drag-painting)
        if (!isCellOccupied(worldPos, m_currentLayer)) {
            ECS::Entity entity = spawnPrefab(m_selectedTilePrefab, worldPos);
            if (world.isValid(entity)) {
                if (auto* spr = world.getComponent<Components::SpriteRenderer>(entity))
                    spr->sortOrder = m_currentLayer;
                m_tilemap.push_back({m_selectedTilePrefab, worldPos, m_currentLayer, entity});
                getOrCreateLayerGroup(m_currentLayer)->entities.push_back(entity);
                m_paintedThisFrame = true;
            }
        }
    } else if (m_paintMode == PaintMode::Erase) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            for (auto it = m_tilemap.begin(); it != m_tilemap.end(); ++it) {
                if (it->layer == m_currentLayer &&
                    world.isValid(it->entity) &&
                    magnitude(it->position - worldPos) < m_gridSize * 0.5f) {
                    removeEntityFromGroups(it->entity);
                    world.destroyEntity(it->entity);
                    m_tilemap.erase(it);
                    m_paintedThisFrame = true;
                    break;
                }
            }
        }
    } else if (m_paintMode == PaintMode::Select) {
        bool  shift  = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                       ImGui::IsKeyDown(ImGuiKey_RightShift);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            bool hitTile = false;
            for (auto& tile : m_tilemap) {
                if (tile.layer == m_currentLayer &&
                    world.isValid(tile.entity) &&
                    magnitude(tile.position - worldPos) < m_gridSize * 0.5f) {
                    if (shift) {
                        // Shift+click: toggle tile in selection
                        auto it = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                            [&](ECS::Entity e){ return e.id == tile.entity.id && e.generation == tile.entity.generation; });
                        if (it != m_selectedEntities.end())
                            m_selectedEntities.erase(it);
                        else
                            m_selectedEntities.push_back(tile.entity);
                    } else {
                        // Plain click: select this tile, start drag
                        m_selectedEntities.clear();
                        m_selectedEntity = tile.entity;
                        m_selectedEntities.push_back(tile.entity);
                        m_pendingEntityDrag = true;
                        m_isDraggingEntity  = false;
                        m_dragStartMousePos = mouseScreen;
                        m_dragClickWorldPos = worldPos;
                        m_gizmoGroupOriginalPos.clear();
                        if (auto* t = world.getComponent<Components::Transform>(tile.entity))
                            m_gizmoGroupOriginalPos.push_back({tile.entity, {t->position.x, t->position.y}});
                    }
                    if (!m_selectedEntities.empty())
                        m_selectedEntity = m_selectedEntities.front();
                    hitTile = true;
                    m_paintedThisFrame = true;
                    break;
                }
            }
            if (!hitTile) {
                // Clicked empty space: start box selection
                if (!shift) {
                    m_selectedEntities.clear();
                    m_selectedEntity = {0, 0};
                }
                m_isBoxSelecting = true;
                m_boxSelectStart = mouseScreen;
                m_boxSelectEnd   = mouseScreen;
            }
        }

        // Box selection update & finalize
        if (m_isBoxSelecting) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                m_boxSelectEnd = mouseScreen;
                ImVec2 a = {std::min(m_boxSelectStart.x, m_boxSelectEnd.x),
                            std::min(m_boxSelectStart.y, m_boxSelectEnd.y)};
                ImVec2 b = {std::max(m_boxSelectStart.x, m_boxSelectEnd.x),
                            std::max(m_boxSelectStart.y, m_boxSelectEnd.y)};
                ImGui::GetWindowDrawList()->AddRectFilled(a, b, IM_COL32(100, 160, 255, 35));
                ImGui::GetWindowDrawList()->AddRect      (a, b, IM_COL32(100, 160, 255, 220), 0.f, 0, 1.5f);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_isBoxSelecting = false;
                float minSX = std::min(m_boxSelectStart.x, m_boxSelectEnd.x);
                float maxSX = std::max(m_boxSelectStart.x, m_boxSelectEnd.x);
                float minSY = std::min(m_boxSelectStart.y, m_boxSelectEnd.y);
                float maxSY = std::max(m_boxSelectStart.y, m_boxSelectEnd.y);
                constexpr float kMinBox = 4.0f;
                if ((maxSX - minSX) > kMinBox || (maxSY - minSY) > kMinBox) {
                    if (!shift) {
                        m_selectedEntities.clear();
                        m_selectedEntity = {0, 0};
                    }
                    for (auto& tile : m_tilemap) {
                        if (!world.isValid(tile.entity)) continue;
                        vec2 sc = worldToScreen(tile.position, camP, m_editorCamZoom,
                                               viewportMin, m_viewportSize);
                        if (sc.x >= minSX && sc.x <= maxSX &&
                            sc.y >= minSY && sc.y <= maxSY) {
                            bool already = std::any_of(m_selectedEntities.begin(), m_selectedEntities.end(),
                                [&](ECS::Entity e){ return e.id == tile.entity.id && e.generation == tile.entity.generation; });
                            if (!already)
                                m_selectedEntities.push_back(tile.entity);
                        }
                    }
                    if (!m_selectedEntities.empty())
                        m_selectedEntity = m_selectedEntities.front();
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// renderPrefabBrowser
// ---------------------------------------------------------------------------

void EditorState::renderPrefabBrowser() {
    using json = nlohmann::json;
    ImGui::Begin("Prefabs");

    auto& world = m_scene.GetWorld();

    // --- Save as Prefab ---
    if (ImGui::CollapsingHeader("Save as Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (world.isValid(m_selectedEntity)) {
            std::string tagName = "Entity";
            if (auto* tag = world.getComponent<Components::Tag>(m_selectedEntity))
                tagName = tag->tag;
            ImGui::TextDisabled("Selected: %s", tagName.c_str());
        } else {
            ImGui::TextDisabled("No entity selected");
        }
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("Name##prefabname",     m_savePrefabNameBuf,     sizeof(m_savePrefabNameBuf));
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("Category##prefabcat",  m_savePrefabCategoryBuf, sizeof(m_savePrefabCategoryBuf));

        bool canSave = world.isValid(m_selectedEntity) && m_savePrefabNameBuf[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save Prefab##save", ImVec2(-1, 0)))
            savePrefabFromEntity(m_selectedEntity, m_savePrefabNameBuf, m_savePrefabCategoryBuf);
        if (!canSave) ImGui::EndDisabled();
    }

    ImGui::Separator();

    // --- Prefab Library ---
    if (ImGui::CollapsingHeader("Prefab Library", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Refresh##prefabrefresh")) loadAvailablePrefabs();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu prefabs", m_availablePrefabs.size());

        float thumbSize  = 64.f;
        float padding    = 8.f;
        float cellSize   = thumbSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int   cols       = std::max(1, static_cast<int>(panelWidth / cellSize));

        ImGui::BeginChild("##prefablist", ImVec2(0, 260), true);
        if (ImGui::BeginTable("##prefabgrid", cols)) {
            for (int i = 0; i < static_cast<int>(m_availablePrefabs.size()); ++i) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                const auto& pname = m_availablePrefabs[i];
                bool isSelected = (m_selectedPrefabIndex == i);

                // Read texture from prefab JSON for thumbnail
                std::string texId;
                {
                    std::ifstream pf("assets/prefabs/" + pname + ".json");
                    if (pf.is_open()) {
                        try {
                            json pj; pf >> pj;
                            if (pj.contains("components") &&
                                pj["components"].contains("spriteRenderer") &&
                                pj["components"]["spriteRenderer"].contains("textureId"))
                                texId = pj["components"]["spriteRenderer"]["textureId"].get<std::string>();
                        } catch (...) {}
                    }
                }

                auto tex = texId.empty() ? nullptr : Assets::TextureManager::Get(texId);
                bool clicked = false;
                if (tex && tex->id() != 0)
                    clicked = ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)tex->id(),
                                                 ImVec2(thumbSize, thumbSize), ImVec2(0,1), ImVec2(1,0));
                else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.4f, 1.f));
                    clicked = ImGui::Button("##fallback", ImVec2(thumbSize, thumbSize));
                    ImGui::PopStyleColor();
                }
                if (clicked) {
                    m_selectedPrefabIndex = i;
                    m_selectedTilePrefab  = pname;
                }

                // Selection highlight
                if (isSelected) {
                    ImVec2 rmin = ImGui::GetItemRectMin();
                    ImVec2 rmax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(rmin, rmax,
                        IM_COL32(255, 200, 0, 255), 0.f, 0, 2.f);
                }

                // Drag source
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("PREFAB_NAME", pname.c_str(), pname.size() + 1);
                    ImGui::Text("Drag: %s", pname.c_str());
                    ImGui::EndDragDropSource();
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Spawn at Origin"))
                        spawnPrefab(pname, {0.f, 0.f});
                    if (ImGui::MenuItem("Delete Prefab"))
                        deletePrefab(pname);
                    ImGui::EndPopup();
                }

                ImGui::TextWrapped("%s", pname.c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderTilePalette
// ---------------------------------------------------------------------------

void EditorState::renderTilePalette() {
    if (!m_showTilePalette || !m_tileMapMode) return;

    ImGui::Begin("Tile Palette", &m_showTilePalette);

    for (int i = 0; i < static_cast<int>(m_availablePrefabs.size()); ++i) {
        const auto& pname = m_availablePrefabs[i];
        bool isSelected = (pname == m_selectedTilePrefab);
        if (ImGui::Selectable(pname.c_str(), isSelected)) {
            m_selectedTilePrefab = pname;
            m_selectedPrefabIndex = i;
            m_paintMode = PaintMode::Paint;
        }
    }

    ImGui::Separator();
    if (!m_selectedTilePrefab.empty())
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Active: %s", m_selectedTilePrefab.c_str());
    else
        ImGui::TextDisabled("No tile selected");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Texture hot-reload
// ---------------------------------------------------------------------------

void EditorState::initTextureFilePaths() {
    m_textureFilePaths.clear();
    m_assetFileTimestamps.clear();

    const std::string configPath = "assets/configs/Textures.json";
    std::ifstream file(configPath);
    if (!file.is_open())
        return;

    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("textures") && j["textures"].is_array()) {
            for (const auto& item : j["textures"]) {
                if (item.contains("name") && item.contains("path")) {
                    std::string name = item["name"].get<std::string>();
                    std::string path = item["path"].get<std::string>();
                    m_textureFilePaths[name] = path;
                    if (std::filesystem::exists(path))
                        m_assetFileTimestamps[name] = std::filesystem::last_write_time(path);
                }
            }
        }
    } catch (const std::exception& e) {
        Core::Engine::Get().logger().warn(
            std::string("[Editor] Failed to parse texture config for hot-reload: ") + e.what());
    }
}

void EditorState::checkAssetHotReload() {
    if (!m_hotReloadEnabled)
        return;

    for (auto& [name, path] : m_textureFilePaths) {
        if (!std::filesystem::exists(path))
            continue;

        auto currentTime = std::filesystem::last_write_time(path);
        auto it = m_assetFileTimestamps.find(name);
        if (it != m_assetFileTimestamps.end() && currentTime != it->second) {
            Assets::TextureManager::Unload(name);
            Assets::TextureManager::LoadTexture(name, path);
            it->second = currentTime;
            Core::Engine::Get().logger().info("[Editor] Hot-reloaded texture: " + name);
        }
    }
}

} // namespace Runtime
