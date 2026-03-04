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
#include <memory>
#include <string>
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

    // ----- Scene helpers -----
    void renderGrid();
    void renderGizmos(const vec2& viewportMin);
    void loadScene(const std::string& path);
    void saveScene();

    // ----- Undo/redo -----
    void executeCommand(std::unique_ptr<IEditorCommand> cmd);
    void undo();
    void redo();

    // ----- References -----
    Platform::InputManager& m_input;
    GameStateManager&       m_gsm;

    // ----- Scene -----
    std::string             m_sceneFilePath;
    Scene                   m_scene{"Editor"};
    Graphics::FrameBuffer   m_framebuffer;

    // ----- Entity selection -----
    ECS::Entity             m_selectedEntity{0, 0};

    // ----- Viewport -----
    vec2                    m_viewportSize{800.0f, 600.0f};
    bool                    m_viewportFocused{false};
    bool                    m_viewportHovered{false};

    // ----- Editor camera -----
    glm::vec2               m_editorCamPos{0.0f, 0.0f};
    float                   m_editorCamZoom{1.0f};

    // ----- Grid -----
    bool                    m_showGrid{true};
    float                   m_gridSize{100.0f};
    bool                    m_snapToGrid{true};

    // ----- Gizmos -----
    enum class GizmoMode { None, Translation, Rotation };
    GizmoMode               m_gizmoMode{GizmoMode::Translation};
    int                     m_gizmoActiveAxis{-1}; // 0=X 1=Y 2=Rotate -1=None
    vec2                    m_gizmoOriginalPos{0.f, 0.f};
    vec2                    m_gizmoClickOffset{0.f, 0.f};
    float                   m_gizmoOriginalRotation{0.f};

    // ----- Rename state -----
    bool                    m_isRenaming{false};
    ECS::Entity             m_renamingEntity{0, 0};
    char                    m_renameBuffer[256]{};

    // ----- Asset browser -----
    std::string             m_selectedAsset;
    char                    m_newAssetNameBuf[128]{};
    char                    m_newAssetPathBuf[256]{};

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
};

} // namespace Runtime
