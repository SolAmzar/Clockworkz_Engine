/******************************************************************************/
/*!
\file           EditorState.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 73% [Amzar] 23% [Lebon] 4%
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
#include "Components/SpriteAnimator.hpp"
#include <glm/vec2.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Runtime {
    struct BSAAssetInfo {
        std::string name;
        std::string path;
        size_t sizeBytes;
        bool isSelected;
    };
}

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
    friend class CreateTilemapTileCmd;
    friend class HierarchyChangeCmd;
    friend class GroupCloneCmd;
    friend class DeleteTilemapTileCmd;

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
    void renderAnimationEditor();
    void renderAudioBrowser();
    void renderPrefabBrowser();
    void renderTilePalette();
    void renderFontBrowser();
    void invalidateBrowserCaches();

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
    bool        updatePrefabCategory(const std::string& prefabName, const std::string& category);
    void        loadAvailableEntityGroups();
    void        saveEntityGroupFromSelection(const std::string& name, const std::string& category);
    std::vector<ECS::Entity> spawnEntityGroup(const std::string& groupName, vec2 position);

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

    // --- Hierarchy groups (folders) ---
    struct HierarchyGroup {
        std::string           name{"Group"};
        std::vector<uint32_t> entityIds;   // stored as IDs (generation-independent)
        bool                  collapsed{false};
        int                   sortOrderBase{0};
        int                   parentGroupIdx{-1};
    };
    struct HierarchyUndoState {
        std::vector<uint32_t>        hierarchyOrder;
        std::vector<HierarchyGroup>  hierarchyGroups;
        std::unordered_set<uint32_t> hiddenEntities;
        std::vector<ECS::Entity>     selectedEntities;
        ECS::Entity                  selectedEntity{0, 0};
        int                          selectedGroupIdx{-1};
        int                          renamingGroupIdx{-1};
        uint32_t                     lastAnchorId{0};
    };
    struct DirectoryListingCache {
        std::string                    dir;
        std::vector<std::filesystem::path> items;
        double                         lastRefreshTime{-1.0};
        bool                           dirty{true};
    };
    struct AssetSearchCache {
        std::string                      query;
        bool                             includeFolders{true};
        std::vector<std::filesystem::path> items;
        std::unordered_map<std::string, int> duplicateCountsByAssetId;
        bool                             dirty{true};
    };
    enum class PaintMode { Paint, Erase, Select };

    // ----- Tilemap helpers -----
    void         handleTileMapInput(const vec2& viewportMin);
    void         saveTilemap(const std::string& filepath);
    void         loadTilemap(const std::string& filepath);
    void         clearTilemap();
    void         flushTilemapCollisionRebuild();
    bool         isCellOccupied(vec2 worldPos, int layer) const;
    EntityGroup* getOrCreateLayerGroup(int layer);
    void         removeEntityFromGroups(ECS::Entity entity);

    // ----- Undo/redo -----
    void executeRecordedCommand(std::unique_ptr<IEditorCommand> cmd);
    void executeCommand(std::unique_ptr<IEditorCommand> cmd);
    void undo();
    void redo();

    // ----- Inspector undo helpers -----
    void captureInspectorBefore();   ///< snapshot entity state at widget activation
    void pushInspectorChange();      ///< diff against current state and push ModifyEntityCmd
    HierarchyUndoState captureHierarchyUndoState() const;
    void applyHierarchyUndoState(const HierarchyUndoState& state);
    std::vector<uint32_t> collectGroupEntityIdsRecursive(int rootGroupIdx) const;

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

    // ----- Hierarchy ordering & groups -----
    std::vector<uint32_t>        m_hierarchyOrder;   // all entity IDs in display order
    std::vector<HierarchyGroup>  m_hierarchyGroups;  // folder groups
    int                          m_selectedGroupIdx{-1};
    int                          m_renamingGroupIdx{-1};

    // ----- Hidden entities (editor-only, not saved as gameplay data) -----
    std::unordered_set<uint32_t> m_hiddenEntities;

    // ----- Viewport -----
    vec2                    m_viewportSize{800.0f, 600.0f};
    bool                    m_viewportFocused{false};
    bool                    m_viewportHovered{false};
    bool                    m_viewportShouldRender{true};
    double                  m_lastViewportRenderTime{-1.0};

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
    bool                    m_showPickBounds{false};
    bool                    m_showMergedColliders{false};

    // ----- UIButton hit-area debug -----
    bool                    m_showUIButtons{false};

    // ----- TextRenderer box debug -----
    bool                    m_showTextBoxes{false};

    // ----- Stats memory graph -----
    float                   m_statsHistory[128]{};
    int                     m_statsHistoryOffset{0};

    // ----- Asset browser -----
    std::string             m_selectedAsset;
    std::vector<std::string> m_selectedAssetPaths;
    std::string             m_selectedAssetFolderPath;
    char                    m_newAssetNameBuf[128]{};
    char                    m_newAssetPathBuf[256]{};
    std::string             m_currentAssetDir;
    char                    m_assetSearchBuf[128]{};
    std::string             m_renamingAssetDirPath;
    char                    m_renameAssetDirBuf[256]{};
    std::string             m_deletingAssetDirPath;
    std::vector<std::string> m_movingAssetPaths;
    DirectoryListingCache   m_assetBrowserCache;
    AssetSearchCache        m_assetSearchCache;

    // ----- Dockspace -----
    bool                    m_dockLayoutInitialized{false};

    // ----- Undo/redo -----
    static constexpr size_t MAX_UNDO = 50;
    static constexpr double VIEWPORT_BACKGROUND_REFRESH_INTERVAL = 0.25;
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;

    // ----- Inspector undo tracking -----
    EntitySnapshot  m_inspectorBefore{};
    ECS::Entity     m_inspectorBeforeEntity{0, 0};

    // ----- Deferred actions -----
    bool                    m_pendingReturnToHub{false};
    bool                    m_pendingSave{false};
    bool                    m_pendingPlay{false};

    // ----- Load scene window -----
    char                    m_loadSceneFilter[128]{};
    std::string             m_pendingLoadPath;
    char                    m_newSceneBuf[128]{};
    std::string             m_renamingScenePath;
    char                    m_renameSceneBuf[128]{};
    std::string             m_cloningScenePath;
    char                    m_cloneSceneBuf[128]{};
    std::string             m_deletingScenePath;
    bool                    m_sceneMetaBuffersDirty{true};
    char                    m_sceneBgmBuf[128]{};
    char                    m_sceneMusicContextBuf[128]{};
    vec2                    m_sceneSpawnPointBuf{0.0f, 0.0f};

    // ----- Asset rename/delete -----
    std::string             m_renamingAsset;
    char                    m_renameAssetBuf[128]{};
    std::vector<std::string> m_deletingAssetPaths;
    bool                    m_assetSearchIncludeFolders{true};

    // ----- Texture hot-reload -----
    void        initTextureFilePaths();
    void        checkAssetHotReload();
    std::unordered_map<std::string, std::string>                       m_textureFilePaths;
    std::unordered_map<std::string, std::filesystem::file_time_type>   m_assetFileTimestamps;
    bool        m_hotReloadEnabled{true};
    float       m_hotReloadTimer{0.f};
    static constexpr float HOT_RELOAD_INTERVAL = 1.0f;
    static constexpr double BROWSER_CACHE_REFRESH_INTERVAL = 3.0;

    // ----- Scripts browser -----
    char                    m_newScriptBuf[128]{};
    char                    m_newScriptFolderBuf[64]{};   // subfolder for new script
    std::string             m_currentScriptDir;
    DirectoryListingCache   m_scriptBrowserCache;
    std::string             m_renamingScriptPath;
    char                    m_renameScriptBuf[256]{};
    std::string             m_deletingScriptPath;
    std::string             m_movingScriptPath;           // script being moved to a folder

    // ----- Animations browser -----
    std::string             m_currentAnimDir;
    DirectoryListingCache   m_animBrowserCache;
    std::string             m_renamingAnimPath;
    char                    m_renameAnimBuf[256]{};
    std::string             m_deletingAnimPath;

    // ----- Animation editor panel -----
    bool                        m_animEditorOpen{false};
    std::string                 m_editingAnimPath{};
    std::string                 m_editingAnimRelPath{};
    Components::SpriteAnimator  m_animEditorAnim{};
    std::string                 m_animEdClip{};
    int                         m_animEdFrame{-1};
    bool                        m_animEdPlaying{false};
    bool                        m_animEdLoop{true};
    float                       m_animEdTimer{0.f};
    int                         m_animEdSheetW{512};
    int                         m_animEdSheetH{512};
    int                         m_animEdCellW{64};
    int                         m_animEdCellH{64};
    int                         m_animEdStartF{0};
    int                         m_animEdEndF{7};
    char                        m_animEdNewClipName[128]{};
    bool                        m_newAnimModalOpen{false};
    char                        m_newAnimNameBuf[128]{};
    std::string                 m_animEdTexture{};       // selected texture for preview
    Graphics::FrameBuffer       m_animPreviewFB{};

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
    DirectoryListingCache   m_audioBrowserCache;
    std::string             m_renamingAudioPath;
    char                    m_renameAudioBuf[256]{};
    std::string             m_editingAudio;
    std::vector<std::string> m_selectedAudioAssets;
    bool                    m_openBatchEditAudioPopup{false};
    bool                    m_batchApplyLoop{false};
    bool                    m_batchAudioLoop{false};
    bool                    m_batchApplyStream{false};
    bool                    m_batchAudioStream{false};
    bool                    m_batchApplyIs3D{false};
    bool                    m_batchAudioIs3D{false};
    bool                    m_batchApplyGroup{false};
    bool                    m_batchAudioIsBGM{false};
    bool                    m_batchApplyVolume{false};
    float                   m_batchAudioVolume{1.0f};
    bool                    m_batchApplyPitch{false};
    float                   m_batchAudioPitch{1.0f};
    bool                    m_batchApplyMinDistance{false};
    float                   m_batchAudioMinDistance{80.0f};
    bool                    m_batchApplyMaxDistance{false};
    float                   m_batchAudioMaxDistance{520.0f};
    bool                    m_batchApplyFalloffStart{false};
    float                   m_batchAudioFalloffStart{120.0f};
    bool                    m_batchApplyFalloffAmount{false};
    float                   m_batchAudioFalloffAmount{0.25f};
    bool                    m_batchApplyFilterInStart{false};
    float                   m_batchAudioFilterInStart{180.0f};
    bool                    m_batchApplyFilterPeak{false};
    float                   m_batchAudioFilterPeak{320.0f};
    bool                    m_batchApplyFilterReleaseEnd{false};
    float                   m_batchAudioFilterReleaseEnd{480.0f};
    char                    m_editAudioNameBuf[128]{};
    char                    m_editAudioPathBuf[256]{};
    bool                    m_editAudioLoop{false};
    bool                    m_editAudioStream{false};
    bool                    m_editAudioIs3D{false};
    float                   m_editAudioVolume{1.0f};
    float                   m_editAudioPitch{1.0f};
    bool                    m_editAudioIsBGM{false};
    float                   m_editAudioMinDistance{80.0f};
    float                   m_editAudioMaxDistance{520.0f};
    float                   m_editAudioFalloffStart{120.0f};
    float                   m_editAudioFalloffAmount{0.25f};
    float                   m_editAudioFilterInStart{180.0f};
    float                   m_editAudioFilterPeak{320.0f};
    float                   m_editAudioFilterReleaseEnd{480.0f};
    std::string             m_deletingAudio;
    std::string             m_deletingAudioPath;
    std::string             m_movingAudioPath;
    bool                    m_openMoveSelectedAudioPopup{false};
    std::string             m_audioBrowserStatusMessage;
    bool                    m_audioBrowserStatusIsError{false};
    double                  m_audioBrowserStatusUntil{0.0};

    // ----- Font browser -----
    char                     m_newFontNameBuf[128]{};
    char                     m_newFontPathBuf[512]{};
    int                      m_newFontSize{32};
    std::string              m_deletingFont;
    bool                     m_pendingFontImportPopup{false};

    // ----- Prefab Browser -----
    struct PrefabBrowserEntry {
        std::string name;
        std::string category;
        std::string textureId;
    };
    struct EntityGroupBrowserEntry {
        std::string name;
        std::string category;
        std::string textureId;
        int         entityCount{0};
    };
    std::vector<std::string>     m_availablePrefabs;
    std::vector<PrefabBrowserEntry> m_availablePrefabEntries;
    std::vector<std::string>     m_availablePrefabCategories;
    int                          m_selectedPrefabIndex{-1};
    char                         m_savePrefabNameBuf[128]{};
    char                         m_savePrefabCategoryBuf[64]{"Custom"};
    bool                         m_savePrefabUseCustomCategory{true};
    std::string                  m_changeCategoryPrefabName;
    char                         m_changeCategoryBuf[64]{};
    bool                         m_changeCategoryUseCustom{false};
    std::string                  m_renamingPrefabPath;
    char                         m_renamePrefabBuf[128]{};
    std::string                  m_cloningPrefabPath;
    char                         m_clonePrefabBuf[128]{};
    std::vector<EntityGroupBrowserEntry> m_availableEntityGroupEntries;
    std::vector<std::string>             m_availableEntityGroupCategories;
    char                                 m_saveEntityGroupNameBuf[128]{};
    char                                 m_saveEntityGroupCategoryBuf[64]{"Custom"};
    bool                                 m_saveEntityGroupUseCustomCategory{true};

    // ----- BSA -----
    void renderBSATab();
    void scanBSAAssets();
    void saveBSASelection();
    void loadBSASelection();

    std::vector<BSAAssetInfo> m_bsaAssets;
    int                       m_bsaLastSelectedIdx{-1};
    bool                      m_showBSAWindow{false};
    bool                      m_wasBSAWindowOpen{false};

    // ----- Tilemap -----
    std::vector<TileData>    m_tilemap;
    std::vector<EntityGroup> m_entityGroups;
    bool                     m_tileMapMode{false};
    bool                     m_showTilePalette{false};
    bool                     m_paintedThisFrame{false};
    bool                     m_tilemapDirty{false}; ///< Set when tiles are added/removed; triggers merged-collider rebuild on mouse release
    bool                     m_tileStrokeActive{false};
    std::unique_ptr<CompositeEditorCmd> m_activeTileStrokeCommand;
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
    vec2                    m_bulkColliderSizeDelta{0.f, 0.f};
    vec2                    m_bulkColliderOffsetDelta{0.f, 0.f};
    vec2                    m_lastPickScreenPos{0.f, 0.f};
    bool                    m_hasLastPickScreenPos{false};
    double                  m_lastPickTimeSeconds{-1000.0};
    static constexpr float  kDragThreshold = 5.0f;         // pixels before drag starts

    // ----- Box selection -----
    bool                    m_isBoxSelecting{false};
    vec2                    m_boxSelectStart{0.f, 0.f};  // screen-space
    vec2                    m_boxSelectEnd{0.f, 0.f};    // screen-space

    // ----- Rename state -----
    bool                    m_isRenaming{false};
    ECS::Entity             m_renamingEntity{0, 0};
    char                    m_renameBuffer[256]{};

    // ----- Hierarchy multi-select anchor (shift+click range) -----
    uint32_t                m_lastAnchorId{0};  // ID of the last plain-clicked entity
};

} // namespace Runtime




