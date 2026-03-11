/******************************************************************************/
/*!
\file           EditorState.hpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Generic engine-level editor state

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "Runtime/EditorCommand.hpp"
#include "Graphics/FrameBuffer.hpp"
#include "ECS/Entity.hpp"
#include "Math/Vec2.hpp"
#include <glm/vec2.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Platform { class InputManager; }
namespace Core    { class Time; }
namespace Runtime { class GameStateManager; }

namespace Runtime {

class EditorState final : public GameState {
public:
    EditorState(Platform::InputManager& input, GameStateManager& gsm,
                std::string scenePath);

    const char* name() const override { return "Editor"; }

    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    // ----- Panels -----
    void renderToolbar();
    void renderHierarchy();
    void renderInspector();
    void renderViewport();
    void renderAssetBrowser();
    void renderSystemsWindow();
    void renderLoadSceneWindow();
    void renderStatsWindow();
    void renderScriptsBrowser();
    void renderAnimationsBrowser();
    void renderAudioBrowser();
    void renderPrefabBrowser();
    void renderTilePalette();
    void renderFontBrowser();

    // ----- Scene helpers -----
    void renderGrid();
    void renderGizmos(const vec2& viewportMin);
    void loadScene(const std::string& path);
    void saveScene();

    // ----- Prefab helpers -----
    void        loadAvailablePrefabs();
    void        savePrefabFromEntity(ECS::Entity entity, const std::string& name, const std::string& category);
    ECS::Entity spawnPrefab(const std::string& prefabName, vec2 position);
    void        deletePrefab(const std::string& prefabName);

    // ----- Tilemap types (declared here so method signatures can use them) -----
    struct TileData {
        std::string prefabName;
        vec2        position{};
        int         layer{0};
        ECS::Entity entity{0, 0};
    };
    struct EntityGroup {
        std::string              name;
        std::vector<ECS::Entity> entities;
    };
    enum class PaintMode { Paint, Erase, Select };

    // ----- Tilemap helpers -----
    void         handleTileMapInput(const vec2& viewportMin);
    void         saveTilemap(const std::string& filepath);
    void         loadTilemap(const std::string& filepath);
    void         clearTilemap();
    bool         isCellOccupied(vec2 worldPos, int layer) const;
    EntityGroup* getOrCreateLayerGroup(int layer);
    void         removeEntityFromGroups(ECS::Entity entity);

    // ----- Undo/redo -----
    void executeCommand(std::unique_ptr<IEditorCommand> cmd);
    void undo();
    void redo();

    // ----- References -----
    Platform::InputManager& m_input;
    GameStateManager&       m_gsm;

    // ----- Scene -----
    std::string             m_sceneFilePath;
    std::string             m_projectRoot;       // Absolute path to the open project root
    std::string             m_engineAssetsRoot;  // Absolute path to game/assets/ (shaders, fonts)
    std::string             m_originalCwd;       // CWD before project opened; restored on exit
    Scene                   m_scene{"Editor"};
    Graphics::FrameBuffer   m_framebuffer;

    // ----- Entity selection -----
    ECS::Entity             m_selectedEntity{0, 0};
    std::vector<ECS::Entity> m_selectedEntities;    // all selected (incl. primary)

    // ----- Viewport -----
    vec2                    m_viewportSize{800.0f, 600.0f};
    bool                    m_viewportFocused{false};
    bool                    m_viewportHovered{false};

    // ----- Editor camera -----
    glm::vec2               m_editorCamPos{0.0f, 0.0f};
    float                   m_editorCamZoom{1.0f};
    double                  m_lastMouseX{0.0};
    double                  m_lastMouseY{0.0};

    // ----- Grid -----
    bool                    m_showGrid{true};
    float                   m_gridSize{100.0f};
    bool                    m_snapToGrid{false};

    // ----- Collider debug -----
    bool                    m_showColliders{false};

    // ----- Stats memory graph -----
    float                   m_statsHistory[128]{};
    int                     m_statsHistoryOffset{0};

    // ----- Asset browser -----
    std::string             m_selectedAsset;
    char                    m_newAssetNameBuf[128]{};
    char                    m_newAssetPathBuf[256]{};
    std::string             m_currentAssetDir;
    std::string             m_renamingAssetDirPath;
    char                    m_renameAssetDirBuf[256]{};
    std::string             m_deletingAssetDirPath;
    std::string             m_movingAssetPath;

    // ----- Dockspace -----
    bool                    m_dockLayoutInitialized{false};

    // ----- Undo/redo -----
    static constexpr size_t MAX_UNDO = 50;
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;

    // ----- Deferred actions -----
    bool                    m_pendingReturnToHub{false};
    bool                    m_pendingSave{false};
    bool                    m_pendingPlay{false};

    // ----- Load scene window -----
    char                    m_loadSceneFilter[128]{};
    std::string             m_pendingLoadPath;
    char                    m_newSceneBuf[128]{};

    // ----- Asset rename/delete -----
    std::string             m_renamingAsset;
    char                    m_renameAssetBuf[128]{};
    std::string             m_deletingAsset;

    // ----- Texture hot-reload -----
    void        initTextureFilePaths();
    void        checkAssetHotReload();
    std::unordered_map<std::string, std::string>                       m_textureFilePaths;
    std::unordered_map<std::string, std::filesystem::file_time_type>   m_assetFileTimestamps;
    bool        m_hotReloadEnabled{true};
    float       m_hotReloadTimer{0.f};
    static constexpr float HOT_RELOAD_INTERVAL = 1.0f;

    // ----- Scripts browser -----
    char                    m_newScriptBuf[128]{};
    char                    m_newScriptFolderBuf[64]{};   // subfolder for new script
    std::string             m_currentScriptDir;
    std::string             m_renamingScriptPath;
    char                    m_renameScriptBuf[256]{};
    std::string             m_deletingScriptPath;
    std::string             m_movingScriptPath;           // script being moved to a folder

    // ----- Animations browser -----
    std::string             m_currentAnimDir;
    std::string             m_renamingAnimPath;
    char                    m_renameAnimBuf[256]{};
    std::string             m_deletingAnimPath;

    // ----- Audio browser -----
    char                    m_newAudioNameBuf[128]{};
    char                    m_newAudioPathBuf[256]{};
    bool                    m_newAudioLoop{false};
    bool                    m_newAudioStream{false};
    bool                    m_newAudioIs3D{false};
    float                   m_newAudioVolume{1.0f};
    float                   m_newAudioPitch{1.0f};
    bool                    m_newAudioIsBGM{false};
    std::string             m_currentAudioDir;
    std::string             m_renamingAudioPath;
    char                    m_renameAudioBuf[256]{};
    std::string             m_editingAudio;
    char                    m_editAudioNameBuf[128]{};
    char                    m_editAudioPathBuf[256]{};
    bool                    m_editAudioLoop{false};
    bool                    m_editAudioStream{false};
    bool                    m_editAudioIs3D{false};
    float                   m_editAudioVolume{1.0f};
    float                   m_editAudioPitch{1.0f};
    bool                    m_editAudioIsBGM{false};
    std::string             m_deletingAudio;
    std::string             m_deletingAudioPath;
    std::string             m_movingAudioPath;

    // ----- Font browser -----
    char                     m_newFontNameBuf[128]{};
    char                     m_newFontPathBuf[512]{};
    int                      m_newFontSize{32};
    std::string              m_deletingFont;
    bool                     m_pendingFontImportPopup{false};

    // ----- Prefab Browser -----
    std::vector<std::string> m_availablePrefabs;
    int                      m_selectedPrefabIndex{-1};
    char                     m_savePrefabNameBuf[128]{};
    char                     m_savePrefabCategoryBuf[64]{"Custom"};

    // ----- Tilemap -----
    std::vector<TileData>    m_tilemap;
    std::vector<EntityGroup> m_entityGroups;
    bool                     m_tileMapMode{false};
    bool                     m_showTilePalette{false};
    bool                     m_paintedThisFrame{false};
    std::string              m_selectedTilePrefab;
    PaintMode                m_paintMode{PaintMode::Paint};
    int                      m_currentLayer{0};
    int                      m_numLayers{3};
    std::vector<bool>        m_layerVisible;
    char                     m_tilemapPathBuf[256]{"assets/scenes/tilemap.json"};

    // ----- Gizmos -----
    enum class GizmoMode { None, Translation, Rotation, Scale };
    GizmoMode               m_gizmoMode{GizmoMode::Translation};
    int                     m_gizmoActiveAxis{-1}; // 0=X 1=Y 2=Rotate -1=None
    bool                    m_gizmoHoveredHandle{false}; // true when mouse is over a handle (prev frame)
    vec2                    m_gizmoOriginalPos{0.f, 0.f};
    vec2                    m_gizmoClickOffset{0.f, 0.f};
    float                   m_gizmoOriginalRotation{0.f};
    vec2                    m_gizmoOriginalScale{1.f, 1.f};

    // Group gizmo: original positions/rotations/scales for all selected entities
    std::vector<std::pair<ECS::Entity, vec2>>  m_gizmoGroupOriginalPos;
    std::vector<std::pair<ECS::Entity, float>> m_gizmoGroupOriginalRot;
    std::vector<std::pair<ECS::Entity, vec2>>  m_gizmoGroupOriginalScale;

    // ----- Direct entity drag (viewport body drag) -----
    bool                    m_pendingEntityDrag{false};
    bool                    m_isDraggingEntity{false};
    vec2                    m_dragStartMousePos{0.f, 0.f};  // screen-space click pos
    vec2                    m_dragClickWorldPos{0.f, 0.f};  // world-space click pos
    static constexpr float  kDragThreshold = 5.0f;         // pixels before drag starts

    // ----- Box selection -----
    bool                    m_isBoxSelecting{false};
    vec2                    m_boxSelectStart{0.f, 0.f};  // screen-space
    vec2                    m_boxSelectEnd{0.f, 0.f};    // screen-space

    // ----- Rename state -----
    bool                    m_isRenaming{false};
    ECS::Entity             m_renamingEntity{0, 0};
    char                    m_renameBuffer[256]{};
};

} // namespace Runtime

