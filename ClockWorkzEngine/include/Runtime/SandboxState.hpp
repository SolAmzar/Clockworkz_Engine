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

  // Entity spawning controls
  int m_spawnCount{1};
  float m_spawnX{0.f};
  float m_spawnY{0.f};

  // Component management
  int m_selectedEntityIdx{0};

  // Drag and Drop State
  ECS::Entity m_draggedEntity{0, 0}; // Invalid entity by default
  bool m_isDragging{false};
  glm::vec2 m_dragOffset{0.f, 0.f};

  // Scene View & Gizmos
  Graphics::FrameBuffer m_framebuffer;
  glm::vec2 m_viewportSize{0.f, 0.f};
  bool m_viewportFocused{false};
  bool m_viewportHovered{false};

  enum class GizmoMode { Translation, Rotation, Scale, None };
  GizmoMode m_gizmoMode{GizmoMode::Translation};
  int m_gizmoActiveAxis{
      -1}; // -1: None, 0: X, 1: Y, 2: Z/Both/Rotate, 3: Center
  glm::vec2 m_gizmoOriginalPos{0.f, 0.f}; // For delta calculation
  glm::vec2 m_gizmoClickOffset{0.f, 0.f};
  float m_gizmoOriginalRotation{0.f};
  glm::vec2 m_gizmoOriginalScale{1.f, 1.f};

  // Helper functions
  void renderSystemsWindow();
  void renderEntityWindow();
  void renderComponentWindow();
  void renderSceneWindow();
  void renderGizmos(const glm::vec2 &windowPos, const glm::vec2 &windowSize,
                    const glm::vec2 &viewportMin);
  void renderStatsWindow();
  void spawnTestEntity(vec2 position);
  void cloneTestEntity(ECS::Entity entity);
  void renderTorchPass(Graphics::RenderingManager &renderer);
};

} // namespace Runtime
