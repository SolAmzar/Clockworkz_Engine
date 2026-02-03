/******************************************************************************/
/*!
\file           SandboxState.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Sandbox state for interactive ECS testing.

                SandboxState provides a visual testing environment for ECS:
                - Entity spawning/destruction with ImGui controls
                - Component add/remove interface
                - Real-time stats and debugging
                - Performance monitoring

                This replaces traditional unit tests with interactive testing.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"
#include "Runtime/GameState.hpp"
#include <glm/vec2.hpp>
#include <vector>

#include "Graphics/FrameBuffer.hpp"
#include "Runtime/Scene.hpp"
#include "SandboxCommands.hpp"
#include "UndoableImGui.hpp"
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace Platform {
class InputManager;
}

namespace Core {
class Time;
}

namespace Runtime {

class GameStateManager;

/**
 * @brief Interactive testing environment for ECS development
 *
 * Provides ImGui-based controls for testing ECS functionality:
 * - Entity creation/destruction
 * - Component management
 * - System testing
 * - Performance profiling
 */
class SandboxState final : public GameState {
public:
  explicit SandboxState(Platform::InputManager &input, GameStateManager &gsm);
  ~SandboxState();

  const char *name() const override { return "Sandbox"; }

  void onEnter() override;
  void onExit() override;
  void update(double dt) override;
  void render(Core::Time *timer) override;
  void renderImGui(Core::Time *timer) override;

private:
  // Debug Render
  void renderDebugColliders(Graphics::RenderingManager &renderer);

  Platform::InputManager &m_input;
  GameStateManager &m_gsm;

  static constexpr size_t MAX_UNDO_LEVELS = 20;

  // Scene (Runtime Logic)
  Scene m_scene;

  bool m_showSystemsWindow{true};

  // Test entities
  std::vector<ECS::Entity> m_testEntities;

  // ImGui window states
  bool m_showEntityWindow{true};
  bool m_showComponentWindow{true};
  bool m_showStatsWindow{true};
  bool m_showColliders{false};
  bool m_showHUD{true};
  bool m_enableShadows{true};
  bool m_dockLayoutInitialized{false};

  // Entity spawning controls
  int m_spawnCount{1};
  float m_spawnX{0.f};
  float m_spawnY{0.f};

  // Component management
  int m_selectedEntityIdx{0};

  // Multi-selection (Shift+Click range, Ctrl+Click toggle)
  std::set<int> m_selectedEntities;  // set of m_testEntities indices
  int m_lastClickedEntityIdx{-1};    // anchor for Shift+Click range

  // Drag and Drop State
  ECS::Entity m_draggedEntity{0, 0}; // Invalid entity by default
  bool m_isDragging{false};
  vec2 m_dragOffset{0.f, 0.f};

  // Scene View & Gizmos
  Graphics::FrameBuffer m_framebuffer;
  vec2 m_viewportSize{0.f, 0.f};
  bool m_viewportFocused{false};
  bool m_viewportHovered{false};

  enum class GizmoMode { Translation, Rotation, Scale, None };
  GizmoMode m_gizmoMode{GizmoMode::Translation};
  int m_gizmoActiveAxis{
      -1}; // -1: None, 0: X, 1: Y, 2: Z/Both/Rotate, 3: Center
  vec2 m_gizmoOriginalPos{0.f, 0.f}; // For delta calculation
  vec2 m_gizmoClickOffset{0.f, 0.f};
  float m_gizmoOriginalRotation{0.f};
  vec2 m_gizmoOriginalScale{1.f, 1.f};

  // Editor Features
  bool m_showGrid{true};
  float m_gridSize{100.0f};
  bool m_isPaused{true};
  bool m_showAssetBrowser = true;
  bool m_showAudioBrowser = true;
  std::string m_selectedAsset;
  bool m_cameraFollowPlayer{true};
  glm::vec2 m_editorCameraPos{0.f, 0.f};

  // Renaming State
  int m_renamingEntityIdx{-1};
  char m_renameBuffer[256] = "";

  // prefab editor
  void renderPrefabEditor();
  void savePrefabFromEntity(ECS::Entity entity, const std::string &name,
                            const std::string &category);
  void loadAvailablePrefabs();
  void spawnPrefab(const std::string &prefabName, vec2 position);
  void deletePrefab(const std::string &prefabName);

  // tilemap editing
  bool m_tileMapMode = false;
  std::string m_selectedTilePrefab = "";
  bool m_showTilePalette = true;

  // Tilemap painting
  enum class PaintMode { Paint, Erase, Select };
  PaintMode m_paintMode = PaintMode::Paint;

  // Game Logic State
  float m_playerDeathTimer{-1.f};

  // Optional: Store tilemap data for serialization
  struct TileData {
    std::string prefabName;
    vec2 position;
    int layer = 0;
    ECS::Entity entity{0, 0};
  };
  std::vector<TileData> m_tilemap;
  std::vector<std::string> m_availableTilemaps;
  int m_selectedTilemapIdx = 0;

  // Layer system
  int m_currentLayer = 0;
  int m_numLayers = 3;
  std::vector<bool> m_layerVisible;
  void refreshTilemapList();

  // Entity grouping (Photoshop-style layer groups)
  struct EntityGroup {
    std::string name;
    std::vector<ECS::Entity> entities;
    bool collapsed = false;
  };
  std::vector<EntityGroup> m_entityGroups;
  int m_selectedGroupIdx{-1};
  bool m_editingGroup{false};
  char m_groupRenameBuffer[256] = "";
  int m_renamingGroupIdx{-1};

  // Group helpers
  int findGroupForEntity(ECS::Entity entity) const;
  EntityGroup *getOrCreateLayerGroup(int layer);
  void removeEntityFromGroups(ECS::Entity entity);
  void rebuildGroupsFromTilemap();

  // Prefab editor features
  bool m_showPrefabEditor = false;
  std::string m_newPrefabName;
  std::string m_newPrefabCategory = "Custom";
  int m_selectedPrefabIndex = -1;
  std::vector<std::string> m_availablePrefabs;

  // tilemap editor
  void renderTilePalette();
  void handleTileMapInput(const vec2 &viewportMin);
  void saveTilemap(const std::string &filepath);
  void loadTilemap(const std::string &filepath);
  void clearTilemap();
  bool isCellOccupied(vec2 worldPos, int layer) const;

  std::vector<std::unique_ptr<Sandbox::SandboxCommand>> m_undoStack;
  std::vector<std::unique_ptr<Sandbox::SandboxCommand>> m_redoStack;

  void executeCommand(std::unique_ptr<Sandbox::SandboxCommand> cmd);
  void undo();
  void redo();

  // Helper functions
  void renderToolbar();
  void renderGrid();
  void renderAssetBrowser();
  void renderAudioBrowser();
  void renderUndoRedoPanel();
  void renderSystemsWindow();
  void renderEntityWindow();
  void renderComponentWindow();
  void renderSceneWindow();
  void renderGizmos(const vec2 &windowPos, const vec2 &windowSize,
                    const vec2 &viewportMin);
  void renderStatsWindow();
  void spawnTestEntity(vec2 position);
  void spawnPlayer();
  void spawnEnemy();
  void cloneTestEntity(ECS::Entity entity);
  void renderTorchPass(Graphics::RenderingManager &renderer);
  void renderActivationZones(Graphics::RenderingManager &renderer);

  // Feature 1: Texture Replacement Dialog
  void renderTextureReplaceDialog();
  bool m_showReplaceDialog{false};
  std::string m_replacingTexture;
  char m_replacePathBuffer[256]{};

  // Feature 2: Asset Hot-Reloading
  void initTextureFilePaths();
  void checkAssetHotReload();
  std::unordered_map<std::string, std::string> m_textureFilePaths;
  std::unordered_map<std::string, std::filesystem::file_time_type>
      m_assetFileTimestamps;
  bool m_hotReloadEnabled{true};
  float m_hotReloadTimer{0.f};
  static constexpr float HOT_RELOAD_INTERVAL = 1.0f;

  // Feature 3: Asset Dependency Tracking
  void renderAssetDependencies(const std::string &textureName);

  // Add Asset popup buffers
  char m_newAssetNameBuffer[128]{};
  char m_newAssetPathBuffer[256]{};
};

} // namespace Runtime
