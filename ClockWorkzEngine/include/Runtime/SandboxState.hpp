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
#include <vector>

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

  // Helper functions
  void renderSystemsWindow();
  void renderEntityWindow();
  void renderComponentWindow();
  void renderStatsWindow();
  void spawnTestEntity(vec2 position);
  void renderTorchPass(Graphics::RenderingManager &renderer);
};

} // namespace Runtime
