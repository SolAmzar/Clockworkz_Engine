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

// Component includes
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/Fragment.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/Patrol.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"

// System includes
#include "Systems/RenderSystem.hpp"

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

  // ECS World
  ECS::World m_world;

  // Systems
  Systems::RenderSystem m_renderSystem;
  bool m_renderSystemEnabled{true};
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

  // Helper functions
  void renderSystemsWindow();
  void renderEntityWindow();
  void renderComponentWindow();
  void renderStatsWindow();
  void spawnTestEntity(vec2 position);
};

} // namespace Runtime
