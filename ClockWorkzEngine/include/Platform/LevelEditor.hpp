/******************************************************************************/
/*!
\file           LevelEditor.hpp
\project        Lights Out - Level Editor
\author(s)      Student Implementation
\brief          Comprehensive level editor with grid-based map editing,
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

    // Grid settings
    struct GridSettings {
        bool enabled = true;
        float cellSize = 32.0f;
        vec4 gridColor = vec4(0.3f, 0.3f, 0.3f, 0.5f);
        vec4 originColor = vec4(1.0f, 0.0f, 0.0f, 0.8f);
        bool snapToGrid = true;
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
    \brief  Main level editor class that handles all editor functionality
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

        // Main loop functions
        void update(double dt);
        void render();
        void renderUI();

        // Editor state
        EditorMode getMode() const { return m_mode; }
        void setMode(EditorMode mode);
        void togglePlayMode();

        // Level management (Rubric 1205 - Advanced)
        bool newLevel(const std::string& name);
        bool saveLevel();
        bool saveLevel(const std::string& filepath);
        bool loadLevel(const std::string& filepath);
        bool saveLevelAs(const std::string& filepath);
        std::vector<std::string> getRecentLevels() const;
        const LevelMetadata& getCurrentLevelMetadata() const { return m_currentLevelMetadata; }

        // Object management
        Gameplay::GameObject* getSelectedObject() { return m_selectedObject; }
        void selectObject(Gameplay::GameObject* obj);
        void deleteSelectedObject();
        void duplicateSelectedObject();

        // Prefab/Archetype system (Rubric 1206 - Advanced)
        void loadPrefabs();
        void createObjectFromPrefab(const std::string& prefabName, vec2 position);
        bool savePrefab(const std::string& name);
        bool loadPrefab(const std::string& filepath);
        const std::vector<PrefabEntry>& getPrefabPalette() const { return m_prefabPalette; }

        // Grid settings
        const GridSettings& getGridSettings() const { return m_gridSettings; }
        void setGridSettings(const GridSettings& settings) { m_gridSettings = settings; }
        vec2 snapToGrid(vec2 position) const;

        // Tool selection
        EditorTool getCurrentTool() const { return m_currentTool; }
        void setCurrentTool(EditorTool tool) { m_currentTool = tool; }

        // Camera control (Rubric 1208 - Advanced)
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
        void renderPrefabPalette();
        void renderSceneView();
        void renderLevelSettings();
        void renderGridSettings();
        void renderCameraControls();

        // Object manipulation
        void handleObjectSelection();
        void handleObjectPlacement();
        void handleObjectDeletion();
        void handleObjectDragging();
        void moveSelectedObject(vec2 delta);

        // Grid rendering
        void renderGrid();
        void renderGizmos();                

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

        // Prefab management helpers
        void registerBuiltInPrefabs();
        void scanPrefabDirectory(const std::string& directory);

    private:
        // Core systems
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
        bool m_isDraggingObject;

        // Camera
        Graphics::Camera2D m_editorCamera;
        vec2 m_cameraPanStart;
        bool m_isPanningCamera;
        float m_cameraSpeed;
        float m_cameraZoomSpeed;

        // Grid
        GridSettings m_gridSettings;

        // Prefab system
        std::vector<PrefabEntry> m_prefabPalette;
        std::string m_selectedPrefab;

        // UI State
        bool m_showHierarchy;
        bool m_showProperties;
        bool m_showPrefabPalette;
        bool m_showSceneView;
        bool m_showLevelSettings;
        bool m_showGridSettings;

        // Recent files
        std::vector<std::string> m_recentLevels;
        const size_t MAX_RECENT_LEVELS = 10;

        // Object creation
        Gameplay::GameObjectType m_objectTypeToPlace;

        // Window dimensions
        float m_windowWidth;
        float m_windowHeight;

        // Gizmo state
        bool m_showGizmos;
        enum class GizmoMode { Translate, Rotate, Scale } m_gizmoMode;

        struct EditorCommand {
            virtual ~EditorCommand() = default;
            virtual void execute() = 0;
            virtual void undo() = 0;
        };
        std::vector<std::unique_ptr<EditorCommand>> m_undoStack;
        std::vector<std::unique_ptr<EditorCommand>> m_redoStack;

        // Performance monitoring
        double m_frameTime;
        int m_objectCount;

        // File dialog state
        bool m_showSaveDialog;
        bool m_showLoadDialog;
        char m_fileDialogBuffer[256];
    };

} // namespace Editor