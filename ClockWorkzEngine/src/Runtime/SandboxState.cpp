/******************************************************************************/
/*!
\file           SandboxState.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of SandboxState for ECS testing.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/SandboxState.hpp"
#include "Assets/TextureManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "Math/Vec2.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <IMGUI/imgui.h>

namespace Runtime {

SandboxState::SandboxState(Platform::InputManager &input, GameStateManager &gsm)
    : m_input(input), m_gsm(gsm) {}

SandboxState::~SandboxState() = default;

void SandboxState::onEnter() {
  // Register all component types
  m_world.registerComponent<Components::Transform>();
  m_world.registerComponent<Components::Velocity>();
  m_world.registerComponent<Components::SpriteRenderer>();
  m_world.registerComponent<Components::Collider>();
  m_world.registerComponent<Components::Health>();
  m_world.registerComponent<Components::Battery>();
  m_world.registerComponent<Components::LightExposure>();
  m_world.registerComponent<Components::Fragment>();
  m_world.registerComponent<Components::Patrol>();
  m_world.registerComponent<Components::AudioSource>();

  // Load necessary shaders for RenderingManager scene pass
  Assets::ResourceManager<Graphics::Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
}

void SandboxState::onExit() {
  // Clean up all test entities
  for (auto entity : m_testEntities) {
    m_world.destroyEntity(entity);
  }
  m_testEntities.clear();
}

void SandboxState::update(double dt) {
  (void)dt;
  // Handle ESC to return to menu
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.pop();
  }

  // Systems will be updated here in Phase 3
}

void SandboxState::render(Core::Time *timer) {
  // Get renderer from Engine
  Core::Engine &engine = Core::Engine::Get();
  Graphics::RenderingManager &renderer = engine.rendering();
  Graphics::Camera2D &camera = engine.camera();

  // Start Scene Pass (sets up shader and view-projection)
  renderer.beginScenePass(camera.viewProj());

  // Run RenderSystem if enabled
  if (m_renderSystemEnabled) {
    m_renderSystem.update(m_world, renderer);
  }

  // End Scene Pass
  renderer.endScenePass();

  // ImGui windows rendered separately in renderImGui()
  (void)timer;
}

void SandboxState::renderImGui(Core::Time *timer) {
  // Render all ImGui windows
  renderSystemsWindow();
  renderEntityWindow();
  renderComponentWindow();
  renderStatsWindow();

  (void)timer;
}

void SandboxState::renderEntityWindow() {
  ImGui::Begin("Entity Management", &m_showEntityWindow);

  ImGui::Text("ECS Sandbox - Phase 1");
  ImGui::Separator();

  // Entity count display
  ImGui::Text("Living Entities: %u", m_world.entityCount());

  ImGui::Separator();
  ImGui::Text("Spawn Entities");

  // Spawn controls
  ImGui::InputInt("Count", &m_spawnCount);
  m_spawnCount = std::max(1, std::min(m_spawnCount, 1000)); // Clamp 1-1000

  ImGui::InputFloat("X Position", &m_spawnX);
  ImGui::InputFloat("Y Position", &m_spawnY);

  if (ImGui::Button("Spawn Entities")) {
    for (int i = 0; i < m_spawnCount; ++i) {
      spawnTestEntity(vec2{m_spawnX, m_spawnY});
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear All")) {
    for (auto entity : m_testEntities) {
      m_world.destroyEntity(entity);
    }
    m_testEntities.clear();
  }

  ImGui::Separator();
  ImGui::Text("Entity List (%zu entities):", m_testEntities.size());

  // Scrollable entity list
  ImGui::BeginChild("EntityList", ImVec2(0, 200), true);

  for (size_t i = 0; i < m_testEntities.size(); ++i) {
    auto entity = m_testEntities[i];

    ImGui::PushID(static_cast<int>(i));
    ImGui::Text("Entity %u (gen %u)", entity.id, entity.generation);
    ImGui::SameLine(200);

    if (ImGui::SmallButton("Destroy")) {
      m_world.destroyEntity(entity);
      m_testEntities.erase(m_testEntities.begin() + i);
      ImGui::PopID();
      break; // Exit loop since we modified the vector
    }

    ImGui::PopID();
  }

  ImGui::EndChild();

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                     "Press ESC to return to menu");

  ImGui::End();
}

void SandboxState::renderComponentWindow() {
  ImGui::Begin("Components", &m_showComponentWindow);

  if (m_testEntities.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       "No entities to inspect");
    ImGui::Text("Spawn some entities first!");
    ImGui::End();
    return;
  }

  // Entity selector
  ImGui::Text("Select Entity:");
  if (m_selectedEntityIdx >= static_cast<int>(m_testEntities.size()))
    m_selectedEntityIdx = 0;

  ImGui::SliderInt("Entity Index", &m_selectedEntityIdx, 0,
                   static_cast<int>(m_testEntities.size()) - 1);

  auto entity = m_testEntities[m_selectedEntityIdx];
  ImGui::Text("Entity ID: %u (gen %u)", entity.id, entity.generation);

  ImGui::Separator();

  // Transform Component
  if (ImGui::CollapsingHeader("Transform")) {
    auto *transform = m_world.getComponent<Components::Transform>(entity);

    if (transform) {
      ImGui::InputFloat2("Position", &transform->position.x);
      ImGui::InputFloat("Rotation", &transform->rotation);
      ImGui::InputFloat2("Scale", &transform->scale.x);

      // Transform cannot be removed (like Unity)
      ImGui::TextDisabled("(Transform is required on all entities)");
    } else {
      // This should never happen since all entities spawn with Transform
      if (ImGui::Button("Add Transform")) {
        m_world.addComponent(entity, Components::Transform{});
      }
    }
  }

  // Velocity Component
  if (ImGui::CollapsingHeader("Velocity")) {
    auto *velocity = m_world.getComponent<Components::Velocity>(entity);

    if (velocity) {
      ImGui::InputFloat2("Velocity", &velocity->velocity.x);
      ImGui::InputFloat("Move Speed", &velocity->moveSpeed);

      if (ImGui::Button("Remove Velocity")) {
        m_world.removeComponent<Components::Velocity>(entity);
      }
    } else {
      if (ImGui::Button("Add Velocity")) {
        m_world.addComponent(entity, Components::Velocity{});
      }
    }
  }

  // SpriteRenderer Component
  if (ImGui::CollapsingHeader("SpriteRenderer")) {
    auto *sprite = m_world.getComponent<Components::SpriteRenderer>(entity);

    if (sprite) {
      // Get texture names from TextureManager
      auto textureNames = Assets::TextureManager::GetLoadedTextureNames();

      if (!textureNames.empty()) {
        // Find current selection
        int currentItem = 0;
        for (size_t i = 0; i < textureNames.size(); i++) {
          if (sprite->textureId == textureNames[i]) {
            currentItem = static_cast<int>(i);
            break;
          }
        }

        // Create combo box
        if (ImGui::BeginCombo("Texture ID", sprite->textureId.c_str())) {
          for (size_t i = 0; i < textureNames.size(); i++) {
            bool isSelected = (currentItem == static_cast<int>(i));
            if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
              sprite->textureId = textureNames[i];
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      } else {
        ImGui::Text("No textures loaded");
      }

      ImGui::InputFloat2("Size", &sprite->size.x);
      ImGui::Checkbox("Visible", &sprite->visible);
      ImGui::ColorEdit4("Color", &sprite->color.r);

      if (ImGui::Button("Remove SpriteRenderer")) {
        m_world.removeComponent<Components::SpriteRenderer>(entity);
      }
    } else {
      if (ImGui::Button("Add SpriteRenderer")) {
        m_world.addComponent(entity, Components::SpriteRenderer{});
      }
    }
  }

  // Collider Component
  if (ImGui::CollapsingHeader("Collider")) {
    auto *collider = m_world.getComponent<Components::Collider>(entity);

    if (collider) {
      ImGui::InputFloat2("Size", &collider->size.x);
      ImGui::InputFloat2("Offset", &collider->offset.x);
      ImGui::Checkbox("Is Solid", &collider->isSolid);
      ImGui::Checkbox("Is Hazard", &collider->isHazard);
      ImGui::Checkbox("Is Trigger", &collider->isTrigger);
      ImGui::Checkbox("Blocks Light", &collider->blocksLight);
      ImGui::Checkbox("Moveable", &collider->moveable);

      if (ImGui::Button("Remove Collider")) {
        m_world.removeComponent<Components::Collider>(entity);
      }
    } else {
      if (ImGui::Button("Add Collider")) {
        m_world.addComponent(entity, Components::Collider{});
      }
    }
  }

  // Health Component
  if (ImGui::CollapsingHeader("Health")) {
    auto *health = m_world.getComponent<Components::Health>(entity);

    if (health) {
      ImGui::SliderFloat("HP", &health->hp, 0.f, health->maxHp);
      ImGui::InputFloat("Max HP", &health->maxHp);
      ImGui::InputFloat("Regen Rate", &health->regenRate);
      ImGui::Checkbox("Invulnerable", &health->invulnerable);
      ImGui::Checkbox("Is Dead", &health->isDead);

      if (ImGui::Button("Remove Health")) {
        m_world.removeComponent<Components::Health>(entity);
      }
    } else {
      if (ImGui::Button("Add Health")) {
        m_world.addComponent(entity, Components::Health{});
      }
    }
  }

  // Battery Component
  if (ImGui::CollapsingHeader("Battery")) {
    auto *battery = m_world.getComponent<Components::Battery>(entity);

    if (battery) {
      ImGui::SliderFloat("Charge", &battery->charge, 0.f, battery->maxCharge);
      ImGui::InputFloat("Max Charge", &battery->maxCharge);
      ImGui::InputFloat("Drain Rate", &battery->drainRate);
      ImGui::InputFloat("Charge Rate", &battery->chargeRate);
      ImGui::Checkbox("Is Charging", &battery->isCharging);

      if (ImGui::Button("Remove Battery")) {
        m_world.removeComponent<Components::Battery>(entity);
      }
    } else {
      if (ImGui::Button("Add Battery")) {
        m_world.addComponent(entity, Components::Battery{});
      }
    }
  }

  // LightExposure Component
  if (ImGui::CollapsingHeader("LightExposure")) {
    auto *light = m_world.getComponent<Components::LightExposure>(entity);

    if (light) {
      ImGui::Text("Exposure Time: %.2f / %.2f", light->exposureTime,
                  light->maxExposureTime);
      ImGui::InputFloat("Max Exposure", &light->maxExposureTime);
      ImGui::Checkbox("In Light", &light->inLight);
      ImGui::Checkbox("Is Dangerous", &light->isDangerous);

      if (ImGui::Button("Remove LightExposure")) {
        m_world.removeComponent<Components::LightExposure>(entity);
      }
    } else {
      if (ImGui::Button("Add LightExposure")) {
        m_world.addComponent(entity, Components::LightExposure{});
      }
    }
  }

  // Fragment Component
  if (ImGui::CollapsingHeader("Fragment")) {
    auto *fragment = m_world.getComponent<Components::Fragment>(entity);

    if (fragment) {
      ImGui::InputInt("Fragment ID", &fragment->fragmentId);
      ImGui::Checkbox("Collected", &fragment->collected);
      ImGui::InputFloat("Bob Speed", &fragment->bobSpeed);
      ImGui::InputFloat("Bob Amount", &fragment->bobAmount);

      if (ImGui::Button("Remove Fragment")) {
        m_world.removeComponent<Components::Fragment>(entity);
      }
    } else {
      if (ImGui::Button("Add Fragment")) {
        m_world.addComponent(entity, Components::Fragment{});
      }
    }
  }

  ImGui::End();
}

void SandboxState::renderStatsWindow() {
  ImGui::Begin("ECS Stats", &m_showStatsWindow);

  ImGui::Text("Performance");
  ImGui::Separator();

  // Entity stats
  ImGui::Text("Entity Count: %u", m_world.entityCount());
  ImGui::Text("Tracked Entities: %zu", m_testEntities.size());

  ImGui::Separator();
  ImGui::Text("Component Counts:");
  ImGui::Text("  Transform: %zu",
              m_world.componentCount<Components::Transform>());
  ImGui::Text("  Velocity: %zu",
              m_world.componentCount<Components::Velocity>());
  ImGui::Text("  SpriteRenderer: %zu",
              m_world.componentCount<Components::SpriteRenderer>());
  ImGui::Text("  Collider: %zu",
              m_world.componentCount<Components::Collider>());
  ImGui::Text("  Health: %zu", m_world.componentCount<Components::Health>());
  ImGui::Text("  Battery: %zu", m_world.componentCount<Components::Battery>());
  ImGui::Text("  LightExposure: %zu",
              m_world.componentCount<Components::LightExposure>());
  ImGui::Text("  Fragment: %zu",
              m_world.componentCount<Components::Fragment>());

  ImGui::Separator();

  // Memory estimation
  size_t entityMemory = m_world.entityCount() * sizeof(ECS::Entity);
  size_t transformMemory = m_world.componentCount<Components::Transform>() *
                           sizeof(Components::Transform);
  size_t velocityMemory = m_world.componentCount<Components::Velocity>() *
                          sizeof(Components::Velocity);
  size_t totalComponentMemory =
      transformMemory + velocityMemory; // Add more as needed

  ImGui::Text("Memory Usage:");
  ImGui::Text("  Entities: %zu bytes (%.2f KB)", entityMemory,
              entityMemory / 1024.0f);
  ImGui::Text("  Components: %zu bytes (%.2f KB)", totalComponentMemory,
              totalComponentMemory / 1024.0f);

  ImGui::Separator();

  // Frame stats
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Phase 2: Components");
  ImGui::Text("All components registered and functional!");

  ImGui::End();
}

void SandboxState::spawnTestEntity(vec2 position) {
  // Create entity
  auto entity = m_world.createEntity();

  // Every entity gets a Transform by default (like Unity)
  Components::Transform transform;
  transform.position = position;
  transform.rotation = 0.f;
  transform.scale = vec2{1.f, 1.f};
  m_world.addComponent(entity, transform);

  // Track it
  m_testEntities.push_back(entity);

  // Other components can be added via the UI
}

void SandboxState::renderSystemsWindow() {
  ImGui::Begin("Systems", &m_showSystemsWindow);

  ImGui::Text("Active Systems:");
  ImGui::Separator();

  // RenderSystem toggle
  ImGui::Checkbox("RenderSystem", &m_renderSystemEnabled);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Draws sprites for entities with Transform + SpriteRenderer");
  }

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Phase 3.1: RenderSystem");
  ImGui::Text("More systems coming in Phase 3.2+");

  ImGui::End();
}

} // namespace Runtime
