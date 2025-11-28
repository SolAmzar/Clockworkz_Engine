/******************************************************************************/
/*!
\file           LevelEditor.hpp
\project        Lights Out - Level Editor
\author(s)      Yusuf,[100%]
\brief          Comprehensive level editor with integrated gizmo system,
                property editing, hierarchy view, and multi-level support.

                Features:
                - Grid-based level editing with snapping
                - Property editor for selected objects
                - Hierarchy window showing all objects
                - Multiple level save/load functionality
                - Prefab/archetype system integration
                - Play/Stop testing mode
                - Drag-and-drop asset placement
                - Free camera navigation with docking support
                - In-world object picking with gizmos
                - INTEGRATED GIZMO: Translate, Rotate, Scale tools

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include "math/Vec2.hpp"
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include <glm/vec4.hpp>
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Prefab.hpp"
#include "Graphics/Camera2D.hpp"
#include <Core/Logger.hpp>
#include <Assets/ResourceManager.hpp>
#include "Platform/UndoSystem.hpp"
#include "IMGUI/imgui.h"
#include "Graphics/FrameBuffer.hpp"

struct GLFWwindow;

namespace Graphics {
    class RenderingManager;
    class ImGuiManager;
    class Camera2D;
}

namespace Platform {
    class InputManager;
    class WindowManager;
}

namespace Runtime {
    class Scene;
}

namespace Platform {

    // Editor modes
    enum class EditorMode {
        Edit,       // Normal editing mode
        Play        // Testing/play mode
    };

    // Tool types for placement
    enum class EditorTool {
        Select,     // Selection and manipulation
        Place,      // Place new objects
        Erase,      // Delete objects
        Pan         // Camera panning
    };

    // INTEGRATED GIZMO: Gizmo mode enumeration
    enum class GizmoMode {
        None,
        Translate,
        Rotate,
        Scale
    };

    // INTEGRATED GIZMO: Axis selection for manipulation
    enum class GizmoAxis {
        None,
        X,
        Y,
        Both  // For uniform scaling or XY plane
    };

    // Prefab palette entry
    struct PrefabEntry {
        std::string name;
        std::string displayName;
        std::string category;
        std::string iconPath;
        Gameplay::GameObjectType type;
    };

    // Level metadata
    struct LevelMetadata {
        std::string name;
        std::string filepath;
        std::string description;
        vec2 spawnPoint = vec2(0.0f, 0.0f);
        int lastModified = 0;
    };

    /******************************************************************************/
    /*!
    \class  LevelEditor
    \brief  Main level editor class with integrated gizmo functionality
    */
    /******************************************************************************/
    class LevelEditor {
    public:
        LevelEditor();
        ~LevelEditor();

        // Initialization
        bool init(GLFWwindow* window,
            Graphics::RenderingManager& renderer,
            Platform::InputManager& input,
            Platform::WindowManager& windowMgr);

        void shutdown();

        // NEW: Load scene from PlayState
        void loadSceneFromPlayState(Runtime::Scene* playStateScene);

        // NEW: Sync changes back to PlayState
        void syncSceneBackToPlayState(Runtime::Scene* playStateScene);

        // Editor activation
        void setActive(bool active);
        bool isPointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon);
        bool isActive() const { return m_editorActive; }

        // Access to framebuffer for external rendering
        Graphics::FrameBuffer& getFramebuffer() { return m_sceneFramebuffer; }
        const Graphics::FrameBuffer& getFramebuffer() const { return m_sceneFramebuffer; }

        Graphics::Camera2D& getCamera() { return m_editorCamera; }
        const Graphics::Camera2D& getCamera() const { return m_editorCamera; }

        void handlePlayerInput(Gameplay::GameObject* player, double dt);

        // NEW: Get play mode state
        bool isInPlayMode() const { return m_mode == EditorMode::Play; }

        // Main loop functions
        void update(double dt);
        void initializePlayMode();
        void renderSaveDialog();
        void renderLoadDialog();
        void render();
        void renderReplace();
        void renderUI();

        // Editor state
        EditorMode getMode() const { return m_mode; }
        void setMode(EditorMode mode);
        void togglePlayMode();

        // Level management
        // 
        // NEW: Play mode simulation
        void updatePlayMode(double dt);
        void updatePhysicsAndGameplay(Runtime::Scene* scene, double dt);

        // NEW: Store gameplay state for play mode
        Gameplay::GameObject* m_playModePlayer;
        float m_playModeLeftBound;
        float m_playModeRightBound;
        float m_playModeBallSpeed;
        int m_playModeBallDir;
        float m_playModeDistanceSinceStep;
        float m_playModeStepDistance;
        float m_playModeTimeSinceStep;
        float m_playModeMinStepInterval;
        vec2 m_playModeLastFootPos;
        bool m_playModeHaveLastFootPos;

        // NEW: Flag to indicate scene was loaded from PlayState
        bool m_loadedFromPlayState;
        bool newLevel(const std::string& name);
        bool saveLevel();
        bool saveLevel(const std::string& filepath);
        bool loadLevel(const std::string& filepath);
        //bool saveLevelAs(const std::string& filepath);
        std::vector<std::string> getRecentLevels() const;
        const LevelMetadata& getCurrentLevelMetadata() const { return m_currentLevelMetadata; }

        // Object management
        Gameplay::GameObject* getSelectedObject() { return m_selectedObject; }
        void selectObject(Gameplay::GameObject* obj);
        void deleteSelectedObject();
        //void duplicateSelectedObject();

        // Prefab/Archetype system (Rubric 1206 - Advanced)
        void loadPrefabs();
        void createObjectFromPrefab(const std::string& prefabName, vec2 position);
        bool savePrefab(const std::string& name);
        bool loadPrefab(const std::string& filepath);
        const std::vector<PrefabEntry>& getPrefabPalette() const { return m_prefabPalette; }

        // Tool selection
        EditorTool getCurrentTool() const { return m_currentTool; }
        void setCurrentTool(EditorTool tool) { m_currentTool = tool; }

        // Camera control
        Graphics::Camera2D& getEditorCamera() { return m_editorCamera; }
        const Graphics::Camera2D& getEditorCamera() const { return m_editorCamera; }
        void resetCamera();

        // Scene access
        Runtime::Scene* getCurrentScene() { return m_currentScene.get(); }

        // Drag-and-drop support (Rubric 1209 - Advanced)
        void handleFileDrop(const std::vector<std::string>& paths);

    private:
        // UI Rendering (ImGui windows)
        void renderMenuBar();
        void renderToolbar();
        void renderHierarchy();
        void renderProperties();
        void copyComponents(Gameplay::GameObject* source, Gameplay::GameObject* dest);
        void renderPrefabPalette();
        void renderSceneView();
        void renderSceneViewDropZone();
        void renderLevelSettings();
        void renderCameraControls();
        void renderAssetBrowser();
        void renderTextureAssets();
        void renderAudioAssets();
        //void handleAssetFileWatch();
        void renderGrid();

        // Object manipulation
        void handleObjectSelection();
        void handleObjectPlacement();
        void handleObjectDeletion();
        void handleObjectDragging();
        void moveSelectedObject(vec2 delta);
        bool m_isDraggingObject = false;

        // Input handling
        void handleEditorInput(double dt);
        void handleCameraInput(double dt);
        void handleKeyboardShortcuts();

        // Level state management
        void saveEditorState();
        void restoreEditorState();
        void markLevelDirty();
        bool isLevelDirty() const { return m_levelDirty; }

        // Utility functions
        Gameplay::GameObject* getObjectAtPosition(vec2 worldPos);
        vec2 screenToWorld(vec2 screenPos) const;
        vec2 worldToScreen(vec2 worldPos) const;
        bool isMouseOverUI() const;
        vec2 snapToGrid(vec2 position) const;

        // Prefab management helpers
        void registerBuiltInPrefabs();
        void scanPrefabDirectory(const std::string& directory);

        // ====================================================================
        // INTEGRATED GIZMO SYSTEM - Private Methods
        // ====================================================================

        // Gizmo update functions for each mode
        void updateGizmoTranslate();
        void updateGizmoRotate();
        void updateGizmoScale();

        // Gizmo render functions for each mode
        void renderGizmoTranslate();
        void renderGizmoRotate();
        void renderGizmoScale();
        void renderGizmo();  // Main gizmo render call

        // Gizmo helper functions
        GizmoAxis getGizmoHoveredAxis(const vec2& mouseWorldPos) const;
        vec2 getGizmoCenterPosition() const;
        float getGizmoScaledSize() const;
        float snapAngleToGrid(float angle) const;
        float snapScaleToGrid(float scale) const;

        vec2 snapToHalfGrid(vec2 position) const;

        bool isMouseOverGizmo(const vec2& mouseWorldPos) const;

    private:
        // Core systems
        std::unique_ptr<Core::Logger> m_logger;
        GLFWwindow* m_window;
        Graphics::RenderingManager* m_renderer;
        Platform::InputManager* m_input;
        Platform::WindowManager* m_windowManager;

        // Scene management
        std::unique_ptr<Runtime::Scene> m_currentScene;
        std::unique_ptr<Runtime::Scene> m_playModeSceneBackup;  // For play/stop
        LevelMetadata m_currentLevelMetadata;
        bool m_levelDirty;

        // Editor state
        EditorMode m_mode;
        EditorTool m_currentTool;

        // Selection and manipulation
        Gameplay::GameObject* m_selectedObject;
        vec2 m_dragStartPos;

        // Camera
        Graphics::Camera2D m_editorCamera;
        vec2 m_cameraPanStart;
        bool m_isPanningCamera;
        float m_cameraSpeed;
        float m_cameraZoomSpeed;

        // Prefab system
        std::vector<PrefabEntry> m_prefabPalette;
        std::string m_selectedPrefab;

        //Undo/Redo systems
        std::vector<std::unique_ptr<Platform::EditorCommand>> m_undoStack;
        std::vector<std::unique_ptr<Platform::EditorCommand>> m_redoStack;
        const size_t MAX_UNDO_LEVELS = 20;  // Limit to 20 undo operations

        // Temporary storage for tracking changes
        vec2 m_transformStartPosition;
        float m_transformStartRotation;
        vec2 m_transformStartScale;
        bool m_isTransforming;

        // UI State
        bool m_showHierarchy;
        bool m_showProperties;
        bool m_showPrefabPalette;
        bool m_showSceneView;
        bool m_showLevelSettings;

        // Recent files
        std::vector<std::string> m_recentLevels;
        const size_t MAX_RECENT_LEVELS = 10;

        // Object creation
        Gameplay::GameObjectType m_objectTypeToPlace;
        std::string m_draggedPrefab;

        // Window dimensions
        float m_windowWidth;
        float m_windowHeight;

        //undo/redo
        void executeCommand(std::unique_ptr<Platform::EditorCommand> cmd);
        void undo();
        void redo();
        void clearRedoStack();

        // Helper methods to record commands
        void recordMoveCommand(Gameplay::GameObject* obj, vec2 oldPos, vec2 newPos);
        void recordRotateCommand(Gameplay::GameObject* obj, float oldRot, float newRot);
        void recordScaleCommand(Gameplay::GameObject* obj, vec2 oldScale, vec2 newScale);

		// Auto-tiling for walls
        void autoTileWalls();

        // ====================================================================
        // INTEGRATED GIZMO SYSTEM - Member Variables
        // ====================================================================

        // Gizmo state
        bool m_showGizmos;
        GizmoMode m_gizmoMode;
        GizmoAxis m_gizmoHoveredAxis;
        GizmoAxis m_gizmoActiveAxis;
        bool m_isGizmoActive;

        // Gizmo interaction state
        vec2 m_gizmoDragStartWorld;
        vec2 m_gizmoInitialPosition;
        float m_gizmoInitialRotation;
        vec2 m_gizmoInitialScale;

        // Gizmo configuration
        float m_gizmoSize;              // Base size of gizmo (80.0f)
        float m_gizmoHandleSize;        // Size of handles (12.0f)
        float m_gizmoLineThickness;     // Line thickness (2.0f)
        bool m_gizmoSnapEnabled;        // Snap to grid/angle
        float m_gizmoSnapAngle;         // Rotation snap in degrees (15.0f)
        float m_gizmoSnapScale;         // Scale snap step (0.1f)

        // Gizmo colors
        glm::vec4 m_gizmoColorX;        // Red for X axis
        glm::vec4 m_gizmoColorY;        // Green for Y axis
        glm::vec4 m_gizmoColorBoth;     // Yellow for both axes
        glm::vec4 m_gizmoColorHover;    // White for hover
        glm::vec4 m_gizmoColorActive;   // Orange for active

        // ====================================================================

        struct EditorCommand {
            virtual ~EditorCommand() = default;
            virtual void execute() = 0;
            virtual void undo() = 0;
        };

        // Performance monitoring
        double m_frameTime;
        int m_objectCount;

        double m_playTime = 0.0;
        bool m_isPaused = false;

        // File dialog state
        bool mShowAssetsBrowser = true;
        std::string mSelectedAssetName;
        std::string mAssetSearchFilter;
        std::string mAssetTypeFilter = "All";
        bool mShowAssetImportDialog = false;
        bool mShowAssetReplaceDialog = false;
        bool m_showSaveDialog;
        bool m_showLoadDialog;
        char m_fileDialogBuffer[256];
        char mAssetNameBuffer[256];

        // Grid settings
        float m_gridSize;           // Size of each grid cell (e.g., 32.0f)
        bool m_enableGridSnap;      // Enable/disable grid snapping
        bool m_showGrid;            // Show/hide grid visualization

        Graphics::FrameBuffer m_sceneFramebuffer;  // Renders game to texture
        ImVec2 m_sceneViewportSize;

        bool m_sceneViewHovered = false;
        bool m_sceneViewFocused = false;
        ImVec2 m_sceneViewPosition = ImVec2(0, 0);

        bool m_editorActive = false;

        bool m_flashlightOn{ true };
        float m_playerBattery{ 100.0f };
        void updateLightExposure(double dt);
        void renderFlashlight();

        vec2 m_lastPlacedGridPos = vec2(-999999.0f, -999999.0f);
    };

} // namespace Platform