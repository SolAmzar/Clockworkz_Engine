/******************************************************************************/
/*!
\file           EditorState.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 62% [Amzar] 37% [Lebon] 1%
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
#include "Runtime/LevelMusicManager.hpp"
#include "Graphics/Shader.hpp"
#include "Memory/MemoryDebugRegistry.hpp"
#include "Memory/IPoolStats.hpp"
#include "Components/Light.hpp"
#include "Components/ParticleEmitter.hpp"
#include "Components/Transform.hpp"
#include "Systems/TilemapCollisionBuilder.hpp"
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
#include "IMGUI/ImGuiFileDialog.h"

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
#  include <shlobj.h>
#endif
#include <iostream>
#include <cmath>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <unordered_set>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>

// Engine components
#include "Components/Velocity.hpp"
#include "Components/AudioSource.hpp"
#include "Components/FootstepAudio.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Components/VideoPlayer.hpp"
#include "Components/LuaScript.hpp"
#include "Scripting/ScriptPropertyReader.hpp"
#include "Systems/LightRenderSystem.hpp"

namespace Runtime {

class HierarchyChangeCmd final : public IEditorCommand {
public:
    HierarchyChangeCmd(EditorState& editor,
                       EditorState::HierarchyUndoState before,
                       EditorState::HierarchyUndoState after,
                       std::string label)
        : m_editor(editor),
          m_before(std::move(before)),
          m_after(std::move(after)),
          m_label(std::move(label)) {}

    void execute() override { m_editor.applyHierarchyUndoState(m_after); }
    void undo() override { m_editor.applyHierarchyUndoState(m_before); }
    const char* label() const override { return m_label.c_str(); }

private:
    EditorState&                 m_editor;
    EditorState::HierarchyUndoState m_before;
    EditorState::HierarchyUndoState m_after;
    std::string                  m_label;
};

class GroupCloneCmd final : public IEditorCommand {
public:
    struct GroupTemplate {
        std::string name;
        bool collapsed{false};
        int sortOrderBase{0};
        int parentTemplateIndex{-1};
        int parentGroupIdxExternal{-1};
        std::vector<size_t> entitySnapshotIndices;
    };

    GroupCloneCmd(EditorState& editor,
                  ECS::World& world,
                  EditorState::HierarchyUndoState before,
                  std::vector<EntitySnapshot> snapshots,
                  std::vector<GroupTemplate> groups)
        : m_editor(editor),
          m_world(world),
          m_before(std::move(before)),
          m_snapshots(std::move(snapshots)),
          m_groups(std::move(groups)) {}

    void execute() override {
        m_createdEntities.clear();
        m_createdEntities.reserve(m_snapshots.size());
        for (const auto& snapshot : m_snapshots)
            m_createdEntities.push_back(snapshot.restore(m_world));

        auto after = m_before;
        std::vector<int> newGroupIndices(m_groups.size(), -1);
        for (size_t i = 0; i < m_groups.size(); ++i) {
            EditorState::HierarchyGroup newGroup;
            newGroup.name = m_groups[i].name;
            newGroup.collapsed = m_groups[i].collapsed;
            newGroup.sortOrderBase = m_groups[i].sortOrderBase;
            newGroup.parentGroupIdx = m_groups[i].parentTemplateIndex >= 0
                ? newGroupIndices[(size_t)m_groups[i].parentTemplateIndex]
                : m_groups[i].parentGroupIdxExternal;
            for (size_t snapshotIndex : m_groups[i].entitySnapshotIndices) {
                if (snapshotIndex < m_createdEntities.size())
                    newGroup.entityIds.push_back(m_createdEntities[snapshotIndex].id);
            }
            newGroupIndices[i] = (int)after.hierarchyGroups.size();
            after.hierarchyGroups.push_back(std::move(newGroup));
        }

        after.selectedEntities = m_createdEntities;
        after.selectedEntity = m_createdEntities.empty() ? ECS::Entity{0, 0} : m_createdEntities.front();
        after.selectedGroupIdx = newGroupIndices.empty() ? -1 : newGroupIndices.front();
        after.renamingGroupIdx = -1;
        after.lastAnchorId = after.selectedEntity.id;
        m_after = std::move(after);
        m_editor.applyHierarchyUndoState(m_after);
    }

    void undo() override {
        m_editor.applyHierarchyUndoState(m_before);
        for (auto it = m_createdEntities.rbegin(); it != m_createdEntities.rend(); ++it) {
            if (m_world.isValid(*it))
                m_world.destroyEntity(*it);
        }
        m_createdEntities.clear();
    }

    const char* label() const override { return "Clone Group"; }

private:
    EditorState& m_editor;
    ECS::World& m_world;
    EditorState::HierarchyUndoState m_before;
    EditorState::HierarchyUndoState m_after;
    std::vector<EntitySnapshot> m_snapshots;
    std::vector<GroupTemplate> m_groups;
    std::vector<ECS::Entity> m_createdEntities;
};

class DeleteTilemapTileCmd final : public IEditorCommand {
public:
    DeleteTilemapTileCmd(EditorState& editor, ECS::World& world, ECS::Entity entity, EditorState::TileData tileData)
        : m_editor(editor), m_entityCmd(world, entity), m_tileData(std::move(tileData)) {}

    void execute() override {
        auto it = std::find_if(m_editor.m_tilemap.begin(), m_editor.m_tilemap.end(),
            [&](const EditorState::TileData& t) {
                return t.entity.id == m_entityCmd.m_current.id && t.entity.generation == m_entityCmd.m_current.generation;
            });
        if (it != m_editor.m_tilemap.end()) {
            m_editor.m_tilemap.erase(it);
            m_editor.m_tilemapDirty = true;
        }
        m_editor.removeEntityFromGroups(m_entityCmd.m_current);
        m_entityCmd.execute();
    }

    void undo() override {
        m_entityCmd.undo();
        m_tileData.entity = m_entityCmd.m_current;
        m_editor.m_tilemap.push_back(m_tileData);
        m_editor.m_tilemapDirty = true;
    }

    const char* label() const override { return "Delete Tile"; }

private:
    EditorState& m_editor;
    DeleteEntityCmd m_entityCmd;
    EditorState::TileData m_tileData;
};

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

template<typename T, typename ExecFn>
static void addCompEntry(const char* label, ECS::Entity entity, ECS::World& world, ExecFn&& exec) {
    bool has = world.hasComponent<T>(entity);
    if (ImGui::MenuItem(label, nullptr, false, !has))
        exec(std::make_unique<Runtime::AddComponentCmd<T>>(world, entity));
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

template<typename CacheT, typename IncludeFn>
static const std::vector<std::filesystem::path>& RefreshDirectoryCache(
    CacheT& cache,
    const std::string& currentDir,
    double now,
    double refreshInterval,
    IncludeFn&& includeFn) {
    namespace fs = std::filesystem;
    if (!cache.dirty &&
        cache.dir == currentDir &&
        cache.lastRefreshTime >= 0.0 &&
        (now - cache.lastRefreshTime) < refreshInterval) {
        return cache.items;
    }

    std::error_code ec;
    cache.items.clear();
    cache.dir = currentDir;

    for (const auto& entry : fs::directory_iterator(currentDir, ec)) {
        if (includeFn(entry.path()))
            cache.items.push_back(entry.path());
    }

    std::sort(cache.items.begin(), cache.items.end(), [](const fs::path& a, const fs::path& b) {
        bool aDir = fs::is_directory(a);
        bool bDir = fs::is_directory(b);
        if (aDir != bDir) return aDir;
        return a.filename().string() < b.filename().string();
    });

    cache.lastRefreshTime = now;
    cache.dirty = false;
    return cache.items;
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

    // Load shaders required by the rendering passes ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â use absolute paths since
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
    scanBSAAssets();
    loadBSASelection();
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

void EditorState::invalidateBrowserCaches() {
    m_assetBrowserCache.dirty = true;
    m_assetSearchCache.dirty = true;
    m_animBrowserCache.dirty = true;
    m_scriptBrowserCache.dirty = true;
    m_audioBrowserCache.dirty = true;
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
        m_gsm.push(std::make_shared<EditorPlayState>(m_input, m_gsm, tempPath, m_showColliders));
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
        if (!m_selectedEntities.empty()) {
            auto& world = m_scene.GetWorld();
            auto compositeCmd = std::make_unique<CompositeEditorCmd>("Delete Selected Entities");
            auto beforeState = captureHierarchyUndoState();
            auto toDelete = m_selectedEntities;
            for (auto e : toDelete) {
                if (world.isValid(e))
                    removeEntityFromGroups(e);
            }
            auto afterState = captureHierarchyUndoState();
            applyHierarchyUndoState(beforeState);
            for (auto e : toDelete) {
                if (!world.isValid(e)) continue;
                auto tileIt = std::find_if(m_tilemap.begin(), m_tilemap.end(),
                    [&](const TileData& t) {
                        return t.entity.id == e.id && t.entity.generation == e.generation;
                    });
                if (tileIt != m_tilemap.end()) {
                    compositeCmd->add(std::make_unique<DeleteTilemapTileCmd>(*this, world, e, *tileIt));
                } else {
                    compositeCmd->add(std::make_unique<DeleteEntityCmd>(world, e));
                }
            }
            compositeCmd->add(std::make_unique<HierarchyChangeCmd>(
                *this,
                std::move(beforeState),
                std::move(afterState),
                "Delete Selected Entities"));
            executeCommand(std::move(compositeCmd));
        }
    }
}

// ---------------------------------------------------------------------------
// Render (framebuffer)
// ---------------------------------------------------------------------------

void EditorState::render(Core::Time* timer) {
    (void)timer;

    auto& engine   = Core::Engine::Get();
    auto& renderer = engine.rendering();
    auto& camera   = engine.camera();
    const double now = ImGui::GetTime();
    const bool shouldBackgroundRefresh =
        !m_viewportShouldRender &&
        m_viewportSize.x > 0.0f &&
        m_viewportSize.y > 0.0f &&
        (m_lastViewportRenderTime < 0.0 ||
         (now - m_lastViewportRenderTime) >= VIEWPORT_BACKGROUND_REFRESH_INTERVAL);
    const bool shouldRenderViewportFrame =
        (m_viewportShouldRender || shouldBackgroundRefresh) &&
        m_viewportSize.x > 0.0f &&
        m_viewportSize.y > 0.0f;

    if (shouldRenderViewportFrame) {
        m_framebuffer.bind();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::vec4 sceneTint(1.0f);
        if (m_scene.isDarknessEnabled()) {
            float intensity = std::clamp(m_scene.getDarknessIntensity(), 0.0f, 1.0f);
            glm::vec3 tintRgb = glm::mix(glm::vec3(1.0f), m_scene.getDarknessColor(), intensity);
            sceneTint = glm::vec4(tintRgb, 1.0f);
        }

        renderer.beginScenePass(camera.viewProj(), sceneTint);
        m_scene.Render(renderer);
        renderer.endScenePass();
        m_scene.RenderText(renderer);

        Systems::renderLightPass(m_scene.GetWorld(), renderer);
        m_lastViewportRenderTime = now;

        // Grid + collider + UIButton + TextBox outlines (debug pass, drawn on top of scene)
        if (m_showGrid || m_showColliders || m_showUIButtons || m_showTextBoxes) {
            renderer.beginDebugPass(camera.viewProj());

        if (m_showGrid)
            renderGrid();

        if (m_showColliders) {
            auto& world = m_scene.GetWorld();

            auto drawCollider = [&](Components::Transform& t,
                                    Components::Collider& c, glm::vec3 col) {
                auto rotVec = [](glm::vec2 v, float a) {
                    return glm::vec2(v.x * std::cos(a) - v.y * std::sin(a),
                                    v.x * std::sin(a) + v.y * std::cos(a));
                };
                glm::vec2 offset = rotVec(
                    glm::vec2(c.offset.x - t.pivot.x, c.offset.y - t.pivot.y),
                    t.rotation);
                glm::vec2 pos(t.position.x + offset.x, t.position.y + offset.y);
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
            };

            // Pass 1: green/yellow — all active non-merged colliders
            world.each<Components::Transform, Components::Collider>(
                [&](ECS::Entity entity, Components::Transform& t, Components::Collider& c) {
                    if (!c.isSolid && !c.isTrigger && !c.isHazard) return;
                    auto* tag = world.getComponent<Components::Tag>(entity);
                    if (tag && tag->tag == "TilemapMergedCollider") return; // drawn in pass 2
                    glm::vec3 col = c.isCombined
                        ? glm::vec3(0.f, 1.f, 1.f)   // cyan
                        : (c.isTrigger
                            ? glm::vec3(1.f, 1.f, 0.f)   // yellow
                            : glm::vec3(0.f, 1.f, 0.f)); // green
                    drawCollider(t, c, col);
                });

            // Pass 2: cyan — greedy-merged wall colliders, drawn on top
            world.each<Components::Transform, Components::Collider>(
                [&](ECS::Entity entity, Components::Transform& t, Components::Collider& c) {
                    auto* tag = world.getComponent<Components::Tag>(entity);
                    if (!tag || tag->tag != "TilemapMergedCollider") return;
                    drawCollider(t, c, glm::vec3(0.f, 1.f, 1.f)); // cyan
                });
        }

        if (m_showUIButtons) {
            auto& world = m_scene.GetWorld();
            world.each<Components::Transform, Components::UIButton>(
                [&](ECS::Entity, Components::Transform& t, Components::UIButton& btn) {
                    float halfW = btn.size.x * t.scale.x * 0.5f;
                    float halfH = btn.size.y * t.scale.y * 0.5f;
                    float cx = t.position.x + btn.offset.x;
                    float cy = t.position.y + btn.offset.y;
                    // Cyan box for hit area
                    renderer.drawLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, 0.f, 1.f, 1.f);
                    renderer.drawLine(cx + halfW, cy - halfH, cx + halfW, cy + halfH, 0.f, 1.f, 1.f);
                    renderer.drawLine(cx + halfW, cy + halfH, cx - halfW, cy + halfH, 0.f, 1.f, 1.f);
                    renderer.drawLine(cx - halfW, cy + halfH, cx - halfW, cy - halfH, 0.f, 1.f, 1.f);
                    // Cross at center
                    renderer.drawLine(cx - 4.f, cy, cx + 4.f, cy, 0.f, 1.f, 1.f);
                    renderer.drawLine(cx, cy - 4.f, cx, cy + 4.f, 0.f, 1.f, 1.f);
                });
        }

        if (m_showTextBoxes) {
            auto& world = m_scene.GetWorld();
            world.each<Components::Transform, Components::TextRenderer>(
                [&](ECS::Entity, Components::Transform& t, Components::TextRenderer& text) {
                    float boxW = text.boxSize.x * t.scale.x;
                    float boxH = text.boxSize.y * t.scale.y;

                    float boxLeft;
                    switch (text.alignment) {
                        case Components::TextAlignment::Center:
                            boxLeft = t.position.x - boxW * 0.5f; break;
                        case Components::TextAlignment::Right:
                            boxLeft = t.position.x - boxW; break;
                        default:
                            boxLeft = t.position.x; break;
                    }
                    float boxTop = t.position.y;

                    // Outer box â€” magenta
                    float r = boxLeft, l = boxLeft + boxW;
                    float top = boxTop, bot = boxTop - boxH;
                    renderer.drawLine(r,   top, l,   top, 1.f, 0.f, 1.f);
                    renderer.drawLine(l,   top, l,   bot, 1.f, 0.f, 1.f);
                    renderer.drawLine(l,   bot, r,   bot, 1.f, 0.f, 1.f);
                    renderer.drawLine(r,   bot, r,   top, 1.f, 0.f, 1.f);

                    // Inner padding area â€” orange (only when padding is non-zero)
                    float padX = text.padding.x * t.scale.x;
                    float padY = text.padding.y * t.scale.y;
                    if (padX > 0.f || padY > 0.f) {
                        float ir = r + padX, il = l - padX;
                        float itop = top - padY, ibot = bot + padY;
                        renderer.drawLine(ir,   itop, il,   itop, 1.f, 0.5f, 0.f);
                        renderer.drawLine(il,   itop, il,   ibot, 1.f, 0.5f, 0.f);
                        renderer.drawLine(il,   ibot, ir,   ibot, 1.f, 0.5f, 0.f);
                        renderer.drawLine(ir,   ibot, ir,   itop, 1.f, 0.5f, 0.f);
                    }
                });
        }

        renderer.endDebugPass();
    }

        m_framebuffer.unbind();
    }

    // Animation editor preview â€” render current frame into m_animPreviewFB
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

    // One-time layout ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â must use "MyDockSpace" to match ImGuiManager
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
        // right column is a single node (Inspector + Stats tabs ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â no split needed)
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
        ImGui::DockBuilderDockWindow("BSA",                    centerBot);
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
    renderBSATab();

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
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Build Size Analyser", nullptr, &m_showBSAWindow);
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
    ImGui::Checkbox("Pick Box", &m_showPickBounds);

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
        const std::string undoLabel = m_undoStack.back()->label();
        if (ImGui::Button("Undo")) undo();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Undo: %s", undoLabel.c_str());
    }
    if (!m_redoStack.empty()) {
        ImGui::SameLine();
        const std::string redoLabel = m_redoStack.back()->label();
        if (ImGui::Button("Redo")) redo();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Redo: %s", redoLabel.c_str());
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
    ImGui::SeparatorText("Scene Darkness");

    bool darknessEnabled = m_scene.isDarknessEnabled();
    if (ImGui::Checkbox("Enable Darkness Tint", &darknessEnabled)) {
        m_scene.setDarknessEnabled(darknessEnabled);
    }

    float darknessIntensity = m_scene.getDarknessIntensity();
    if (ImGui::SliderFloat("Darkness Level", &darknessIntensity, 0.0f, 1.0f, "%.2f")) {
        m_scene.setDarknessIntensity(darknessIntensity);
    }

    glm::vec3 darknessColor = m_scene.getDarknessColor();
    if (ImGui::ColorEdit3("Darkness Color", glm::value_ptr(darknessColor))) {
        m_scene.setDarknessColor(darknessColor);
    }

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
                  << "    \"musicContext\": \"\",\n"
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

    if (!m_renamingScenePath.empty())
        ImGui::OpenPopup("RenameScenePopup##scn");
    if (ImGui::BeginPopupModal("RenameScenePopup##scn", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path src(m_renamingScenePath);
        ImGui::Text("Rename scene:");
        ImGui::TextDisabled("%s", src.filename().string().c_str());
        ImGui::SetNextItemWidth(260.0f);
        bool hitEnter = ImGui::InputText("New name##renscene", m_renameSceneBuf,
                                         sizeof(m_renameSceneBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        bool doRename = false;
        if (ImGui::Button("Rename") || hitEnter) doRename = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_renamingScenePath.clear();
            m_renameSceneBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        if (doRename && m_renameSceneBuf[0] != '\0') {
            std::string newName = m_renameSceneBuf;
            if (newName.size() < 5 || newName.substr(newName.size() - 5) != ".json")
                newName += ".json";

            fs::path dst = src.parent_path() / newName;
            std::error_code renameEc;
            if (dst != src && !fs::exists(dst)) {
                fs::rename(src, dst, renameEc);
                if (!renameEc) {
                    if (m_sceneFilePath == src.string())
                        m_sceneFilePath = dst.string();
                    m_renamingScenePath.clear();
                    m_renameSceneBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    if (!m_cloningScenePath.empty())
        ImGui::OpenPopup("CloneScenePopup##scn");
    if (ImGui::BeginPopupModal("CloneScenePopup##scn", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path src(m_cloningScenePath);
        ImGui::Text("Clone scene:");
        ImGui::TextDisabled("%s", src.filename().string().c_str());
        ImGui::SetNextItemWidth(260.0f);
        bool hitEnter = ImGui::InputText("Copy name##clonescene", m_cloneSceneBuf,
                                         sizeof(m_cloneSceneBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        bool doClone = false;
        if (ImGui::Button("Clone") || hitEnter) doClone = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel##clonescene")) {
            m_cloningScenePath.clear();
            m_cloneSceneBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        if (doClone && m_cloneSceneBuf[0] != '\0') {
            std::string newName = m_cloneSceneBuf;
            if (newName.size() < 5 || newName.substr(newName.size() - 5) != ".json")
                newName += ".json";

            fs::path dst = src.parent_path() / newName;
            std::error_code copyEc;
            if (dst != src && !fs::exists(dst)) {
                fs::copy_file(src, dst, copyEc);
                if (!copyEc) {
                    m_cloningScenePath.clear();
                    m_cloneSceneBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    if (!m_deletingScenePath.empty())
        ImGui::OpenPopup("DeleteScenePopup##scn");
    if (ImGui::BeginPopupModal("DeleteScenePopup##scn", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path target(m_deletingScenePath);
        bool isCurrentScene = (fs::path(m_sceneFilePath) == target);

        ImGui::Text("Delete scene JSON?");
        ImGui::TextDisabled("%s", target.filename().string().c_str());
        if (isCurrentScene)
            ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f),
                               "The currently open scene cannot be deleted.");

        if (isCurrentScene) ImGui::BeginDisabled();
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            std::error_code removeEc;
            fs::remove(target, removeEc);
            if (!removeEc) {
                m_deletingScenePath.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        if (isCurrentScene) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_deletingScenePath.clear();
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
        ImGui::PushID(fname.c_str());
        if (isCurrent)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));

        if (ImGui::Selectable(fname.c_str(), isCurrent))
            m_pendingLoadPath = entry.path().string();

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Load"))
                m_pendingLoadPath = entry.path().string();
            if (ImGui::MenuItem("Rename")) {
                m_renamingScenePath = entry.path().string();
                strncpy_s(m_renameSceneBuf, entry.path().stem().string().c_str(),
                          sizeof(m_renameSceneBuf) - 1);
            }
            if (ImGui::MenuItem("Clone")) {
                m_cloningScenePath = entry.path().string();
                std::string cloneName = entry.path().stem().string() + "_copy";
                strncpy_s(m_cloneSceneBuf, cloneName.c_str(),
                          sizeof(m_cloneSceneBuf) - 1);
            }
            bool isCurrentScene = (entry.path().filename().string() == curFile);
            if (isCurrentScene) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Delete")) {
                m_deletingScenePath = entry.path().string();
            }
            if (isCurrentScene) ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        if (isCurrent)
            ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderHierarchy
// ---------------------------------------------------------------------------

void EditorState::renderHierarchy() {
    ImGui::Begin("Hierarchy");
    auto& world = m_scene.GetWorld();
    bool hierarchyChanged = false;
    std::string hierarchyChangeLabel = "Modify Hierarchy";
    HierarchyUndoState hierarchyBefore{};
    auto markHierarchyChanged = [&](const char* label) {
        if (!hierarchyChanged) {
            hierarchyBefore = captureHierarchyUndoState();
            hierarchyChanged = true;
            hierarchyChangeLabel = label;
        }
    };

    // -----------------------------------------------------------------------
    // 1. Build id â†’ full-entity map (needed to call world APIs)
    // -----------------------------------------------------------------------
    std::unordered_map<uint32_t, ECS::Entity> idMap;
    world.each<Components::Transform>([&](ECS::Entity e, Components::Transform&) {
        idMap[e.id] = e;
    });
    world.each<Components::Tag>([&](ECS::Entity e, Components::Tag&) {
        if (!world.hasComponent<Components::Transform>(e))
            idMap[e.id] = e;
    });

    // -----------------------------------------------------------------------
    // 2. Sync m_hierarchyOrder: remove stale IDs, append new ones
    // -----------------------------------------------------------------------
    std::unordered_set<uint32_t> liveIds;
    for (auto& [id, _] : idMap) liveIds.insert(id);

    // Remove stale IDs, then deduplicate in one pass
    {
        std::unordered_set<uint32_t> seen;
        m_hierarchyOrder.erase(
            std::remove_if(m_hierarchyOrder.begin(), m_hierarchyOrder.end(),
                [&](uint32_t id){
                    if (liveIds.find(id) == liveIds.end()) return true; // stale
                    return !seen.insert(id).second;                     // duplicate
                }),
            m_hierarchyOrder.end());
    }

    {
        std::unordered_set<uint32_t> ordered(m_hierarchyOrder.begin(), m_hierarchyOrder.end());
        for (uint32_t id : liveIds)
            if (!ordered.count(id))
                m_hierarchyOrder.push_back(id);
    }

    // Sync group entity lists: remove stale IDs and collapse duplicates.
    // Some older write paths appended directly without checking membership.
    for (auto& grp : m_hierarchyGroups) {
        std::unordered_set<uint32_t> seen;
        grp.entityIds.erase(
            std::remove_if(grp.entityIds.begin(), grp.entityIds.end(),
                [&](uint32_t id){
                    if (liveIds.find(id) == liveIds.end()) return true;
                    return !seen.insert(id).second;
                }),
            grp.entityIds.end());
    }

    // -----------------------------------------------------------------------
    // 3. Build set of grouped entity IDs
    // -----------------------------------------------------------------------
    std::unordered_set<uint32_t> groupedIds;
    for (auto& grp : m_hierarchyGroups)
        for (uint32_t id : grp.entityIds)
            groupedIds.insert(id);

    // -----------------------------------------------------------------------
    // 4. Helper lambdas
    // -----------------------------------------------------------------------
    auto selectNew = [&](ECS::Entity e) {
        m_selectedEntities.clear();
        m_selectedEntity = e;
        m_selectedEntities.push_back(e);
        m_selectedGroupIdx = -1;
    };

    auto isEntitySelected = [&](ECS::Entity e) -> bool {
        if (m_selectedEntity.id == e.id && m_selectedEntity.generation == e.generation)
            return true;
        for (auto& se : m_selectedEntities)
            if (se.id == e.id && se.generation == e.generation) return true;
        return false;
    };

    auto getDisplayName = [&](ECS::Entity e) -> std::string {
        if (auto* tag = world.getComponent<Components::Tag>(e)) return tag->tag;
        char buf[32]; snprintf(buf, sizeof(buf), "Entity %u", e.id);
        return buf;
    };

    auto getDraggedEntityIds = [&](uint32_t draggedId) {
        std::unordered_set<uint32_t> selSet;
        for (auto& se : m_selectedEntities) {
            if (world.isValid(se))
                selSet.insert(se.id);
        }

        std::vector<uint32_t> draggedIds;
        std::unordered_set<uint32_t> seen;
        if (!selSet.count(draggedId)) {
            draggedIds.push_back(draggedId);
            return draggedIds;
        }

        for (auto& se : m_selectedEntities) {
            if (!world.isValid(se)) continue;
            if (!seen.insert(se.id).second) continue;
            draggedIds.push_back(se.id);
        }
        if (draggedIds.empty())
            draggedIds.push_back(draggedId);
        return draggedIds;
    };

    auto moveEntitiesIntoGroup = [&](const std::vector<uint32_t>& draggedIds, HierarchyGroup& targetGroup) {
        if (draggedIds.empty()) return;
        markHierarchyChanged("Move Into Group");

        std::unordered_set<uint32_t> draggedSet(draggedIds.begin(), draggedIds.end());
        for (auto& otherGroup : m_hierarchyGroups) {
            otherGroup.entityIds.erase(
                std::remove_if(otherGroup.entityIds.begin(), otherGroup.entityIds.end(),
                    [&](uint32_t id) { return draggedSet.count(id) > 0; }),
                otherGroup.entityIds.end());
        }

        m_hierarchyOrder.erase(
            std::remove_if(m_hierarchyOrder.begin(), m_hierarchyOrder.end(),
                [&](uint32_t id) { return draggedSet.count(id) > 0; }),
            m_hierarchyOrder.end());

        for (uint32_t id : draggedIds) {
            if (std::find(targetGroup.entityIds.begin(), targetGroup.entityIds.end(), id) ==
                targetGroup.entityIds.end()) {
                targetGroup.entityIds.push_back(id);
            }
            groupedIds.insert(id);
        }
    };

    // Reorder: if the dragged entity is selected, move ALL selected entities
    // together (maintaining relative order) to the drop position.
    // If dragging an unselected entity, move only that one.
    auto reorderFlat = [&](uint32_t draggedId, int destIndex) {
        markHierarchyChanged("Reorder Hierarchy");
        // Build selected ID set
        std::unordered_set<uint32_t> selSet;
        for (auto& se : m_selectedEntities) selSet.insert(se.id);

        if (!selSet.count(draggedId)) {
            // Dragging an unselected entity â€” move it alone
            auto it = std::find(m_hierarchyOrder.begin(), m_hierarchyOrder.end(), draggedId);
            if (it == m_hierarchyOrder.end()) return;
            int srcIdx = (int)(it - m_hierarchyOrder.begin());
            if (srcIdx == destIndex || srcIdx + 1 == destIndex) return;
            m_hierarchyOrder.erase(it);
            if (destIndex > srcIdx) --destIndex;
            m_hierarchyOrder.insert(m_hierarchyOrder.begin() + destIndex, draggedId);
            return;
        }

        // Dragging a selected entity â€” move all selected together.
        // 1. Record selected IDs in their current relative order.
        std::vector<uint32_t> toMove;
        for (uint32_t id : m_hierarchyOrder)
            if (selSet.count(id)) toMove.push_back(id);

        // 2. Capture the anchor entity (what's currently AT destIndex).
        uint32_t anchorId = 0;
        bool     hasAnchor = (destIndex < (int)m_hierarchyOrder.size());
        if (hasAnchor) anchorId = m_hierarchyOrder[destIndex];

        // 3. Remove all selected from the flat list.
        m_hierarchyOrder.erase(
            std::remove_if(m_hierarchyOrder.begin(), m_hierarchyOrder.end(),
                [&](uint32_t id){ return selSet.count(id) > 0; }),
            m_hierarchyOrder.end());

        // 4. Find where to insert (locate anchor in the trimmed list).
        int insertPos = (int)m_hierarchyOrder.size(); // default: end
        if (hasAnchor && !selSet.count(anchorId)) {
            for (int i = 0; i < (int)m_hierarchyOrder.size(); ++i) {
                if (m_hierarchyOrder[i] == anchorId) { insertPos = i; break; }
            }
        }

        // 5. Insert the group in their original relative order.
        m_hierarchyOrder.insert(m_hierarchyOrder.begin() + insertPos,
                                toMove.begin(), toMove.end());
    };

    // -----------------------------------------------------------------------
    // 5. Toolbar buttons
    // -----------------------------------------------------------------------
    if (ImGui::Button("+ Entity")) {
        EntitySnapshot snap;
        snap.tagName   = "Entity";
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
    ImGui::SameLine();
    if (ImGui::Button("+ Group")) {
        markHierarchyChanged("Create Group");
        HierarchyGroup grp;
        grp.name = "Group";
        grp.parentGroupIdx = m_selectedGroupIdx;
        for (auto& e : m_selectedEntities) grp.entityIds.push_back(e.id);
        m_hierarchyGroups.push_back(std::move(grp));
        m_selectedGroupIdx = (int)m_hierarchyGroups.size() - 1;
        m_selectedEntities.clear();
        m_selectedEntity = {0, 0};
    }

    ImGui::Separator();

    // -----------------------------------------------------------------------
    // Search filter
    // -----------------------------------------------------------------------
    static ImGuiTextFilter filter;
    ImGui::TextDisabled("Search:");
    ImGui::SameLine();
    filter.Draw("##HierarchySearch", ImGui::GetContentRegionAvail().x);

    ImGui::Separator();

    auto passesHierarchyFilter = [&](ECS::Entity entity) {
        if (!world.isValid(entity)) return false;
        if (auto* tag = world.getComponent<Components::Tag>(entity))
            if (tag->tag == "TilemapMergedCollider") return false;
        return filter.PassFilter(getDisplayName(entity).c_str());
    };

    // -----------------------------------------------------------------------
    // 6. Render ungrouped entities with drag-drop reorder
    // -----------------------------------------------------------------------
    // Drop-zone payload tags
    static constexpr char k_dragTag[] = "HRC_ENTITY";
    static constexpr char k_groupDragTag[] = "HRC_GROUP";

    for (int i = 0; i < (int)m_hierarchyOrder.size(); ++i) {
        uint32_t uid = m_hierarchyOrder[i];
        if (groupedIds.count(uid)) continue; // Skip grouped entities
        auto mapIt = idMap.find(uid);
        if (mapIt == idMap.end()) continue;
        ECS::Entity e = mapIt->second;
        // Skip merged collider entities — shown in their own section below
        if (auto* tag = world.getComponent<Components::Tag>(e))
            if (tag->tag == "TilemapMergedCollider") continue;

        if (!filter.PassFilter(getDisplayName(e).c_str())) continue;

        ImGui::PushID(i);

        // --- Thin drop-zone ABOVE this row ---
        {
            ImGui::PushID((int)(uid * 1000u + 1u));
            ImGui::InvisibleButton("##drop", ImVec2(-1.0f, 4.0f));
            if (ImGui::BeginDragDropTarget()) {
                if (auto* p = ImGui::AcceptDragDropPayload(k_dragTag)) {
                    uint32_t srcId = *(const uint32_t*)p->Data;
                    reorderFlat(srcId, i);
                }
                ImGui::EndDragDropTarget();
            }
            // Visual line when something is being dragged over
            if (ImGui::IsItemHovered())
                ImGui::GetWindowDrawList()->AddLine(
                    ImGui::GetItemRectMin(),
                    ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y),
                    IM_COL32(100, 180, 255, 200), 2.0f);
            ImGui::PopID();
        }

        // --- Entity rename mode ---
        if (m_isRenaming &&
            m_renamingEntity.id == e.id &&
            m_renamingEntity.generation == e.generation) {
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
                if (auto* tag = world.getComponent<Components::Tag>(e))
                    tag->tag = m_renameBuffer;
                m_isRenaming = false;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                m_isRenaming = false;
            ImGui::PopID();
            continue;
        }

        // --- Selectable row (greyed out when hidden) ---
        bool isHidden = m_hiddenEntities.count(uid) > 0;
        bool selected = isEntitySelected(e);
        char rowLabel[160];
        snprintf(rowLabel, sizeof(rowLabel), "%s%s (ID:%u)##e%u",
                 isHidden ? "[H] " : "", getDisplayName(e).c_str(), uid, uid);
        if (isHidden) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));

        if (ImGui::Selectable(rowLabel, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            ImGui::ClearActiveID();
            m_selectedGroupIdx = -1;
            bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
            bool alt   = ImGui::IsKeyDown(ImGuiKey_LeftAlt)   || ImGui::IsKeyDown(ImGuiKey_RightAlt);

            if (shift && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                // Build the visible ungrouped order (same order as rendered)
                std::vector<uint32_t> visible;
                for (uint32_t id : m_hierarchyOrder) {
                    if (groupedIds.count(id)) continue;
                    auto visibleIt = idMap.find(id);
                    if (visibleIt == idMap.end()) continue;
                    if (!passesHierarchyFilter(visibleIt->second)) continue;
                    visible.push_back(id);
                }

                int anchorIdx = -1, clickIdx = -1;
                for (int vi = 0; vi < (int)visible.size(); ++vi) {
                    if (visible[vi] == m_lastAnchorId) anchorIdx = vi;
                    if (visible[vi] == uid)            clickIdx  = vi;
                }

                if (anchorIdx < 0 || clickIdx < 0) {
                    // No valid anchor yet â€” fall back to plain select
                    m_selectedEntities.clear();
                    m_selectedEntity   = e;
                    m_selectedEntities.push_back(e);
                    m_lastAnchorId     = uid;
                } else {
                    int lo = std::min(anchorIdx, clickIdx);
                    int hi = std::max(anchorIdx, clickIdx);
                    m_selectedEntities.clear();
                    for (int vi = lo; vi <= hi; ++vi) {
                        auto it2 = idMap.find(visible[vi]);
                        if (it2 != idMap.end()) m_selectedEntities.push_back(it2->second);
                    }
                    // Keep anchor as primary selection
                    auto anchorIt = idMap.find(m_lastAnchorId);
                    m_selectedEntity = (anchorIt != idMap.end())
                        ? anchorIt->second : m_selectedEntities.front();
                }
            } else if (alt && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                // Alt+click: toggle individual entity without resetting anchor
                auto it = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                    [&](ECS::Entity se){ return se.id == e.id; });
                if (it != m_selectedEntities.end()) {
                    m_selectedEntities.erase(it);
                    if (m_selectedEntity.id == e.id)
                        m_selectedEntity = m_selectedEntities.empty()
                            ? ECS::Entity{0,0} : m_selectedEntities.front();
                } else {
                    m_selectedEntities.push_back(e);
                    if (!world.isValid(m_selectedEntity)) m_selectedEntity = e;
                }
                // Do NOT update m_lastAnchorId for alt+click
            } else {
                // Plain click: select only this entity and set as anchor
                m_selectedEntities.clear();
                m_selectedEntity   = e;
                m_selectedEntities.push_back(e);
                m_lastAnchorId     = uid;
            }

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_isRenaming     = true;
                m_renamingEntity = e;
                if (auto* tag = world.getComponent<Components::Tag>(e))
                    strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
            }
        }
        if (isHidden) ImGui::PopStyleColor();

        // --- Drag source ---
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload(k_dragTag, &uid, sizeof(uint32_t));
            ImGui::Text("  %s", getDisplayName(e).c_str());
            ImGui::EndDragDropSource();
        }
        // --- Drop on row = place after it ---
        if (ImGui::BeginDragDropTarget()) {
            if (auto* p = ImGui::AcceptDragDropPayload(k_dragTag)) {
                uint32_t srcId = *(const uint32_t*)p->Data;
                reorderFlat(srcId, i + 1);
            }
            ImGui::EndDragDropTarget();
        }

        // --- Right-click context menu ---
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_isRenaming     = true;
                m_renamingEntity = e;
                if (auto* tag = world.getComponent<Components::Tag>(e))
                    strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
            }
            if (ImGui::MenuItem("Clone")) {
                EntitySnapshot snap = EntitySnapshot::capture(world, e);
                snap.transform.position.x += m_gridSize;
                auto cmd = std::make_unique<CreateEntityCmd>(world, snap);
                auto* rawCmd = cmd.get();
                executeCommand(std::move(cmd));
                selectNew(rawCmd->createdEntity());
            }
            // Add to existing group
            if (!m_hierarchyGroups.empty() && ImGui::BeginMenu("Add to Group")) {
                for (int gi = 0; gi < (int)m_hierarchyGroups.size(); ++gi) {
                    if (ImGui::MenuItem(m_hierarchyGroups[gi].name.c_str())) {
                        markHierarchyChanged("Add To Group");
                        auto& entityIds = m_hierarchyGroups[gi].entityIds;
                        if (std::find(entityIds.begin(), entityIds.end(), uid) == entityIds.end())
                            entityIds.push_back(uid);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            // Hide / Show (Photoshop-style visibility toggle)
            if (m_hiddenEntities.count(uid)) {
                if (ImGui::MenuItem("Show")) {
                    m_hiddenEntities.erase(uid);
                    if (auto* sr = world.getComponent<Components::SpriteRenderer>(e)) sr->visible = true;
                    if (auto* tr = world.getComponent<Components::TextRenderer>(e))   tr->visible = true;
                    if (auto* lt = world.getComponent<Components::Light>(e))           lt->active  = true;
                }
            } else {
                if (ImGui::MenuItem("Hide")) {
                    m_hiddenEntities.insert(uid);
                    if (auto* sr = world.getComponent<Components::SpriteRenderer>(e)) sr->visible = false;
                    if (auto* tr = world.getComponent<Components::TextRenderer>(e))   tr->visible = false;
                    if (auto* lt = world.getComponent<Components::Light>(e))           lt->active  = false;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                m_selectedEntities.erase(
                    std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                        [&](ECS::Entity se){ return se.id == e.id && se.generation == e.generation; }),
                    m_selectedEntities.end());
                if (m_selectedEntity.id == e.id && m_selectedEntity.generation == e.generation)
                    m_selectedEntity = {0, 0};
                executeCommand(std::make_unique<DeleteEntityCmd>(world, e));
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Drop zone at bottom of flat list
    {
        ImGui::PushID(999999);
        ImGui::InvisibleButton("##dropbottom", ImVec2(-1.0f, 4.0f));
        if (ImGui::BeginDragDropTarget()) {
            if (auto* p = ImGui::AcceptDragDropPayload(k_dragTag)) {
                uint32_t srcId = *(const uint32_t*)p->Data;
                reorderFlat(srcId, (int)m_hierarchyOrder.size());
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }

    // -----------------------------------------------------------------------
    // 7. Render groups
    // -----------------------------------------------------------------------
    if (!m_hierarchyGroups.empty())
        ImGui::Separator();

    auto isDescendantGroup = [&](int potentialChildIdx, int potentialAncestorIdx) {
        int parentIdx = potentialChildIdx;
        while (parentIdx >= 0 && parentIdx < (int)m_hierarchyGroups.size()) {
            if (parentIdx == potentialAncestorIdx) return true;
            parentIdx = m_hierarchyGroups[parentIdx].parentGroupIdx;
        }
        return false;
    };

    auto reindexGroupsAfterErase = [&](int removedIdx) {
        for (auto& group : m_hierarchyGroups) {
            if (group.parentGroupIdx == removedIdx) {
                group.parentGroupIdx = -1;
            } else if (group.parentGroupIdx > removedIdx) {
                --group.parentGroupIdx;
            }
        }

        if (m_selectedGroupIdx == removedIdx) m_selectedGroupIdx = -1;
        else if (m_selectedGroupIdx > removedIdx) --m_selectedGroupIdx;

        if (m_renamingGroupIdx == removedIdx) m_renamingGroupIdx = -1;
        else if (m_renamingGroupIdx > removedIdx) --m_renamingGroupIdx;
    };

    auto remapGroupIndices = [&](const std::vector<int>& oldToNew) {
        for (auto& group : m_hierarchyGroups) {
            if (group.parentGroupIdx >= 0 &&
                group.parentGroupIdx < (int)oldToNew.size()) {
                group.parentGroupIdx = oldToNew[group.parentGroupIdx];
            }
        }

        if (m_selectedGroupIdx >= 0 && m_selectedGroupIdx < (int)oldToNew.size())
            m_selectedGroupIdx = oldToNew[m_selectedGroupIdx];
        else if (m_selectedGroupIdx >= 0)
            m_selectedGroupIdx = -1;

        if (m_renamingGroupIdx >= 0 && m_renamingGroupIdx < (int)oldToNew.size())
            m_renamingGroupIdx = oldToNew[m_renamingGroupIdx];
        else if (m_renamingGroupIdx >= 0)
            m_renamingGroupIdx = -1;
    };

    auto moveGroupBefore = [&](int fromIdx, int beforeIdx) {
        if (fromIdx < 0 || beforeIdx < 0 ||
            fromIdx >= (int)m_hierarchyGroups.size() ||
            beforeIdx >= (int)m_hierarchyGroups.size() ||
            fromIdx == beforeIdx) {
            return;
        }
        markHierarchyChanged("Reorder Group");

        std::vector<HierarchyGroup> reordered;
        reordered.reserve(m_hierarchyGroups.size());
        std::vector<int> oldToNew(m_hierarchyGroups.size(), -1);
        HierarchyGroup movedGroup = m_hierarchyGroups[fromIdx];

        for (int oldIdx = 0; oldIdx < (int)m_hierarchyGroups.size(); ++oldIdx) {
            if (oldIdx == beforeIdx && oldToNew[fromIdx] == -1) {
                oldToNew[fromIdx] = (int)reordered.size();
                reordered.push_back(movedGroup);
            }
            if (oldIdx == fromIdx) continue;
            oldToNew[oldIdx] = (int)reordered.size();
            reordered.push_back(m_hierarchyGroups[oldIdx]);
        }

        if (oldToNew[fromIdx] == -1) {
            oldToNew[fromIdx] = (int)reordered.size();
            reordered.push_back(movedGroup);
        }

        m_hierarchyGroups = std::move(reordered);
        remapGroupIndices(oldToNew);
    };

    std::function<void(int)> eraseGroupRecursive = [&](int rootIdx) {
        if (rootIdx < 0 || rootIdx >= (int)m_hierarchyGroups.size()) return;

        for (int childIdx = (int)m_hierarchyGroups.size() - 1; childIdx >= 0; --childIdx) {
            if (m_hierarchyGroups[childIdx].parentGroupIdx == rootIdx)
                eraseGroupRecursive(childIdx);
        }

        m_hierarchyGroups.erase(m_hierarchyGroups.begin() + rootIdx);
        reindexGroupsAfterErase(rootIdx);
    };

    std::function<void(int, std::vector<int>&)> collectGroupSubtree =
        [&](int rootIdx, std::vector<int>& out) {
            if (rootIdx < 0 || rootIdx >= (int)m_hierarchyGroups.size()) return;
            out.push_back(rootIdx);
            for (int childIdx = 0; childIdx < (int)m_hierarchyGroups.size(); ++childIdx) {
                if (m_hierarchyGroups[childIdx].parentGroupIdx == rootIdx)
                    collectGroupSubtree(childIdx, out);
            }
        };

    std::vector<int> groupDepths(m_hierarchyGroups.size(), 0);
    for (int idx = 0; idx < (int)m_hierarchyGroups.size(); ++idx) {
        int parentIdx = m_hierarchyGroups[idx].parentGroupIdx;
        if (parentIdx == idx || parentIdx >= (int)m_hierarchyGroups.size())
            m_hierarchyGroups[idx].parentGroupIdx = -1;
    }
    std::function<void(int, int, std::vector<int>&)> appendGroupOrder =
        [&](int parentIdx, int depth, std::vector<int>& out) {
            for (int idx = 0; idx < (int)m_hierarchyGroups.size(); ++idx) {
                if (m_hierarchyGroups[idx].parentGroupIdx != parentIdx) continue;
                groupDepths[idx] = depth;
                out.push_back(idx);
                if (!m_hierarchyGroups[idx].collapsed)
                    appendGroupOrder(idx, depth + 1, out);
            }
        };

    std::vector<int> groupRenderOrder;
    groupRenderOrder.reserve(m_hierarchyGroups.size());
    appendGroupOrder(-1, 0, groupRenderOrder);

    for (int orderIdx = 0; orderIdx < (int)groupRenderOrder.size(); ++orderIdx) {
        int gi = groupRenderOrder[orderIdx];
        auto& grp = m_hierarchyGroups[gi];
        bool groupSel = (m_selectedGroupIdx == gi);
        float groupIndent = 16.0f * (float)groupDepths[gi];
        if (groupIndent > 0.0f) ImGui::Indent(groupIndent);

        // --- Group rename mode ---
        if (m_renamingGroupIdx == gi) {
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##grprename", m_renameBuffer, sizeof(m_renameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
                markHierarchyChanged("Rename Group");
                grp.name          = m_renameBuffer;
                m_renamingGroupIdx = -1;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                m_renamingGroupIdx = -1;
            if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
            continue;
        }

        // --- TreeNode header ---
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (groupSel) flags |= ImGuiTreeNodeFlags_Selected;

        char grpLabel[160];
        snprintf(grpLabel, sizeof(grpLabel), "%s  [sort base: %d]##grp%d",
                 grp.name.c_str(), grp.sortOrderBase, gi);

        ImGui::PushID(200000 + gi);
        ImGui::InvisibleButton("##groupdropbefore", ImVec2(-1.0f, 4.0f));
        if (ImGui::BeginDragDropTarget()) {
            if (auto* p = ImGui::AcceptDragDropPayload(k_groupDragTag)) {
                int draggedGroupIdx = *(const int*)p->Data;
                if (draggedGroupIdx >= 0 &&
                    draggedGroupIdx < (int)m_hierarchyGroups.size() &&
                    draggedGroupIdx != gi &&
                    m_hierarchyGroups[draggedGroupIdx].parentGroupIdx == grp.parentGroupIdx) {
                    moveGroupBefore(draggedGroupIdx, gi);
                    ImGui::EndDragDropTarget();
                    ImGui::PopID();
                    if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
                    break;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered())
            ImGui::GetWindowDrawList()->AddLine(
                ImGui::GetItemRectMin(),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y),
                IM_COL32(100, 180, 255, 200), 2.0f);
        ImGui::PopID();

        const bool wasCollapsed = grp.collapsed;
        ImGui::SetNextItemOpen(!grp.collapsed, ImGuiCond_Always);
        bool open = ImGui::TreeNodeEx(grpLabel, flags);
        grp.collapsed = !open;
        if (open && wasCollapsed) {
            for (int siblingIdx = 0; siblingIdx < (int)m_hierarchyGroups.size(); ++siblingIdx) {
                if (siblingIdx == gi) continue;
                auto& sibling = m_hierarchyGroups[siblingIdx];
                if (sibling.parentGroupIdx != grp.parentGroupIdx) continue;
                sibling.collapsed = true;
            }
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_selectedGroupIdx = gi;
            m_selectedEntities.clear();
            m_selectedEntity = {0, 0};
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload(k_groupDragTag, &gi, sizeof(int));
            ImGui::Text("%s", grp.name.c_str());
            ImGui::EndDragDropSource();
        }

        // --- Drop entity onto group header â†’ add to group ---
        if (ImGui::BeginDragDropTarget()) {
            if (auto* p = ImGui::AcceptDragDropPayload(k_dragTag)) {
                uint32_t srcId = *(const uint32_t*)p->Data;
                moveEntitiesIntoGroup(getDraggedEntityIds(srcId), grp);
            }
            if (auto* p = ImGui::AcceptDragDropPayload(k_groupDragTag)) {
                int draggedGroupIdx = *(const int*)p->Data;
                if (draggedGroupIdx != gi &&
                    draggedGroupIdx >= 0 &&
                    draggedGroupIdx < (int)m_hierarchyGroups.size() &&
                    !isDescendantGroup(gi, draggedGroupIdx)) {
                    markHierarchyChanged("Nest Group");
                    m_hierarchyGroups[draggedGroupIdx].parentGroupIdx = gi;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // --- Group right-click menu ---
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename Group")) {
                m_renamingGroupIdx = gi;
                strncpy_s(m_renameBuffer, grp.name.c_str(), sizeof(m_renameBuffer) - 1);
            }
            if (ImGui::MenuItem("Clone Group")) {
                std::vector<int> subtreeIndices;
                collectGroupSubtree(gi, subtreeIndices);

                std::unordered_map<int, int> oldToTemplate;
                std::vector<EntitySnapshot> cloneSnapshots;
                std::vector<GroupCloneCmd::GroupTemplate> cloneGroups;
                cloneGroups.reserve(subtreeIndices.size());

                for (size_t templateIdx = 0; templateIdx < subtreeIndices.size(); ++templateIdx) {
                    int oldIdx = subtreeIndices[templateIdx];
                    oldToTemplate[oldIdx] = (int)templateIdx;
                }

                for (size_t templateIdx = 0; templateIdx < subtreeIndices.size(); ++templateIdx) {
                    const auto& sourceGroup = m_hierarchyGroups[subtreeIndices[templateIdx]];
                    GroupCloneCmd::GroupTemplate cloneGroup;
                    cloneGroup.name = sourceGroup.name + " Copy";
                    cloneGroup.collapsed = sourceGroup.collapsed;
                    cloneGroup.sortOrderBase = sourceGroup.sortOrderBase;
                    cloneGroup.parentTemplateIndex = -1;
                    cloneGroup.parentGroupIdxExternal = sourceGroup.parentGroupIdx;

                    auto parentIt = oldToTemplate.find(sourceGroup.parentGroupIdx);
                    if (parentIt != oldToTemplate.end()) {
                        cloneGroup.parentTemplateIndex = parentIt->second;
                        cloneGroup.parentGroupIdxExternal = -1;
                    }

                    for (uint32_t entityId : sourceGroup.entityIds) {
                        auto entityIt = idMap.find(entityId);
                        if (entityIt == idMap.end()) continue;
                        EntitySnapshot snapshot = EntitySnapshot::capture(world, entityIt->second);
                        snapshot.tagName += " Copy";
                        snapshot.transform.position.x += m_gridSize;
                        cloneGroup.entitySnapshotIndices.push_back(cloneSnapshots.size());
                        cloneSnapshots.push_back(std::move(snapshot));
                    }

                    cloneGroups.push_back(std::move(cloneGroup));
                }

                executeCommand(std::make_unique<GroupCloneCmd>(
                    *this,
                    world,
                    captureHierarchyUndoState(),
                    std::move(cloneSnapshots),
                    std::move(cloneGroups)));
            }
            if (ImGui::MenuItem("Add Subgroup")) {
                markHierarchyChanged("Create Group");
                HierarchyGroup child;
                child.name = "Group";
                child.parentGroupIdx = gi;
                m_hierarchyGroups.push_back(std::move(child));
                m_selectedGroupIdx = (int)m_hierarchyGroups.size() - 1;
                m_selectedEntities.clear();
                m_selectedEntity = {0, 0};
                ImGui::EndPopup();
                if (open) ImGui::TreePop();
                if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
                break;
            }
            if (grp.parentGroupIdx != -1 && ImGui::MenuItem("Move To Root")) {
                markHierarchyChanged("Move Group To Root");
                grp.parentGroupIdx = -1;
            }
            if (ImGui::MenuItem("Disband (keep entities)")) {
                markHierarchyChanged("Disband Group");
                for (uint32_t id : grp.entityIds) m_hierarchyOrder.push_back(id);
                for (auto& otherGroup : m_hierarchyGroups) {
                    if (otherGroup.parentGroupIdx == gi)
                        otherGroup.parentGroupIdx = grp.parentGroupIdx;
                }
                m_hierarchyGroups.erase(m_hierarchyGroups.begin() + gi);
                reindexGroupsAfterErase(gi);
                if (open) ImGui::TreePop();
                ImGui::EndPopup();
                if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
                break;
            }
            if (ImGui::MenuItem("Delete Group + Entities")) {
                markHierarchyChanged("Delete Group");
                std::vector<uint32_t> idsToDelete = grp.entityIds;
                for (int groupIdx = 0; groupIdx < (int)m_hierarchyGroups.size(); ++groupIdx) {
                    if (groupIdx == gi) continue;
                    if (!isDescendantGroup(groupIdx, gi)) continue;
                    idsToDelete.insert(idsToDelete.end(),
                                       m_hierarchyGroups[groupIdx].entityIds.begin(),
                                       m_hierarchyGroups[groupIdx].entityIds.end());
                }
                auto beforeState = hierarchyChanged ? hierarchyBefore : captureHierarchyUndoState();
                eraseGroupRecursive(gi);
                auto deleteGroupCmd = std::make_unique<CompositeEditorCmd>("Delete Group + Entities");
                for (uint32_t id : idsToDelete) {
                    auto it = idMap.find(id);
                    if (it != idMap.end())
                        deleteGroupCmd->add(std::make_unique<DeleteEntityCmd>(world, it->second));
                }
                deleteGroupCmd->add(std::make_unique<HierarchyChangeCmd>(
                    *this,
                    std::move(beforeState),
                    captureHierarchyUndoState(),
                    "Delete Group"));
                executeCommand(std::move(deleteGroupCmd));
                hierarchyChanged = false;
                hierarchyChangeLabel.clear();
                if (open) ImGui::TreePop();
                ImGui::EndPopup();
                if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
                break;
            }
            ImGui::EndPopup();
        }

        if (open) {
            // --- Sort order base editor ---
            int newBase = grp.sortOrderBase;
            ImGui::SetNextItemWidth(120.0f);
            ImGui::PushID(gi * 10000 + 5000);
            if (ImGui::InputInt("Sort Order Base", &newBase)) {
                int delta = newBase - grp.sortOrderBase;
                grp.sortOrderBase = newBase;
                for (uint32_t id : grp.entityIds) {
                    auto it = idMap.find(id);
                    if (it == idMap.end()) continue;
                    if (auto* sr = world.getComponent<Components::SpriteRenderer>(it->second))
                        sr->sortOrder += delta;
                }
            }
            ImGui::PopID();

            ImGui::Separator();

            // --- Render group members ---
            for (int mi = 0; mi < (int)grp.entityIds.size(); ++mi) {
                uint32_t uid = grp.entityIds[mi];
                auto mapIt = idMap.find(uid);
                if (mapIt == idMap.end()) continue;
                ECS::Entity e = mapIt->second;

                if (!filter.PassFilter(getDisplayName(e).c_str())) continue;

                ImGui::PushID(mi);

                bool isHiddenMember = m_hiddenEntities.count(uid) > 0;
                bool selected = isEntitySelected(e);
                char rowLabel[160];
                snprintf(rowLabel, sizeof(rowLabel), "  %s%s (ID:%u)##ge%u",
                         isHiddenMember ? "[H] " : "", getDisplayName(e).c_str(), uid, uid);
                if (isHiddenMember) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));

                if (ImGui::Selectable(rowLabel, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    ImGui::ClearActiveID();
                    m_selectedGroupIdx = -1;
                    bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
                    bool alt   = ImGui::IsKeyDown(ImGuiKey_LeftAlt)   || ImGui::IsKeyDown(ImGuiKey_RightAlt);

                    if (shift && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        // Range select within this group's visible filtered list
                        std::vector<uint32_t> visibleMembers;
                        for (uint32_t memberId : grp.entityIds) {
                            auto memberIt = idMap.find(memberId);
                            if (memberIt == idMap.end()) continue;
                            if (!passesHierarchyFilter(memberIt->second)) continue;
                            visibleMembers.push_back(memberId);
                        }

                        int anchorIdx = -1, clickIdx = -1;
                        for (int vi = 0; vi < (int)visibleMembers.size(); ++vi) {
                            if (visibleMembers[vi] == m_lastAnchorId) anchorIdx = vi;
                            if (visibleMembers[vi] == uid)            clickIdx  = vi;
                        }

                        if (anchorIdx < 0 || clickIdx < 0) {
                            m_selectedEntities.clear();
                            m_selectedEntity = e;
                            m_selectedEntities.push_back(e);
                            m_lastAnchorId   = uid;
                        } else {
                            int lo = std::min(anchorIdx, clickIdx);
                            int hi = std::max(anchorIdx, clickIdx);
                            m_selectedEntities.clear();
                            for (int vi = lo; vi <= hi; ++vi) {
                                auto it2 = idMap.find(visibleMembers[vi]);
                                if (it2 != idMap.end()) m_selectedEntities.push_back(it2->second);
                            }
                            auto anchorIt = idMap.find(m_lastAnchorId);
                            m_selectedEntity = (anchorIt != idMap.end())
                                ? anchorIt->second : m_selectedEntities.front();
                        }
                    } else if (alt && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        // Alt+click: toggle individual member without resetting anchor
                        auto it = std::find_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                            [&](ECS::Entity se){ return se.id == e.id; });
                        if (it != m_selectedEntities.end()) {
                            m_selectedEntities.erase(it);
                            if (m_selectedEntity.id == e.id)
                                m_selectedEntity = m_selectedEntities.empty()
                                    ? ECS::Entity{0,0} : m_selectedEntities.front();
                        } else {
                            m_selectedEntities.push_back(e);
                            if (!world.isValid(m_selectedEntity)) m_selectedEntity = e;
                        }
                    } else {
                        m_selectedEntities.clear();
                        m_selectedEntity = e;
                        m_selectedEntities.push_back(e);
                        m_lastAnchorId   = uid;
                    }

                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        m_isRenaming     = true;
                        m_renamingEntity = e;
                        if (auto* tag = world.getComponent<Components::Tag>(e))
                            strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
                    }
                }
                if (isHiddenMember) ImGui::PopStyleColor();

                // Drag source (entity inside group can be dragged out)
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    markHierarchyChanged("Remove From Group");
                    ImGui::SetDragDropPayload(k_dragTag, &uid, sizeof(uint32_t));
                    // Remove from group on drag (will land ungrouped or in another group)
                    grp.entityIds.erase(grp.entityIds.begin() + mi);
                    groupedIds.erase(uid);
                    m_hierarchyOrder.push_back(uid); // add back to flat list
                    ImGui::Text("  %s", getDisplayName(e).c_str());
                    ImGui::EndDragDropSource();
                    --mi; // compensate for erased element
                }

                // Right-click context in group
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        m_isRenaming     = true;
                        m_renamingEntity = e;
                        if (auto* tag = world.getComponent<Components::Tag>(e))
                            strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer) - 1);
                    }
                    if (ImGui::MenuItem("Remove from Group")) {
                        markHierarchyChanged("Remove From Group");
                        grp.entityIds.erase(grp.entityIds.begin() + mi);
                        groupedIds.erase(uid);
                        m_hierarchyOrder.push_back(uid);
                        ImGui::EndPopup();
                        ImGui::PopID();
                        --mi; continue;
                    }
                    ImGui::Separator();
                    if (m_hiddenEntities.count(uid)) {
                        if (ImGui::MenuItem("Show")) {
                            m_hiddenEntities.erase(uid);
                            if (auto* sr = world.getComponent<Components::SpriteRenderer>(e)) sr->visible = true;
                            if (auto* tr = world.getComponent<Components::TextRenderer>(e))   tr->visible = true;
                            if (auto* lt = world.getComponent<Components::Light>(e))           lt->active  = true;
                        }
                    } else {
                        if (ImGui::MenuItem("Hide")) {
                            m_hiddenEntities.insert(uid);
                            if (auto* sr = world.getComponent<Components::SpriteRenderer>(e)) sr->visible = false;
                            if (auto* tr = world.getComponent<Components::TextRenderer>(e))   tr->visible = false;
                            if (auto* lt = world.getComponent<Components::Light>(e))           lt->active  = false;
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) {
                        m_selectedEntities.erase(
                            std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                                [&](ECS::Entity se){ return se.id == e.id && se.generation == e.generation; }),
                            m_selectedEntities.end());
                        if (m_selectedEntity.id == e.id) m_selectedEntity = {0,0};
                        grp.entityIds.erase(grp.entityIds.begin() + mi);
                        groupedIds.erase(uid);
                        executeCommand(std::make_unique<DeleteEntityCmd>(world, e));
                        ImGui::EndPopup();
                        ImGui::PopID();
                        --mi; continue;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (groupIndent > 0.0f) ImGui::Unindent(groupIndent);
    }

    ImGui::InvisibleButton("##grouprootdrop", ImVec2(-1.0f, 8.0f));
    if (ImGui::BeginDragDropTarget()) {
        if (auto* p = ImGui::AcceptDragDropPayload(k_groupDragTag)) {
            int draggedGroupIdx = *(const int*)p->Data;
            if (draggedGroupIdx >= 0 && draggedGroupIdx < (int)m_hierarchyGroups.size()) {
                markHierarchyChanged("Move Group To Root");
                m_hierarchyGroups[draggedGroupIdx].parentGroupIdx = -1;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // -----------------------------------------------------------------------
    // 8. Merged Colliders section (read-only, collapsed by default)
    // -----------------------------------------------------------------------
    {
        std::vector<ECS::Entity> generatedMergedEntities;
        std::unordered_map<uint32_t, std::vector<ECS::Entity>> compositeGroups;
        std::vector<ECS::Entity> authoredCombinedEntities;
        world.each<Components::Collider>([&](ECS::Entity e, Components::Collider& collider) {
            bool isGeneratedMerged = false;
            if (auto* tag = world.getComponent<Components::Tag>(e))
                isGeneratedMerged = tag->tag == "TilemapMergedCollider";

            if (isGeneratedMerged) {
                generatedMergedEntities.push_back(e);
                return;
            }
            if (!collider.isCombined) return;

            if (collider.combinedGroupId != 0)
                compositeGroups[collider.combinedGroupId].push_back(e);
            else
                authoredCombinedEntities.push_back(e);
        });

        int mergedSectionCount =
            static_cast<int>(generatedMergedEntities.size() + authoredCombinedEntities.size());
        for (const auto& [groupId, entities] : compositeGroups)
            mergedSectionCount += static_cast<int>(entities.size());

        if (mergedSectionCount > 0) {
            ImGui::Separator();
            char sectionLabel[64];
            snprintf(sectionLabel, sizeof(sectionLabel),
                     "Merged Colliders  [%d]##mergedsec", mergedSectionCount);
            ImGuiTreeNodeFlags sectionFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f)); // cyan label
            bool sectionOpen = ImGui::TreeNodeEx(sectionLabel, sectionFlags);
            ImGui::PopStyleColor();

            if (sectionOpen) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 0.9f, 1.0f));
                std::vector<ECS::Entity> mergedToDelete;
                auto renderMergedRow = [&](ECS::Entity me, const char* prefix) {
                    char rowLabel[160];
                    if (auto* c = world.getComponent<Components::Collider>(me)) {
                        if (c->combinedGroupId != 0) {
                            snprintf(rowLabel, sizeof(rowLabel),
                                     "%s[%.0fx%.0f] Group %u (ID:%u)",
                                     prefix, c->size.x, c->size.y, c->combinedGroupId, me.id);
                        } else {
                            snprintf(rowLabel, sizeof(rowLabel),
                                     "%s[%.0fx%.0f] (ID:%u)",
                                     prefix, c->size.x, c->size.y, me.id);
                        }
                    } else {
                        snprintf(rowLabel, sizeof(rowLabel), "%s(ID:%u)", prefix, me.id);
                    }

                    ImGui::PushID((int)me.id);
                    bool selected = isEntitySelected(me);
                    if (ImGui::Selectable(rowLabel, selected))
                        selectNew(me);

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete"))
                            mergedToDelete.push_back(me);
                        if (auto* collider = world.getComponent<Components::Collider>(me)) {
                            if (collider->combinedGroupId != 0 && ImGui::MenuItem("Clear Composite Group")) {
                                collider->combinedGroupId = 0;
                                collider->isCombined = false;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                };

                for (auto& [groupId, entities] : compositeGroups) {
                    char groupLabel[96];
                    snprintf(groupLabel, sizeof(groupLabel),
                             "Composite Group %u  [%d]##compgrp%u",
                             groupId, (int)entities.size(), groupId);
                    if (ImGui::TreeNodeEx(groupLabel, ImGuiTreeNodeFlags_SpanAvailWidth)) {
                        for (auto& entity : entities)
                            renderMergedRow(entity, "  ");
                        ImGui::TreePop();
                    }
                }
                for (auto& me : authoredCombinedEntities)
                    renderMergedRow(me, "* ");
                for (auto& me : generatedMergedEntities)
                    renderMergedRow(me, "~ ");

                for (auto& me : mergedToDelete) {
                    m_selectedEntities.erase(
                        std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
                            [&](ECS::Entity se){ return se.id == me.id && se.generation == me.generation; }),
                        m_selectedEntities.end());
                    if (m_selectedEntity.id == me.id && m_selectedEntity.generation == me.generation)
                        m_selectedEntity = {0, 0};
                    executeCommand(std::make_unique<DeleteEntityCmd>(world, me));
                }
                ImGui::PopStyleColor();
                ImGui::TreePop();
            }
        }
    }

    if (hierarchyChanged) {
        executeCommand(std::make_unique<HierarchyChangeCmd>(
            *this,
            std::move(hierarchyBefore),
            captureHierarchyUndoState(),
            hierarchyChangeLabel));
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderInspector
// ---------------------------------------------------------------------------

void EditorState::renderInspector() {
    ImGui::Begin("Inspector");

    auto& world = m_scene.GetWorld();
    auto execCmd = [this](std::unique_ptr<IEditorCommand> cmd) { executeCommand(std::move(cmd)); };
    auto selectedValidEntities = [&]() {
        std::vector<ECS::Entity> entities;
        std::unordered_set<uint32_t> seen;
        for (const auto& e : m_selectedEntities) {
            if (!world.isValid(e)) continue;
            if (!seen.insert(e.id).second) continue;
            entities.push_back(e);
        }
        if (entities.empty() && world.isValid(m_selectedEntity))
            entities.push_back(m_selectedEntity);
        return entities;
    };
    auto applyToSelected = [&](auto&& fn) {
        for (const auto& e : selectedValidEntities())
            fn(e);
    };
    auto selectedTransformEntities = [&]() {
        std::vector<ECS::Entity> entities;
        for (const auto& e : selectedValidEntities()) {
            if (world.getComponent<Components::Transform>(e))
                entities.push_back(e);
        }
        return entities;
    };
    auto selectionPivotFromOriginalPositions = [&]() {
        vec2 pivot{0.f, 0.f};
        if (m_gizmoGroupOriginalPos.empty())
            return pivot;
        for (const auto& [_, pos] : m_gizmoGroupOriginalPos)
            pivot += pos;
        return pivot / static_cast<float>(m_gizmoGroupOriginalPos.size());
    };
    auto applySelectionRotation = [&](float delta) {
        const vec2 pivot = selectionPivotFromOriginalPositions();
        const float c = std::cos(delta);
        const float s = std::sin(delta);
        for (size_t i = 0; i < m_gizmoGroupOriginalPos.size(); ++i) {
            const auto& entity = m_gizmoGroupOriginalPos[i].first;
            auto* transform = world.getComponent<Components::Transform>(entity);
            if (!transform) continue;

            const vec2 originalPos = m_gizmoGroupOriginalPos[i].second;
            const vec2 offset = originalPos - pivot;
            transform->position = {
                pivot.x + offset.x * c - offset.y * s,
                pivot.y + offset.x * s + offset.y * c
            };

            if (i < m_gizmoGroupOriginalRot.size())
                transform->rotation = m_gizmoGroupOriginalRot[i].second + delta;
        }
    };

    // --- Group inspector (shown when a group folder is selected) ---
    if (m_selectedGroupIdx >= 0 && m_selectedGroupIdx < (int)m_hierarchyGroups.size()) {
        auto& grp = m_hierarchyGroups[m_selectedGroupIdx];
        ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.2f, 1.0f), "Group: %s", grp.name.c_str());
        ImGui::Separator();

        // Rename
        static char grpNameBuf[128];
        strncpy_s(grpNameBuf, grp.name.c_str(), sizeof(grpNameBuf) - 1);
        if (ImGui::InputText("Name##grpname", grpNameBuf, sizeof(grpNameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue))
            grp.name = grpNameBuf;

        if (ImGui::Button("Select Group Members", ImVec2(-1, 0))) {
            m_selectedEntities.clear();
            std::unordered_set<uint32_t> groupIds(grp.entityIds.begin(), grp.entityIds.end());
            std::unordered_set<uint32_t> seen;
            world.each<Components::Transform>([&](ECS::Entity e, Components::Transform&) {
                if (!groupIds.count(e.id) || !seen.insert(e.id).second) return;
                m_selectedEntities.push_back(e);
            });
            world.each<Components::Tag>([&](ECS::Entity e, Components::Tag&) {
                if (!groupIds.count(e.id) || !seen.insert(e.id).second) return;
                m_selectedEntities.push_back(e);
            });
            m_selectedEntity = m_selectedEntities.empty() ? ECS::Entity{0, 0} : m_selectedEntities.front();
            m_selectedGroupIdx = -1;
        }
        ImGui::TextDisabled("(%d entities in group)", (int)grp.entityIds.size());
        ImGui::End();
        return;

  ImGui::Separator();

  // Tag
  if (auto *tag = world.getComponent<Components::Tag>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
      char buf[256];
      strncpy_s(buf, tag->tag.c_str(), sizeof(buf) - 1);
      if (ImGui::InputText("Name", buf, sizeof(buf)))
        tag->tag = buf;
    }
  }

  // Transform
  if (auto *t = world.getComponent<Components::Transform>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      float pos[2] = {t->position.x, t->position.y};
      if (ImGui::DragFloat2("Position", pos, 1.0f)) {
        t->position = {pos[0], pos[1]};
      }
      // Undo on release
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        vec2 newPos = {pos[0], pos[1]};
        vec2 oldPos = m_gizmoOriginalPos; // reuse for non-gizmo edits
        executeCommand(std::make_unique<MoveEntityCmd>(world, m_selectedEntity,
                                                       oldPos, newPos));
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
    auto *sr = world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
    if (ImGui::CollapsingHeader("SpriteRenderer",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      char texBuf[256];
      strncpy_s(texBuf, sr->textureId.c_str(), sizeof(texBuf) - 1);
      if (ImGui::InputText("Texture ID", texBuf, sizeof(texBuf)))
        sr->textureId = texBuf;
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *pl =
                ImGui::AcceptDragDropPayload("TEXTURE_ASSET"))
          sr->textureId = std::filesystem::path(static_cast<const char *>(pl->Data)).stem().string();
        ImGui::EndDragDropTarget();
      }

      // Show texture thumbnail if loaded
      auto tex = Assets::TextureManager::Get(sr->textureId);
      if (tex && tex->id() != 0) {
        ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(48, 48),
                     ImVec2(0, 1), ImVec2(1, 0));
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

  // VideoPlayer
  if (world.hasComponent<Components::VideoPlayer>(m_selectedEntity)) {
    bool rm = false;
    bool open = ImGui::CollapsingHeader("VideoPlayer",
                                        ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
    if (ImGui::SmallButton("X##vp"))
      rm = true;

    if (open) {
      auto *vp = world.getComponent<Components::VideoPlayer>(m_selectedEntity);

      char pathBuf[256];
      strncpy_s(pathBuf, vp->filepath.c_str(), sizeof(pathBuf) - 1);
      if (ImGui::InputText("Filepath##vp", pathBuf, sizeof(pathBuf))) {
        vp->filepath = pathBuf;
      }
      ImGui::SameLine();
      if (ImGui::Button("Browse##vp")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        IGFD::FileDialog::Instance()->OpenDialog(
            "ChooseVideoFileDlg", "Choose Video File",
            "Video files (*.mp4 *.avi *.mkv *.webm){.mp4,.avi,.mkv,.webm},.*",
            config);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *pl =
                ImGui::AcceptDragDropPayload("VIDEO_ASSET")) {
          vp->filepath = static_cast<const char *>(pl->Data);
        }
        ImGui::EndDragDropTarget();
      }

      ImGui::Checkbox("Loop##vp", &vp->loop);
      ImGui::SameLine();
      ImGui::Checkbox("Autoplay##vp", &vp->autoplay);
      ImGui::Checkbox("Mute##vp", &vp->mute);

      ImGui::SliderFloat("Speed##vp", &vp->playbackSpeed, 0.1f, 2.0f);

      ImGui::Text("State: %s", vp->playbackState.c_str());
      ImGui::Text("Resolution: %dx%d (%.1f fps)", vp->width, vp->height,
                  vp->fps);

      float cTime = vp->currentTime;
      if (ImGui::SliderFloat("Time##vp", &cTime, 0.0f,
                             vp->duration > 0 ? vp->duration : 100.0f)) {
        // To seek, we unfortunately need the ECS system hook, but we modify
        // time. The current VideoPlayer component does not have a "seek
        // target", but we can just rely on the Play/Pause logic for now.
      }
      ImGui::Text("Duration: %.2f", vp->duration);

      bool playing = vp->isPlaying;
      if (ImGui::Checkbox("Is Playing##vp", &playing)) {
        vp->isPlaying = playing;
      }
    }
    if (rm) {
      world.removeComponent<Components::VideoPlayer>(m_selectedEntity);
    }
  }

  // Collider
  if (ImGui::CollapsingHeader("Collider")) {
    auto *c = world.getComponent<Components::Collider>(m_selectedEntity);
    if (c) {
      ImGui::Checkbox("Show Outline", &m_showColliders);
      ImGui::Separator();

      // Shape selector — changing this affects collision logic immediately
      const char *shapeNames[] = {"Box", "Circle"};
      int currentShape = static_cast<int>(c->shape);
      if (ImGui::Combo("Shape##col", &currentShape, shapeNames, 2))
        c->shape = static_cast<Components::Collider::Shape>(currentShape);

      if (c->shape == Components::Collider::Shape::Box) {
        ImGui::DragFloat2("Size##col", &c->size.x, 1.0f);
      } else {
        ImGui::DragFloat("Radius##col", &c->radius, 1.0f);
      }

      ImGui::DragFloat2("Offset##col", &c->offset.x, 1.0f);
      ImGui::Checkbox("Solid##col", &c->isSolid);
      ImGui::Checkbox("Trigger##col", &c->isTrigger);
      ImGui::Checkbox("Blocks Light##col", &c->blocksLight);
      ImGui::Checkbox("Moveable##col", &c->moveable);
      if (c->moveable)
        ImGui::DragFloat("Mass##col", &c->mass, 0.1f);

      if (ImGui::Button("Fit to Sprite##col")) {
        auto *spr =
            world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
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
                                        ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
    if (ImGui::SmallButton("X##rv"))
      rm = true;
    if (open) {
      auto *vel = world.getComponent<Components::Velocity>(m_selectedEntity);
      float vv[2] = {vel->velocity.x, vel->velocity.y};
      if (ImGui::DragFloat2("Velocity##vel", vv, 0.5f))
        vel->velocity = {vv[0], vv[1]};
      ImGui::DragFloat("MoveSpeed##vel", &vel->moveSpeed, 1.0f);
    }
    if (rm)
      world.removeComponent<Components::Velocity>(m_selectedEntity);
  }

  // ── AudioSource ─────────────────────────────────────────────────────────
  if (world.hasComponent<Components::AudioSource>(m_selectedEntity)) {
    bool rm = false;
    bool open = ImGui::CollapsingHeader("AudioSource##hdr",
                                        ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
    if (ImGui::SmallButton("X##ras"))
      rm = true;
    if (open) {
      auto *au = world.getComponent<Components::AudioSource>(m_selectedEntity);
      char sndbuf[256];
      snprintf(sndbuf, sizeof(sndbuf), "%s", au->soundId.c_str());
      if (ImGui::InputText("Sound ID##au", sndbuf, sizeof(sndbuf)))
        au->soundId = sndbuf;
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *pl =
                ImGui::AcceptDragDropPayload("AUDIO_ASSET"))
          au->soundId = static_cast<const char *>(pl->Data);
        ImGui::EndDragDropTarget();
      }
      ImGui::SliderFloat("Volume##au", &au->volume, 0.0f, 1.0f);
      ImGui::Checkbox("Loop##au", &au->loop);
      ImGui::SameLine();
      ImGui::Checkbox("PlayOnStart##au", &au->playOnStart);
    }
    if (rm)
      world.removeComponent<Components::AudioSource>(m_selectedEntity);
  }

  // ── SpriteAnimator ──────────────────────────────────────────────────────
  if (world.hasComponent<Components::SpriteAnimator>(m_selectedEntity)) {
    bool rm = false;
    bool open = ImGui::CollapsingHeader("SpriteAnimator##hdr",
                                        ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
    if (ImGui::SmallButton("X##rsa"))
      rm = true;
    if (open) {
      auto *sa =
          world.getComponent<Components::SpriteAnimator>(m_selectedEntity);
      char fbuf[256];
      snprintf(fbuf, sizeof(fbuf), "%s", sa->filename.c_str());
      if (ImGui::InputText("File##sa", fbuf, sizeof(fbuf)))
        sa->filename = fbuf;
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *pl =
                ImGui::AcceptDragDropPayload("ANIMATION_ASSET")) {
          sa->filename = static_cast<const char *>(pl->Data);
          sa->loadFromPath("assets/animations/" + sa->filename);
        }
        ImGui::EndDragDropTarget();
      }
      char cbuf[128];
      snprintf(cbuf, sizeof(cbuf), "%s", sa->currentClip.c_str());
      if (ImGui::InputText("Current Clip##sa", cbuf, sizeof(cbuf)))
        sa->currentClip = cbuf;
      ImGui::Checkbox("Playing##sa", &sa->playing);
      ImGui::SameLine();
      ImGui::Checkbox("Paused##sa", &sa->paused);
    }
    if (rm)
      world.removeComponent<Components::SpriteAnimator>(m_selectedEntity);
  }

  // ── StateMachine ────────────────────────────────────────────────────────
  if (world.hasComponent<Components::StateMachine>(m_selectedEntity)) {
    bool rm = false;
    bool open = ImGui::CollapsingHeader("StateMachine##hdr",
                                        ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
    if (ImGui::SmallButton("X##rsm"))
      rm = true;
    if (open) {
      auto *sm = world.getComponent<Components::StateMachine>(m_selectedEntity);
      char sbuf[128];
      snprintf(sbuf, sizeof(sbuf), "%s", sm->currentState.c_str());
      if (ImGui::InputText("Current State##sm", sbuf, sizeof(sbuf)))
        sm->currentState = sbuf;
      ImGui::BeginDisabled(true);
      ImGui::DragFloat("State Timer##sm", &sm->stateTimer, 0.0f);
      ImGui::EndDisabled();
    }
    if (rm)
      world.removeComponent<Components::StateMachine>(m_selectedEntity);
  }

  // ── LuaScript (multi-slot) ───────────────────────────────────────────────
  if (world.hasComponent<Components::LuaScript>(m_selectedEntity)) {
    namespace fs = std::filesystem;
    auto *ls = world.getComponent<Components::LuaScript>(m_selectedEntity);
    bool removeComponent = false;

    for (int si = 0; si < (int)ls->scripts.size(); ++si) {
      auto &entry = ls->scripts[si];
      ImGui::PushID(si);

      // Header name: derive from script filename (e.g. "health.lua" → "Health")
      std::string headerName = "Script";
      if (!entry.scriptPath.empty()) {
        std::string stem = fs::path(entry.scriptPath).stem().string();
        if (!stem.empty())
          stem[0] = (char)toupper((unsigned char)stem[0]);
        headerName = stem;
      }

      bool open = ImGui::CollapsingHeader(headerName.c_str(),
                                          ImGuiTreeNodeFlags_DefaultOpen |
                                              ImGuiTreeNodeFlags_AllowOverlap);
      ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
      if (ImGui::SmallButton("X##lse")) {
        ls->scripts.erase(ls->scripts.begin() + si);
        ImGui::PopID();
        --si;
        continue;
      }

      if (open) {
        char spbuf[256];
        snprintf(spbuf, sizeof(spbuf), "%s", entry.scriptPath.c_str());
        if (ImGui::InputText("Script Path##lse", spbuf, sizeof(spbuf)))
          entry.scriptPath = spbuf;
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload *pl =
                  ImGui::AcceptDragDropPayload("SCRIPT_ASSET"))
            entry.scriptPath = static_cast<const char *>(pl->Data);
          ImGui::EndDragDropTarget();
        }
        ImGui::Checkbox("Enabled##lse", &entry.enabled);

        if (!entry.scriptPath.empty()) {
          auto &propDefs =
              Scripting::ScriptPropertyReader::read(entry.scriptPath);
          if (!propDefs.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Script Properties");
            for (auto &def : propDefs) {
              if (def.label.empty())
                continue;
              if (!entry.properties.count(def.name))
                entry.properties[def.name] = def.defaultValue;
              auto &val = entry.properties[def.name];
              if (def.type == "float") {
                float v = std::get<float>(val);
                if (ImGui::DragFloat(def.label.c_str(), &v, 0.1f))
                  val = v;
              } else if (def.type == "int") {
                int v = std::get<int>(val);
                if (ImGui::DragInt(def.label.c_str(), &v))
                  val = v;
              } else if (def.type == "bool") {
                bool v = std::get<bool>(val);
                if (ImGui::Checkbox(def.label.c_str(), &v))
                  val = v;
              } else if (def.type == "string") {
                std::string sv = std::get<std::string>(val);
                char strbuf[256];
                snprintf(strbuf, sizeof(strbuf), "%s", sv.c_str());
                if (ImGui::InputText(def.label.c_str(), strbuf, sizeof(strbuf)))
                  val = std::string(strbuf);
              } else if (def.type == "texture") {
                std::string sv = std::get<std::string>(val);
                char strbuf[256];
                snprintf(strbuf, sizeof(strbuf), "%s", sv.c_str());
                if (ImGui::InputText(def.label.c_str(), strbuf, sizeof(strbuf)))
                  val = std::string(strbuf);
                if (ImGui::BeginDragDropTarget()) {
                  if (const ImGuiPayload *pl =
                          ImGui::AcceptDragDropPayload("TEXTURE_ASSET"))
                    val = std::filesystem::path(static_cast<const char *>(pl->Data)).stem().string();
                  ImGui::EndDragDropTarget();
                }
              } else if (def.type == "animation") {
                std::string sv = std::get<std::string>(val);
                char strbuf[256];
                snprintf(strbuf, sizeof(strbuf), "%s", sv.c_str());
                if (ImGui::InputText(def.label.c_str(), strbuf, sizeof(strbuf)))
                  val = std::string(strbuf);
                if (ImGui::BeginDragDropTarget()) {
                  if (const ImGuiPayload *pl =
                          ImGui::AcceptDragDropPayload("ANIMATION_ASSET"))
                    val = std::string(static_cast<const char *>(pl->Data));
                  ImGui::EndDragDropTarget();
                }
              }
            }
        }
        }       // close if (!entry.scriptPath.empty())
      }         // close if (open)
      ImGui::PopID();
    }           // close for (int si = 0; ...)
    if (removeComponent)
      world.removeComponent<Components::LuaScript>(m_selectedEntity);
  }             // close if (world.hasComponent<LuaScript>)
        ImGui::TextDisabled("(%d entities in group)", (int)grp.entityIds.size());
        ImGui::End();
        return;
    }

    if (m_sceneMetaBuffersDirty) {
        strncpy_s(m_sceneBgmBuf, m_scene.getBGM().c_str(), sizeof(m_sceneBgmBuf) - 1);
        strncpy_s(m_sceneMusicContextBuf, m_scene.getMusicContext().c_str(), sizeof(m_sceneMusicContextBuf) - 1);
        m_sceneSpawnPointBuf = m_scene.getSpawnPoint();
        m_sceneMetaBuffersDirty = false;
    }

    if (ImGui::CollapsingHeader("Scene Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::InputText("Music Context##scene", m_sceneMusicContextBuf, sizeof(m_sceneMusicContextBuf)))
            m_scene.setMusicContext(m_sceneMusicContextBuf);
        if (ImGui::InputText("Legacy BGM##scene", m_sceneBgmBuf, sizeof(m_sceneBgmBuf)))
            m_scene.setBGM(m_sceneBgmBuf);

        float sfxVolume = m_scene.getSFXVolume();
        if (ImGui::SliderFloat("Scene SFX Volume##scene", &sfxVolume, 0.0f, 1.0f))
            m_scene.setSFXVolume(sfxVolume);
        if (ImGui::DragFloat2("Spawn Point##scene", &m_sceneSpawnPointBuf.x, 1.0f))
            m_scene.setSpawnPoint(m_sceneSpawnPointBuf);

        ImGui::TextDisabled("An entity tagged 'PlayerSpawn' overrides this point in-game.");
        if (world.isValid(m_selectedEntity) &&
            world.getComponent<Components::Transform>(m_selectedEntity)) {
            if (ImGui::Button("Use Selected As PlayerSpawn", ImVec2(-1, 0))) {
                auto* tag = world.getComponent<Components::Tag>(m_selectedEntity);
                if (!tag) {
                    world.addComponent(m_selectedEntity, Components::Tag{});
                    tag = world.getComponent<Components::Tag>(m_selectedEntity);
                }
                if (tag)
                    tag->tag = "PlayerSpawn";
                if (auto* transform = world.getComponent<Components::Transform>(m_selectedEntity)) {
                    m_sceneSpawnPointBuf = transform->position;
                    m_scene.setSpawnPoint(m_sceneSpawnPointBuf);
                }
            }
        }
    }

    ImGui::Separator();

    if (!world.isValid(m_selectedEntity)) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add Component", ImVec2(-1, 0)))
        ImGui::OpenPopup("##AddComp");

    if (ImGui::BeginPopup("##AddComp")) {
        if (ImGui::BeginMenu("Core")) {
            addCompEntry<Components::Velocity>("Velocity", m_selectedEntity, world, execCmd);
            addCompEntry<Components::Tag>("Tag", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            addCompEntry<Components::SpriteRenderer>("SpriteRenderer", m_selectedEntity, world, execCmd);
            addCompEntry<Components::SpriteAnimator>("SpriteAnimator", m_selectedEntity, world, execCmd);
            addCompEntry<Components::TextRenderer>("TextRenderer", m_selectedEntity, world, execCmd);
            addCompEntry<Components::VideoPlayer>("VideoPlayer", m_selectedEntity, world, execCmd);
            addCompEntry<Components::Light>("Light", m_selectedEntity, world, execCmd);
            addCompEntry<Components::ParticleEmitter>("ParticleEmitter", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics")) {
            addCompEntry<Components::Collider>("Collider", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio")) {
            addCompEntry<Components::AudioSource>("AudioSource", m_selectedEntity, world, execCmd);
            addCompEntry<Components::FootstepAudio>("FootstepAudio", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI")) {
            addCompEntry<Components::UIButton>("UIButton", m_selectedEntity, world, execCmd);
            addCompEntry<Components::UISlider>("UISlider", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera")) {
            addCompEntry<Components::CameraComponent>("CameraComponent", m_selectedEntity, world, execCmd);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scripting")) {
            // LuaScript: always available ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â adds a new script slot each click
            if (ImGui::MenuItem("LuaScript")) {
                if (!world.hasComponent<Components::LuaScript>(m_selectedEntity))
                    world.addComponent(m_selectedEntity, Components::LuaScript{});
                world.getComponent<Components::LuaScript>(m_selectedEntity)->scripts.push_back({});
            }
            addCompEntry<Components::StateMachine>("StateMachine", m_selectedEntity, world, execCmd);
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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            std::string hierarchyPath = "Root";
            int containingGroupIdx = -1;
            for (int i = 0; i < static_cast<int>(m_hierarchyGroups.size()); ++i) {
                const auto& group = m_hierarchyGroups[i];
                if (std::find(group.entityIds.begin(), group.entityIds.end(), m_selectedEntity.id) !=
                    group.entityIds.end()) {
                    containingGroupIdx = i;
                    break;
                }
            }

            if (containingGroupIdx >= 0) {
                std::vector<std::string> pathParts;
                int currentGroupIdx = containingGroupIdx;
                while (currentGroupIdx >= 0 &&
                       currentGroupIdx < static_cast<int>(m_hierarchyGroups.size())) {
                    pathParts.push_back(m_hierarchyGroups[currentGroupIdx].name);
                    int parentIdx = m_hierarchyGroups[currentGroupIdx].parentGroupIdx;
                    if (parentIdx == currentGroupIdx)
                        break;
                    currentGroupIdx = parentIdx;
                }
                std::reverse(pathParts.begin(), pathParts.end());

                hierarchyPath.clear();
                for (size_t i = 0; i < pathParts.size(); ++i) {
                    if (i > 0)
                        hierarchyPath += " / ";
                    hierarchyPath += pathParts[i];
                }
            }

            ImGui::TextDisabled("Hierarchy: %s", hierarchyPath.c_str());
        }
    }

    // Transform
    if (auto* t = world.getComponent<Components::Transform>(m_selectedEntity)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto transformSelection = selectedTransformEntities();
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
                if (transformSelection.size() > 1) {
                    applySelectionRotation(rot - m_gizmoOriginalRotation);
                } else {
                    t->rotation = rot;
                }
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (transformSelection.size() > 1) {
                    auto rotateSelectionCmd = std::make_unique<CompositeEditorCmd>("Rotate Selection");
                    for (size_t i = 0; i < m_gizmoGroupOriginalPos.size(); ++i) {
                        const auto& entity = m_gizmoGroupOriginalPos[i].first;
                        auto* transform = world.getComponent<Components::Transform>(entity);
                        if (!transform) continue;

                        const vec2 originalPos = m_gizmoGroupOriginalPos[i].second;
                        const vec2 finalPos = transform->position;
                        if (finalPos.x != originalPos.x || finalPos.y != originalPos.y) {
                            rotateSelectionCmd->add(std::make_unique<MoveEntityCmd>(
                                world, entity, originalPos, finalPos));
                        }

                        if (i < m_gizmoGroupOriginalRot.size()) {
                            const float originalRot = m_gizmoGroupOriginalRot[i].second;
                            if (transform->rotation != originalRot) {
                                rotateSelectionCmd->add(std::make_unique<RotateEntityCmd>(
                                    world, entity, originalRot, transform->rotation));
                            }
                        }
                    }
                    if (!rotateSelectionCmd->commands.empty())
                        executeCommand(std::move(rotateSelectionCmd));
                } else {
                    executeCommand(std::make_unique<RotateEntityCmd>(
                        world, m_selectedEntity, m_gizmoOriginalRotation, rot));
                }
            }
            if (ImGui::IsItemActivated()) {
                m_gizmoOriginalRotation = t->rotation;
                m_gizmoGroupOriginalPos.clear();
                m_gizmoGroupOriginalRot.clear();
                for (const auto& entity : transformSelection) {
                    if (auto* transform = world.getComponent<Components::Transform>(entity)) {
                        m_gizmoGroupOriginalPos.push_back({entity, transform->position});
                        m_gizmoGroupOriginalRot.push_back({entity, transform->rotation});
                    }
                }
            }

            float scale[2] = {t->scale.x, t->scale.y};
            if (ImGui::DragFloat2("Scale", scale, 0.01f))
                t->scale = {scale[0], scale[1]};
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            float pivot[2] = {t->pivot.x, t->pivot.y};
            if (ImGui::DragFloat2("Offset", pivot, 0.5f))
                t->pivot = {pivot[0], pivot[1]};
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            ImGui::Checkbox("Snap to Grid##transform", &t->snapToGrid);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
    }

    // SpriteRenderer
    if (world.hasComponent<Components::SpriteRenderer>(m_selectedEntity)) {
        bool rmSprite = false;
        bool openSprite = ImGui::CollapsingHeader("SpriteRenderer##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rsr")) rmSprite = true;
        if (openSprite) {
            auto* sr = world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
            char texBuf[256];
            strncpy_s(texBuf, sr->textureId.c_str(), sizeof(texBuf) - 1);
            if (ImGui::InputText("Texture ID", texBuf, sizeof(texBuf)))
                sr->textureId = texBuf;
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
                    captureInspectorBefore();
                    sr->textureId = std::filesystem::path(static_cast<const char*>(pl->Data)).stem().string();
                    pushInspectorChange();
                }
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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            ImGui::ColorEdit4("Color", glm::value_ptr(sr->color));
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            ImGui::Checkbox("Visible", &sr->visible);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            ImGui::DragInt("Sort Order", &sr->sortOrder);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rmSprite) execCmd(std::make_unique<RemoveComponentCmd<Components::SpriteRenderer>>(world, m_selectedEntity));
    } else {
        if (ImGui::Button("+ Add SpriteRenderer"))
            execCmd(std::make_unique<AddComponentCmd<Components::SpriteRenderer>>(world, m_selectedEntity));
    }

    // Collider
    if (ImGui::CollapsingHeader("Collider")) {
        auto selected = selectedValidEntities();
        const int selectedCount = (int)selected.size();
        int colliderCount = 0;
        ECS::Entity representative{0, 0};
        for (const auto& e : selected) {
            if (!world.hasComponent<Components::Collider>(e)) continue;
            ++colliderCount;
            if (!world.isValid(representative))
                representative = e;
        }
        auto* c = world.isValid(representative)
            ? world.getComponent<Components::Collider>(representative)
            : world.getComponent<Components::Collider>(m_selectedEntity);

        if (selectedCount > 1) {
            ImGui::TextDisabled("%d selected, %d with colliders", selectedCount, colliderCount);
            auto nextCombinedGroupId = [&]() -> uint32_t {
                uint32_t maxGroupId = 0;
                world.each<Components::Collider>([&](ECS::Entity, Components::Collider& collider) {
                    maxGroupId = std::max(maxGroupId, collider.combinedGroupId);
                });
                return maxGroupId + 1;
            };
            if (colliderCount < selectedCount) {
                if (ImGui::Button("Add Collider To Selected##colbulk", ImVec2(-1, 0))) {
                    applyToSelected([&](ECS::Entity e) {
                        if (!world.hasComponent<Components::Collider>(e))
                            world.addComponent(e, Components::Collider{});
                    });
                    colliderCount = selectedCount;
                    representative = selected.front();
                    c = world.getComponent<Components::Collider>(representative);
                }
            }
            if (colliderCount > 0) {
                if (colliderCount > 1) {
                    bool canMarkCombined = true;
                    bool canCombineColliders = true;
                    for (const auto& e : selected) {
                        auto* col = world.getComponent<Components::Collider>(e);
                        if (!col) continue;
                        if (col->shape != Components::Collider::Shape::Box) {
                            canMarkCombined = false;
                            canCombineColliders = false;
                            break;
                        }
                    }

                    bool mergeWithNeighbors = c ? c->isCombined : false;
                    if (!canMarkCombined) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Checkbox("Merge With Neighbors On Save##colbulk", &mergeWithNeighbors)) {
                        const uint32_t newGroupId = mergeWithNeighbors ? nextCombinedGroupId() : 0;
                        applyToSelected([&](ECS::Entity e) {
                            if (auto* col = world.getComponent<Components::Collider>(e)) {
                                if (col->shape == Components::Collider::Shape::Box) {
                                    col->isCombined = mergeWithNeighbors;
                                    col->combinedGroupId = mergeWithNeighbors ? newGroupId : 0;
                                }
                            }
                        });
                    }
                    if (ImGui::Button("Create Composite Collider Group##colbulk", ImVec2(-1, 0))) {
                        const uint32_t newGroupId = nextCombinedGroupId();
                        applyToSelected([&](ECS::Entity e) {
                            if (auto* col = world.getComponent<Components::Collider>(e)) {
                                if (col->shape == Components::Collider::Shape::Box) {
                                    col->isCombined = true;
                                    col->combinedGroupId = newGroupId;
                                }
                            }
                        });
                    }
                    if (ImGui::Button("Clear Composite Group##colbulk", ImVec2(-1, 0))) {
                        applyToSelected([&](ECS::Entity e) {
                            if (auto* col = world.getComponent<Components::Collider>(e)) {
                                col->combinedGroupId = 0;
                                if (col->shape == Components::Collider::Shape::Box)
                                    col->isCombined = false;
                            }
                        });
                    }
                    if (!canMarkCombined) {
                        ImGui::EndDisabled();
                        ImGui::TextDisabled("Auto-merge applies to box colliders only.");
                    }
                    if (c && c->combinedGroupId != 0)
                        ImGui::TextDisabled("Composite Group: %u", c->combinedGroupId);

                    float bulkSizeDelta[2] = {m_bulkColliderSizeDelta.x, m_bulkColliderSizeDelta.y};
                    if (ImGui::DragFloat2("Bulk Size Delta##colbulk", bulkSizeDelta, 1.0f)) {
                        m_bulkColliderSizeDelta = {bulkSizeDelta[0], bulkSizeDelta[1]};
                    }
                    if (ImGui::Button("Apply Size Delta##colbulk", ImVec2(-1, 0))) {
                        applyToSelected([&](ECS::Entity e) {
                            if (auto* col = world.getComponent<Components::Collider>(e)) {
                                if (col->shape != Components::Collider::Shape::Box) return;
                                col->size.x = std::max(1.0f, col->size.x + m_bulkColliderSizeDelta.x);
                                col->size.y = std::max(1.0f, col->size.y + m_bulkColliderSizeDelta.y);
                            }
                        });
                    }

                    float bulkOffsetDelta[2] = {m_bulkColliderOffsetDelta.x, m_bulkColliderOffsetDelta.y};
                    if (ImGui::DragFloat2("Bulk Offset Delta##colbulk", bulkOffsetDelta, 1.0f)) {
                        m_bulkColliderOffsetDelta = {bulkOffsetDelta[0], bulkOffsetDelta[1]};
                    }
                    if (ImGui::Button("Apply Offset Delta##colbulk", ImVec2(-1, 0))) {
                        applyToSelected([&](ECS::Entity e) {
                            if (auto* col = world.getComponent<Components::Collider>(e))
                                col->offset = {
                                    col->offset.x + m_bulkColliderOffsetDelta.x,
                                    col->offset.y + m_bulkColliderOffsetDelta.y
                                };
                        });
                    }

                    if (!canCombineColliders) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Button("Combine Colliders##colbulk", ImVec2(-1, 0))) {
                        ECS::Entity targetEntity = world.isValid(m_selectedEntity) ? m_selectedEntity : representative;
                        auto* targetTransform = world.getComponent<Components::Transform>(targetEntity);
                        auto* targetCollider = world.getComponent<Components::Collider>(targetEntity);
                        if (targetTransform && targetCollider) {
                            float minX = std::numeric_limits<float>::max();
                            float minY = std::numeric_limits<float>::max();
                            float maxX = std::numeric_limits<float>::lowest();
                            float maxY = std::numeric_limits<float>::lowest();

                            for (const auto& e : selected) {
                                auto* t = world.getComponent<Components::Transform>(e);
                                auto* col = world.getComponent<Components::Collider>(e);
                                if (!t || !col || col->shape != Components::Collider::Shape::Box) continue;

                                const vec2 center{
                                    t->position.x + col->offset.x,
                                    t->position.y + col->offset.y
                                };
                                const vec2 halfExtents{
                                    std::abs(col->size.x * t->scale.x) * 0.5f,
                                    std::abs(col->size.y * t->scale.y) * 0.5f
                                };
                                minX = std::min(minX, center.x - halfExtents.x);
                                minY = std::min(minY, center.y - halfExtents.y);
                                maxX = std::max(maxX, center.x + halfExtents.x);
                                maxY = std::max(maxY, center.y + halfExtents.y);
                            }

                            if (minX <= maxX && minY <= maxY) {
                                const vec2 mergedCenter{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
                                const vec2 mergedSize{maxX - minX, maxY - minY};
                                const float invScaleX = std::abs(targetTransform->scale.x) > 0.0001f ? 1.0f / std::abs(targetTransform->scale.x) : 1.0f;
                                const float invScaleY = std::abs(targetTransform->scale.y) > 0.0001f ? 1.0f / std::abs(targetTransform->scale.y) : 1.0f;

                                targetCollider->shape = Components::Collider::Shape::Box;
                                targetCollider->size = {mergedSize.x * invScaleX, mergedSize.y * invScaleY};
                                targetCollider->offset = {mergedCenter.x - targetTransform->position.x,
                                                          mergedCenter.y - targetTransform->position.y};
                                targetCollider->isCombined = true;
                                targetCollider->combinedGroupId = 0;

                                for (const auto& e : selected) {
                                    if (e.id == targetEntity.id && e.generation == targetEntity.generation) continue;
                                    if (world.hasComponent<Components::Collider>(e))
                                        world.removeComponent<Components::Collider>(e);
                                }

                                colliderCount = 1;
                                representative = targetEntity;
                                c = targetCollider;
                            }
                        }
                    }
                    if (!canCombineColliders) {
                        ImGui::EndDisabled();
                        ImGui::TextDisabled("Combine requires box colliders.");
                    }
                }
                if (ImGui::Button("Remove Colliders From Selected##colbulk", ImVec2(-1, 0))) {
                    applyToSelected([&](ECS::Entity e) {
                        if (world.hasComponent<Components::Collider>(e))
                            world.removeComponent<Components::Collider>(e);
                    });
                    c = nullptr;
                }
            }
            if (c)
                ImGui::Separator();
        }

        if (c) {
            ImGui::Checkbox("Show Outline", &m_showColliders);
            ImGui::Separator();

            // Shape selector ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â changing this affects collision logic immediately
            const char* shapeNames[] = {"Box", "Circle"};
            int currentShape = static_cast<int>(c->shape);
            if (ImGui::Combo("Shape##col", &currentShape, shapeNames, 2)) {
                auto newShape = static_cast<Components::Collider::Shape>(currentShape);
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->shape = newShape;
                });
            }

            if (c->shape == Components::Collider::Shape::Box) {
                float size[2] = {c->size.x, c->size.y};
                if (ImGui::DragFloat2("Size##col", size, 1.0f)) {
                    applyToSelected([&](ECS::Entity e) {
                        if (auto* col = world.getComponent<Components::Collider>(e))
                            col->size = {size[0], size[1]};
                    });
                }
            } else {
                float radius = c->radius;
                if (ImGui::DragFloat("Radius##col", &radius, 1.0f)) {
                    applyToSelected([&](ECS::Entity e) {
                        if (auto* col = world.getComponent<Components::Collider>(e))
                            col->radius = radius;
                    });
                }
            }

            float offset[2] = {c->offset.x, c->offset.y};
            if (ImGui::DragFloat2("Offset##col", offset, 1.0f)) {
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->offset = {offset[0], offset[1]};
                });
            }

            bool solid = c->isSolid;
            if (ImGui::Checkbox("Solid##col", &solid)) {
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->isSolid = solid;
                });
            }
            bool trigger = c->isTrigger;
            if (ImGui::Checkbox("Trigger##col", &trigger)) {
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->isTrigger = trigger;
                });
            }
            bool blocksLight = c->blocksLight;
            if (ImGui::Checkbox("Blocks Light##col", &blocksLight)) {
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->blocksLight = blocksLight;
                });
            }
            bool moveable = c->moveable;
            if (ImGui::Checkbox("Moveable##col", &moveable)) {
                applyToSelected([&](ECS::Entity e) {
                    if (auto* col = world.getComponent<Components::Collider>(e))
                        col->moveable = moveable;
                });
            }
            if (c->shape == Components::Collider::Shape::Box) {
                bool mergeWithNeighbors = c->isCombined;
                if (ImGui::Checkbox("Merge With Neighbors On Save##col", &mergeWithNeighbors)) {
                    applyToSelected([&](ECS::Entity e) {
                        if (auto* col = world.getComponent<Components::Collider>(e)) {
                            if (col->shape == Components::Collider::Shape::Box) {
                                col->isCombined = mergeWithNeighbors;
                                if (!mergeWithNeighbors)
                                    col->combinedGroupId = 0;
                            }
                        }
                    });
                }
                if (c->combinedGroupId != 0)
                    ImGui::TextDisabled("Composite Group: %u", c->combinedGroupId);
            }
            if (c->moveable) {
                float mass = c->mass;
                if (ImGui::DragFloat("Mass##col", &mass, 0.1f)) {
                    applyToSelected([&](ECS::Entity e) {
                        if (auto* col = world.getComponent<Components::Collider>(e))
                            col->mass = mass;
                    });
                }
            }

            {
                // Collision Category — shown as a combo: Default(1), Player(1), Enemy(2), Wall(4)
                static const char* categoryNames[] = { "Default (1)", "Player (1)", "Enemy (2)", "Wall (4)" };
                static const uint32_t categoryValues[] = { 1, 1, 2, 4 };
                int currentIdx = 0;
                for (int i = 1; i < 4; ++i)
                    if (c->collisionCategory == categoryValues[i]) { currentIdx = i; break; }
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::BeginCombo("Category##col", categoryNames[currentIdx])) {
                    for (int i = 0; i < 4; ++i) {
                        bool selected = (currentIdx == i);
                        if (ImGui::Selectable(categoryNames[i], selected)) {
                            const uint32_t newCategory = categoryValues[i];
                            applyToSelected([&](ECS::Entity e) {
                                if (auto* col = world.getComponent<Components::Collider>(e))
                                    col->collisionCategory = newCategory;
                            });
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            if (ImGui::Button("Fit to Sprite##col")) {
                applyToSelected([&](ECS::Entity e) {
                    auto* col = world.getComponent<Components::Collider>(e);
                    auto* spr = world.getComponent<Components::SpriteRenderer>(e);
                    if (!col || !spr) return;
                    if (col->shape == Components::Collider::Shape::Box)
                        col->size = spr->size;
                    else
                        col->radius = std::max(spr->size.x, spr->size.y) * 0.5f;
                    col->offset = {0.f, 0.f};
                });
            }
            ImGui::SameLine();
            if (ImGui::Button(selectedCount > 1 ? "Remove From Selected##col" : "Remove##col")) {
                applyToSelected([&](ECS::Entity e) {
                    if (world.hasComponent<Components::Collider>(e))
                        world.removeComponent<Components::Collider>(e);
                });
            }
        } else {
            if (ImGui::Button(selectedCount > 1 ? "Add Collider To Selected##col" : "Add Collider##col")) {
                applyToSelected([&](ECS::Entity e) {
                    if (!world.hasComponent<Components::Collider>(e))
                        world.addComponent(e, Components::Collider{});
                });
            }
        }
    }

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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat("MoveSpeed##vel", &vel->moveSpeed, 1.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::Velocity>>(world, m_selectedEntity));
    }

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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("AUDIO_ASSET")) {
                    captureInspectorBefore();
                    au->soundId = static_cast<const char*>(pl->Data);
                    pushInspectorChange();
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SliderFloat("Volume##au", &au->volume, 0.0f, 1.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::Checkbox("Loop##au", &au->loop);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::SameLine();
            ImGui::Checkbox("PlayOnStart##au", &au->playOnStart);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::AudioSource>>(world, m_selectedEntity));
    }

    // FootstepAudio
    if (world.hasComponent<Components::FootstepAudio>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("FootstepAudio##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rfa")) rm = true;
        if (open) {
            auto* fa = world.getComponent<Components::FootstepAudio>(m_selectedEntity);

            static const char* presetLabels[] = {"Custom", "Concrete", "Tile", "Wood"};
            int presetIndex = 0;
            if (fa->soundPrefix == "fs_concrete_") presetIndex = 1;
            else if (fa->soundPrefix == "fs_tile_") presetIndex = 2;
            else if (fa->soundPrefix == "fs_wood_") presetIndex = 3;
            if (ImGui::Combo("Preset##fa", &presetIndex, presetLabels, IM_ARRAYSIZE(presetLabels))) {
                if (presetIndex == 1) {
                    fa->soundPrefix = "fs_concrete_";
                    fa->concretePrefix = "fs_concrete_";
                    fa->tilePrefix = "fs_tile_";
                    fa->woodPrefix = "fs_concrete_";
                    fa->variationCount = 4;
                } else if (presetIndex == 2) {
                    fa->soundPrefix = "fs_tile_";
                    fa->concretePrefix = "fs_concrete_";
                    fa->tilePrefix = "fs_tile_";
                    fa->woodPrefix = "fs_concrete_";
                    fa->variationCount = 4;
                } else if (presetIndex == 3) {
                    fa->soundPrefix = "fs_concrete_";
                    fa->concretePrefix = "fs_concrete_";
                    fa->tilePrefix = "fs_tile_";
                    fa->woodPrefix = "fs_concrete_";
                    fa->variationCount = 4;
                }
            }

            ImGui::Checkbox("Use Surface Sounds##fa", &fa->useSurfaceSounds);

            char defaultBuf[256]; snprintf(defaultBuf, sizeof(defaultBuf), "%s", fa->soundPrefix.c_str());
            if (ImGui::InputText("Default Prefix##fa", defaultBuf, sizeof(defaultBuf)))
                fa->soundPrefix = defaultBuf;

            char concreteBuf[256]; snprintf(concreteBuf, sizeof(concreteBuf), "%s", fa->concretePrefix.c_str());
            if (ImGui::InputText("Concrete Prefix##fa", concreteBuf, sizeof(concreteBuf)))
                fa->concretePrefix = concreteBuf;

            char tileBuf[256]; snprintf(tileBuf, sizeof(tileBuf), "%s", fa->tilePrefix.c_str());
            if (ImGui::InputText("Tile Prefix##fa", tileBuf, sizeof(tileBuf)))
                fa->tilePrefix = tileBuf;

            char woodBuf[256]; snprintf(woodBuf, sizeof(woodBuf), "%s", fa->woodPrefix.c_str());
            if (ImGui::InputText("Wood Prefix##fa", woodBuf, sizeof(woodBuf)))
                fa->woodPrefix = woodBuf;

            ImGui::DragFloat("Base Interval##fa", &fa->interval, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Fast Speed Scale##fa", &fa->sprintIntervalScale, 0.01f, 0.1f, 2.0f);
            ImGui::DragFloat("Target Speed##fa", &fa->sprintSpeedThreshold, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Min Speed##fa", &fa->minSpeed, 0.01f, 0.0f, 1000.0f);
            ImGui::DragFloat("Reset Delay##fa", &fa->resetDelay, 0.01f, 0.0f, 1.0f);
            ImGui::SliderFloat("Volume##fa", &fa->volume, 0.0f, 1.0f);
            ImGui::DragInt("Variations##fa", &fa->variationCount, 1.0f, 1, 16);
            const float previewSpeed = std::max(fa->minSpeed, fa->sprintSpeedThreshold);
            float previewInterval = fa->interval;
            if (fa->sprintSpeedThreshold > fa->minSpeed) {
                const float normalizedSpeed = (previewSpeed - fa->minSpeed) /
                    (fa->sprintSpeedThreshold - fa->minSpeed);
                const float intervalScale = 1.0f + (fa->sprintIntervalScale - 1.0f) * normalizedSpeed;
                previewInterval *= intervalScale;
            } else {
                previewInterval *= fa->sprintIntervalScale;
            }
            ImGui::TextDisabled("At target speed, footsteps play every %.3fs", previewInterval);
            ImGui::BeginDisabled(true);
            ImGui::DragFloat("Timer Remaining##fa", &fa->timerRemaining, 0.0f);
            ImGui::EndDisabled();
        }
        if (rm) world.removeComponent<Components::FootstepAudio>(m_selectedEntity);
    }
    if (world.hasComponent<Components::ParticleEmitter>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("ParticleEmitter##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rpe")) rm = true;
        if (open) {
            auto* pe = world.getComponent<Components::ParticleEmitter>(m_selectedEntity);
            auto& sys = pe->system();

            bool active = pe->active();
            if (ImGui::Checkbox("Active##pe", &active)) {
                captureInspectorBefore();
                pe->setActive(active);
                pushInspectorChange();
            }

            float localOffset[2] = {pe->localOffset.x, pe->localOffset.y};
            if (ImGui::DragFloat2("Local Offset##pe", localOffset, 0.1f)) {
                captureInspectorBefore();
                pe->localOffset = {localOffset[0], localOffset[1]};
                pushInspectorChange();
            }

            ImGui::DragFloat("Rate##pe", &sys.rate, 1.0f, 0.0f, 5000.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            float life[2] = {sys.lifeMin, sys.lifeMax};
            if (ImGui::DragFloat2("Life Range##pe", life, 0.01f, 0.01f, 60.0f)) {
                captureInspectorBefore();
                sys.lifeMin = std::min(life[0], life[1]);
                sys.lifeMax = std::max(life[0], life[1]);
                pushInspectorChange();
            }

            float speed[2] = {sys.speedMin, sys.speedMax};
            if (ImGui::DragFloat2("Speed Range##pe", speed, 1.0f, 0.0f, 5000.0f)) {
                captureInspectorBefore();
                sys.speedMin = std::min(speed[0], speed[1]);
                sys.speedMax = std::max(speed[0], speed[1]);
                pushInspectorChange();
            }

            ImGui::DragFloat("Spread##pe", &sys.spreadDeg, 0.5f, 0.0f, 180.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            float direction[2] = {sys.dir.x, sys.dir.y};
            if (ImGui::DragFloat2("Direction##pe", direction, 0.01f, -1.0f, 1.0f)) {
                captureInspectorBefore();
                sys.dir = {direction[0], direction[1]};
                pushInspectorChange();
            }

            float acceleration[2] = {sys.acceleration.x, sys.acceleration.y};
            if (ImGui::DragFloat2("Acceleration##pe", acceleration, 1.0f, -5000.0f, 5000.0f)) {
                captureInspectorBefore();
                sys.acceleration = {acceleration[0], acceleration[1]};
                pushInspectorChange();
            }

            float size[2] = {sys.sizeStart, sys.sizeEnd};
            if (ImGui::DragFloat2("Size Start/End##pe", size, 0.05f, 0.0f, 1000.0f)) {
                captureInspectorBefore();
                sys.sizeStart = size[0];
                sys.sizeEnd = size[1];
                pushInspectorChange();
            }

            float colorStart[4] = {sys.colorStart.r, sys.colorStart.g, sys.colorStart.b, sys.colorStart.a};
            if (ImGui::ColorEdit4("Color Start##pe", colorStart)) {
                captureInspectorBefore();
                sys.colorStart = {colorStart[0], colorStart[1], colorStart[2], colorStart[3]};
                pushInspectorChange();
            }

            float colorEnd[4] = {sys.colorEnd.r, sys.colorEnd.g, sys.colorEnd.b, sys.colorEnd.a};
            if (ImGui::ColorEdit4("Color End##pe", colorEnd)) {
                captureInspectorBefore();
                sys.colorEnd = {colorEnd[0], colorEnd[1], colorEnd[2], colorEnd[3]};
                pushInspectorChange();
            }

            ImGui::DragFloat("Spawn Radius##pe", &sys.spawnRadius, 0.1f, 0.0f, 1000.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

            bool looping = sys.looping;
            if (ImGui::Checkbox("Looping##pe", &looping)) {
                captureInspectorBefore();
                sys.looping = looping;
                pushInspectorChange();
            }
            ImGui::SameLine();
            bool worldSpace = sys.worldSpace;
            if (ImGui::Checkbox("World Space##pe", &worldSpace)) {
                captureInspectorBefore();
                sys.worldSpace = worldSpace;
                pushInspectorChange();
            }

            if (ImGui::Button("Burst 32##pe")) {
                if (auto* t = world.getComponent<Components::Transform>(m_selectedEntity))
                    pe->burst(ps::v2{t->position.x, t->position.y}, 32);
                else
                    pe->burst(32);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu alive", sys.aliveCount());
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::ParticleEmitter>>(world, m_selectedEntity));
    }
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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ANIMATION_ASSET")) {
                    captureInspectorBefore();
                    sa->filename = static_cast<const char*>(pl->Data);
                    sa->loadFromPath("assets/animations/" + sa->filename);
                    pushInspectorChange();
                }
                ImGui::EndDragDropTarget();
            }
            char cbuf[128]; snprintf(cbuf, sizeof(cbuf), "%s", sa->currentClip.c_str());
            if (ImGui::InputText("Current Clip##sa", cbuf, sizeof(cbuf))) sa->currentClip = cbuf;
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::Checkbox("Playing##sa", &sa->playing);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::SameLine();
            ImGui::Checkbox("Paused##sa", &sa->paused);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::SpriteAnimator>>(world, m_selectedEntity));
    }

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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::BeginDisabled(true);
            ImGui::DragFloat("State Timer##sm", &sm->stateTimer, 0.0f);
            ImGui::EndDisabled();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::StateMachine>>(world, m_selectedEntity));
    }

    if (world.hasComponent<Components::LuaScript>(m_selectedEntity)) {
        namespace fs = std::filesystem;
        auto* ls = world.getComponent<Components::LuaScript>(m_selectedEntity);
        bool removeComponent = false;

        for (int si = 0; si < (int)ls->scripts.size(); ++si) {
            auto& entry = ls->scripts[si];
            ImGui::PushID(si);

            // Header name: derive from script filename (e.g. "health.lua" ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ "Health")
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
                if (ImGui::IsItemActivated()) captureInspectorBefore();
                if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SCRIPT_ASSET")) {
                        captureInspectorBefore();
                        entry.scriptPath = static_cast<const char*>(pl->Data);
                        pushInspectorChange();
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::Checkbox("Enabled##lse", &entry.enabled);
                if (ImGui::IsItemActivated()) captureInspectorBefore();
                if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

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
                                if (ImGui::IsItemActivated()) captureInspectorBefore();
                                if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                            } else if (def.type == "int") {
                                int v = std::get<int>(val);
                                if (ImGui::DragInt(def.label.c_str(), &v)) val = v;
                                if (ImGui::IsItemActivated()) captureInspectorBefore();
                                if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                            } else if (def.type == "bool") {
                                bool v = std::get<bool>(val);
                                if (ImGui::Checkbox(def.label.c_str(), &v)) val = v;
                                if (ImGui::IsItemActivated()) captureInspectorBefore();
                                if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                            } else if (def.type == "string") {
                                std::string sv = std::get<std::string>(val);
                                const bool isActivationZoneActionType =
                                    entry.scriptPath == "components/activation_zone.lua" &&
                                    def.name == "actionType";
                                if (isActivationZoneActionType) {
                                    static const char* actionTypes[] = {
                                        "None",
                                        "PlayAudio",
                                        "ChangeScene"
                                    };
                                    int currentIndex = 0;
                                    for (int idx = 0; idx < IM_ARRAYSIZE(actionTypes); ++idx) {
                                        if (sv == actionTypes[idx]) {
                                            currentIndex = idx;
                                            break;
                                        }
                                    }
                                    if (ImGui::Combo(def.label.c_str(), &currentIndex, actionTypes, IM_ARRAYSIZE(actionTypes))) {
                                        val = std::string(actionTypes[currentIndex]);
                                    }
                                    if (ImGui::IsItemActivated()) captureInspectorBefore();
                                    if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                                    continue;
                                }
                                std::string loweredName = def.name;
                                std::string loweredLabel = def.label;
                                std::transform(loweredName.begin(), loweredName.end(), loweredName.begin(),
                                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                                std::transform(loweredLabel.begin(), loweredLabel.end(), loweredLabel.begin(),
                                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                                const bool looksLikeSoundId =
                                    loweredName.find("sfx") != std::string::npos ||
                                    loweredName.find("sound") != std::string::npos ||
                                    loweredName.find("audio") != std::string::npos ||
                                    loweredLabel.find("sfx") != std::string::npos ||
                                    loweredLabel.find("sound") != std::string::npos ||
                                    loweredLabel.find("audio") != std::string::npos;

                                const bool looksLikeSoundPrefix =
                                    loweredName.find("prefix") != std::string::npos ||
                                    loweredLabel.find("prefix") != std::string::npos;
                                auto applyDroppedAudio = [&](bool asPrefix) {
                                    if (!ImGui::BeginDragDropTarget())
                                        return false;

                                    bool changed = false;
                                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_ASSET")) {
                                        std::string droppedSound = static_cast<const char*>(payload->Data);
                                        if (asPrefix) {
                                            while (!droppedSound.empty() &&
                                                std::isdigit(static_cast<unsigned char>(droppedSound.back()))) {
                                                droppedSound.pop_back();
                                            }
                                            if (!droppedSound.empty() &&
                                                (droppedSound.back() == '_' || droppedSound.back() == '-')) {
                                                // Keep a single separator at the end so variation indices append cleanly.
                                            }
                                        }
                                        val = droppedSound;
                                        changed = true;
                                    }
                                    ImGui::EndDragDropTarget();
                                    return changed;
                                };
                                if (looksLikeSoundId && !looksLikeSoundPrefix) {
                                    auto soundNames = Assets::SoundManager::GetLoadedSoundNames();
                                    std::sort(soundNames.begin(), soundNames.end());
                                    if (!sv.empty() && std::find(soundNames.begin(), soundNames.end(), sv) == soundNames.end())
                                        soundNames.insert(soundNames.begin(), sv);

                                    std::vector<const char*> items;
                                    items.reserve(soundNames.size() + 1);
                                    items.push_back("<none>");

                                    int currentIndex = 0;
                                    for (std::size_t soundIndex = 0; soundIndex < soundNames.size(); ++soundIndex) {
                                        items.push_back(soundNames[soundIndex].c_str());
                                        if (soundNames[soundIndex] == sv)
                                            currentIndex = static_cast<int>(soundIndex) + 1;
                                    }

                                    std::string comboLabel = std::string(def.label) + "##soundprop";
                                    if (ImGui::Combo(comboLabel.c_str(), &currentIndex, items.data(), static_cast<int>(items.size()))) {
                                        val = currentIndex == 0 ? std::string() : soundNames[static_cast<std::size_t>(currentIndex - 1)];
                                        sv = std::get<std::string>(val);
                                    }
                                    if (applyDroppedAudio(false))
                                        sv = std::get<std::string>(val);
                                    if (ImGui::IsItemActivated()) captureInspectorBefore();
                                    if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();

                                    std::string inputLabel = std::string("Override ") + def.label + "##soundproptext";
                                    char soundbuf[256]; snprintf(soundbuf, sizeof(soundbuf), "%s", sv.c_str());
                                    if (ImGui::InputText(inputLabel.c_str(), soundbuf, sizeof(soundbuf)))
                                        val = std::string(soundbuf);
                                    if (applyDroppedAudio(false))
                                        sv = std::get<std::string>(val);
                                    if (ImGui::IsItemActivated()) captureInspectorBefore();
                                    if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                                } else {
                                    char soundbuf[256]; snprintf(soundbuf, sizeof(soundbuf), "%s", sv.c_str());
                                    if (ImGui::InputText(def.label.c_str(), soundbuf, sizeof(soundbuf)))
                                        val = std::string(soundbuf);
                                    if (applyDroppedAudio(looksLikeSoundPrefix))
                                        sv = std::get<std::string>(val);
                                    if (ImGui::IsItemActivated()) captureInspectorBefore();
                                    if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
                                }
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

    if (world.hasComponent<Components::TextRenderer>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("TextRenderer##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rtr")) rm = true;
        if (open) {
            auto* textComp = world.getComponent<Components::TextRenderer>(m_selectedEntity);
            char tbuf[512]; snprintf(tbuf, sizeof(tbuf), "%s", textComp->text.c_str());
            if (ImGui::InputTextMultiline("Text##tr", tbuf, sizeof(tbuf), ImVec2(-1, 60)))
                textComp->text = tbuf;
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            {
                auto fontKeys = Assets::ResourceManager<Assets::FontManager>::GetKeys();
                std::sort(fontKeys.begin(), fontKeys.end());
                int curIdx = 0;
                for (int i = 0; i < (int)fontKeys.size(); ++i)
                    if (fontKeys[i] == textComp->fontId) { curIdx = i; break; }
                std::vector<const char*> fontCStrs;
                fontCStrs.reserve(fontKeys.size());
                for (const auto& k : fontKeys) fontCStrs.push_back(k.c_str());
                if (ImGui::Combo("Font ID##tr", &curIdx, fontCStrs.data(), (int)fontCStrs.size())) {
                    captureInspectorBefore();
                    textComp->fontId = fontKeys[curIdx];
                    pushInspectorChange();
                }
            }
            ImGui::DragFloat("Font Size##tr", &textComp->fontSize, 0.01f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::ColorEdit3("Color##tr", glm::value_ptr(textComp->color));
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::Checkbox("Visible##tr", &textComp->visible);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::SameLine();
            ImGui::Checkbox("Show Box##tr", &m_showTextBoxes);
            ImGui::DragFloat2("Box Size##tr", &textComp->boxSize.x, 1.0f, 0.0f, 0.0f, "%.0f");
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat2("Padding##tr", &textComp->padding.x, 0.5f, 0.0f, 0.0f, "%.1f");
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            const char* alignItems[] = {"Left", "Center", "Right"};
            int alignIdx = static_cast<int>(textComp->alignment);
            if (ImGui::Combo("H Align##tr", &alignIdx, alignItems, 3)) {
                captureInspectorBefore();
                textComp->alignment = static_cast<Components::TextAlignment>(alignIdx);
                pushInspectorChange();
            }
            const char* vAlignItems[] = {"Top", "Middle", "Bottom"};
            int vAlignIdx = static_cast<int>(textComp->verticalAlign);
            if (ImGui::Combo("V Align##tr", &vAlignIdx, vAlignItems, 3)) {
                captureInspectorBefore();
                textComp->verticalAlign = static_cast<Components::VerticalAlignment>(vAlignIdx);
                pushInspectorChange();
            }
            ImGui::Checkbox("Word Wrap##tr", &textComp->wordWrap);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat("Line Spacing##tr", &textComp->lineSpacing, 0.05f, 0.1f, 5.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::TextRenderer>>(world, m_selectedEntity));
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
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat2("Offset##lt", &lt->offset.x, 1.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::ColorEdit3("Color##lt", &lt->color.r);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat("Radius##lt", &lt->radius, 1.0f, 0.0f, 2000.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat("Angle##lt", &lt->angle, 0.5f, 0.0f, 360.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::SliderFloat("Intensity##lt", &lt->intensity, 0.0f, 1.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::Light>>(world, m_selectedEntity));
    }

    if (world.hasComponent<Components::VideoPlayer>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("VideoPlayer##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rvp")) rm = true;
        if (open) {
            auto *vp = world.getComponent<Components::VideoPlayer>(m_selectedEntity);
            char pathBuf[256];
            snprintf(pathBuf, sizeof(pathBuf), "%s", vp->filepath.c_str());
            if (ImGui::InputText("Filepath##vp", pathBuf, sizeof(pathBuf)))
                vp->filepath = pathBuf;

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload *pl = ImGui::AcceptDragDropPayload("VIDEO_ASSET"))
                    vp->filepath = static_cast<const char *>(pl->Data);
                ImGui::EndDragDropTarget();
            }

            ImGui::Checkbox("Loop##vp", &vp->loop);
            ImGui::SameLine();
            ImGui::Checkbox("Autoplay##vp", &vp->autoplay);
            ImGui::SliderFloat("Speed##vp", &vp->playbackSpeed, 0.1f, 2.0f);

            float cTime = vp->currentTime;
            ImGui::SliderFloat("Time##vp", &cTime, 0.0f,
                               vp->duration > 0.0f ? vp->duration : 100.0f);
            ImGui::Text("Duration: %.2f", vp->duration);
            ImGui::Text("Resolution: %d x %d", vp->width, vp->height);
            ImGui::Text("FPS: %.2f", vp->fps);
            ImGui::Text("State: %s", vp->playbackState.c_str());

            bool playing = vp->isPlaying;
            if (ImGui::Checkbox("Is Playing##vp", &playing))
                vp->isPlaying = playing;
        }
        if (rm)
            execCmd(std::make_unique<RemoveComponentCmd<Components::VideoPlayer>>(
                world, m_selectedEntity));
    }

    // UIButton
    if (world.hasComponent<Components::UIButton>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("UIButton##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##ruib")) rm = true;
        if (open) {
            auto *btn = world.getComponent<Components::UIButton>(m_selectedEntity);
            char sbuf[256]; snprintf(sbuf, sizeof(sbuf), "%s", btn->uiScript.c_str());
            if (ImGui::InputText("UI Script##uib", sbuf, sizeof(sbuf)))
                btn->uiScript = sbuf;
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SCRIPT_ASSET")) {
                    captureInspectorBefore();
                    btn->uiScript = static_cast<const char*>(pl->Data);
                    pushInspectorChange();
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::Checkbox("Interactable##uib",   &btn->interactable);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::Checkbox("Show Background##uib", &btn->showBackground);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat2("Hit Size##uib",      &btn->size.x,   1.f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat2("Hit Offset##uib",    &btn->offset.x, 1.f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            if (ImGui::Button("Follow Sprite##uib")) {
                auto *sprite = world.getComponent<Components::SpriteRenderer>(m_selectedEntity);
                if (sprite) {
                    captureInspectorBefore();
                    btn->size   = sprite->size;
                    btn->offset = {0.f, 0.f};
                    pushInspectorChange();
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("Show Hit Area##uib",   &m_showUIButtons);
            ImGui::ColorEdit3("Normal Color##uib",  glm::value_ptr(btn->normalColor));
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::ColorEdit3("Hover Color##uib",   glm::value_ptr(btn->hoverColor));
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::UIButton>>(world, m_selectedEntity));
    }


    if (world.hasComponent<Components::UISlider>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("UISlider##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##ruisl")) rm = true;
        if (open) {
            auto *sl = world.getComponent<Components::UISlider>(m_selectedEntity);
            char keyBuf[64]; snprintf(keyBuf, sizeof(keyBuf), "%s", sl->settingKey.c_str());
            if (ImGui::InputText("Setting Key##uisl", keyBuf, sizeof(keyBuf)))
                sl->settingKey = keyBuf;
            ImGui::DragFloat("Min Value##uisl", &sl->minValue, 0.01f);
            ImGui::DragFloat("Max Value##uisl", &sl->maxValue, 0.01f);
            ImGui::SliderFloat("Value##uisl", &sl->value, sl->minValue, sl->maxValue);
            ImGui::DragFloat("Track Width##uisl", &sl->trackWidth, 1.0f, 1.0f, 2000.0f);
            ImGui::DragFloat("Track Height##uisl", &sl->trackHeight, 1.0f, 1.0f, 400.0f);
            ImGui::DragFloat("Handle Size##uisl", &sl->handleSize, 1.0f, 0.0f, 400.0f);
            ImGui::ColorEdit4("Track Color##uisl", glm::value_ptr(sl->trackColor));
            ImGui::ColorEdit4("Fill Color##uisl", glm::value_ptr(sl->fillColor));
            ImGui::ColorEdit4("Handle Color##uisl", glm::value_ptr(sl->handleColor));
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::UISlider>>(world, m_selectedEntity));
    }

    // CameraComponent
    if (world.hasComponent<Components::CameraComponent>(m_selectedEntity)) {
        bool rm = false;
        bool open = ImGui::CollapsingHeader("CameraComponent##hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 18.f);
        if (ImGui::SmallButton("X##rcam")) rm = true;
        if (open) {
            auto* cam = world.getComponent<Components::CameraComponent>(m_selectedEntity);
            ImGui::Checkbox("Is Main##cam",  &cam->isMain);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat("Zoom##cam",    &cam->zoom, 0.01f, 0.01f, 20.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
            ImGui::DragFloat2("Offset##cam", &cam->offset.x, 1.0f);
            if (ImGui::IsItemActivated()) captureInspectorBefore();
            if (ImGui::IsItemDeactivatedAfterEdit()) pushInspectorChange();
        }
        if (rm) execCmd(std::make_unique<RemoveComponentCmd<Components::CameraComponent>>(world, m_selectedEntity));
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// renderViewport  (scene image + entity picking + gizmos)
// ---------------------------------------------------------------------------

void EditorState::renderViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool viewportWindowVisible =
        ImGui::Begin("Scene##EditorViewport", nullptr,
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);

    m_viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_viewportShouldRender = viewportWindowVisible &&
                             !ImGui::IsWindowCollapsed() &&
                             avail.x > 0.0f &&
                             avail.y > 0.0f;
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
    m_viewportHovered = ImGui::IsItemHovered() ||
                        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // Compute viewport bounds (for gizmos + picking)
    ImVec2 winPos    = ImGui::GetWindowPos();
    ImVec2 contMin   = ImGui::GetWindowContentRegionMin();
    vec2   vpMin     = {winPos.x + contMin.x, winPos.y + contMin.y};

    auto& cam   = Core::Engine::Get().camera();
    vec2  camP  = {cam.position().x, cam.position().y};
    auto& ecs   = m_scene.GetWorld();

    ECS::Entity hoveredPickEntity{0, 0};
    if (m_viewportHovered && !m_tileMapMode) {
        ImVec2 hoverMouse = ImGui::GetMousePos();
        vec2 hoverWorldPos = screenToWorld({hoverMouse.x, hoverMouse.y}, camP, m_editorCamZoom, vpMin, m_viewportSize);

        auto containsHoverPickPoint = [&](ECS::Entity e) {
            if (!ecs.isValid(e) || m_hiddenEntities.count(e.id)) return false;

            auto* t = ecs.getComponent<Components::Transform>(e);
            if (!t) return false;

            vec2 center{t->position.x, t->position.y};
            if (auto* sr = ecs.getComponent<Components::SpriteRenderer>(e)) {
                if (!sr->visible) return false;
                float hw = (sr->size.x * std::abs(t->scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t->scale.y)) * 0.5f;
                return (hoverWorldPos.x >= center.x - hw && hoverWorldPos.x <= center.x + hw &&
                        hoverWorldPos.y >= center.y - hh && hoverWorldPos.y <= center.y + hh);
            }

            float d = glm::length(glm::vec2(hoverWorldPos.x - center.x, hoverWorldPos.y - center.y));
            return d < 60.0f;
        };

        struct HoverPickCandidate {
            ECS::Entity entity{0, 0};
            bool hasSprite{false};
            int sortOrder{std::numeric_limits<int>::min()};
            float distance{std::numeric_limits<float>::max()};
        };

        std::vector<HoverPickCandidate> hoveredPickCandidates;
        ecs.each<Components::Transform>([&](ECS::Entity e, Components::Transform& t) {
            if (m_hiddenEntities.count(e.id)) return;

            vec2 center{t.position.x, t.position.y};
            if (auto* sr = ecs.getComponent<Components::SpriteRenderer>(e)) {
                if (!sr->visible) return;
                float hw = (sr->size.x * std::abs(t.scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t.scale.y)) * 0.5f;
                bool inside = (hoverWorldPos.x >= center.x - hw && hoverWorldPos.x <= center.x + hw &&
                               hoverWorldPos.y >= center.y - hh && hoverWorldPos.y <= center.y + hh);
                if (inside)
                    hoveredPickCandidates.push_back({e, true, sr->sortOrder, 0.0f});
            } else {
                float d = glm::length(glm::vec2(hoverWorldPos.x - center.x, hoverWorldPos.y - center.y));
                if (d < 60.0f)
                    hoveredPickCandidates.push_back({e, false, std::numeric_limits<int>::min(), d});
            }
        });

        std::stable_sort(hoveredPickCandidates.begin(), hoveredPickCandidates.end(),
            [](const HoverPickCandidate& a, const HoverPickCandidate& b) {
                if (a.hasSprite != b.hasSprite) return a.hasSprite > b.hasSprite;
                if (a.hasSprite && b.hasSprite) {
                    if (a.sortOrder != b.sortOrder) return a.sortOrder > b.sortOrder;
                    return a.entity.id > b.entity.id;
                }
                if (a.distance != b.distance) return a.distance < b.distance;
                return a.entity.id > b.entity.id;
            });

        if (!hoveredPickCandidates.empty()) {
            hoveredPickEntity = hoveredPickCandidates.front().entity;

            const float pickCycleThreshold = 8.0f;
            const double pickCycleCooldownSeconds = 3.0;
            bool repeatedPickArea = false;
            if (m_hasLastPickScreenPos) {
                float dx = hoverMouse.x - m_lastPickScreenPos.x;
                float dy = hoverMouse.y - m_lastPickScreenPos.y;
                repeatedPickArea = std::sqrt(dx * dx + dy * dy) <= pickCycleThreshold;
            }

            double secondsSinceLastPick = ImGui::GetTime() - m_lastPickTimeSeconds;
            bool canCyclePick = repeatedPickArea && secondsSinceLastPick <= pickCycleCooldownSeconds;

            if (canCyclePick && containsHoverPickPoint(m_selectedEntity)) {
                for (size_t i = 0; i < hoveredPickCandidates.size(); ++i) {
                    const ECS::Entity candidate = hoveredPickCandidates[i].entity;
                    if (candidate.id == m_selectedEntity.id &&
                        candidate.generation == m_selectedEntity.generation) {
                        hoveredPickEntity = hoveredPickCandidates[(i + 1) % hoveredPickCandidates.size()].entity;
                        break;
                    }
                }
            }
        }
    }

    if (m_showPickBounds && ecs.isValid(hoveredPickEntity)) {
        if (auto* t = ecs.getComponent<Components::Transform>(hoveredPickEntity)) {
            vec2 center{t->position.x, t->position.y};
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 fillCol = IM_COL32(255, 180, 70, 40);
            ImU32 lineCol = IM_COL32(255, 180, 70, 220);

            if (auto* sr = ecs.getComponent<Components::SpriteRenderer>(hoveredPickEntity)) {
                float hw = (sr->size.x * std::abs(t->scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t->scale.y)) * 0.5f;

                vec2 a = worldToScreen({center.x - hw, center.y + hh}, camP, m_editorCamZoom, vpMin, m_viewportSize);
                vec2 b = worldToScreen({center.x + hw, center.y - hh}, camP, m_editorCamZoom, vpMin, m_viewportSize);
                ImVec2 minPt{std::min(a.x, b.x), std::min(a.y, b.y)};
                ImVec2 maxPt{std::max(a.x, b.x), std::max(a.y, b.y)};
                dl->AddRectFilled(minPt, maxPt, fillCol);
                dl->AddRect(minPt, maxPt, lineCol, 0.f, 0, 2.0f);
            } else {
                constexpr float pickRadius = 60.0f;
                vec2 a = worldToScreen({center.x - pickRadius, center.y + pickRadius}, camP, m_editorCamZoom, vpMin, m_viewportSize);
                vec2 b = worldToScreen({center.x + pickRadius, center.y - pickRadius}, camP, m_editorCamZoom, vpMin, m_viewportSize);
                ImVec2 minPt{std::min(a.x, b.x), std::min(a.y, b.y)};
                ImVec2 maxPt{std::max(a.x, b.x), std::max(a.y, b.y)};
                dl->AddRectFilled(minPt, maxPt, fillCol);
                dl->AddRect(minPt, maxPt, lineCol, 0.f, 0, 2.0f);
            }
        }
    }

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

        auto containsPickPoint = [&](ECS::Entity e) {
            if (!ecs.isValid(e) || m_hiddenEntities.count(e.id)) return false;

            auto* t = ecs.getComponent<Components::Transform>(e);
            if (!t) return false;

            vec2 center{t->position.x, t->position.y};
            if (auto* sr = ecs.getComponent<Components::SpriteRenderer>(e)) {
                if (!sr->visible) return false;
                // Axis-aligned bounds test (ignores rotation, good enough for picking)
                float hw = (sr->size.x * std::abs(t->scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t->scale.y)) * 0.5f;
                return (worldPos.x >= center.x - hw && worldPos.x <= center.x + hw &&
                        worldPos.y >= center.y - hh && worldPos.y <= center.y + hh);
            }

            float d = glm::length(glm::vec2(worldPos.x - center.x, worldPos.y - center.y));
            return d < 60.0f;
        };

        struct PickCandidate {
            ECS::Entity entity{0, 0};
            bool hasSprite{false};
            int sortOrder{std::numeric_limits<int>::min()};
            float distance{std::numeric_limits<float>::max()};
        };

        std::vector<PickCandidate> pickCandidates;
        ecs.each<Components::Transform>([&](ECS::Entity e, Components::Transform& t) {
            if (m_hiddenEntities.count(e.id)) return; // skip hidden entities

            vec2 center{t.position.x, t.position.y};
            if (auto* sr = ecs.getComponent<Components::SpriteRenderer>(e)) {
                if (!sr->visible) return;
                float hw = (sr->size.x * std::abs(t.scale.x)) * 0.5f;
                float hh = (sr->size.y * std::abs(t.scale.y)) * 0.5f;
                bool inside = (worldPos.x >= center.x - hw && worldPos.x <= center.x + hw &&
                               worldPos.y >= center.y - hh && worldPos.y <= center.y + hh);
                if (inside) {
                    pickCandidates.push_back({e, true, sr->sortOrder, 0.0f});
                }
            } else {
                float d = glm::length(glm::vec2(worldPos.x - center.x, worldPos.y - center.y));
                if (d < 60.0f) {
                    pickCandidates.push_back({e, false, std::numeric_limits<int>::min(), d});
                }
            }
        });

        std::stable_sort(pickCandidates.begin(), pickCandidates.end(),
            [](const PickCandidate& a, const PickCandidate& b) {
                if (a.hasSprite != b.hasSprite) return a.hasSprite > b.hasSprite;
                if (a.hasSprite && b.hasSprite) {
                    if (a.sortOrder != b.sortOrder) return a.sortOrder > b.sortOrder;
                    return a.entity.id > b.entity.id;
                }
                if (a.distance != b.distance) return a.distance < b.distance;
                return a.entity.id > b.entity.id;
            });

        ECS::Entity bestEnt{0, 0};
        if (!pickCandidates.empty()) {
            bestEnt = pickCandidates.front().entity;

            const float pickCycleThreshold = 8.0f;
            const double pickCycleCooldownSeconds = 3.0;
            bool repeatedPickArea = false;
            if (m_hasLastPickScreenPos) {
                float dx = mp.x - m_lastPickScreenPos.x;
                float dy = mp.y - m_lastPickScreenPos.y;
                repeatedPickArea = std::sqrt(dx * dx + dy * dy) <= pickCycleThreshold;
            }

            double secondsSinceLastPick = ImGui::GetTime() - m_lastPickTimeSeconds;
            bool canCyclePick = repeatedPickArea && secondsSinceLastPick <= pickCycleCooldownSeconds;

            if (canCyclePick && containsPickPoint(m_selectedEntity)) {
                for (size_t i = 0; i < pickCandidates.size(); ++i) {
                    const ECS::Entity candidate = pickCandidates[i].entity;
                    if (candidate.id == m_selectedEntity.id &&
                        candidate.generation == m_selectedEntity.generation) {
                        bestEnt = pickCandidates[(i + 1) % pickCandidates.size()].entity;
                        break;
                    }
                }
            }
        }

        bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                     ImGui::IsKeyDown(ImGuiKey_RightShift);
        bool alt = ImGui::IsKeyDown(ImGuiKey_LeftAlt) ||
                   ImGui::IsKeyDown(ImGuiKey_RightAlt);

        if (shift) {
            if (ecs.isValid(bestEnt)) {
                m_lastPickScreenPos = {mp.x, mp.y};
                m_hasLastPickScreenPos = true;
                m_lastPickTimeSeconds = ImGui::GetTime();
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
                m_lastPickScreenPos = {mp.x, mp.y};
                m_hasLastPickScreenPos = true;
                // Shift+click empty space: start additive box selection
                m_isBoxSelecting = true;
                m_boxSelectStart = {mp.x, mp.y};
                m_boxSelectEnd   = {mp.x, mp.y};
            }
        } else {
            bool handledPlainClick = false;
            if (alt &&
                ecs.isValid(bestEnt) &&
                ecs.isValid(m_selectedEntity) &&
                bestEnt.id == m_selectedEntity.id &&
                bestEnt.generation == m_selectedEntity.generation) {
                EntitySnapshot snap = EntitySnapshot::capture(ecs, m_selectedEntity);
                auto cmd = std::make_unique<CreateEntityCmd>(ecs, snap);
                auto* rawCmd = cmd.get();
                executeCommand(std::move(cmd));

                ECS::Entity clonedEntity = rawCmd->createdEntity();
                m_selectedEntities.clear();
                m_selectedEntity = clonedEntity;
                if (ecs.isValid(clonedEntity))
                    m_selectedEntities.push_back(clonedEntity);

                m_lastPickScreenPos = {mp.x, mp.y};
                m_hasLastPickScreenPos = true;
                m_lastPickTimeSeconds = ImGui::GetTime();

                if (ecs.isValid(clonedEntity)) {
                    m_pendingEntityDrag = true;
                    m_isDraggingEntity  = false;
                    m_dragStartMousePos = {mp.x, mp.y};
                    m_dragClickWorldPos = worldPos;
                    m_gizmoGroupOriginalPos.clear();
                    if (auto* t = ecs.getComponent<Components::Transform>(clonedEntity))
                        m_gizmoGroupOriginalPos.push_back({clonedEntity, {t->position.x, t->position.y}});
                }
                handledPlainClick = true;
            }

            if (!handledPlainClick) {
            // Check if clicking an already-selected entity
            bool alreadySelected = ecs.isValid(bestEnt) &&
                std::any_of(m_selectedEntities.begin(), m_selectedEntities.end(),
                    [&](ECS::Entity e){ return e.id == bestEnt.id && e.generation == bestEnt.generation; });

            m_lastPickScreenPos = {mp.x, mp.y};
            m_hasLastPickScreenPos = true;
            m_lastPickTimeSeconds = ImGui::GetTime();

            if (!alreadySelected) {
                // New entity ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â clear selection and select this one
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
                // Clicked empty space ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â start box selection
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
            // Box too small = just a click on empty space ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ selection stays cleared
        }
    }

    // ----- Drag-drop: texture / prefab from browsers ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¾ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ spawn entity -----
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
            std::string texName = std::filesystem::path(static_cast<const char*>(pl->Data)).stem().string();
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

    std::vector<ECS::Entity> transformSelection;
    std::unordered_set<uint32_t> seenTransformIds;
    for (const auto& e : m_selectedEntities) {
        if (!world.isValid(e)) continue;
        if (!seenTransformIds.insert(e.id).second) continue;
        if (world.getComponent<Components::Transform>(e))
            transformSelection.push_back(e);
    }
    if (transformSelection.empty())
        transformSelection.push_back(m_selectedEntity);

    vec2 gizmoPivot = trans->position;
    if (m_gizmoMode == GizmoMode::Rotation && transformSelection.size() > 1) {
        gizmoPivot = {0.f, 0.f};
        for (const auto& e : transformSelection) {
            if (auto* selectionTransform = world.getComponent<Components::Transform>(e))
                gizmoPivot += selectionTransform->position;
        }
        gizmoPivot /= static_cast<float>(transformSelection.size());
    }

    auto&       cam    = Core::Engine::Get().camera();
    vec2        camPos = {cam.position().x, cam.position().y};
    float       zoom   = cam.zoom();

    vec2 objScreen = worldToScreen(
        gizmoPivot, camPos, zoom,
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

        // Start drag ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â capture group original positions
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

        // Drag ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â move primary and apply same delta to group
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
            m_gizmoGroupOriginalPos.clear();
            m_gizmoGroupOriginalRot.clear();
            for (const auto& e : transformSelection) {
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    m_gizmoGroupOriginalPos.push_back({e, t->position});
                    m_gizmoGroupOriginalRot.push_back({e, t->rotation});
                }
            }
        }

        if (mouseDown && m_gizmoActiveAxis == 2) {
            float currentAngle = std::atan2(mousePos.y - origin.y,
                                             mousePos.x - origin.x);
            float delta = currentAngle - m_gizmoClickOffset.x;
            vec2 pivot{0.f, 0.f};
            if (!m_gizmoGroupOriginalPos.empty()) {
                for (const auto& [_, pos] : m_gizmoGroupOriginalPos)
                    pivot += pos;
                pivot /= static_cast<float>(m_gizmoGroupOriginalPos.size());
            }
            const float c = std::cos(delta);
            const float s = std::sin(delta);
            for (size_t i = 0; i < m_gizmoGroupOriginalPos.size(); ++i) {
                const auto& e = m_gizmoGroupOriginalPos[i].first;
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    const vec2 origPos = m_gizmoGroupOriginalPos[i].second;
                    const vec2 offset = origPos - pivot;
                    t->position = {
                        pivot.x + offset.x * c - offset.y * s,
                        pivot.y + offset.x * s + offset.y * c
                    };
                    if (i < m_gizmoGroupOriginalRot.size())
                        t->rotation = m_gizmoGroupOriginalRot[i].second + delta;
                }
            }
        }

        if (releasedAxis == 2) {
            auto rotateSelectionCmd = std::make_unique<CompositeEditorCmd>("Rotate Selection");
            for (size_t i = 0; i < m_gizmoGroupOriginalPos.size(); ++i) {
                const auto& e = m_gizmoGroupOriginalPos[i].first;
                if (auto* t = world.getComponent<Components::Transform>(e)) {
                    const vec2 origPos = m_gizmoGroupOriginalPos[i].second;
                    if (t->position.x != origPos.x || t->position.y != origPos.y) {
                        rotateSelectionCmd->add(std::make_unique<MoveEntityCmd>(
                            world, e, origPos, t->position));
                    }
                    if (i < m_gizmoGroupOriginalRot.size()) {
                        const float origRot = m_gizmoGroupOriginalRot[i].second;
                        if (t->rotation != origRot) {
                            rotateSelectionCmd->add(std::make_unique<RotateEntityCmd>(
                                world, e, origRot, t->rotation));
                        }
                    }
                }
            }
            executeCommand(std::move(rotateSelectionCmd));
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

        // Start drag ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â capture group original scales
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

        // Drag ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â project mouse movement onto axis, apply as scale delta
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
            continue; // primary ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â already shown by gizmo or green outline elsewhere
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

    {
        auto dropped = Core::Engine::Get().window().consumeDroppedFiles();
        std::vector<std::string> unhandledDrops;
        for (const auto& filePath : dropped) {
            std::string ext = fs::path(filePath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            bool handled = false;
            static const char* kImgExts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
            for (auto e : kImgExts) {
                if (ext == e) {
                    std::string stem = fs::path(filePath).stem().string();
                    // Import into CURRENT directory
                    Assets::TextureManager::ImportTexture(filePath.c_str(), stem.c_str(),
                                                         "assets/configs/Textures.json",
                                                         m_currentAssetDir);
                    invalidateBrowserCaches();
                    handled = true;
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
                handled = true;
            }
            if (!handled) {
                unhandledDrops.push_back(filePath);
            }
        }
        auto& remainingDrops = Core::Engine::Get().window().droppedFilesMutable();
        remainingDrops.insert(remainingDrops.end(), unhandledDrops.begin(), unhandledDrops.end());
    }

    if (m_currentAssetDir != assetsRoot.generic_string()) {
        if (ImGui::Button("<- Back##assets")) {
            m_currentAssetDir = fs::path(m_currentAssetDir).parent_path().generic_string();
            m_selectedAssetPaths.clear();
            m_selectedAssetFolderPath.clear();
            m_assetBrowserCache.dirty = true;
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentAssetDir, assetsRoot, ec).generic_string().c_str());
    ImGui::TextDisabled("Path:");
    ImGui::SameLine();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", fs::absolute(m_currentAssetDir, ec).generic_string().c_str());
    ImGui::PopTextWrapPos();
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
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##AssetSearch", "Search asset ID...", m_assetSearchBuf, sizeof(m_assetSearchBuf));
    ImGui::SameLine();
    ImGui::Checkbox("Include Folders##AssetSearch", &m_assetSearchIncludeFolders);
    if (!m_selectedAssetPaths.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%zu selected", m_selectedAssetPaths.size());
        ImGui::SameLine();
        if (ImGui::Button("Move Selected##assets")) {
            m_movingAssetPaths = m_selectedAssetPaths;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected##assets")) {
            m_deletingAssetPaths = m_selectedAssetPaths;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Selection##assets")) {
            m_selectedAssetPaths.clear();
            m_selectedAssetFolderPath.clear();
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_F2, false) &&
        !ImGui::IsPopupOpen("##RenameAsset") &&
        !ImGui::IsPopupOpen("##RenameAssetDir")) {
        if (m_selectedAssetPaths.size() == 1) {
            fs::path selectedPath(m_selectedAssetPaths.front());
            m_renamingAsset = selectedPath.stem().string();
            strncpy_s(m_renameAssetBuf, m_renamingAsset.c_str(), sizeof(m_renameAssetBuf) - 1);
        } else if (!m_selectedAssetFolderPath.empty()) {
            fs::path selectedFolder(m_selectedAssetFolderPath);
            m_renamingAssetDirPath = m_selectedAssetFolderPath;
            strncpy_s(m_renameAssetDirBuf, selectedFolder.filename().string().c_str(),
                      sizeof(m_renameAssetDirBuf) - 1);
        }
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
            Assets::TextureManager::ImportTexture(m_newAssetPathBuf, m_newAssetNameBuf,
                                                 "assets/configs/Textures.json",
                                                 m_currentAssetDir);
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAsset.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (!m_deletingAssetPaths.empty()) ImGui::OpenPopup("##DeleteAsset");
    if (ImGui::BeginPopupModal("##DeleteAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_deletingAssetPaths.size() == 1) {
            ImGui::Text("Delete '%s' from disk?", fs::path(m_deletingAssetPaths.front()).stem().string().c_str());
        } else {
            ImGui::Text("Delete %zu assets from disk?", m_deletingAssetPaths.size());
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            for (const auto& assetPath : m_deletingAssetPaths) {
                Assets::TextureManager::DeleteFromDisk(fs::path(assetPath).stem().string());
            }
            for (const auto& assetPath : m_deletingAssetPaths) {
                m_selectedAssetPaths.erase(
                    std::remove(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), assetPath),
                    m_selectedAssetPaths.end());
            }
            m_deletingAssetPaths.clear();
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAssetPaths.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAssetDirPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAssetDirPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Move asset modal
    if (!m_movingAssetPaths.empty()) {
        ImGui::OpenPopup("##MoveAsset");
    }
    if (ImGui::BeginPopupModal("##MoveAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_movingAssetPaths.size() == 1) {
            const std::string movingAssetName = fs::path(m_movingAssetPaths.front()).filename().string();
            ImGui::Text("Move '%s' to:", movingAssetName.c_str());
        } else {
            ImGui::Text("Move %zu assets to:", m_movingAssetPaths.size());
        }
        ImGui::Separator();

        std::vector<std::string> assetFolders;
        assetFolders.push_back("(root)");
        for (const auto& entry : fs::recursive_directory_iterator(assetsRoot, ec)) {
            if (entry.is_directory()) {
                assetFolders.push_back(fs::relative(entry.path(), assetsRoot, ec).generic_string());
            }
        }

        for (const auto& folder : assetFolders) {
            if (ImGui::Selectable(folder.c_str())) {
                fs::path destDir = (folder == "(root)") ? assetsRoot : (assetsRoot / folder);
                for (const auto& movingAssetPath : m_movingAssetPaths) {
                    fs::path destFile = destDir / fs::path(movingAssetPath).filename();
                    if (!fs::exists(destFile)) {
                        fs::rename(fs::path(movingAssetPath), destFile, ec);
                    }
                }
                m_movingAssetPaths.clear();
                invalidateBrowserCaches();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_movingAssetPaths.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    auto isTextureFile = [](const fs::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
    };
    auto toLower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    const std::string searchQuery = toLower(m_assetSearchBuf);
    const bool searching = !searchQuery.empty();

    if (searching) {
        const bool searchSettingsChanged =
            m_assetSearchCache.dirty ||
            m_assetSearchCache.query != searchQuery ||
            m_assetSearchCache.includeFolders != m_assetSearchIncludeFolders;
        if (searchSettingsChanged) {
            std::vector<fs::path> searchResults;
            std::unordered_map<std::string, int> searchResultCountsByAssetId;

            for (const auto& entry : fs::recursive_directory_iterator(assetsRoot, ec)) {
                const fs::path& path = entry.path();
                const bool isDir = entry.is_directory();
                const bool isTexture = entry.is_regular_file() && isTextureFile(path);
                if ((isDir && !m_assetSearchIncludeFolders) || (!isDir && !isTexture)) {
                    continue;
                }

                const std::string fileName = toLower(path.filename().string());
                const std::string assetId = toLower(path.stem().string());
                const std::string relativePath = toLower(fs::relative(path, assetsRoot, ec).generic_string());
                if (fileName.find(searchQuery) != std::string::npos ||
                    assetId.find(searchQuery) != std::string::npos ||
                    relativePath.find(searchQuery) != std::string::npos) {
                    searchResults.push_back(path);
                    if (!isDir)
                        ++searchResultCountsByAssetId[assetId];
                }
            }

            std::sort(searchResults.begin(), searchResults.end(),
                [](const fs::path& a, const fs::path& b) {
                    const bool aDir = fs::is_directory(a);
                    const bool bDir = fs::is_directory(b);
                    if (aDir != bDir)
                        return aDir;
                    return a.generic_string() < b.generic_string();
                });

            std::unordered_set<std::string> seenAssetIds;
            std::vector<fs::path> dedupedResults;
            dedupedResults.reserve(searchResults.size());
            for (const auto& result : searchResults) {
                if (fs::is_directory(result)) {
                    dedupedResults.push_back(result);
                    continue;
                }

                const std::string assetId = toLower(result.stem().string());
                if (seenAssetIds.insert(assetId).second)
                    dedupedResults.push_back(result);
            }

            m_assetSearchCache.query = searchQuery;
            m_assetSearchCache.includeFolders = m_assetSearchIncludeFolders;
            m_assetSearchCache.items = std::move(dedupedResults);
            m_assetSearchCache.duplicateCountsByAssetId = std::move(searchResultCountsByAssetId);
            m_assetSearchCache.dirty = false;
        }
    }

    std::vector<fs::path> items;
    const auto& searchResultCountsByAssetId = m_assetSearchCache.duplicateCountsByAssetId;
    if (searching) {
        items = m_assetSearchCache.items;
    } else {
        items = RefreshDirectoryCache(
            m_assetBrowserCache,
            m_currentAssetDir,
            ImGui::GetTime(),
            BROWSER_CACHE_REFRESH_INTERVAL,
            [&](const fs::path& path) {
                if (fs::is_directory(path)) return true;
                return isTextureFile(path);
            });
    }

    std::vector<fs::path> folderItems;
    std::vector<fs::path> assetItems;
    folderItems.reserve(items.size());
    assetItems.reserve(items.size());
    for (const auto& path : items) {
        if (fs::is_directory(path))
            folderItems.push_back(path);
        else
            assetItems.push_back(path);
    }

    if (searching) {
        ImGui::TextDisabled("Search results: %zu", items.size());
    }

    float thumbSize = 64.0f;
    float padding = 8.0f;
    float cellSize = thumbSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelWidth / cellSize));

    auto renderAssetGroup = [&](const char* tableId, const char* label, const std::vector<fs::path>& groupItems) {
        ImGui::SeparatorText(label);
        if (groupItems.empty()) {
            ImGui::TextDisabled("No %s.", toLower(label).c_str());
            return;
        }

        auto moveAssetsToFolder = [&](const fs::path& destFolder, const fs::path& draggedPath) {
            std::vector<fs::path> sources;
            const std::string draggedPathString = draggedPath.generic_string();
            const bool dragFromSelection =
                std::find(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), draggedPathString) != m_selectedAssetPaths.end();
            if (dragFromSelection && m_selectedAssetPaths.size() > 1) {
                for (const auto& selectedPath : m_selectedAssetPaths)
                    sources.emplace_back(selectedPath);
            } else {
                sources.push_back(draggedPath);
            }

            for (const auto& src : sources) {
                fs::path destPath = destFolder / src.filename();
                if (!fs::exists(destPath))
                    fs::rename(src, destPath, ec);
            }
            invalidateBrowserCaches();
        };

        if (ImGui::BeginTable(tableId, cols)) {
            for (const auto& path : groupItems) {
                ImGui::TableNextColumn();
                std::string fname = path.filename().string();
                std::string stem = path.stem().string();
                bool isDir = fs::is_directory(path);
                const std::string itemId = path.generic_string();
                ImGui::PushID(itemId.c_str());

                if (isDir) {
                    const std::string folderPathString = path.generic_string();
                    const bool isSelectedFolder = (m_selectedAssetFolderPath == folderPathString);
                    if (isSelectedFolder)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
                    ImGui::Button("##icon", ImVec2(thumbSize, thumbSize));
                    ImGui::PopStyleColor();
                    if (isSelectedFolder)
                        ImGui::PopStyleColor();
                    DrawFolderIcon();
                    if (ImGui::IsItemClicked()) {
                        m_selectedAssetFolderPath = folderPathString;
                        m_selectedAssetPaths.clear();
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        m_currentAssetDir = path.generic_string();
                        m_selectedAssetPaths.clear();
                        m_selectedAssetFolderPath.clear();
                        m_assetBrowserCache.dirty = true;
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payloadAsset = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
                            fs::path src((const char*)payloadAsset->Data);
                            if (!src.empty()) {
                                moveAssetsToFolder(path, src);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::BeginPopupContextItem()) {
                        m_selectedAssetFolderPath = folderPathString;
                        m_selectedAssetPaths.clear();
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
                        // Automatically import if it's not in config but exists on disk in assets/textures
                        if (Assets::TextureManager::GetFilePath(stem).empty()) {
                            Assets::TextureManager::ImportTexture(path.generic_string(), stem,
                                                                 "assets/configs/Textures.json",
                                                                 path.parent_path().generic_string());
                        } else {
                            Assets::TextureManager::LoadTexture(stem, path.generic_string());
                        }
                        tex = Assets::TextureManager::Get(stem);
                    }

                    const std::string pathString = path.generic_string();
                    bool isSelected = std::find(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), pathString) != m_selectedAssetPaths.end();
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                    if (ImGui::ImageButton("##tb", (ImTextureID)(intptr_t)(tex?tex->id():0), ImVec2(thumbSize, thumbSize), ImVec2(0,1), ImVec2(1,0))) {
                        m_selectedAsset = stem;
                        m_selectedAssetFolderPath.clear();
                        if (ImGui::GetIO().KeyShift) {
                            if (isSelected) {
                                m_selectedAssetPaths.erase(
                                    std::remove(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), pathString),
                                    m_selectedAssetPaths.end());
                            } else {
                                m_selectedAssetPaths.push_back(pathString);
                            }
                        } else {
                            m_selectedAssetPaths.clear();
                            m_selectedAssetPaths.push_back(pathString);
                        }
                        if (searching) {
                            m_currentAssetDir = path.parent_path().generic_string();
                            m_assetBrowserCache.dirty = true;
                        }
                    }
                    if (isSelected) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) {
                        const std::string relativePath = fs::relative(path, assetsRoot, ec).generic_string();
                        ImGui::SetTooltip("%s\n%s", stem.c_str(), relativePath.c_str());
                    }

                    if (ImGui::BeginDragDropSource()) {
                        std::string pstr = path.generic_string();
                        ImGui::SetDragDropPayload("TEXTURE_ASSET", pstr.c_str(), pstr.size() + 1);
                        ImGui::Text("%s", stem.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        m_selectedAssetFolderPath.clear();
                        const bool hasMultiSelection = isSelected && m_selectedAssetPaths.size() > 1;
                        if (ImGui::MenuItem("Apply to selected")) {
                            if (auto* sr = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(m_selectedEntity))
                                sr->textureId = stem;
                        }
                        if (ImGui::MenuItem("Rename")) { m_renamingAsset = stem; strncpy_s(m_renameAssetBuf, stem.c_str(), sizeof(m_renameAssetBuf)-1); }
                        if (ImGui::MenuItem(hasMultiSelection ? "Move Selected to..." : "Move to...")) {
                            if (hasMultiSelection)
                                m_movingAssetPaths = m_selectedAssetPaths;
                            else
                                m_movingAssetPaths = { pathString };
                        }
                        if (ImGui::MenuItem("Duplicate")) {
                            fs::path src = path;
                            fs::path dst = src.parent_path() / (src.stem().string() + "_copy" + src.extension().string());
                            int n = 1;
                            while (fs::exists(dst))
                                dst = src.parent_path() / (src.stem().string() + "_copy" + std::to_string(n++) + src.extension().string());
                            fs::copy_file(src, dst, ec);
                            invalidateBrowserCaches();
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(hasMultiSelection ? "Delete Selected" : "Delete")) {
                            if (hasMultiSelection)
                                m_deletingAssetPaths = m_selectedAssetPaths;
                            else
                                m_deletingAssetPaths = { pathString };
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::TextUnformatted(fname.c_str());
                if (searching) {
                    const std::string relativePath = fs::relative(path, assetsRoot, ec).generic_string();
                    ImGui::TextDisabled("%s", relativePath.c_str());
                    if (!isDir) {
                        const auto countIt = searchResultCountsByAssetId.find(toLower(stem));
                        if (countIt != searchResultCountsByAssetId.end() && countIt->second > 1) {
                            ImGui::TextDisabled("(+%d duplicates)", countIt->second - 1);
                        }
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    };

    renderAssetGroup("##AssetFolderTbl", searching ? "Matching Folders" : "Folders", folderItems);
    renderAssetGroup("##AssetFileTbl", searching ? "Matching Assets" : "Assets", assetItems);
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

    if (m_currentAnimDir != animRoot.generic_string()) {
        if (ImGui::Button("<- Back##anim")) {
            m_currentAnimDir = fs::path(m_currentAnimDir).parent_path().generic_string();
            m_animBrowserCache.dirty = true;
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
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAnimPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::Separator();


    const auto& items = RefreshDirectoryCache(
        m_animBrowserCache,
        m_currentAnimDir,
        ImGui::GetTime(),
        BROWSER_CACHE_REFRESH_INTERVAL,
        [&](const fs::path& path) {
            return fs::is_directory(path) || path.extension() == ".json";
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
                if (ImGui::Button("##icon", ImVec2(kIconW, kIconH))) {
                    m_currentAnimDir = path.generic_string();
                    m_animBrowserCache.dirty = true;
                }
                ImGui::PopStyleColor();
                DrawFolderIcon();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ANIMATION_ASSET")) {
                        fs::path src(animRoot / (const char*)pl->Data);
                        fs::rename(src, path / src.filename(), ec);
                        invalidateBrowserCaches();
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
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", fname.c_str());

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

    // â”€â”€ Top row: texture picker (left) + preview image (right) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // Preview image â€” top-right corner, same line as the group above
    ImGui::SameLine(ImGui::GetWindowWidth() - 148.f);
    if (m_animPreviewFB.getTextureID() != 0) {
        ImGui::Image(reinterpret_cast<void*>(static_cast<uint64_t>(
                         m_animPreviewFB.getTextureID())),
                     ImVec2(140.f, 140.f), ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
    } else {
        ImGui::Dummy(ImVec2(140.f, 140.f));
    }

    ImGui::Separator();

    // â”€â”€ Clip selection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Playback controls â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Frame list â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

            // â”€â”€ Selected frame editor â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Grid generator â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Bulk duration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    // â”€â”€ Save â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

    if (m_currentScriptDir != scriptsRoot.generic_string()) {
        if (ImGui::Button("<- Back")) {
            m_currentScriptDir = fs::path(m_currentScriptDir).parent_path().generic_string();
            m_scriptBrowserCache.dirty = true;
        }
        ImGui::SameLine();
    }
    ImGui::Text("Location: %s", fs::relative(m_currentScriptDir, scriptsRoot, ec).generic_string().c_str());
    ImGui::Separator();

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
                invalidateBrowserCaches();
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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_renamingScriptPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

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
                invalidateBrowserCaches();
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
            invalidateBrowserCaches();
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


    const auto& items = RefreshDirectoryCache(
        m_scriptBrowserCache,
        m_currentScriptDir,
        ImGui::GetTime(),
        BROWSER_CACHE_REFRESH_INTERVAL,
        [&](const fs::path& path) {
            return fs::is_directory(path) || path.extension() == ".lua";
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
                            invalidateBrowserCaches();
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            // Navigation / Opening
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (isDir) {
                    m_currentScriptDir = path.generic_string();
                    m_scriptBrowserCache.dirty = true;
                } else {
                    OpenInVSCode(fs::absolute(path).string());
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", isDir ? "Double-click to open folder" : fname.c_str());

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
                    invalidateBrowserCaches();
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

    if (m_pendingFontImportPopup) {
        m_pendingFontImportPopup = false;
        ImGui::OpenPopup("##AddFont");
    }

    if (ImGui::Button("Add Font")) {
        m_newFontNameBuf[0] = '\0';
        m_newFontPathBuf[0] = '\0';
        m_newFontSize = 32;
        ImGui::OpenPopup("##AddFont");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(drag .ttf/.otf here)");
    ImGui::Separator();

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
    const std::string kSoundConfig = "assets/configs/sounds.json";

    {
        auto dropped = Core::Engine::Get().window().consumeDroppedFiles();
        std::vector<std::string> unhandledDrops;
        std::vector<std::string> importedSounds;
        for (const auto& filePath : dropped) {
            std::string ext = fs::path(filePath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            const bool isAudioDrop = (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac");
            if (!isAudioDrop) {
                unhandledDrops.push_back(filePath);
                continue;
            }

            fs::path sourcePath(filePath);
            std::string baseName = sourcePath.stem().string();
            if (baseName.empty()) {
                baseName = "audio";
            }

            Assets::SoundConfig existingCfg;
            std::string uniqueName = baseName;
            int duplicateIndex = 1;
            while (Assets::SoundManager::GetConfig(uniqueName, existingCfg, kSoundConfig)) {
                uniqueName = baseName + "_" + std::to_string(duplicateIndex++);
            }

            Assets::SoundConfig cfg;
            cfg.name = uniqueName;
            cfg.path = filePath;
            cfg.group = "SFX";

            if (!Assets::SoundManager::ImportSound(filePath, uniqueName, cfg, kSoundConfig)) {
                continue;
            }

            fs::path importedPath = audioRoot / sourcePath.filename();
            if (fs::exists(importedPath) && fs::path(m_currentAudioDir) != audioRoot) {
                fs::create_directories(fs::path(m_currentAudioDir), ec);
                fs::path destPath = fs::path(m_currentAudioDir) / sourcePath.filename();
                int pathDuplicateIndex = 1;
                while (fs::exists(destPath)) {
                    destPath = fs::path(m_currentAudioDir) /
                        (sourcePath.stem().string() + "_" + std::to_string(pathDuplicateIndex++) + sourcePath.extension().string());
                }
                fs::rename(importedPath, destPath, ec);
                if (!ec) {
                    Assets::SoundManager::UpdatePathInConfig(uniqueName, destPath.generic_string(), kSoundConfig);
                }
            }

            importedSounds.push_back(uniqueName);
        }

        if (!importedSounds.empty()) {
            m_selectedAudioAssets = importedSounds;
            invalidateBrowserCaches();
        }

        auto& remainingDrops = Core::Engine::Get().window().droppedFilesMutable();
        remainingDrops.insert(remainingDrops.end(), unhandledDrops.begin(), unhandledDrops.end());
    }

    ImGui::Text("Location: %s", fs::relative(m_currentAudioDir, audioRoot, ec).generic_string().c_str());
    ImGui::Separator();

    struct MusicContextEditorEntry {
        char id[128]{};
        char tracks[512]{};
        float fadeOutSeconds{0.5f};
    };
    static std::vector<MusicContextEditorEntry> s_musicContextEntries;
    static bool s_musicContextsLoaded = false;
    auto reloadMusicContexts = [&]() {
        s_musicContextEntries.clear();
        for (const auto& ctx : Runtime::LevelMusicManager::GetContexts()) {
            MusicContextEditorEntry entry{};
            strncpy_s(entry.id, ctx.id.c_str(), sizeof(entry.id) - 1);
            std::string joinedTracks;
            for (size_t i = 0; i < ctx.layers.size(); ++i) {
                if (i > 0) {
                    joinedTracks += ", ";
                }
                joinedTracks += ctx.layers[i].track + ":" + std::to_string(ctx.layers[i].volume);
            }
            strncpy_s(entry.tracks, joinedTracks.c_str(), sizeof(entry.tracks) - 1);
            entry.fadeOutSeconds = ctx.fadeOutSeconds;
            s_musicContextEntries.push_back(entry);
        }
        s_musicContextsLoaded = true;
    };
    if (!s_musicContextsLoaded) {
        reloadMusicContexts();
    }
    auto loadBatchAudioFromSelection = [&]() {
        m_batchApplyLoop = false;
        m_batchApplyStream = false;
        m_batchApplyIs3D = false;
        m_batchApplyGroup = false;
        m_batchApplyVolume = false;
        m_batchApplyPitch = false;
        m_batchApplyMinDistance = false;
        m_batchApplyMaxDistance = false;
        m_batchApplyFalloffStart = false;
        m_batchApplyFalloffAmount = false;
        m_batchApplyFilterInStart = false;
        m_batchApplyFilterPeak = false;
        m_batchApplyFilterReleaseEnd = false;

        if (m_selectedAudioAssets.empty()) {
            return;
        }

        Assets::SoundConfig cfg;
        if (!Assets::SoundManager::GetConfig(m_selectedAudioAssets.front(), cfg, kSoundConfig)) {
            return;
        }

        m_batchAudioLoop = cfg.loop;
        m_batchAudioStream = cfg.stream;
        m_batchAudioIs3D = cfg.is3D;
        m_batchAudioIsBGM = (cfg.group == "BGM");
        m_batchAudioVolume = cfg.volume;
        m_batchAudioPitch = cfg.pitch;
        m_batchAudioMinDistance = cfg.minDistance;
        m_batchAudioMaxDistance = cfg.maxDistance;
        m_batchAudioFalloffStart = cfg.falloffStart;
        m_batchAudioFalloffAmount = cfg.falloffAmount;
        m_batchAudioFilterInStart = cfg.filterInStart;
        m_batchAudioFilterPeak = cfg.filterPeak;
        m_batchAudioFilterReleaseEnd = cfg.filterReleaseEnd;
    };
    if (ImGui::CollapsingHeader("Music Contexts", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Reload Contexts##musicctx")) {
            reloadMusicContexts();
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Context##musicctx")) {
            s_musicContextEntries.push_back({});
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Contexts##musicctx")) {
            std::vector<Runtime::MusicContextConfig> contexts;
            contexts.reserve(s_musicContextEntries.size());
            for (const auto& entry : s_musicContextEntries) {
                Runtime::MusicContextConfig ctx;
                ctx.id = entry.id;

                const std::string rawTracks = entry.tracks;
                size_t start = 0;
                while (start < rawTracks.size()) {
                    const size_t end = rawTracks.find_first_of(",\n\r", start);
                    std::string token = rawTracks.substr(
                        start, end == std::string::npos ? std::string::npos : end - start);
                    const size_t first = token.find_first_not_of(" \t");
                    const size_t last = token.find_last_not_of(" \t");
                    if (first != std::string::npos && last != std::string::npos) {
                        std::string layerText = token.substr(first, last - first + 1);
                        float layerVolume = 1.0f;
                        const size_t volumeSep = layerText.find(':');
                        if (volumeSep != std::string::npos) {
                            const std::string volumeText = layerText.substr(volumeSep + 1);
                            layerText = layerText.substr(0, volumeSep);
                            try {
                                layerVolume = std::clamp(std::stof(volumeText), 0.0f, 1.0f);
                            } catch (...) {
                                layerVolume = 1.0f;
                            }
                        }
                        ctx.layers.push_back({layerText, layerVolume});
                    }
                    if (end == std::string::npos) {
                        break;
                    }
                    start = end + 1;
                }

                ctx.fadeOutSeconds = std::max(0.0f, entry.fadeOutSeconds);
                if (!ctx.id.empty()) {
                    contexts.push_back(std::move(ctx));
                }
            }
            if (Runtime::LevelMusicManager::SetContexts(contexts)) {
                reloadMusicContexts();
            }
        }

        for (size_t i = 0; i < s_musicContextEntries.size(); ++i) {
            auto& entry = s_musicContextEntries[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::InputText("Context ID", entry.id, sizeof(entry.id));
            ImGui::InputText("Track Layers", entry.tracks, sizeof(entry.tracks));
            ImGui::TextDisabled("Format: track_id:volume, track_id:volume");
            ImGui::SliderFloat("Fade Out (s)", &entry.fadeOutSeconds, 0.0f, 3.0f);
            if (ImGui::Button("Remove##musicctx")) {
                s_musicContextEntries.erase(s_musicContextEntries.begin() + static_cast<std::ptrdiff_t>(i));
                ImGui::PopID();
                break;
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
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
    if (m_currentAudioDir != audioRoot.generic_string()) {
        ImGui::SameLine();
        if (ImGui::Button("<- Back##audio")) {
            m_currentAudioDir = fs::path(m_currentAudioDir).parent_path().generic_string();
            m_selectedAudioAssets.clear();
            m_audioBrowserCache.dirty = true;
        }
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
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAudio.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (!m_editingAudio.empty()) ImGui::OpenPopup("##EditAudio");
    if (ImGui::BeginPopupModal("##EditAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Edit Audio: %s", m_editingAudio.c_str());
        ImGui::Separator();
        ImGui::InputText("Name##editaudio", m_editAudioNameBuf, sizeof(m_editAudioNameBuf));
        ImGui::InputText("Path##editaudio", m_editAudioPathBuf, sizeof(m_editAudioPathBuf));
        ImGui::Checkbox("Loop##editaudio", &m_editAudioLoop); ImGui::SameLine();
        ImGui::Checkbox("Stream##editaudio", &m_editAudioStream); ImGui::SameLine();
        ImGui::Checkbox("3D##editaudio", &m_editAudioIs3D);
        ImGui::Checkbox("BGM##editaudio", &m_editAudioIsBGM);
        ImGui::SliderFloat("Volume##editaudio", &m_editAudioVolume, 0.0f, 1.0f);
        ImGui::SliderFloat("Pitch##editaudio", &m_editAudioPitch, 0.1f, 3.0f);
        if (m_editAudioIs3D) {
            ImGui::SeparatorText("3D Tuning");
            ImGui::SliderFloat("Min Distance##editaudio", &m_editAudioMinDistance, 1.0f, 256.0f);
            ImGui::SliderFloat("Max Distance##editaudio", &m_editAudioMaxDistance, 8.0f, 1024.0f);
            ImGui::SliderFloat("Falloff Start##editaudio", &m_editAudioFalloffStart, 0.0f, 512.0f);
            ImGui::SliderFloat("Falloff Amount##editaudio", &m_editAudioFalloffAmount, 0.0f, 0.98f);
            ImGui::SliderFloat("Filter In Start##editaudio", &m_editAudioFilterInStart, 0.0f, 512.0f);
            ImGui::SliderFloat("Filter Peak##editaudio", &m_editAudioFilterPeak, 1.0f, 768.0f);
            ImGui::SliderFloat("Filter Release End##editaudio", &m_editAudioFilterReleaseEnd, 2.0f, 1024.0f);
        }

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            Assets::SoundConfig cfg;
            cfg.name = m_editAudioNameBuf;
            cfg.path = m_editAudioPathBuf;
            cfg.loop = m_editAudioLoop;
            cfg.stream = m_editAudioStream;
            cfg.is3D = m_editAudioIs3D;
            cfg.volume = m_editAudioVolume;
            cfg.pitch = m_editAudioPitch;
            cfg.group = m_editAudioIsBGM ? "BGM" : "SFX";
            cfg.minDistance = m_editAudioMinDistance;
            cfg.maxDistance = std::max(m_editAudioMinDistance + 1.0f, m_editAudioMaxDistance);
            cfg.falloffStart = std::clamp(m_editAudioFalloffStart, 0.0f, cfg.maxDistance - 1.0f);
            cfg.falloffAmount = std::clamp(m_editAudioFalloffAmount, 0.0f, 0.98f);
            cfg.filterInStart = std::clamp(m_editAudioFilterInStart, 0.0f, cfg.maxDistance - 1.0f);
            cfg.filterPeak = std::clamp(m_editAudioFilterPeak, cfg.filterInStart + 1.0f, cfg.maxDistance - 0.5f);
            cfg.filterReleaseEnd = std::clamp(m_editAudioFilterReleaseEnd, cfg.filterPeak + 1.0f, cfg.maxDistance);
            if (Assets::SoundManager::UpdateConfig(m_editingAudio, cfg, kSoundConfig)) {
                m_editingAudio.clear();
                invalidateBrowserCaches();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_editingAudio.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_openBatchEditAudioPopup && !m_selectedAudioAssets.empty()) {
        ImGui::OpenPopup("##BatchEditAudio");
        m_openBatchEditAudioPopup = false;
    }
    if (ImGui::BeginPopupModal("##BatchEditAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Edit %zu audio file(s)", m_selectedAudioAssets.size());
        ImGui::TextDisabled("Enable only the settings you want to apply to the whole selection.");
        ImGui::Separator();

        ImGui::Checkbox("Apply Loop##batch", &m_batchApplyLoop);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyLoop);
        ImGui::Checkbox("Loop##batchvalue", &m_batchAudioLoop);
        ImGui::EndDisabled();

        ImGui::Checkbox("Apply Stream##batch", &m_batchApplyStream);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyStream);
        ImGui::Checkbox("Stream##batchvalue", &m_batchAudioStream);
        ImGui::EndDisabled();

        ImGui::Checkbox("Apply 3D##batch", &m_batchApplyIs3D);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyIs3D);
        ImGui::Checkbox("3D##batchvalue", &m_batchAudioIs3D);
        ImGui::EndDisabled();

        ImGui::Checkbox("Apply Group##batch", &m_batchApplyGroup);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyGroup);
        ImGui::Checkbox("BGM Group##batchvalue", &m_batchAudioIsBGM);
        ImGui::EndDisabled();

        ImGui::Checkbox("Apply Volume##batch", &m_batchApplyVolume);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyVolume);
        ImGui::SliderFloat("Volume##batchvalue", &m_batchAudioVolume, 0.0f, 1.0f);
        ImGui::EndDisabled();

        ImGui::Checkbox("Apply Pitch##batch", &m_batchApplyPitch);
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_batchApplyPitch);
        ImGui::SliderFloat("Pitch##batchvalue", &m_batchAudioPitch, 0.1f, 3.0f);
        ImGui::EndDisabled();

        const bool show3DTuning = m_batchAudioIs3D || m_batchApplyMinDistance || m_batchApplyMaxDistance ||
                                  m_batchApplyFalloffStart || m_batchApplyFalloffAmount ||
                                  m_batchApplyFilterInStart || m_batchApplyFilterPeak ||
                                  m_batchApplyFilterReleaseEnd;
        if (show3DTuning) {
            ImGui::SeparatorText("3D Tuning");

            ImGui::Checkbox("Apply Min Distance##batch", &m_batchApplyMinDistance);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyMinDistance);
            ImGui::SliderFloat("Min Distance##batchvalue", &m_batchAudioMinDistance, 1.0f, 256.0f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Max Distance##batch", &m_batchApplyMaxDistance);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyMaxDistance);
            ImGui::SliderFloat("Max Distance##batchvalue", &m_batchAudioMaxDistance, 8.0f, 1024.0f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Falloff Start##batch", &m_batchApplyFalloffStart);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyFalloffStart);
            ImGui::SliderFloat("Falloff Start##batchvalue", &m_batchAudioFalloffStart, 0.0f, 512.0f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Falloff Amount##batch", &m_batchApplyFalloffAmount);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyFalloffAmount);
            ImGui::SliderFloat("Falloff Amount##batchvalue", &m_batchAudioFalloffAmount, 0.0f, 0.98f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Filter In Start##batch", &m_batchApplyFilterInStart);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyFilterInStart);
            ImGui::SliderFloat("Filter In Start##batchvalue", &m_batchAudioFilterInStart, 0.0f, 512.0f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Filter Peak##batch", &m_batchApplyFilterPeak);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyFilterPeak);
            ImGui::SliderFloat("Filter Peak##batchvalue", &m_batchAudioFilterPeak, 1.0f, 768.0f);
            ImGui::EndDisabled();

            ImGui::Checkbox("Apply Filter Release End##batch", &m_batchApplyFilterReleaseEnd);
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_batchApplyFilterReleaseEnd);
            ImGui::SliderFloat("Filter Release End##batchvalue", &m_batchAudioFilterReleaseEnd, 2.0f, 1024.0f);
            ImGui::EndDisabled();
        }

        if (ImGui::Button("Apply To Selected", ImVec2(140, 0))) {
            for (const auto& soundName : m_selectedAudioAssets) {
                Assets::SoundConfig cfg;
                if (!Assets::SoundManager::GetConfig(soundName, cfg, kSoundConfig)) {
                    continue;
                }

                if (m_batchApplyLoop) cfg.loop = m_batchAudioLoop;
                if (m_batchApplyStream) cfg.stream = m_batchAudioStream;
                if (m_batchApplyIs3D) cfg.is3D = m_batchAudioIs3D;
                if (m_batchApplyGroup) cfg.group = m_batchAudioIsBGM ? "BGM" : "SFX";
                if (m_batchApplyVolume) cfg.volume = m_batchAudioVolume;
                if (m_batchApplyPitch) cfg.pitch = m_batchAudioPitch;
                if (m_batchApplyMinDistance) cfg.minDistance = m_batchAudioMinDistance;
                if (m_batchApplyMaxDistance) cfg.maxDistance = m_batchAudioMaxDistance;
                if (m_batchApplyFalloffStart) cfg.falloffStart = m_batchAudioFalloffStart;
                if (m_batchApplyFalloffAmount) cfg.falloffAmount = m_batchAudioFalloffAmount;
                if (m_batchApplyFilterInStart) cfg.filterInStart = m_batchAudioFilterInStart;
                if (m_batchApplyFilterPeak) cfg.filterPeak = m_batchAudioFilterPeak;
                if (m_batchApplyFilterReleaseEnd) cfg.filterReleaseEnd = m_batchAudioFilterReleaseEnd;

                cfg.maxDistance = std::max(cfg.minDistance + 1.0f, cfg.maxDistance);
                cfg.falloffStart = std::clamp(cfg.falloffStart, 0.0f, cfg.maxDistance - 1.0f);
                cfg.falloffAmount = std::clamp(cfg.falloffAmount, 0.0f, 0.98f);
                cfg.filterInStart = std::clamp(cfg.filterInStart, 0.0f, cfg.maxDistance - 1.0f);
                cfg.filterPeak = std::clamp(cfg.filterPeak, cfg.filterInStart + 1.0f, cfg.maxDistance - 0.5f);
                cfg.filterReleaseEnd = std::clamp(cfg.filterReleaseEnd, cfg.filterPeak + 1.0f, cfg.maxDistance);
                Assets::SoundManager::UpdateConfig(soundName, cfg, kSoundConfig);
            }
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##batchaudio", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

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
                const std::string soundName = isAudioDir
                    ? std::string{}
                    : Assets::SoundManager::GetNameForPath(renamePath.generic_string(), kSoundConfig);
                fs::rename(renamePath, dst, ec);
                if (!isAudioDir && ec.value() == 0 && !soundName.empty())
                    Assets::SoundManager::UpdatePathInConfig(soundName, dst.generic_string(), kSoundConfig);
            }
            m_renamingAudioPath.clear();
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_renamingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

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
            invalidateBrowserCaches();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_deletingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    auto moveAudioFileToFolder = [&](const std::string& soundName, const fs::path& srcPath, const fs::path& destDir) {
        if (soundName.empty() || srcPath.empty() || destDir.empty()) {
            return;
        }

        std::error_code moveEc;
        fs::create_directories(destDir, moveEc);
        if (moveEc) {
            return;
        }

        const fs::path normalizedSrc = srcPath.lexically_normal();
        const fs::path normalizedDestDir = destDir.lexically_normal();
        if (normalizedSrc.parent_path() == normalizedDestDir) {
            return;
        }

        fs::path destFile = normalizedDestDir / normalizedSrc.filename();
        int duplicateIndex = 1;
        while (fs::exists(destFile)) {
            destFile = normalizedDestDir /
                (normalizedSrc.stem().string() + "_" + std::to_string(duplicateIndex++) + normalizedSrc.extension().string());
        }

        fs::rename(normalizedSrc, destFile, moveEc);
        if (!moveEc) {
            Assets::SoundManager::UpdatePathInConfig(soundName, destFile.generic_string(), kSoundConfig);
            invalidateBrowserCaches();
        }
    };

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
                const std::string soundName =
                    Assets::SoundManager::GetNameForPath(fs::path(m_movingAudioPath).generic_string(), kSoundConfig);
                moveAudioFileToFolder(soundName, fs::path(m_movingAudioPath), destDir);
                m_movingAudioPath.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) { m_movingAudioPath.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (m_openMoveSelectedAudioPopup && !m_selectedAudioAssets.empty()) {
        ImGui::OpenPopup("##MoveSelectedAudio");
        m_openMoveSelectedAudioPopup = false;
    }
    if (ImGui::BeginPopupModal("##MoveSelectedAudio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Move %zu selected audio file(s) to:", m_selectedAudioAssets.size());
        ImGui::Separator();
        std::vector<std::string> audioFolders;
        audioFolders.push_back("(root)");
        for (const auto& entry : fs::recursive_directory_iterator(audioRoot, ec))
            if (entry.is_directory())
                audioFolders.push_back(fs::relative(entry.path(), audioRoot, ec).generic_string());
        for (const auto& folder : audioFolders) {
            if (ImGui::Selectable(folder.c_str())) {
                fs::path destDir = (folder == "(root)") ? audioRoot : audioRoot / folder;
                for (const auto& soundName : m_selectedAudioAssets) {
                    std::string srcPath = Assets::SoundManager::GetFilePath(soundName, kSoundConfig);
                    if (srcPath.empty()) continue;
                    moveAudioFileToFolder(soundName, fs::path(srcPath), destDir);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel##moveselectedaudio", ImVec2(100, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    if (!m_selectedAudioAssets.empty()) {
        ImGui::Text("Selected: %zu", m_selectedAudioAssets.size());
        ImGui::SameLine();
        if (ImGui::Button("Edit Selected Settings##audio")) {
            loadBatchAudioFromSelection();
            m_openBatchEditAudioPopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Move Selected##audio")) {
            m_openMoveSelectedAudioPopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Selection##audio")) {
            m_selectedAudioAssets.clear();
        }
        ImGui::Separator();
    }

    const auto& items = RefreshDirectoryCache(
        m_audioBrowserCache,
        m_currentAudioDir,
        ImGui::GetTime(),
        BROWSER_CACHE_REFRESH_INTERVAL,
        [&](const fs::path& path) {
            if (fs::is_directory(path)) return true;
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
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
            std::string soundName = isDir
                ? std::string{}
                : Assets::SoundManager::GetNameForPath(path.generic_string(), kSoundConfig);
            const std::string& soundKey = soundName.empty() ? stem : soundName;
            ImGui::PushID(fname.c_str());

            float colW = ImGui::GetContentRegionAvail().x;
            float iconOX = (colW - kIconW) * 0.5f;
            if (iconOX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconOX);

            if (isDir) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
                ImGui::Button("##icon", ImVec2(kIconW, kIconH));
                ImGui::PopStyleColor();
                DrawFolderIcon();
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    m_currentAudioDir = path.generic_string();
                    m_selectedAudioAssets.clear();
                    m_audioBrowserCache.dirty = true;
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payloadFile = ImGui::AcceptDragDropPayload("AUDIO_ASSET_FILE")) {
                        fs::path src((const char*)payloadFile->Data);
                        const std::string movedName = Assets::SoundManager::GetNameForPath(src.generic_string(), kSoundConfig);
                        fs::path dest = path / src.filename();
                        fs::rename(src, dest, ec);
                        if (ec.value() == 0 && !movedName.empty())
                            Assets::SoundManager::UpdatePathInConfig(movedName, dest.generic_string(), kSoundConfig);
                        invalidateBrowserCaches();
                    } else if (const ImGuiPayload* payloadAsset = ImGui::AcceptDragDropPayload("AUDIO_ASSET")) {
                        std::string srcPath = Assets::SoundManager::GetFilePath((const char*)payloadAsset->Data, kSoundConfig);
                        if (!srcPath.empty()) {
                            fs::path src(srcPath);
                            fs::path dest = path / src.filename();
                            fs::rename(src, dest, ec);
                            if (ec.value() == 0)
                                Assets::SoundManager::UpdatePathInConfig((const char*)payloadAsset->Data, dest.generic_string(), kSoundConfig);
                            invalidateBrowserCaches();
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
                const bool isSelected = std::find(m_selectedAudioAssets.begin(), m_selectedAudioAssets.end(), soundKey) != m_selectedAudioAssets.end();
                ImVec4 buttonColor = isSelected
                    ? ImVec4(0.22f, 0.45f, 0.72f, 1.0f)
                    : ImVec4(0.12f, 0.25f, 0.18f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
                ImGui::Button("##icon", ImVec2(kIconW, kIconH));
                ImGui::PopStyleColor();
                DrawAudioIcon();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", stem.c_str());

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    const bool ctrlHeld = ImGui::GetIO().KeyCtrl;
                    if (ctrlHeld) {
                        auto selectedIt = std::find(m_selectedAudioAssets.begin(), m_selectedAudioAssets.end(), soundKey);
                        if (selectedIt != m_selectedAudioAssets.end()) {
                            m_selectedAudioAssets.erase(selectedIt);
                        } else {
                            m_selectedAudioAssets.push_back(soundKey);
                        }
                    } else {
                        m_selectedAudioAssets.clear();
                        m_selectedAudioAssets.push_back(soundKey);
                    }
                }

                if (ImGui::BeginDragDropSource()) {
                    std::string pstr = path.generic_string();
                    ImGui::SetDragDropPayload("AUDIO_ASSET_FILE", pstr.c_str(), pstr.size() + 1);
                    ImGui::SetDragDropPayload("AUDIO_ASSET", soundKey.c_str(), soundKey.size() + 1);
                    ImGui::Text("%s", soundKey.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) Assets::SoundManager::Play(soundKey);

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Play")) Assets::SoundManager::Play(soundKey);
                    if (ImGui::MenuItem("Stop")) Assets::SoundManager::Stop(soundKey);
                    if (std::find(m_selectedAudioAssets.begin(), m_selectedAudioAssets.end(), soundKey) != m_selectedAudioAssets.end() &&
                        m_selectedAudioAssets.size() > 1 && ImGui::MenuItem("Edit Selected Settings")) {
                        loadBatchAudioFromSelection();
                        m_openBatchEditAudioPopup = true;
                    }
                    if (std::find(m_selectedAudioAssets.begin(), m_selectedAudioAssets.end(), soundKey) != m_selectedAudioAssets.end() &&
                        m_selectedAudioAssets.size() > 1 && ImGui::MenuItem("Move Selected")) {
                        m_openMoveSelectedAudioPopup = true;
                    }
                    if (ImGui::MenuItem("Edit Settings")) {
                        Assets::SoundConfig cfg;
                        if (Assets::SoundManager::GetConfig(soundKey, cfg, kSoundConfig)) {
                            m_editingAudio = soundKey;
                            strncpy_s(m_editAudioNameBuf, cfg.name.c_str(), sizeof(m_editAudioNameBuf) - 1);
                            strncpy_s(m_editAudioPathBuf, cfg.path.c_str(), sizeof(m_editAudioPathBuf) - 1);
                            m_editAudioLoop = cfg.loop;
                            m_editAudioStream = cfg.stream;
                            m_editAudioIs3D = cfg.is3D;
                            m_editAudioVolume = cfg.volume;
                            m_editAudioPitch = cfg.pitch;
                            m_editAudioIsBGM = (cfg.group == "BGM");
                            m_editAudioMinDistance = cfg.minDistance;
                            m_editAudioMaxDistance = cfg.maxDistance;
                            m_editAudioFalloffStart = cfg.falloffStart;
                            m_editAudioFalloffAmount = cfg.falloffAmount;
                            m_editAudioFilterInStart = cfg.filterInStart;
                            m_editAudioFilterPeak = cfg.filterPeak;
                            m_editAudioFilterReleaseEnd = cfg.filterReleaseEnd;
                        }
                    }
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
                    if (ImGui::MenuItem("Delete")) m_deletingAudio = soundKey;
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
    // Clean up any merged colliders from the previous scene before the world is replaced.
    Systems::TilemapCollisionBuilder::clear(m_scene.GetWorld());
    if (!m_scene.LoadFromFile(path))
        std::cerr << "[EditorState] Failed to load: " << path << "\n";
    m_sceneMetaBuffersDirty = true;
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedEntity = {0, 0};
    m_selectedEntities.clear();
    m_hierarchyOrder.clear();
    m_hierarchyGroups.clear();
    m_hiddenEntities.clear();
    m_selectedGroupIdx  = -1;
    m_renamingGroupIdx  = -1;
    m_lastAnchorId      = 0;

    // Restore hierarchy groups + hidden entities from JSON (entity names â†’ IDs)
    {
        using json = nlohmann::json;
        std::ifstream fin(path);
        if (fin.is_open()) {
            json root;
            fin >> root;
            auto& world = m_scene.GetWorld();
            std::vector<uint32_t> objectIndexToId;
            world.each<Components::Transform>([&](ECS::Entity e, Components::Transform&) {
                objectIndexToId.push_back(e.id);
            });
            if (root.contains("hierarchyGroups") && root["hierarchyGroups"].is_array()) {
                // Build name â†’ entity ID map from world
                std::unordered_map<std::string, uint32_t> nameToId;
                world.each<Components::Tag>([&](ECS::Entity e, Components::Tag& tag) {
                    nameToId[tag.tag] = e.id;
                });
                for (auto& jgrp : root["hierarchyGroups"]) {
                    HierarchyGroup grp;
                    grp.name          = jgrp.value("name", "Group");
                    grp.collapsed     = jgrp.value("collapsed", false);
                    grp.sortOrderBase = jgrp.value("sortOrderBase", 0);
                    grp.parentGroupIdx = jgrp.value("parentGroupIdx", -1);
                    if (jgrp.contains("entityIndices") && jgrp["entityIndices"].is_array()) {
                        for (auto& jidx : jgrp["entityIndices"]) {
                            int idx = jidx.get<int>();
                            if (idx >= 0 && idx < (int)objectIndexToId.size())
                                grp.entityIds.push_back(objectIndexToId[idx]);
                        }
                    } else if (jgrp.contains("entities") && jgrp["entities"].is_array()) {
                        for (auto& jname : jgrp["entities"]) {
                            std::string n = jname.get<std::string>();
                            auto it = nameToId.find(n);
                            if (it != nameToId.end())
                                grp.entityIds.push_back(it->second);
                        }
                    }
                    m_hierarchyGroups.push_back(std::move(grp));
                }
            }
            // Restore flat hierarchy display order
            if (root.contains("hierarchyOrderIndices") && root["hierarchyOrderIndices"].is_array()) {
                for (auto& jidx : root["hierarchyOrderIndices"]) {
                    int idx = jidx.get<int>();
                    if (idx >= 0 && idx < (int)objectIndexToId.size())
                        m_hierarchyOrder.push_back(objectIndexToId[idx]);
                }
            } else if (root.contains("hierarchyOrder") && root["hierarchyOrder"].is_array()) {
                auto& world3 = m_scene.GetWorld();
                std::unordered_map<std::string, uint32_t> nameToId3;
                world3.each<Components::Tag>([&](ECS::Entity e, Components::Tag& tag) {
                    nameToId3[tag.tag] = e.id;
                });
                for (auto& jname : root["hierarchyOrder"]) {
                    std::string n = jname.get<std::string>();
                    auto it = nameToId3.find(n);
                    if (it != nameToId3.end())
                        m_hierarchyOrder.push_back(it->second);
                }
            }

            // Restore hidden entities
            if (root.contains("hiddenEntityIndices") && root["hiddenEntityIndices"].is_array()) {
                for (auto& jidx : root["hiddenEntityIndices"]) {
                    int idx = jidx.get<int>();
                    if (idx < 0 || idx >= (int)objectIndexToId.size()) continue;
                    uint32_t id = objectIndexToId[idx];
                    m_hiddenEntities.insert(id);
                    world.each<Components::Tag>([&](ECS::Entity e, Components::Tag&) {
                        if (e.id != id) return;
                        if (auto* sr = world.getComponent<Components::SpriteRenderer>(e)) sr->visible = false;
                        if (auto* tr = world.getComponent<Components::TextRenderer>(e))   tr->visible = false;
                        if (auto* lt = world.getComponent<Components::Light>(e))           lt->active  = false;
                    });
                }
            } else if (root.contains("hiddenEntities") && root["hiddenEntities"].is_array()) {
                auto& world2 = m_scene.GetWorld();
                std::unordered_map<std::string, uint32_t> nameToId2;
                world2.each<Components::Tag>([&](ECS::Entity e, Components::Tag& tag) {
                    nameToId2[tag.tag] = e.id;
                });
                for (auto& jname : root["hiddenEntities"]) {
                    std::string n = jname.get<std::string>();
                    auto it = nameToId2.find(n);
                    if (it == nameToId2.end()) continue;
                    uint32_t id = it->second;
                    m_hiddenEntities.insert(id);
                    // Re-apply visibility=false so the scene matches what was saved
                    world2.each<Components::Tag>([&](ECS::Entity e, Components::Tag&) {
                        if (e.id != id) return;
                        if (auto* sr = world2.getComponent<Components::SpriteRenderer>(e)) sr->visible = false;
                        if (auto* tr = world2.getComponent<Components::TextRenderer>(e))   tr->visible = false;
                        if (auto* lt = world2.getComponent<Components::Light>(e))           lt->active  = false;
                    });
                }
            }
        }
    }

    // Auto-build greedy merged colliders from all solid static wall entities.
    // Individual tile colliders get isSolid=false (hidden from debug view);
    // merged boxes are drawn in cyan so you can see which walls were greedy-merged.
    {
        auto& world = m_scene.GetWorld();
        std::vector<Systems::TilemapTileEntry> entries;
        const float tileSizeTolerance = 1.0f;
        const float rotationTolerance = 0.01f; // radians — excludes rotated objects
        world.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity e, Components::Transform& t, Components::Collider& c) {
                if (!c.isSolid || c.isTrigger || c.moveable) return;
                if (c.shape == Components::Collider::Shape::Circle) return;
                if (std::abs(t.rotation) > rotationTolerance) return;
                const bool gridSized =
                    std::abs(c.size.x - m_gridSize) <= tileSizeTolerance &&
                    std::abs(c.size.y - m_gridSize) <= tileSizeTolerance;
                if (!gridSized && !c.isCombined) return;
                // Exclude dynamic entities (player, enemies) by tag
                auto* tag = world.getComponent<Components::Tag>(e);
                if (tag && (tag->tag == "Player" || tag->tag == "Enemy"
                            || tag->tag == "Enemy (Clone)" || tag->tag == "TilemapMergedCollider"))
                    return;
                constexpr uint32_t kUniqueCombinedMergeBucket = 0xFFFFFFFFu;
                int layerIdx = 0;
                if (auto* layer = world.getComponent<Components::Layer>(e))
                    layerIdx = layer->index;
                const uint32_t mergeBucketId =
                    c.isCombined ? (c.combinedGroupId != 0 ? c.combinedGroupId : kUniqueCombinedMergeBucket) : 0u;
                entries.push_back({t.position, e, layerIdx, mergeBucketId});
            });
        if (!entries.empty())
            Systems::TilemapCollisionBuilder::build(world, entries, m_gridSize, m_gridSize);
    }
}

void EditorState::saveScene() {
    if (m_sceneFilePath.empty()) {
        std::cerr << "[EditorState] No file path ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â cannot save.\n";
        return;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(m_sceneFilePath).parent_path(), ec);
    // Clear merged colliders before saving so tiles are serialized with isSolid=true.
    // Rebuild them after so the editor continues to work correctly.
    auto& saveWorld = m_scene.GetWorld();
    Systems::TilemapCollisionBuilder::clear(saveWorld);
    SceneSerializer::Serialize(m_scene, m_sceneFilePath);
    // Rebuild merged colliders so editor state is restored after save.
    {
        std::vector<Systems::TilemapTileEntry> saveEntries;
        const float saveTileSizeTol = 1.0f;
        saveWorld.each<Components::Transform, Components::Collider>(
            [&](ECS::Entity e, Components::Transform& t, Components::Collider& c) {
                if (!c.isSolid || c.isTrigger || c.moveable) return;
                if (c.shape != Components::Collider::Shape::Box) return;
                if (std::abs(t.rotation) > 0.01f) return;
                const bool gridSized =
                    std::abs(c.size.x - m_gridSize) <= saveTileSizeTol &&
                    std::abs(c.size.y - m_gridSize) <= saveTileSizeTol;
                if (!gridSized && !c.isCombined) return;
                auto* tag = saveWorld.getComponent<Components::Tag>(e);
                if (tag && (tag->tag == "Player" || tag->tag == "Enemy"
                            || tag->tag == "Enemy (Clone)" || tag->tag == "TilemapMergedCollider"))
                    return;
                constexpr uint32_t kUniqueCombinedMergeBucket = 0xFFFFFFFFu;
                int layerIdx = 0;
                if (auto* layer = saveWorld.getComponent<Components::Layer>(e))
                    layerIdx = layer->index;
                const uint32_t mergeBucketId =
                    c.isCombined ? (c.combinedGroupId != 0 ? c.combinedGroupId : kUniqueCombinedMergeBucket) : 0u;
                saveEntries.push_back({t.position, e, layerIdx, mergeBucketId});
            });
        if (!saveEntries.empty())
            Systems::TilemapCollisionBuilder::build(saveWorld, saveEntries, m_gridSize, m_gridSize);
    }

    // Append hierarchy groups metadata (entity names used as stable keys)
    {
        using json = nlohmann::json;
        std::ifstream fin(m_sceneFilePath);
        json root;
        fin >> root;
        fin.close();

        // Build entity ID â†’ tag name map
        auto& world = m_scene.GetWorld();
        std::unordered_map<uint32_t, std::string> idToName;
        std::unordered_map<uint32_t, int> idToObjectIndex;
        world.each<Components::Tag>([&](ECS::Entity e, Components::Tag& tag) {
            idToName[e.id] = tag.tag;
        });
        int objectIndex = 0;
        world.each<Components::Transform>([&](ECS::Entity e, Components::Transform&) {
            idToObjectIndex[e.id] = objectIndex++;
        });

        json jGroups = json::array();
        for (auto& grp : m_hierarchyGroups) {
            json jg;
            jg["name"]          = grp.name;
            jg["collapsed"]     = grp.collapsed;
            jg["sortOrderBase"] = grp.sortOrderBase;
            jg["parentGroupIdx"] = grp.parentGroupIdx;
            jg["entities"]      = json::array();
            jg["entityIndices"] = json::array();
            for (uint32_t id : grp.entityIds) {
                auto it = idToName.find(id);
                if (it != idToName.end())
                    jg["entities"].push_back(it->second);
                auto idxIt = idToObjectIndex.find(id);
                if (idxIt != idToObjectIndex.end())
                    jg["entityIndices"].push_back(idxIt->second);
            }
            jGroups.push_back(jg);
        }
        root["hierarchyGroups"] = jGroups;

        // Save flat entity display order
        json jOrder = json::array();
        json jOrderIndices = json::array();
        for (uint32_t id : m_hierarchyOrder) {
            auto it = idToName.find(id);
            if (it != idToName.end()) jOrder.push_back(it->second);
            auto idxIt = idToObjectIndex.find(id);
            if (idxIt != idToObjectIndex.end()) jOrderIndices.push_back(idxIt->second);
        }
        root["hierarchyOrder"] = jOrder;
        root["hierarchyOrderIndices"] = jOrderIndices;

        // Save hidden entity names
        json jHidden = json::array();
        json jHiddenIndices = json::array();
        for (uint32_t id : m_hiddenEntities) {
            auto it = idToName.find(id);
            if (it != idToName.end()) jHidden.push_back(it->second);
            auto idxIt = idToObjectIndex.find(id);
            if (idxIt != idToObjectIndex.end()) jHiddenIndices.push_back(idxIt->second);
        }
        root["hiddenEntities"] = jHidden;
        root["hiddenEntityIndices"] = jHiddenIndices;

        std::ofstream fout(m_sceneFilePath);
        fout << std::setw(4) << root << std::endl;
    }

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

void EditorState::captureInspectorBefore() {
    m_inspectorBeforeEntity = m_selectedEntity;
    m_inspectorBefore = EntitySnapshot::capture(m_scene.GetWorld(), m_selectedEntity);
}

void EditorState::pushInspectorChange() {
    auto& world = m_scene.GetWorld();
    if (!world.isValid(m_inspectorBeforeEntity)) return;
    auto after = EntitySnapshot::capture(world, m_inspectorBeforeEntity);
    executeCommand(std::make_unique<ModifyEntityCmd>(
        world, m_inspectorBeforeEntity,
        std::move(m_inspectorBefore), std::move(after)));
}

EditorState::HierarchyUndoState EditorState::captureHierarchyUndoState() const {
    HierarchyUndoState state;
    state.hierarchyOrder = m_hierarchyOrder;
    state.hierarchyGroups = m_hierarchyGroups;
    state.hiddenEntities = m_hiddenEntities;
    state.selectedEntities = m_selectedEntities;
    state.selectedEntity = m_selectedEntity;
    state.selectedGroupIdx = m_selectedGroupIdx;
    state.renamingGroupIdx = m_renamingGroupIdx;
    state.lastAnchorId = m_lastAnchorId;
    return state;
}

void EditorState::applyHierarchyUndoState(const HierarchyUndoState& state) {
    m_hierarchyOrder = state.hierarchyOrder;
    m_hierarchyGroups = state.hierarchyGroups;
    m_hiddenEntities = state.hiddenEntities;
    m_selectedEntities = state.selectedEntities;
    m_selectedEntity = state.selectedEntity;
    m_selectedGroupIdx = state.selectedGroupIdx;
    m_renamingGroupIdx = state.renamingGroupIdx;
    m_lastAnchorId = state.lastAnchorId;
}

// ---------------------------------------------------------------------------
// Prefab helpers
// ---------------------------------------------------------------------------

void EditorState::loadAvailablePrefabs() {
    namespace fs = std::filesystem;
    const std::vector<std::string> previousCategoryOrder = m_availablePrefabCategories;
    m_availablePrefabs.clear();
    m_availablePrefabEntries.clear();
    m_availablePrefabCategories.clear();
    std::string dir = "assets/prefabs/";
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            std::string category = "Uncategorized";
            std::string textureId;
            std::ifstream prefabFile(entry.path());
            if (prefabFile.is_open()) {
                try {
                    nlohmann::json prefabJson;
                    prefabFile >> prefabJson;
                    if (prefabJson.contains("category") && prefabJson["category"].is_string()) {
                        category = prefabJson["category"].get<std::string>();
                        if (category.empty())
                            category = "Uncategorized";
                    }
                    if (prefabJson.contains("components") &&
                        prefabJson["components"].contains("spriteRenderer") &&
                        prefabJson["components"]["spriteRenderer"].contains("textureId") &&
                        prefabJson["components"]["spriteRenderer"]["textureId"].is_string()) {
                        textureId = prefabJson["components"]["spriteRenderer"]["textureId"].get<std::string>();
                    }
                } catch (...) {}
            }

            m_availablePrefabEntries.push_back({
                entry.path().stem().string(),
                category,
                textureId
            });
        }
    }

    std::sort(m_availablePrefabEntries.begin(), m_availablePrefabEntries.end(),
        [](const PrefabBrowserEntry& a, const PrefabBrowserEntry& b) {
            if (a.category != b.category)
                return a.category < b.category;
            return a.name < b.name;
        });

    std::vector<std::string> discoveredCategories;
    for (const auto& prefab : m_availablePrefabEntries) {
        m_availablePrefabs.push_back(prefab.name);
        if (std::find(discoveredCategories.begin(), discoveredCategories.end(),
                      prefab.category) == discoveredCategories.end()) {
            discoveredCategories.push_back(prefab.category);
        }
    }

    for (const auto& category : previousCategoryOrder) {
        if (std::find(discoveredCategories.begin(), discoveredCategories.end(), category) !=
                discoveredCategories.end()) {
            m_availablePrefabCategories.push_back(category);
        }
    }

    for (const auto& category : discoveredCategories) {
        if (std::find(m_availablePrefabCategories.begin(), m_availablePrefabCategories.end(),
                      category) == m_availablePrefabCategories.end()) {
            m_availablePrefabCategories.push_back(category);
        }
    }

    m_selectedPrefabIndex = -1;
    if (!m_selectedTilePrefab.empty()) {
        for (int i = 0; i < static_cast<int>(m_availablePrefabs.size()); ++i) {
            if (m_availablePrefabs[i] == m_selectedTilePrefab) {
                m_selectedPrefabIndex = i;
                break;
            }
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
            {"scale",    {{"x", t->scale.x},    {"y", t->scale.y}}},
            {"pivot",    {{"x", t->pivot.x},    {"y", t->pivot.y}}},
            {"snapToGrid", t->snapToGrid}};

    if (auto* sr = world.getComponent<Components::SpriteRenderer>(entity))
        comps["spriteRenderer"] = {
            {"textureId", sr->textureId},
            {"uv",    {sr->uv.x, sr->uv.y, sr->uv.z, sr->uv.w}},
            {"size",  {{"x", sr->size.x},  {"y", sr->size.y}}},
            {"color", {{"r", sr->color.r}, {"g", sr->color.g},
                       {"b", sr->color.b}, {"a", sr->color.a}}},
            {"visible", sr->visible},
            {"unlit", sr->unlit},
            {"sortOrder", sr->sortOrder}};

    if (auto* c = world.getComponent<Components::Collider>(entity))
        comps["collider"] = {
            {"shape",    static_cast<int>(c->shape)},
            {"width",    c->size.x}, {"height",  c->size.y},
            {"offset",   {{"x", c->offset.x}, {"y", c->offset.y}}},
            {"radius",   c->radius},
            {"isSolid",  c->isSolid},  {"isTrigger",   c->isTrigger},
            {"blocksLight", c->blocksLight}, {"isHazard", c->isHazard},
            {"moveable", c->moveable},
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

    if (auto* textComp = world.getComponent<Components::TextRenderer>(entity))
        comps["textRenderer"] = {{"text", textComp->text}, {"fontId", textComp->fontId},
                                  {"fontSize", textComp->fontSize},
                                  {"color", {{"r", textComp->color.r}, {"g", textComp->color.g}, {"b", textComp->color.b}}},
                                  {"visible", textComp->visible},
                                  {"boxSize", {{"x", textComp->boxSize.x}, {"y", textComp->boxSize.y}}},
                                  {"alignment", static_cast<int>(textComp->alignment)},
                                  {"verticalAlignment", static_cast<int>(textComp->verticalAlign)},
                                  {"wordWrap", textComp->wordWrap},
                                  {"lineSpacing", textComp->lineSpacing},
                                  {"padding", {{"x", textComp->padding.x}, {"y", textComp->padding.y}}}};

    if (auto* lt = world.getComponent<Components::Light>(entity))
        comps["light"] = {
            {"active", lt->active},
            {"offset", {{"x", lt->offset.x}, {"y", lt->offset.y}}},
            {"color", {{"r", lt->color.r}, {"g", lt->color.g}, {"b", lt->color.b}}},
            {"radius", lt->radius},
            {"angle", lt->angle},
            {"intensity", lt->intensity}};

    if (auto* fa = world.getComponent<Components::FootstepAudio>(entity))
        comps["footstepAudio"] = {
            {"interval", fa->interval},
            {"sprintIntervalScale", fa->sprintIntervalScale},
            {"sprintSpeedThreshold", fa->sprintSpeedThreshold},
            {"minSpeed", fa->minSpeed},
            {"resetDelay", fa->resetDelay},
            {"volume", fa->volume},
            {"variationCount", fa->variationCount},
            {"useSurfaceSounds", fa->useSurfaceSounds},
            {"soundPrefix", fa->soundPrefix},
            {"concretePrefix", fa->concretePrefix},
            {"tilePrefix", fa->tilePrefix},
            {"woodPrefix", fa->woodPrefix}};

    if (auto* pe = world.getComponent<Components::ParticleEmitter>(entity)) {
        auto& sys = pe->system();
        comps["particleEmitter"] = {
            {"active", pe->active()},
            {"rate", sys.rate},
            {"lifeMin", sys.lifeMin},
            {"lifeMax", sys.lifeMax},
            {"speedMin", sys.speedMin},
            {"speedMax", sys.speedMax},
            {"sizeStart", sys.sizeStart},
            {"sizeEnd", sys.sizeEnd},
            {"spreadDeg", sys.spreadDeg},
            {"spawnRadius", sys.spawnRadius},
            {"looping", sys.looping},
            {"worldSpace", sys.worldSpace},
            {"direction", {{"x", sys.dir.x}, {"y", sys.dir.y}}},
            {"acceleration", {{"x", sys.acceleration.x}, {"y", sys.acceleration.y}}},
            {"localOffset", {{"x", pe->localOffset.x}, {"y", pe->localOffset.y}}},
            {"colorStart", {{"r", sys.colorStart.r}, {"g", sys.colorStart.g},
                            {"b", sys.colorStart.b}, {"a", sys.colorStart.a}}},
            {"colorEnd", {{"r", sys.colorEnd.r}, {"g", sys.colorEnd.g},
                          {"b", sys.colorEnd.b}, {"a", sys.colorEnd.a}}}
        };
    }

    if (auto* btn = world.getComponent<Components::UIButton>(entity))
        comps["uiButton"] = {
            {"action", btn->action},
            {"uiScript", btn->uiScript},
            {"interactable", btn->interactable},
            {"showBackground", btn->showBackground},
            {"size", {{"x", btn->size.x}, {"y", btn->size.y}}},
            {"offset", {{"x", btn->offset.x}, {"y", btn->offset.y}}},
            {"normalColor", {{"r", btn->normalColor.r}, {"g", btn->normalColor.g}, {"b", btn->normalColor.b}}},
            {"hoverColor", {{"r", btn->hoverColor.r}, {"g", btn->hoverColor.g}, {"b", btn->hoverColor.b}}}};

    if (auto* sl = world.getComponent<Components::UISlider>(entity))
        comps["uiSlider"] = {
            {"settingKey", sl->settingKey},
            {"minValue", sl->minValue},
            {"maxValue", sl->maxValue},
            {"value", sl->value},
            {"trackWidth", sl->trackWidth},
            {"trackHeight", sl->trackHeight},
            {"handleSize", sl->handleSize},
            {"trackColor", {{"r", sl->trackColor.r}, {"g", sl->trackColor.g},
                            {"b", sl->trackColor.b}, {"a", sl->trackColor.a}}},
            {"fillColor", {{"r", sl->fillColor.r}, {"g", sl->fillColor.g},
                           {"b", sl->fillColor.b}, {"a", sl->fillColor.a}}},
            {"handleColor", {{"r", sl->handleColor.r}, {"g", sl->handleColor.g},
                             {"b", sl->handleColor.b}, {"a", sl->handleColor.a}}}};

    if (auto* cam = world.getComponent<Components::CameraComponent>(entity))
        comps["cameraComponent"] = {
            {"isMain", cam->isMain},
            {"zoom", cam->zoom},
            {"offset", {{"x", cam->offset.x}, {"y", cam->offset.y}}}};

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

bool EditorState::updatePrefabCategory(const std::string& prefabName, const std::string& category) {
    using json = nlohmann::json;
    namespace fs = std::filesystem;

    if (prefabName.empty() || category.empty())
        return false;

    const fs::path prefabPath = fs::path("assets/prefabs") / (prefabName + ".json");
    std::ifstream in(prefabPath);
    if (!in.is_open())
        return false;

    json prefabJson;
    try {
        in >> prefabJson;
    } catch (...) {
        return false;
    }
    in.close();

    prefabJson["category"] = category;

    std::ofstream out(prefabPath);
    if (!out.is_open())
        return false;

    out << prefabJson.dump(4);
    out.close();

    loadAvailablePrefabs();
    return true;
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

    if (!pj.contains("components")) return entity;
    const auto& c = pj["components"];

    std::string entityTag = prefabName;
    if (c.contains("tag")) entityTag = c["tag"].value("tag", prefabName);
    world.addComponent(entity, Components::Tag{entityTag});

    if (c.contains("transform")) {
        Components::Transform t;
        t.position = position;
        t.rotation = c["transform"].value("rotation", 0.f);
        if (c["transform"].contains("scale")) {
            t.scale.x = c["transform"]["scale"].value("x", 1.f);
            t.scale.y = c["transform"]["scale"].value("y", 1.f);
        }
        if (c["transform"].contains("pivot")) {
            t.pivot.x = c["transform"]["pivot"].value("x", 0.f);
            t.pivot.y = c["transform"]["pivot"].value("y", 0.f);
        }
        t.snapToGrid = c["transform"].value("snapToGrid", false);
        world.addComponent(entity, t);
    } else {
        Components::Transform t; t.position = position;
        world.addComponent(entity, t);
    }

    if (c.contains("spriteRenderer")) {
        Components::SpriteRenderer sr;
        sr.textureId = c["spriteRenderer"].value("textureId", "");
        if (c["spriteRenderer"].contains("uv") &&
            c["spriteRenderer"]["uv"].is_array() &&
            c["spriteRenderer"]["uv"].size() == 4) {
            sr.uv = {
                c["spriteRenderer"]["uv"][0],
                c["spriteRenderer"]["uv"][1],
                c["spriteRenderer"]["uv"][2],
                c["spriteRenderer"]["uv"][3]
            };
        }
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
        sr.visible = c["spriteRenderer"].value("visible", true);
        sr.unlit = c["spriteRenderer"].value("unlit", false);
        sr.sortOrder = c["spriteRenderer"].value("sortOrder", 0);
        world.addComponent(entity, sr);
    }

    if (c.contains("collider")) {
        Components::Collider col;
        col.shape   = static_cast<Components::Collider::Shape>(c["collider"].value("shape", 0));
        col.size.x  = c["collider"].value("width",  50.f);
        col.size.y  = c["collider"].value("height", 50.f);
        if (c["collider"].contains("offset")) {
            col.offset.x = c["collider"]["offset"].value("x", 0.f);
            col.offset.y = c["collider"]["offset"].value("y", 0.f);
        }
        col.radius  = c["collider"].value("radius", 25.f);
        col.isSolid     = c["collider"].value("isSolid",     true);
        col.isTrigger   = c["collider"].value("isTrigger",   false);
        col.blocksLight = c["collider"].value("blocksLight",  false);
        col.isHazard    = c["collider"].value("isHazard",    false);
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
        auto& textRendererJson = c["textRenderer"];
        Components::TextRenderer textComp;
        textComp.text      = textRendererJson.value("text",    "");
        textComp.fontId    = textRendererJson.value("fontId",  "Debug");
        textComp.fontSize  = textRendererJson.value("fontSize", 1.f);
        textComp.visible   = textRendererJson.value("visible",  true);
        textComp.alignment = static_cast<Components::TextAlignment>(textRendererJson.value("alignment", 0));
        textComp.verticalAlign = static_cast<Components::VerticalAlignment>(textRendererJson.value("verticalAlignment", 0));
        textComp.wordWrap  = textRendererJson.value("wordWrap",    true);
        textComp.lineSpacing = textRendererJson.value("lineSpacing", 1.f);
        if (textRendererJson.contains("color")) {
            textComp.color.r = textRendererJson["color"].value("r", 1.f);
            textComp.color.g = textRendererJson["color"].value("g", 1.f);
            textComp.color.b = textRendererJson["color"].value("b", 1.f);
        }
        if (textRendererJson.contains("boxSize")) {
            textComp.boxSize.x = textRendererJson["boxSize"].value("x", 200.f);
            textComp.boxSize.y = textRendererJson["boxSize"].value("y", 100.f);
        }
        if (textRendererJson.contains("padding")) {
            textComp.padding.x = textRendererJson["padding"].value("x", 0.f);
            textComp.padding.y = textRendererJson["padding"].value("y", 0.f);
        }
        world.addComponent(entity, textComp);
    }

    if (c.contains("light")) {
        auto& lc = c["light"];
        Components::Light lt;
        lt.active = lc.value("active", true);
        if (lc.contains("offset")) {
            lt.offset.x = lc["offset"].value("x", lt.offset.x);
            lt.offset.y = lc["offset"].value("y", lt.offset.y);
        }
        if (lc.contains("color")) {
            lt.color.r = lc["color"].value("r", lt.color.r);
            lt.color.g = lc["color"].value("g", lt.color.g);
            lt.color.b = lc["color"].value("b", lt.color.b);
        }
        lt.radius = lc.value("radius", lt.radius);
        lt.angle = lc.value("angle", lt.angle);
        lt.intensity = lc.value("intensity", lt.intensity);
        world.addComponent(entity, lt);
    }

    if (c.contains("footstepAudio")) {
        auto& fac = c["footstepAudio"];
        Components::FootstepAudio fa;
        fa.interval = fac.value("interval", fa.interval);
        fa.sprintIntervalScale = fac.value("sprintIntervalScale", fa.sprintIntervalScale);
        fa.sprintSpeedThreshold = fac.value("sprintSpeedThreshold", fa.sprintSpeedThreshold);
        fa.minSpeed = fac.value("minSpeed", fa.minSpeed);
        fa.resetDelay = fac.value("resetDelay", fa.resetDelay);
        fa.volume = fac.value("volume", fa.volume);
        fa.variationCount = fac.value("variationCount", fa.variationCount);
        fa.useSurfaceSounds = fac.value("useSurfaceSounds", fa.useSurfaceSounds);
        fa.soundPrefix = fac.value("soundPrefix", fa.soundPrefix);
        fa.concretePrefix = fac.value("concretePrefix", fa.concretePrefix);
        fa.tilePrefix = fac.value("tilePrefix", fa.tilePrefix);
        fa.woodPrefix = fac.value("woodPrefix", fa.woodPrefix);
        world.addComponent(entity, fa);
    }

    if (c.contains("particleEmitter")) {
        auto& pec = c["particleEmitter"];
        Components::ParticleEmitter pe;
        pe.setActive(pec.value("active", true));
        pe.localOffset.x = pec.contains("localOffset") ? pec["localOffset"].value("x", 0.f) : 0.f;
        pe.localOffset.y = pec.contains("localOffset") ? pec["localOffset"].value("y", 0.f) : 0.f;
        auto& sys = pe.system();
        sys.rate = pec.value("rate", sys.rate);
        sys.lifeMin = pec.value("lifeMin", sys.lifeMin);
        sys.lifeMax = pec.value("lifeMax", sys.lifeMax);
        sys.speedMin = pec.value("speedMin", sys.speedMin);
        sys.speedMax = pec.value("speedMax", sys.speedMax);
        sys.sizeStart = pec.value("sizeStart", sys.sizeStart);
        sys.sizeEnd = pec.value("sizeEnd", sys.sizeEnd);
        sys.spreadDeg = pec.value("spreadDeg", sys.spreadDeg);
        sys.spawnRadius = pec.value("spawnRadius", sys.spawnRadius);
        sys.looping = pec.value("looping", sys.looping);
        sys.worldSpace = pec.value("worldSpace", sys.worldSpace);
        if (pec.contains("direction")) {
            sys.dir.x = pec["direction"].value("x", sys.dir.x);
            sys.dir.y = pec["direction"].value("y", sys.dir.y);
        }
        if (pec.contains("acceleration")) {
            sys.acceleration.x = pec["acceleration"].value("x", sys.acceleration.x);
            sys.acceleration.y = pec["acceleration"].value("y", sys.acceleration.y);
        }
        if (pec.contains("colorStart")) {
            sys.colorStart.r = pec["colorStart"].value("r", sys.colorStart.r);
            sys.colorStart.g = pec["colorStart"].value("g", sys.colorStart.g);
            sys.colorStart.b = pec["colorStart"].value("b", sys.colorStart.b);
            sys.colorStart.a = pec["colorStart"].value("a", sys.colorStart.a);
        }
        if (pec.contains("colorEnd")) {
            sys.colorEnd.r = pec["colorEnd"].value("r", sys.colorEnd.r);
            sys.colorEnd.g = pec["colorEnd"].value("g", sys.colorEnd.g);
            sys.colorEnd.b = pec["colorEnd"].value("b", sys.colorEnd.b);
            sys.colorEnd.a = pec["colorEnd"].value("a", sys.colorEnd.a);
        }
        world.addComponent(entity, pe);
    }

    if (c.contains("uiButton")) {
        auto& ub = c["uiButton"];
        Components::UIButton btn;
        btn.action = ub.value("action", "");
        btn.uiScript = ub.value("uiScript", "");
        btn.interactable = ub.value("interactable", true);
        btn.showBackground = ub.value("showBackground", true);
        if (ub.contains("size")) {
            btn.size.x = ub["size"].value("x", 0.f);
            btn.size.y = ub["size"].value("y", 0.f);
        }
        if (ub.contains("offset")) {
            btn.offset.x = ub["offset"].value("x", 0.f);
            btn.offset.y = ub["offset"].value("y", 0.f);
        }
        if (ub.contains("normalColor")) {
            btn.normalColor.r = ub["normalColor"].value("r", 1.f);
            btn.normalColor.g = ub["normalColor"].value("g", 1.f);
            btn.normalColor.b = ub["normalColor"].value("b", 1.f);
        }
        if (ub.contains("hoverColor")) {
            btn.hoverColor.r = ub["hoverColor"].value("r", 1.f);
            btn.hoverColor.g = ub["hoverColor"].value("g", 0.8f);
            btn.hoverColor.b = ub["hoverColor"].value("b", 0.f);
        }
        world.addComponent(entity, btn);
    }

    if (c.contains("uiSlider")) {
        auto& us = c["uiSlider"];
        Components::UISlider sl;
        sl.settingKey = us.value("settingKey", "");
        sl.minValue = us.value("minValue", 0.f);
        sl.maxValue = us.value("maxValue", 1.f);
        sl.value = us.value("value", 1.f);
        sl.trackWidth = us.value("trackWidth", 400.f);
        sl.trackHeight = us.value("trackHeight", 18.f);
        sl.handleSize = us.value("handleSize", 22.f);
        if (us.contains("trackColor")) {
            sl.trackColor.r = us["trackColor"].value("r", sl.trackColor.r);
            sl.trackColor.g = us["trackColor"].value("g", sl.trackColor.g);
            sl.trackColor.b = us["trackColor"].value("b", sl.trackColor.b);
            sl.trackColor.a = us["trackColor"].value("a", sl.trackColor.a);
        }
        if (us.contains("fillColor")) {
            sl.fillColor.r = us["fillColor"].value("r", sl.fillColor.r);
            sl.fillColor.g = us["fillColor"].value("g", sl.fillColor.g);
            sl.fillColor.b = us["fillColor"].value("b", sl.fillColor.b);
            sl.fillColor.a = us["fillColor"].value("a", sl.fillColor.a);
        }
        if (us.contains("handleColor")) {
            sl.handleColor.r = us["handleColor"].value("r", sl.handleColor.r);
            sl.handleColor.g = us["handleColor"].value("g", sl.handleColor.g);
            sl.handleColor.b = us["handleColor"].value("b", sl.handleColor.b);
            sl.handleColor.a = us["handleColor"].value("a", sl.handleColor.a);
        }
        world.addComponent(entity, sl);
    }

    if (c.contains("cameraComponent")) {
        auto& cc = c["cameraComponent"];
        Components::CameraComponent cam;
        cam.isMain = cc.value("isMain", true);
        cam.zoom = cc.value("zoom", 1.f);
        if (cc.contains("offset")) {
            cam.offset.x = cc["offset"].value("x", 0.f);
            cam.offset.y = cc["offset"].value("y", 0.f);
        }
        world.addComponent(entity, cam);
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
        if (it != g.entities.end())
            g.entities.erase(it, g.entities.end());
    }

    for (auto& grp : m_hierarchyGroups) {
        grp.entityIds.erase(
            std::remove(grp.entityIds.begin(), grp.entityIds.end(), entity.id),
            grp.entityIds.end());
    }

    m_hierarchyOrder.erase(
        std::remove(m_hierarchyOrder.begin(), m_hierarchyOrder.end(), entity.id),
        m_hierarchyOrder.end());

    m_hiddenEntities.erase(entity.id);
    m_selectedEntities.erase(
        std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(),
            [&](ECS::Entity e) {
                return e.id == entity.id && e.generation == entity.generation;
            }),
        m_selectedEntities.end());

    if (m_selectedEntity.id == entity.id && m_selectedEntity.generation == entity.generation)
        m_selectedEntity = m_selectedEntities.empty() ? ECS::Entity{0, 0} : m_selectedEntities.front();
}

void EditorState::clearTilemap() {
    auto& world = m_scene.GetWorld();
    // Remove merged colliders and restore individual tile isSolid flags
    // before destroying the tile entities themselves.
    Systems::TilemapCollisionBuilder::clear(world);
    m_tilemapDirty = false;
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
    // Build merged colliders to eliminate ghost collisions at tile borders.
    {
        std::vector<Systems::TilemapTileEntry> entries;
        entries.reserve(m_tilemap.size());
        for (const auto& td : m_tilemap)
            entries.push_back({td.position, td.entity, td.layer});
        Systems::TilemapCollisionBuilder::build(world, entries, m_gridSize, m_gridSize);
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

    if (m_paintMode != PaintMode::Select) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_viewportHovered)
            m_tileStrokeActive = true;

        if (m_tileStrokeActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_tileStrokeActive = false;
            if (m_tilemapDirty) {
                m_tilemapDirty = false;
                Systems::TilemapCollisionBuilder::clear(world);
                std::vector<Systems::TilemapTileEntry> entries;
                entries.reserve(m_tilemap.size());
                for (const auto& td : m_tilemap)
                    entries.push_back({td.position, td.entity, td.layer});
                Systems::TilemapCollisionBuilder::build(world, entries, m_gridSize, m_gridSize);
            }
        }

        if (!m_tileStrokeActive || !ImGui::IsMouseDown(ImGuiMouseButton_Left) || !m_viewportHovered)
            return;
    } else if (!m_viewportHovered) {
        return;
    }

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

                if (m_selectedGroupIdx >= 0 && m_selectedGroupIdx < (int)m_hierarchyGroups.size()) {
                    auto& entityIds = m_hierarchyGroups[m_selectedGroupIdx].entityIds;
                    if (std::find(entityIds.begin(), entityIds.end(), entity.id) == entityIds.end())
                        entityIds.push_back(entity.id);
                }
                m_paintedThisFrame = true;
                m_tilemapDirty = true;
            }
        }
    } else if (m_paintMode == PaintMode::Erase) {
        for (auto it = m_tilemap.begin(); it != m_tilemap.end(); ++it) {
            if (it->layer == m_currentLayer &&
                world.isValid(it->entity) &&
                magnitude(it->position - worldPos) < m_gridSize * 0.5f) {
                removeEntityFromGroups(it->entity);
                world.destroyEntity(it->entity);
                m_tilemap.erase(it);
                m_paintedThisFrame = true;
                m_tilemapDirty = true;
                break;
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
    namespace fs = std::filesystem;
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
        int categoryComboIndex = static_cast<int>(m_availablePrefabCategories.size());
        if (!m_savePrefabUseCustomCategory) {
            for (int i = 0; i < static_cast<int>(m_availablePrefabCategories.size()); ++i) {
                if (m_availablePrefabCategories[i] == m_savePrefabCategoryBuf) {
                    categoryComboIndex = i;
                    break;
                }
            }
            if (categoryComboIndex == static_cast<int>(m_availablePrefabCategories.size()))
                m_savePrefabUseCustomCategory = true;
        }

        const char* categoryPreview =
            (categoryComboIndex < static_cast<int>(m_availablePrefabCategories.size()))
            ? m_availablePrefabCategories[categoryComboIndex].c_str()
            : "Custom...";

        if (ImGui::BeginCombo("Category##prefabcatcombo", categoryPreview)) {
            for (int i = 0; i < static_cast<int>(m_availablePrefabCategories.size()); ++i) {
                const bool selected = (categoryComboIndex == i);
                if (ImGui::Selectable(m_availablePrefabCategories[i].c_str(), selected)) {
                    categoryComboIndex = i;
                    m_savePrefabUseCustomCategory = false;
                    strncpy_s(m_savePrefabCategoryBuf, m_availablePrefabCategories[i].c_str(),
                              sizeof(m_savePrefabCategoryBuf) - 1);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }

            const bool customSelected =
                (categoryComboIndex == static_cast<int>(m_availablePrefabCategories.size()));
            if (ImGui::Selectable("Custom...", customSelected)) {
                categoryComboIndex = static_cast<int>(m_availablePrefabCategories.size());
                m_savePrefabUseCustomCategory = true;
                if (m_savePrefabCategoryBuf[0] == '\0')
                    strncpy_s(m_savePrefabCategoryBuf, "Custom", sizeof(m_savePrefabCategoryBuf) - 1);
            }
            if (customSelected) ImGui::SetItemDefaultFocus();
            ImGui::EndCombo();
        }

        if (m_savePrefabUseCustomCategory) {
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText("Custom Category##prefabcat",  m_savePrefabCategoryBuf,
                             sizeof(m_savePrefabCategoryBuf));
        } else {
            ImGui::TextDisabled("Using existing category: %s", m_savePrefabCategoryBuf);
        }

        bool canSave = world.isValid(m_selectedEntity) &&
                       m_savePrefabNameBuf[0] != '\0' &&
                       m_savePrefabCategoryBuf[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save Prefab##save", ImVec2(-1, 0)))
            savePrefabFromEntity(m_selectedEntity, m_savePrefabNameBuf, m_savePrefabCategoryBuf);
        if (!canSave) ImGui::EndDisabled();
    }

    ImGui::Separator();

    if (!m_renamingPrefabPath.empty())
        ImGui::OpenPopup("RenamePrefabPopup##pfb");
    if (ImGui::BeginPopupModal("RenamePrefabPopup##pfb", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path src(m_renamingPrefabPath);
        std::string oldStem = src.stem().string();
        ImGui::Text("Rename prefab:");
        ImGui::TextDisabled("%s", src.filename().string().c_str());
        ImGui::SetNextItemWidth(260.0f);
        bool hitEnter = ImGui::InputText("New name##renprefab", m_renamePrefabBuf,
                                         sizeof(m_renamePrefabBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        bool doRename = false;
        if (ImGui::Button("Rename") || hitEnter) doRename = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel##renprefab")) {
            m_renamingPrefabPath.clear();
            m_renamePrefabBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        if (doRename && m_renamePrefabBuf[0] != '\0') {
            fs::path dst = src.parent_path() /
                           (std::string(m_renamePrefabBuf) + src.extension().string());
            std::error_code renameEc;
            if (dst != src && !fs::exists(dst)) {
                fs::rename(src, dst, renameEc);
                if (!renameEc) {
                    std::string newStem = dst.stem().string();
                    if (m_selectedTilePrefab == oldStem)
                        m_selectedTilePrefab = newStem;
                    loadAvailablePrefabs();
                    for (int idx = 0; idx < static_cast<int>(m_availablePrefabs.size()); ++idx) {
                        if (m_availablePrefabs[idx] == newStem) {
                            m_selectedPrefabIndex = idx;
                            break;
                        }
                    }
                    m_renamingPrefabPath.clear();
                    m_renamePrefabBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    if (!m_cloningPrefabPath.empty())
        ImGui::OpenPopup("ClonePrefabPopup##pfb");
    if (ImGui::BeginPopupModal("ClonePrefabPopup##pfb", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path src(m_cloningPrefabPath);
        ImGui::Text("Clone prefab:");
        ImGui::TextDisabled("%s", src.filename().string().c_str());
        ImGui::SetNextItemWidth(260.0f);
        bool hitEnter = ImGui::InputText("Copy name##cloneprefab", m_clonePrefabBuf,
                                         sizeof(m_clonePrefabBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);

        bool doClone = false;
        if (ImGui::Button("Clone") || hitEnter) doClone = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel##cloneprefab")) {
            m_cloningPrefabPath.clear();
            m_clonePrefabBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        if (doClone && m_clonePrefabBuf[0] != '\0') {
            fs::path dst = src.parent_path() /
                           (std::string(m_clonePrefabBuf) + src.extension().string());
            std::error_code copyEc;
            if (dst != src && !fs::exists(dst)) {
                fs::copy_file(src, dst, copyEc);
                if (!copyEc) {
                    loadAvailablePrefabs();
                    m_cloningPrefabPath.clear();
                    m_clonePrefabBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    if (!m_changeCategoryPrefabName.empty())
        ImGui::OpenPopup("ChangePrefabCategoryPopup##pfb");
    if (ImGui::BeginPopupModal("ChangePrefabCategoryPopup##pfb", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Change prefab category:");
        ImGui::TextDisabled("%s", m_changeCategoryPrefabName.c_str());

        int categoryComboIndex = static_cast<int>(m_availablePrefabCategories.size());
        if (!m_changeCategoryUseCustom) {
            for (int i = 0; i < static_cast<int>(m_availablePrefabCategories.size()); ++i) {
                if (m_availablePrefabCategories[i] == m_changeCategoryBuf) {
                    categoryComboIndex = i;
                    break;
                }
            }
            if (categoryComboIndex == static_cast<int>(m_availablePrefabCategories.size()))
                m_changeCategoryUseCustom = true;
        }

        const char* categoryPreview =
            (categoryComboIndex < static_cast<int>(m_availablePrefabCategories.size()))
            ? m_availablePrefabCategories[categoryComboIndex].c_str()
            : "Custom...";

        if (ImGui::BeginCombo("Category##changeprefabcatcombo", categoryPreview)) {
            for (int i = 0; i < static_cast<int>(m_availablePrefabCategories.size()); ++i) {
                const bool selected = (categoryComboIndex == i);
                if (ImGui::Selectable(m_availablePrefabCategories[i].c_str(), selected)) {
                    categoryComboIndex = i;
                    m_changeCategoryUseCustom = false;
                    strncpy_s(m_changeCategoryBuf, m_availablePrefabCategories[i].c_str(),
                              sizeof(m_changeCategoryBuf) - 1);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }

            const bool customSelected =
                (categoryComboIndex == static_cast<int>(m_availablePrefabCategories.size()));
            if (ImGui::Selectable("Custom...", customSelected)) {
                categoryComboIndex = static_cast<int>(m_availablePrefabCategories.size());
                m_changeCategoryUseCustom = true;
                if (m_changeCategoryBuf[0] == '\0')
                    strncpy_s(m_changeCategoryBuf, "Custom", sizeof(m_changeCategoryBuf) - 1);
            }
            if (customSelected) ImGui::SetItemDefaultFocus();
            ImGui::EndCombo();
        }

        if (m_changeCategoryUseCustom) {
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("Custom Category##changeprefabcat", m_changeCategoryBuf,
                             sizeof(m_changeCategoryBuf));
        } else {
            ImGui::TextDisabled("Using existing category: %s", m_changeCategoryBuf);
        }

        bool doApply = false;
        if (ImGui::Button("Apply")) doApply = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel##changeprefabcat")) {
            m_changeCategoryPrefabName.clear();
            m_changeCategoryBuf[0] = '\0';
            m_changeCategoryUseCustom = false;
            ImGui::CloseCurrentPopup();
        }

        if (doApply && m_changeCategoryBuf[0] != '\0') {
            if (updatePrefabCategory(m_changeCategoryPrefabName, m_changeCategoryBuf)) {
                m_changeCategoryPrefabName.clear();
                m_changeCategoryBuf[0] = '\0';
                m_changeCategoryUseCustom = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    // --- Prefab Library ---
    if (ImGui::CollapsingHeader("Prefab Library", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Refresh##prefabrefresh")) loadAvailablePrefabs();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu prefabs across %zu categories",
                            m_availablePrefabs.size(), m_availablePrefabCategories.size());

        float thumbSize  = 64.f;
        float padding    = 8.f;
        float cellSize   = thumbSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int   cols       = std::max(1, static_cast<int>(panelWidth / cellSize));
        auto categoryCount = [&](const std::string& category) {
            return static_cast<int>(std::count_if(
                m_availablePrefabEntries.begin(), m_availablePrefabEntries.end(),
                [&](const PrefabBrowserEntry& prefab) { return prefab.category == category; }));
        };

        auto renderPrefabTile = [&](const PrefabBrowserEntry& prefab, int index) {
            ImGui::PushID(index);
            const auto& pname = prefab.name;
            const bool isSelected = (m_selectedPrefabIndex == index);
            auto tex = prefab.textureId.empty() ? nullptr : Assets::TextureManager::Get(prefab.textureId);
            bool clicked = false;
            if (tex && tex->id() != 0) {
                clicked = ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)tex->id(),
                                             ImVec2(thumbSize, thumbSize), ImVec2(0,1), ImVec2(1,0));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.4f, 1.f));
                clicked = ImGui::Button("##fallback", ImVec2(thumbSize, thumbSize));
                ImGui::PopStyleColor();
            }
            if (clicked) {
                m_selectedPrefabIndex = index;
                m_selectedTilePrefab  = pname;
            }

            if (isSelected) {
                ImVec2 rmin = ImGui::GetItemRectMin();
                ImVec2 rmax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(rmin, rmax,
                    IM_COL32(255, 200, 0, 255), 0.f, 0, 2.f);
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("PREFAB_NAME", pname.c_str(), pname.size() + 1);
                ImGui::Text("Drag: %s", pname.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Spawn at Origin"))
                    spawnPrefab(pname, {0.f, 0.f});
                if (ImGui::MenuItem("Rename")) {
                    m_renamingPrefabPath = (fs::path("assets/prefabs") / (pname + ".json")).string();
                    strncpy_s(m_renamePrefabBuf, pname.c_str(),
                              sizeof(m_renamePrefabBuf) - 1);
                }
                if (ImGui::MenuItem("Clone")) {
                    m_cloningPrefabPath = (fs::path("assets/prefabs") / (pname + ".json")).string();
                    std::string cloneName = pname + "_copy";
                    strncpy_s(m_clonePrefabBuf, cloneName.c_str(),
                              sizeof(m_clonePrefabBuf) - 1);
                }
                if (ImGui::BeginMenu("Change Category")) {
                    for (const auto& category : m_availablePrefabCategories) {
                        if (category == prefab.category)
                            continue;
                        if (ImGui::MenuItem(category.c_str()))
                            updatePrefabCategory(pname, category);
                    }
                    if (ImGui::MenuItem("Custom...")) {
                        m_changeCategoryPrefabName = pname;
                        strncpy_s(m_changeCategoryBuf, prefab.category.c_str(),
                                  sizeof(m_changeCategoryBuf) - 1);
                        m_changeCategoryUseCustom = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Delete Prefab"))
                    deletePrefab(pname);
                ImGui::EndPopup();
            }

            ImGui::TextWrapped("%s", pname.c_str());
            ImGui::TextDisabled("%s", prefab.category.c_str());
            ImGui::PopID();
        };

        const float prefabListHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##prefablist", ImVec2(0, prefabListHeight), true);
        for (const auto& category : m_availablePrefabCategories) {
            const int currentCategoryCount = categoryCount(category);

            std::string headerLabel = category + " (" + std::to_string(currentCategoryCount) +
                                      ")##prefabcat_" + category;
            const bool headerOpen =
                ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("PREFAB_CATEGORY_NAME", category.c_str(), category.size() + 1);
                ImGui::Text("Move category: %s", category.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_CATEGORY_NAME")) {
                    const char* draggedCategory = static_cast<const char*>(payload->Data);
                    if (draggedCategory != nullptr && category != draggedCategory) {
                        auto fromIt = std::find(m_availablePrefabCategories.begin(),
                                                m_availablePrefabCategories.end(), draggedCategory);
                        auto toIt = std::find(m_availablePrefabCategories.begin(),
                                              m_availablePrefabCategories.end(), category);
                        if (fromIt != m_availablePrefabCategories.end() &&
                            toIt != m_availablePrefabCategories.end()) {
                            const std::string movedCategory = *fromIt;
                            const auto fromIndex = std::distance(m_availablePrefabCategories.begin(), fromIt);
                            const auto toIndex = std::distance(m_availablePrefabCategories.begin(), toIt);
                            m_availablePrefabCategories.erase(fromIt);
                            auto insertIt = m_availablePrefabCategories.begin() + toIndex;
                            if (fromIndex < toIndex)
                                --insertIt;
                            m_availablePrefabCategories.insert(insertIt, movedCategory);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (!headerOpen)
                continue;

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_NAME")) {
                    const char* droppedPrefab = static_cast<const char*>(payload->Data);
                    if (droppedPrefab != nullptr)
                        updatePrefabCategory(droppedPrefab, category);
                }
                ImGui::EndDragDropTarget();
            }

            std::string tableId = "##prefabgrid_" + category;
            if (ImGui::BeginTable(tableId.c_str(), cols)) {
                for (int i = 0; i < static_cast<int>(m_availablePrefabEntries.size()); ++i) {
                    if (m_availablePrefabEntries[i].category != category)
                        continue;
                    ImGui::TableNextColumn();
                    renderPrefabTile(m_availablePrefabEntries[i], i);
                }
                ImGui::EndTable();
            }
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

// ---------------------------------------------------------------------------
// BSA Logic
// ---------------------------------------------------------------------------

static std::string formatBytes(size_t bytes) {
  if (bytes < 1024) return std::to_string(bytes) + " B";
  if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
  if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
  return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

void EditorState::scanBSAAssets() {
  m_bsaAssets.clear();
  std::error_code ec;
  for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/", ec)) {
    if (entry.is_regular_file()) {
      BSAAssetInfo info;
      info.name = entry.path().filename().string();
      info.path = entry.path().generic_string();
      info.sizeBytes = std::filesystem::file_size(entry, ec);
      info.isSelected = false;
      m_bsaAssets.push_back(info);
    }
  }
}

void EditorState::saveBSASelection() {
  nlohmann::json j = nlohmann::json::array();
  for (const auto& asset : m_bsaAssets) {
    if (asset.isSelected) {
      j.push_back(asset.path);
    }
  }
  std::ofstream out("assets/configs/bsa_selections.json");
  if (out.is_open()) {
    out << j.dump(4);
  }
}

void EditorState::loadBSASelection() {
  std::ifstream in("assets/configs/bsa_selections.json");
  if (!in.is_open()) return;
  try {
    nlohmann::json j;
    in >> j;
    for (auto& asset : m_bsaAssets) {
      for (const auto& path : j) {
        if (asset.path == path.get<std::string>()) {
          asset.isSelected = true;
          break;
        }
      }
    }
  } catch (...) {}
}

void EditorState::renderBSATab() {
  if (m_showBSAWindow && !m_wasBSAWindowOpen) {
    scanBSAAssets();
    loadBSASelection();
  }
  m_wasBSAWindowOpen = m_showBSAWindow;

  if (!m_showBSAWindow) return;

  ImGui::Begin("BSA", &m_showBSAWindow);

  if (ImGui::Button("Select All")) {
    for (auto& asset : m_bsaAssets) asset.isSelected = true;
    saveBSASelection();
  }
  ImGui::SameLine();
  if (ImGui::Button("Deselect All")) {
    for (auto& asset : m_bsaAssets) asset.isSelected = false;
    saveBSASelection();
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    scanBSAAssets();
    loadBSASelection();
  }

  size_t totalSize = 0;
  for (const auto& asset : m_bsaAssets) {
    if (asset.isSelected) totalSize += asset.sizeBytes;
  }
  
  ImGui::Text("Total Selected Size: %s", formatBytes(totalSize).c_str());
  
  if (ImGui::Button("Export Selected", ImVec2(120, 0))) {
#ifdef _WIN32
    // Use native Windows folder-picker dialog
    BROWSEINFOA bi = {};
    bi.lpszTitle  = "Select destination folder for export";
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
      char folderPath[MAX_PATH] = {};
      if (SHGetPathFromIDListA(pidl, folderPath)) {
        std::string destDir = folderPath;
        int successCount = 0;
        for (const auto& asset : m_bsaAssets) {
          if (asset.isSelected) {
            std::error_code ec;
            std::filesystem::path destPath = std::filesystem::path(destDir) / std::filesystem::path(asset.path).filename();
            std::filesystem::copy_file(asset.path, destPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) successCount++;
          }
        }
        Core::Engine::Get().logger().info("[BSA] Exported " + std::to_string(successCount) + " items to " + destDir);
      }
      CoTaskMemFree(pidl);
    }
#endif
  }

  if (ImGui::BeginChild("AssetList", ImVec2(0, 0), true)) {
    if (ImGui::BeginTable("BSAAssets", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 50.0f);
      ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableHeadersRow();

      int rowIdx = 0;
      for (auto& asset : m_bsaAssets) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        bool checked = asset.isSelected;
        ImGui::PushID(asset.path.c_str());
        if (ImGui::Checkbox("##sel", &checked)) {
          if (ImGui::GetIO().KeyShift && m_bsaLastSelectedIdx >= 0 && m_bsaLastSelectedIdx < (int)m_bsaAssets.size()) {
            int start = std::min(m_bsaLastSelectedIdx, rowIdx);
            int end = std::max(m_bsaLastSelectedIdx, rowIdx);
            for (int i = start; i <= end; ++i) {
              m_bsaAssets[i].isSelected = checked;
            }
          } else {
            asset.isSelected = checked;
            m_bsaLastSelectedIdx = rowIdx;
          }
          saveBSASelection();
        }
        ImGui::PopID();
        
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", asset.path.c_str());
        
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", formatBytes(asset.sizeBytes).c_str());
        rowIdx++;
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

} // namespace Runtime































