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

// MUST be included before any other OpenGL-related headers
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"
#include "Core/Engine.hpp"
#include "Core/Profiler.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_internal.h"
#include "Math/Vec2.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Systems/AISystem.hpp"
#include "Systems/AnimationSystem.hpp"
#include "Systems/AudioSystem.hpp"
#include "Systems/StateMachineSystem.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/glm.hpp>

namespace Runtime {

// Helper struct for type identity (moved to namespace scope)
template <typename T> struct Identity {
  using type = T;
};

SandboxState::SandboxState(Platform::InputManager &input, GameStateManager &gsm)
    : m_input(input), m_gsm(gsm) {}

SandboxState::~SandboxState() = default;

void SandboxState::onEnter() {
  // Init Scene (registers components)
  m_scene.Init();

  // Load necessary shaders for RenderingManager scene pass
  Assets::ResourceManager<Graphics::Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
  Assets::ResourceManager<Graphics::Shader>::Load(
      "light", "assets/shaders/light.vert", "assets/shaders/light.frag");

  // Init Framebuffer for Scene View
  m_framebuffer.create(800, 600);
}

void SandboxState::onExit() {
  // Clean up all test entities
  m_scene.Clear();
  m_testEntities.clear();
  m_framebuffer.destroy();
}

void SandboxState::update(double dt) {
  (void)dt;
  // Handle ESC to return to menu
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.pop();
  }

  // Update Scene Logic
  m_scene.Update(dt, m_input);
}

void SandboxState::render(Core::Time *timer) {
  (void)timer;
  // We render the scene into the framebuffer for the editor view
  m_framebuffer.bind();

  // Get renderer from Engine
  Core::Engine &engine = Core::Engine::Get();
  Graphics::RenderingManager &renderer = engine.rendering();
  Graphics::Camera2D &camera = engine.camera();

  // Clear screen (implied by beginFrame/endFrame usually, but here we might
  // need manual clear if FB doesn't auto-clear) Assuming RenderingManager or
  // engine handles clearing, but usually it clears backbuffer. For FB, we
  // should clear it.
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Start Scene Pass (sets up shader and view-projection)
  renderer.beginScenePass(camera.viewProj());

  // Run Scene Render
  m_scene.Render(renderer);

  // End Scene Pass
  renderer.endScenePass();

  // Render Torch Pass
  // Render Torch Pass
  renderTorchPass(renderer);

  // Debug Collider Pass
  if (m_showColliders)
    renderDebugColliders(renderer);

  // Debug Shadow Toggle (Optional: move to a better window)
  // For now, no separate UI call here, relied on renderStatsWindow/renderImGui
  // to set flag Wait, I need to add the checkbox to the UI. Code search showed

  // Render HUD (Battery)
  if (m_showHUD && m_viewportSize.x > 0 && m_viewportSize.y > 0) {
    // Determine Ortho Projection for UI (0,0 bottom-left, to matches GL
    // default) We need glm::ortho. If not available, we might need to include
    // it. Assuming it is available via glm headers.
    glm::mat4 uiProj =
        glm::ortho(0.0f, m_viewportSize.x, 0.0f, m_viewportSize.y, -1.0f, 1.0f);

    // Begin UI Pass (reuses Scene Pass setup but with UI projection)
    renderer.beginScenePass(uiProj);

    auto &world = m_scene.GetWorld();
    world.each<Components::PlayerController, Components::Battery>(
        [&](ECS::Entity, Components::PlayerController &,
            Components::Battery &batt) {
          float pct = batt.charge / batt.maxCharge;

          std::string texId = "energy flat";
          if (pct > 0.75f)
            texId = "energy 4";
          else if (pct > 0.50f)
            texId = "energy 3";
          else if (pct > 0.25f)
            texId = "energy 2";
          else if (pct > 0.0f)
            texId = "energy 1";

          glm::vec2 uiSize(128.0f, 64.0f);
          float margin = 20.0f;
          // Top-Right positioning
          // x = ViewportWidth - Margin - Width
          // y = ViewportHeight - Margin - Height (Top anchor for BottomLeft
          // origin)
          glm::vec2 uiPos(m_viewportSize.x - margin - uiSize.x,
                          m_viewportSize.y - margin - uiSize.y);

          auto tex = Assets::TextureManager::Get(texId);
          if (tex) {
            renderer.drawSprite(tex->id(), uiPos, uiSize, 0.0f,
                                glm::vec4(0, 0, 1, 1), glm::vec4(1), true);
          }
        });

    renderer.endScenePass();
  }

  m_framebuffer.unbind();
}

void SandboxState::renderTorchPass(Graphics::RenderingManager &renderer) {
  auto &world = m_scene.GetWorld();
  Graphics::Camera2D &camera = Core::Engine::Get().camera();

  world.each<Components::Transform, Components::TorchComponent>(
      [&](ECS::Entity entity, Components::Transform &trans,
          Components::TorchComponent &torch) {
        if (!torch.active)
          return;

        // Check battery if present
        float batteryFactor = 1.0f;
        auto *batt = world.getComponent<Components::Battery>(entity);
        if (batt) {
          batteryFactor = batt->charge / batt->maxCharge;
          if (batt->charge <= 0.0f)
            return;
        }

        // Calculate Torch World Position
        float angle = trans.rotation;
        glm::vec2 torchOffsetLocal(torch.offset.x, torch.offset.y);
        glm::vec2 torchOffsetWorld(torchOffsetLocal.x * std::cos(angle) -
                                       torchOffsetLocal.y * std::sin(angle),
                                   torchOffsetLocal.x * std::sin(angle) +
                                       torchOffsetLocal.y * std::cos(angle));

        glm::vec2 originGLM =
            glm::vec2(trans.position.x, trans.position.y) + torchOffsetWorld;

        // Create simple light polygon (Fan)
        std::vector<glm::vec2> lightPoly;
        int segments = 100; // Increased segments for smoother shadows
        float halfFov = glm::radians(torch.angle) * 0.5f;
        float startAngle = angle - halfFov;
        float totalAngle = glm::radians(torch.angle);
        float step = totalAngle / segments;

        if (!m_enableShadows) {
          // SIMPLE FAN (No Shadows)
          for (int i = 0; i <= segments; ++i) {
            float theta = startAngle + step * i;
            lightPoly.emplace_back(originGLM.x + std::cos(theta) * torch.radius,
                                   originGLM.y +
                                       std::sin(theta) * torch.radius);
          }
        } else {
          // RAYCAST SHADOWS
          // Local Raycast Helper
          auto Raycast = [&](glm::vec2 start, glm::vec2 dir,
                             float maxDist) -> glm::vec2 {
            float closestDist = maxDist;
            glm::vec2 closestPoint = start + dir * maxDist;

            // Helper to rotate vector by angle (radians)
            auto RotateVec = [](glm::vec2 v, float angle) -> glm::vec2 {
              float s = std::sin(angle);
              float c = std::cos(angle);
              return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
            };

            // Iterate all potential occluders
            world.each<Components::Transform, Components::Collider>(
                [&](ECS::Entity, Components::Transform &obsTrans,
                    Components::Collider &obsCol) {
                  if (!obsCol.isSolid || !obsCol.blocksLight)
                    return;

                  // 1. Calculate World Center with Rotated Offset
                  glm::vec2 offsetRotated =
                      RotateVec(glm::vec2(obsCol.offset.x, obsCol.offset.y),
                                obsTrans.rotation);
                  glm::vec2 obsCenter =
                      glm::vec2(obsTrans.position.x, obsTrans.position.y) +
                      offsetRotated;

                  if (obsCol.shape == Components::Collider::Shape::Box) {
                    // --- OBB Logic (Oriented Bounding Box) ---
                    // Transform Ray into Box's Local Space
                    // Local Ray Origin = Rotate(WorldRayStart - BoxCenter,
                    // -Angle) Local Ray Dir    = Rotate(WorldRayDir, -Angle)

                    glm::vec2 localStart =
                        RotateVec(start - obsCenter, -obsTrans.rotation);
                    glm::vec2 localDir = RotateVec(dir, -obsTrans.rotation);

                    // AABB in Local Space is centered at (0,0) with
                    // half-extents
                    // Apply Transform Scale to Box Size
                    glm::vec2 scaledSize =
                        glm::vec2(obsCol.size.x * obsTrans.scale.x,
                                  obsCol.size.y * obsTrans.scale.y);
                    glm::vec2 halfSize = scaledSize * 0.5f;

                    glm::vec2 min = -halfSize;
                    glm::vec2 max = halfSize;

                    // Standard Slab Method in Local Space
                    glm::vec2 invDir;
                    invDir.x = (std::abs(localDir.x) < 1e-6f)
                                   ? 1e6f
                                   : 1.0f / localDir.x;
                    invDir.y = (std::abs(localDir.y) < 1e-6f)
                                   ? 1e6f
                                   : 1.0f / localDir.y;

                    glm::vec2 t1 = (min - localStart) * invDir;
                    glm::vec2 t2 = (max - localStart) * invDir;

                    float tmin =
                        std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y));
                    float tmax =
                        std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));

                    // If hit, tmin is the distance along the ray.
                    // Since rotation preserves distance, tmin is valid in World
                    // Space too.
                    if (tmax >= std::max(0.0f, tmin) && tmin < closestDist) {
                      closestDist = tmin;
                      closestPoint = start + dir * tmin;
                    }
                  } else if (obsCol.shape ==
                             Components::Collider::Shape::Circle) {
                    // --- Circle Logic ---
                    // Circle is rotation invariant, but we must use the correct
                    // Center (offset logic above)
                    // Apply Transform Scale to Radius
                    float scaledRadius =
                        obsCol.radius *
                        std::max(obsTrans.scale.x, obsTrans.scale.y);

                    float r = scaledRadius;
                    glm::vec2 oc = start - obsCenter;
                    float b = 2.0f * glm::dot(oc, dir);
                    float c = glm::dot(oc, oc) - r * r;
                    float discriminant = b * b - 4.0f * c;

                    if (discriminant >= 0.0f) {
                      float sqrtD = std::sqrt(discriminant);
                      // t1 is entry, t2 is exit
                      float t1 = (-b - sqrtD) * 0.5f;

                      if (t1 > 0.0f && t1 < closestDist) {
                        closestDist = t1;
                        closestPoint = start + dir * t1;
                      }
                    }
                  }
                });
            return closestPoint;
          };

          for (int i = 0; i <= segments; ++i) {
            float theta = startAngle + step * i;
            glm::vec2 rayDir(std::cos(theta), std::sin(theta));
            lightPoly.push_back(Raycast(originGLM, rayDir, torch.radius));
          }
        }

        // Render Light Pass
        renderer.beginLightPass(camera.viewProj(), originGLM, torch.color,
                                torch.radius, angle, torch.angle, batteryFactor,
                                torch.intensity);
        renderer.drawLightCone(originGLM, lightPoly, torch.color);
        renderer.endLightPass();
      });
}

void SandboxState::renderDebugColliders(Graphics::RenderingManager &renderer) {
  auto &world = m_scene.GetWorld();
  renderer.beginDebugPass(Core::Engine::Get().camera().viewProj());

  world.each<Components::Transform, Components::Collider>(
      [&](ECS::Entity, Components::Transform &t, Components::Collider &c) {
        // Rotate Offset
        auto RotateVec = [](glm::vec2 v, float angle) -> glm::vec2 {
          float s = std::sin(angle);
          float c_ang = std::cos(angle);
          return glm::vec2(v.x * c_ang - v.y * s, v.x * s + v.y * c_ang);
        };

        glm::vec2 offsetRot =
            RotateVec(glm::vec2(c.offset.x, c.offset.y), t.rotation);
        glm::vec2 pos(t.position.x + offsetRot.x, t.position.y + offsetRot.y);

        glm::vec3 color = c.isTrigger ? glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow
                                      : glm::vec3(0.0f, 1.0f, 0.0f); // Green

        if (c.shape == Components::Collider::Shape::Box) {
          // Draw Rotated Rect using Lines
          glm::vec2 half = glm::vec2(c.size.x, c.size.y) * 0.5f;
          // Corners in local space
          glm::vec2 p1 =
              RotateVec(glm::vec2(-half.x, -half.y), t.rotation) + pos;
          glm::vec2 p2 =
              RotateVec(glm::vec2(half.x, -half.y), t.rotation) + pos;
          glm::vec2 p3 = RotateVec(glm::vec2(half.x, half.y), t.rotation) + pos;
          glm::vec2 p4 =
              RotateVec(glm::vec2(-half.x, half.y), t.rotation) + pos;

          renderer.drawLine(p1.x, p1.y, p2.x, p2.y, color.r, color.g, color.b);
          renderer.drawLine(p2.x, p2.y, p3.x, p3.y, color.r, color.g, color.b);
          renderer.drawLine(p3.x, p3.y, p4.x, p4.y, color.r, color.g, color.b);
          renderer.drawLine(p4.x, p4.y, p1.x, p1.y, color.r, color.g, color.b);

        } else {
          renderer.drawCircle(pos.x, pos.y, c.radius, color.r, color.g,
                              color.b);
        }
      });
  renderer.endDebugPass();
}

void SandboxState::renderImGui(Core::Time *timer) {
  // Begin DockSpace
  Core::Engine::Get().imguiManager().beginDockspace();

  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  static bool dockLayoutInitialized = false;

  if (!dockLayoutInitialized) {
    dockLayoutInitialized = true;
    ImGui::DockBuilderRemoveNode(dockspace_id); // Clear any existing layout
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_left_id = 0;
    ImGuiID dock_right_id = 0;

    // Split: Left (20%) and the rest
    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f,
                                &dock_left_id, &dock_right_id);

    // Split the Rest: Center and Right (25% of the rest? or absolute?)
    // We want to split 'dock_right_id' (which is center+right) into Right (25%
    // of total width ≈ 30% of remaining)
    ImGuiID dock_center_id = 0;
    ImGuiID dock_actual_right_id = 0;
    ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Right, 0.25f,
                                &dock_actual_right_id, &dock_center_id);

    // Dock Windows
    // Left: Entity Management, Systems
    ImGui::DockBuilderDockWindow("Entity Management", dock_left_id);
    ImGui::DockBuilderDockWindow("Systems", dock_left_id);

    // Right: Components, ECS Stats (Tabbed)
    // Docking multiple windows to the same ID stacks them as tabs
    ImGui::DockBuilderDockWindow("Components", dock_actual_right_id);
    ImGui::DockBuilderDockWindow("ECS Stats", dock_actual_right_id);

    // Center: Empty (implicitly open)
    ImGui::DockBuilderDockWindow("Scene", dock_center_id);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Render all ImGui windows
  renderSystemsWindow();
  renderEntityWindow();
  renderComponentWindow();
  renderStatsWindow();
  renderSceneWindow();

  // End DockSpace
  Core::Engine::Get().imguiManager().endDockspace();

  (void)timer;
}

void SandboxState::renderEntityWindow() {
  auto &world = m_scene.GetWorld();

  ImGui::Begin("Entity Management", &m_showEntityWindow);

  ImGui::Text("ECS Sandbox - Phase 1");
  ImGui::Separator();

  // Serialization UI
  static char currentLevelName[256] = "Level_Save.json";
  ImGui::InputText("Filename", currentLevelName, 256);

  if (ImGui::Button("Save Level")) {
    std::string path = "assets/scenes/" + std::string(currentLevelName);
    SceneSerializer::Serialize(world, path);
    ImGui::OpenPopup("Saved");
  }

  if (ImGui::BeginPopup("Saved")) {
    ImGui::Text("Level Saved Successfully!");
    if (ImGui::Button("Close"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::SameLine();

  if (ImGui::Button("Load Level")) {
    m_scene.Clear();
    m_testEntities.clear(); // Clear old tracked entities

    std::string path = "assets/scenes/" + std::string(currentLevelName);
    if (SceneSerializer::Deserialize(world, path)) {
      // Success - populate m_testEntities with loaded entities
      world.each<Components::Transform>(
          [&](ECS::Entity entity, Components::Transform &) {
            m_testEntities.push_back(entity);
          });
    } else {
      ImGui::OpenPopup("LoadError");
    }
  }

  if (ImGui::BeginPopup("LoadError")) {
    ImGui::Text("Failed to load level.");
    if (ImGui::Button("Close"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::Separator();

  // Entity count display
  ImGui::Text("Living Entities: %u", world.entityCount());

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

  ImGui::Separator();
  ImGui::Text("View Options");
  // Debug Toggle
  ImGui::Checkbox("Show Colliders", &m_showColliders);
  ImGui::Checkbox("Show HUD", &m_showHUD);

  ImGui::Separator();
  if (ImGui::Button("Clear All Entities")) {
    m_scene.Clear();
    m_testEntities.clear();
  }
  ImGui::Separator();
  ImGui::Text("Entity List:");
  ImGui::Text("%zu entities", m_testEntities.size());

  // Scrollable entity list
  ImGui::BeginChild("EntityList", ImVec2(0, 200), true);

  int entityToClone = -1;
  int entityToDelete = -1;

  for (size_t i = 0; i < m_testEntities.size(); ++i) {
    auto entity = m_testEntities[i];

    ImGui::PushID(static_cast<int>(i));

    char label[64];
    snprintf(label, sizeof(label), "Entity %u (gen %u)", entity.id,
             entity.generation);

    // Selection Logic
    bool isSelected = (m_selectedEntityIdx == (int)i);
    float availWidth = ImGui::GetContentRegionAvail().x;

    // Limit Selectable width to leave room for buttons (approx 60px)
    if (ImGui::Selectable(label, isSelected, 0, ImVec2(availWidth - 60, 0))) {
      m_selectedEntityIdx = (int)i;
    }

    // Right Click Context Menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::Selectable("Clone")) {
        entityToClone = (int)i;
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::Selectable("Delete")) {
        entityToDelete = (int)i;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("C")) { // Clone
      entityToClone = (int)i;
    }
    ImGui::SameLine();
    if (ImGui::Button("X")) { // Destroy
      entityToDelete = (int)i;
    }

    ImGui::PopID();
  }

  // Handle deferred actions
  if (entityToClone != -1) {
    if (entityToClone >= 0 && entityToClone < (int)m_testEntities.size())
      cloneTestEntity(m_testEntities[entityToClone]);
  }

  if (entityToDelete != -1) {
    if (entityToDelete >= 0 && entityToDelete < (int)m_testEntities.size()) {
      // auto &world = m_scene.GetWorld(); // Shadowed variable removed
      world.destroyEntity(m_testEntities[entityToDelete]);
      m_testEntities.erase(m_testEntities.begin() + entityToDelete);

      // Fix selection index if needed
      if (m_selectedEntityIdx == entityToDelete)
        m_selectedEntityIdx = -1;
      else if (m_selectedEntityIdx > entityToDelete)
        m_selectedEntityIdx--;
    }
  }

  ImGui::EndChild();

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                     "Press ESC to return to menu");

  ImGui::End();
}

void SandboxState::renderComponentWindow() {
  auto &world = m_scene.GetWorld();

  ImGui::Begin("Components", &m_showComponentWindow);

  if (m_testEntities.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       "No entities to inspect");
    ImGui::Text("Spawn some entities first!");
    ImGui::End();
    return;
  }

  // Entity selector info
  if (m_selectedEntityIdx >= static_cast<int>(m_testEntities.size()))
    m_selectedEntityIdx = 0;

  // Sync Logic: Ensure index is valid
  if (m_selectedEntityIdx < 0 || m_testEntities.empty()) {
    ImGui::Text("No entity selected.");
    ImGui::End();
    return;
  }

  auto entity = m_testEntities[m_selectedEntityIdx];
  ImGui::Text("Inspecting: Entity %u (gen %u)", entity.id, entity.generation);

  ImGui::Separator();

  // Transform Component
  if (ImGui::CollapsingHeader("Transform")) {
    auto *transform = world.getComponent<Components::Transform>(entity);

    if (transform) {
      ImGui::InputFloat2("Position", &transform->position.x);
      ImGui::InputFloat("Rotation", &transform->rotation);
      ImGui::InputFloat2("Scale", &transform->scale.x);
      ImGui::TextDisabled("(Transform is required on all entities)");
    } else {
      if (ImGui::Button("Add Transform")) {
        world.addComponent(entity, Components::Transform{});
      }
    }
  }

  // Velocity Component
  if (ImGui::CollapsingHeader("Velocity")) {
    auto *velocity = world.getComponent<Components::Velocity>(entity);

    if (velocity) {
      ImGui::InputFloat2("Velocity", &velocity->velocity.x);
      ImGui::InputFloat("Move Speed", &velocity->moveSpeed);

      if (ImGui::Button("Remove Velocity")) {
        world.removeComponent<Components::Velocity>(entity);
      }
    } else {
      if (ImGui::Button("Add Velocity")) {
        world.addComponent(entity, Components::Velocity{});
      }
    }
  }

  // PlayerController Component (NEW)
  if (ImGui::CollapsingHeader("PlayerController")) {
    auto *pc = world.getComponent<Components::PlayerController>(entity);
    if (pc) {
      ImGui::InputFloat("Walk Multiplier", &pc->walkMultiplier);
      ImGui::InputFloat("Sprint Multiplier", &pc->sprintMultiplier);

      if (ImGui::Button("Remove PlayerController")) {
        world.removeComponent<Components::PlayerController>(entity);
      }
    } else {
      if (ImGui::Button("Add PlayerController")) {
        world.addComponent(entity, Components::PlayerController{});
      }
    }
  }

  // SpriteRenderer Component
  if (ImGui::CollapsingHeader("SpriteRenderer")) {
    auto *sprite = world.getComponent<Components::SpriteRenderer>(entity);

    if (sprite) {
      auto textureNames = Assets::TextureManager::GetLoadedTextureNames();

      if (!textureNames.empty()) {
        int currentItem = 0;
        for (size_t i = 0; i < textureNames.size(); i++) {
          if (sprite->textureId == textureNames[i]) {
            currentItem = static_cast<int>(i);
            break;
          }
        }

        if (ImGui::BeginCombo("Texture ID", sprite->textureId.c_str())) {
          for (size_t i = 0; i < textureNames.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            bool isSelected = (currentItem == static_cast<int>(i));

            // Preview Image
            auto tex = Assets::TextureManager::Get(textureNames[i]);
            if (tex) {
              ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(32, 32));
              ImGui::SameLine();
            }

            if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
              sprite->textureId = textureNames[i];
            }
            if (isSelected)
              ImGui::SetItemDefaultFocus();
            ImGui::PopID();
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
        world.removeComponent<Components::SpriteRenderer>(entity);
      }
    } else {
      if (ImGui::Button("Add SpriteRenderer")) {
        world.addComponent(entity, Components::SpriteRenderer{});
      }
    }
  }

  // Collider
  if (ImGui::CollapsingHeader("Collider")) {
    auto *c = world.getComponent<Components::Collider>(entity);
    if (c) {
      // Shape Selector
      const char *shapeNames[] = {"Box", "Circle"};
      int currentShape = (int)c->shape;
      if (ImGui::Combo("Shape", &currentShape, shapeNames, 2)) {
        c->shape = (Components::Collider::Shape)currentShape;
      }

      if (c->shape == Components::Collider::Shape::Box) {
        ImGui::InputFloat2("Size", &c->size.x);
      } else {
        ImGui::InputFloat("Radius", &c->radius);
      }

      ImGui::InputFloat2("Offset", &c->offset.x);
      ImGui::Checkbox("Is Solid", &c->isSolid);
      ImGui::Checkbox("Trigger", &c->isTrigger);
      ImGui::Checkbox("Blocks Light", &c->blocksLight);
      ImGui::Checkbox("Moveable", &c->moveable);

      if (ImGui::Button("Fit to Sprite")) {
        auto *spr = world.getComponent<Components::SpriteRenderer>(entity);
        if (spr) {
          if (c->shape == Components::Collider::Shape::Box) {
            c->size = spr->size;
          } else {
            // Radius is half the max dimension
            c->radius = std::max(spr->size.x, spr->size.y) * 0.5f;
          }
        }
      }

      if (ImGui::Button("Remove Collider"))
        world.removeComponent<Components::Collider>(entity);
    } else {
      if (ImGui::Button("Add Collider"))
        world.addComponent(entity, Components::Collider{});
    }
  }

  // Health
  if (ImGui::CollapsingHeader("Health")) {
    auto *h = world.getComponent<Components::Health>(entity);
    if (h) {
      ImGui::SliderFloat("HP", &h->hp, 0, h->maxHp);
      ImGui::InputFloat("Max HP", &h->maxHp);
      ImGui::Checkbox("Is Dead", &h->isDead);
      if (ImGui::Button("Remove Health"))
        world.removeComponent<Components::Health>(entity);
    } else {
      if (ImGui::Button("Add Health"))
        world.addComponent(entity, Components::Health{});
    }
  }

  // Battery
  if (ImGui::CollapsingHeader("Battery")) {
    auto *b = world.getComponent<Components::Battery>(entity);
    if (b) {
      ImGui::SliderFloat("Charge", &b->charge, 0, b->maxCharge);
      ImGui::Checkbox("Is Charging", &b->isCharging);
      if (ImGui::Button("Remove Battery"))
        world.removeComponent<Components::Battery>(entity);
    } else {
      if (ImGui::Button("Add Battery"))
        world.addComponent(entity, Components::Battery{});
    }
  }

  // LightExposure
  if (ImGui::CollapsingHeader("LightExposure")) {
    auto *l = world.getComponent<Components::LightExposure>(entity);
    if (l) {
      ImGui::Text("Exposure: %.2f / %.2f", l->exposureTime, l->maxExposureTime);
      ImGui::Checkbox("In Light", &l->inLight);
      if (ImGui::Button("Remove LightExposure"))
        world.removeComponent<Components::LightExposure>(entity);
    } else {
      if (ImGui::Button("Add LightExposure"))
        world.addComponent(entity, Components::LightExposure{});
    }
  }

  // Fragment
  if (ImGui::CollapsingHeader("Fragment")) {
    auto *f = world.getComponent<Components::Fragment>(entity);
    if (f) {
      ImGui::InputInt("ID", &f->fragmentId);
      ImGui::Checkbox("Collected", &f->collected);
      if (ImGui::Button("Remove Fragment"))
        world.removeComponent<Components::Fragment>(entity);
    } else {
      if (ImGui::Button("Add Fragment"))
        world.addComponent(entity, Components::Fragment{});
    }
  }

  // FragmentCollector
  if (ImGui::CollapsingHeader("FragmentCollector")) {
    auto *fc = world.getComponent<Components::FragmentCollector>(entity);
    if (fc) {
      ImGui::InputInt("Required Fragments", &fc->requiredFragments);
      ImGui::InputInt("Collected Count", &fc->collectedCount);
      ImGui::InputFloat("Pickup Radius", &fc->pickupRadius);

      if (ImGui::Button("Remove Collector"))
        world.removeComponent<Components::FragmentCollector>(entity);
    } else {
      if (ImGui::Button("Add Collector"))
        world.addComponent(entity, Components::FragmentCollector{});
    }
  }

  // EnemyAI
  if (ImGui::CollapsingHeader("EnemyAI")) {
    auto *ai = world.getComponent<Components::EnemyAI>(entity);
    if (ai) {
      const char *states[] = {"Patrol", "Chase", "Alert", "Death"};
      int s = (int)ai->state;
      if (ImGui::Combo("State", &s, states, 4))
        ai->state = (Components::AIState)s;
      ImGui::InputFloat("Detect Radius", &ai->detectionRadius);
      if (ImGui::Button("Remove EnemyAI"))
        world.removeComponent<Components::EnemyAI>(entity);
    } else {
      if (ImGui::Button("Add EnemyAI"))
        world.addComponent(entity, Components::EnemyAI{});
    }
  }

  // AudioSource
  if (ImGui::CollapsingHeader("AudioSource")) {
    auto *a = world.getComponent<Components::AudioSource>(entity);
    if (a) {
      char buf[64];
      strcpy_s(buf, sizeof(buf), a->soundId.c_str());
      if (ImGui::InputText("Sound ID", buf, 64))
        a->soundId = buf;
      ImGui::SliderFloat("Volume", &a->volume, 0.0f, 1.0f);
      if (ImGui::Button("Play OneShot"))
        Core::Engine::Get().audio().playOneShot(a->soundId, Audio::Group::SFX,
                                                a->volume);
      if (ImGui::Button("Remove AudioSource"))
        world.removeComponent<Components::AudioSource>(entity);
    } else {
      if (ImGui::Button("Add AudioSource"))
        world.addComponent(entity, Components::AudioSource{"ui_select", 1.0f});
    }
  }

  // SpriteAnimator
  if (ImGui::CollapsingHeader("SpriteAnimator")) {
    auto *anim = world.getComponent<Components::SpriteAnimator>(entity);
    if (anim) {
      ImGui::Checkbox("Playing", &anim->playing);
      ImGui::Checkbox("Paused", &anim->paused);

      if (ImGui::BeginCombo("Current Clip", anim->currentClip.c_str())) {
        for (const auto &[name, clip] : anim->clips) {
          bool isSelected = (anim->currentClip == name);
          if (ImGui::Selectable(name.c_str(), isSelected)) {
            anim->currentClip = name;
            anim->timer = 0.f;
            anim->frameIndex = 0;
          }
          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      // Load logic
      static std::vector<std::string> animFiles;
      static bool filesScanned = false;
      if (!filesScanned || ImGui::Button("Refresh/Scan Files")) {
        animFiles.clear();
        std::string path = "assets/animations";
        if (std::filesystem::exists(path)) {
          for (const auto &entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() == ".json")
              animFiles.push_back(entry.path().filename().string());
          }
        }
        filesScanned = true;
      }

      static int selectedFileIdx = 0;
      if (!animFiles.empty()) {
        if (selectedFileIdx >= animFiles.size())
          selectedFileIdx = 0;
        if (ImGui::Combo(
                "File", &selectedFileIdx,
                [](void *d, int n, const char **out) {
                  *out = (*(std::vector<std::string> *)d)[n].c_str();
                  return true;
                },
                &animFiles, (int)animFiles.size())) {
        }
        if (ImGui::Button("Load Animation")) {
          anim->loadFromPath("assets/animations/" + animFiles[selectedFileIdx]);
        }
      }

      if (ImGui::Button("Remove Animator")) {
        world.removeComponent<Components::SpriteAnimator>(entity);
      }
    } else {
      if (ImGui::Button("Add Animator")) {
        world.addComponent(entity, Components::SpriteAnimator{});
      }
    }
  }

  // Torch
  if (ImGui::CollapsingHeader("Torch")) {
    auto *torch = world.getComponent<Components::TorchComponent>(entity);
    if (torch) {
      bool active = torch->active;
      if (ImGui::Checkbox("Active", &active))
        torch->active = active;
      ImGui::DragFloat2("Offset", &torch->offset.x);
      ImGui::ColorEdit3("Color", &torch->color.r);
      ImGui::DragFloat("Radius", &torch->radius);
      ImGui::DragFloat("Angle", &torch->angle);
      ImGui::SliderFloat("Intensity", &torch->intensity, 0.0f, 1.0f);
      if (ImGui::Button("Remove Torch")) {
        world.removeComponent<Components::TorchComponent>(entity);
      }
    } else {
      if (ImGui::Button("Add Torch"))
        world.addComponent(entity, Components::TorchComponent{});
    }
  }

  // StateMachine
  if (ImGui::CollapsingHeader("StateMachine")) {
    auto *sm = world.getComponent<Components::StateMachine>(entity);
    if (sm) {
      ImGui::Text("Current State: %s", sm->currentState.c_str());
      ImGui::Text("State Timer: %.2f", sm->stateTimer);
      if (ImGui::Button("Remove StateMachine"))
        world.removeComponent<Components::StateMachine>(entity);
    } else {
      if (ImGui::Button("Add StateMachine"))
        world.addComponent(entity, Components::StateMachine{});
    }
  }

  ImGui::End();
}

void SandboxState::renderStatsWindow() {
  auto &world = m_scene.GetWorld();
  ImGui::Begin("ECS Stats", &m_showStatsWindow);

  ImGui::Text("Performance");
  ImGui::Separator();

  // Entity stats
  ImGui::Text("Entity Count: %u", world.entityCount());
  ImGui::Text("Tracked Entities: %zu", m_testEntities.size());

  ImGui::Separator();
  ImGui::Text("Component Counts:");
  ImGui::Text("  Transform: %zu",
              world.componentCount<Components::Transform>());
  ImGui::Text("  Velocity: %zu", world.componentCount<Components::Velocity>());
  ImGui::Text("  SpriteRenderer: %zu",
              world.componentCount<Components::SpriteRenderer>());

  ImGui::Separator();

  // Memory estimation
  size_t entityMemory = world.entityCount() * sizeof(ECS::Entity);
  ImGui::Text("Memory Usage:");
  ImGui::Text("  Entities: %zu bytes (%.2f KB)", entityMemory,
              entityMemory / 1024.0f);

  ImGui::Separator();

  // Frame stats
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

  ImGui::Separator();
  ImGui::Text("Debug Views");
  ImGui::Checkbox("Show Colliders", &m_showColliders);
  ImGui::Checkbox("Enable Shadows", &m_enableShadows);

  ImGui::End();
}

void SandboxState::spawnTestEntity(vec2 position) {
  // Create entity
  auto &world = m_scene.GetWorld();
  auto entity = world.createEntity();

  // Every entity gets a Transform by default (like Unity)
  Components::Transform transform;
  transform.position = position;
  transform.rotation = 0.f;
  transform.scale = vec2{1.f, 1.f};
  world.addComponent(entity, transform);

  // Track it
  m_testEntities.push_back(entity);
}

void SandboxState::cloneTestEntity(ECS::Entity sourceEntity) {
  auto &world = m_scene.GetWorld();

  // Create new entity
  auto newEntity = world.createEntity();

  // Helper lambda for copying components cleanly without C++20 template lambdas
  auto copyXYZ = [&](auto typeIdentity) {
    using T = typename decltype(typeIdentity)::type;
    T *comp = world.getComponent<T>(sourceEntity);
    if (comp) {
      world.addComponent(newEntity, *comp);
    }
  };

  // Copy Components
  copyXYZ(Identity<Components::Velocity>{});
  copyXYZ(Identity<Components::SpriteRenderer>{});
  copyXYZ(Identity<Components::Collider>{});
  copyXYZ(Identity<Components::Health>{});
  copyXYZ(Identity<Components::Battery>{});
  copyXYZ(Identity<Components::LightExposure>{});
  copyXYZ(Identity<Components::Fragment>{});
  copyXYZ(Identity<Components::FragmentCollector>{});
  copyXYZ(Identity<Components::EnemyAI>{});
  copyXYZ(Identity<Components::AudioSource>{});
  copyXYZ(Identity<Components::SpriteAnimator>{});
  copyXYZ(Identity<Components::TorchComponent>{});
  copyXYZ(Identity<Components::StateMachine>{});
  copyXYZ(Identity<Components::PlayerController>{});

  // Transform (Offset slightly so we see it)
  auto *trans = world.getComponent<Components::Transform>(sourceEntity);
  if (trans) {
    Components::Transform newTrans = *trans;
    newTrans.position.x += 20.f;
    newTrans.position.y -= 20.f;
    world.addComponent(newEntity, newTrans);
  }

  // Track it
  m_testEntities.push_back(newEntity);
  // Select the new one
  m_selectedEntityIdx = (int)m_testEntities.size() - 1;
}

void SandboxState::renderSystemsWindow() {
  ImGui::Begin("Systems", &m_showSystemsWindow);

  ImGui::Text("Active Systems:");
  ImGui::Separator();

  ImGui::Checkbox("RenderSystem", &m_scene.m_renderSystemEnabled);
  ImGui::Checkbox("PhysicsSystem", &m_scene.m_physicsSystemEnabled);
  ImGui::Checkbox("CollisionSystem", &m_scene.m_collisionSystemEnabled);
  ImGui::Checkbox("BatterySystem", &m_scene.m_batterySystemEnabled);
  ImGui::Checkbox("LightExposureSystem", &m_scene.m_lightSystemEnabled);
  ImGui::Checkbox("FragmentSystem", &m_scene.m_fragmentSystemEnabled);
  ImGui::Checkbox("AISystem", &m_scene.m_aiSystemEnabled);
  ImGui::Checkbox("AudioSystem", &m_scene.m_audioSystemEnabled);
  ImGui::Checkbox("AnimationSystem", &m_scene.m_animSystemEnabled);
  ImGui::Checkbox("StateMachineSystem", &m_scene.m_stateMachineSystemEnabled);
  ImGui::Checkbox("PlayerSystem", &m_scene.m_playerSystemEnabled);

  ImGui::End();
}

void SandboxState::renderSceneWindow() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("Scene", nullptr,
               ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  m_viewportFocused = ImGui::IsWindowFocused();
  m_viewportHovered = ImGui::IsWindowHovered();

  ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
  if (m_viewportSize.x != viewportPanelSize.x ||
      m_viewportSize.y != viewportPanelSize.y) {
    m_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};
    m_framebuffer.resize((int)m_viewportSize.x, (int)m_viewportSize.y);
    Core::Engine::Get().camera().setViewport((int)m_viewportSize.x,
                                             (int)m_viewportSize.y);
  }

  uint64_t textureID = m_framebuffer.getTextureID();
  // Invert V coordinate because OpenGL textures are upside down relative to
  // ImGui
  ImGui::Image(reinterpret_cast<void *>(textureID), viewportPanelSize,
               ImVec2(0, 1), ImVec2(1, 0));

  // Determine viewport bounds
  ImVec2 windowPos = ImGui::GetWindowPos();
  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
  glm::vec2 viewportMin = {windowPos.x + contentMin.x,
                           windowPos.y + contentMin.y};

  // Gizmos and Picking
  renderGizmos({windowPos.x, windowPos.y},
               {viewportPanelSize.x, viewportPanelSize.y}, viewportMin);

  // Handle Object Picking (Selection)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_viewportHovered &&
      m_gizmoActiveAxis == -1) {
    // Create a dummy gizmo check context or just assume no gizmo if axis is -1
    // Convert mouse to world space
    ImVec2 mousePos = ImGui::GetMousePos();
    glm::vec2 localMouse = {mousePos.x - viewportMin.x,
                            mousePos.y - viewportMin.y};

    Graphics::Camera2D &cam = Core::Engine::Get().camera();
    float zoom = cam.zoom();
    glm::vec2 camPos = cam.position();

    // Adjust for center origin
    glm::vec2 centeredMouse = {
        localMouse.x - m_viewportSize.x * 0.5f,
        -(localMouse.y -
          m_viewportSize.y * 0.5f) // Flip Y because World Y is up, Screen Y is
                                   // down (pixel space)
    };

    glm::vec2 worldPos = camPos + centeredMouse * (1.0f / zoom);

    // Iterate m_testEntities
    auto &world = m_scene.GetWorld();
    float bestDist = FLT_MAX;
    int bestIdx = -1;

    for (int i = 0; i < (int)m_testEntities.size(); ++i) {
      auto entity = m_testEntities[i];
      auto *trans = world.getComponent<Components::Transform>(entity);
      if (trans) {
        // Simple distance check for selection (assume rad 50)
        float dist = glm::distance(
            glm::vec2(trans->position.x, trans->position.y), worldPos);
        if (dist < 50.0f && dist < bestDist) {
          bestDist = dist;
          bestIdx = i;
        }
      }
    }

    if (bestIdx != -1) {
      m_selectedEntityIdx = bestIdx;
    }
  }

  // Gizmo Mode Toggles Hotkeys
  // Gizmo Mode Toggles Hotkeys
  if (m_viewportFocused) {
    if (ImGui::IsKeyPressed(ImGuiKey_W))
      m_gizmoMode = GizmoMode::Translation;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
      m_gizmoMode = GizmoMode::Rotation;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
      m_gizmoMode = GizmoMode::Scale;

    // Cloning (Ctrl + D)
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
      if (m_selectedEntityIdx >= 0 &&
          m_selectedEntityIdx < (int)m_testEntities.size()) {
        cloneTestEntity(m_testEntities[m_selectedEntityIdx]);
      }
    }

    // Deleting (Delete)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
      if (m_selectedEntityIdx >= 0 &&
          m_selectedEntityIdx < (int)m_testEntities.size()) {
        auto &world = m_scene.GetWorld();
        world.destroyEntity(m_testEntities[m_selectedEntityIdx]);
        m_testEntities.erase(m_testEntities.begin() + m_selectedEntityIdx);
        m_selectedEntityIdx = -1;
        m_gizmoActiveAxis = -1; // Reset gizmo
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

void SandboxState::renderGizmos(const glm::vec2 &windowPos,
                                const glm::vec2 &windowSize,
                                const glm::vec2 &viewportMin) {
  (void)windowPos;
  (void)windowSize;
  if (m_selectedEntityIdx < 0 ||
      m_selectedEntityIdx >= (int)m_testEntities.size())
    return;

  auto &world = m_scene.GetWorld();
  auto entity = m_testEntities[m_selectedEntityIdx];
  auto *trans = world.getComponent<Components::Transform>(entity);
  if (!trans)
    return;

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  Graphics::Camera2D &cam = Core::Engine::Get().camera();

  // World to Screen
  glm::vec2 camPos = cam.position();
  float zoom = cam.zoom();

  glm::vec2 worldDelta =
      glm::vec2(trans->position.x, trans->position.y) - camPos;
  glm::vec2 screenCenter = viewportMin + m_viewportSize * 0.5f;
  // Note: World Y is up, Screen Y is down.
  glm::vec2 objectScreenPos =
      screenCenter + glm::vec2(worldDelta.x * zoom, -worldDelta.y * zoom);

  float handleThickness = 6.0f;
  float handleLength = 60.0f;
  float interactionThresh = 8.0f; // Screen pixels tolerance

  ImVec2 origin = {objectScreenPos.x, objectScreenPos.y};
  ImU32 colX = IM_COL32(200, 50, 50, 255);
  ImU32 colY = IM_COL32(50, 200, 50, 255);
  ImU32 colZ = IM_COL32(50, 50, 200, 255);
  ImU32 colHover = IM_COL32(255, 255, 0, 255);

  ImVec2 mousePos = ImGui::GetMousePos();
  bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (mouseReleased) {
    m_gizmoActiveAxis = -1;
  }

  // Helper: Distance from point p to segment v-w
  auto distToSegment = [](ImVec2 p, ImVec2 v, ImVec2 w) {
    float l2 = static_cast<float>(pow(v.x - w.x, 2) + pow(v.y - w.y, 2));
    if (l2 == 0)
      return static_cast<float>(sqrt(pow(p.x - v.x, 2) + pow(p.y - v.y, 2)));
    float t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
    t = std::max(0.0f, std::min(1.0f, t));
    ImVec2 projection = {v.x + t * (w.x - v.x), v.y + t * (w.y - v.y)};
    return static_cast<float>(
        sqrt(pow(p.x - projection.x, 2) + pow(p.y - projection.y, 2)));
  };

  // Calculate Rotated Axis Directions (Screen Space)
  // World rotation is radians CCW.
  // Screen space Y is flipped.
  float angle = trans->rotation;
  float c = cos(angle);
  float s = sin(angle);

  // World space basis vectors
  // X: (c, s)
  // Y: (-s, c)
  // Screen space transform: y component negated.
  glm::vec2 xAxisScreen = glm::vec2(c, -s);
  glm::vec2 yAxisScreen = glm::vec2(-s, -c);

  // Normalize handle vectors in screen pixels
  ImVec2 xHandleVec = {xAxisScreen.x * handleLength,
                       xAxisScreen.y * handleLength};
  ImVec2 yHandleVec = {yAxisScreen.x * handleLength,
                       yAxisScreen.y * handleLength};

  // Gizmo Logic
  if (m_gizmoMode == GizmoMode::Translation) {
    ImVec2 xEnd = {origin.x + xHandleVec.x, origin.y + xHandleVec.y};
    ImVec2 yEnd = {origin.x + yHandleVec.x, origin.y + yHandleVec.y};

    bool hoverX = distToSegment(mousePos, origin, xEnd) < interactionThresh;
    bool hoverY = distToSegment(mousePos, origin, yEnd) < interactionThresh;

    if (m_gizmoActiveAxis == 0)
      hoverX = true;
    if (m_gizmoActiveAxis == 1)
      hoverY = true;

    drawList->AddLine(origin, xEnd, hoverX ? colHover : colX, handleThickness);
    drawList->AddLine(origin, yEnd, hoverY ? colHover : colY, handleThickness);

    // Arrow heads (Manual rotation...)
    // Simpler: Just stick to lines or render triangle at end manually rotated
    // X Head
    {
      glm::vec2 dir = glm::normalize(xAxisScreen);
      glm::vec2 perp = {-dir.y, dir.x}; // 90 deg screen space
      ImVec2 p1 = {xEnd.x, xEnd.y};
      ImVec2 p2 = {xEnd.x - dir.x * 10 + perp.x * 5,
                   xEnd.y - dir.y * 10 + perp.y * 5};
      ImVec2 p3 = {xEnd.x - dir.x * 10 - perp.x * 5,
                   xEnd.y - dir.y * 10 - perp.y * 5};
      drawList->AddTriangleFilled(p1, p2, p3, hoverX ? colHover : colX);
    }
    // Y Head
    {
      glm::vec2 dir = glm::normalize(yAxisScreen);
      glm::vec2 perp = {-dir.y, dir.x};
      ImVec2 p1 = {yEnd.x, yEnd.y};
      ImVec2 p2 = {yEnd.x - dir.x * 10 + perp.x * 5,
                   yEnd.y - dir.y * 10 + perp.y * 5};
      ImVec2 p3 = {yEnd.x - dir.x * 10 - perp.x * 5,
                   yEnd.y - dir.y * 10 - perp.y * 5};
      drawList->AddTriangleFilled(p1, p2, p3, hoverY ? colHover : colY);
    }

    if (mouseClicked && (hoverX || hoverY) && !m_isDragging) {
      m_gizmoActiveAxis = hoverX ? 0 : 1;
      m_gizmoOriginalPos = {trans->position.x, trans->position.y};
      m_gizmoClickOffset = {mousePos.x, mousePos.y};
    }

    if (mouseDown && m_gizmoActiveAxis != -1) {
      // Drag logic needs to project mouse movement onto the axis in Screen
      // Space Then apply that magnitude to World Space axis
      float dx = (mousePos.x - m_gizmoClickOffset.x);
      float dy = (mousePos.y - m_gizmoClickOffset.y);

      // Dot product with screen axis to get magnitude
      glm::vec2 screenMove(dx, dy);
      glm::vec2 axisDir = (m_gizmoActiveAxis == 0) ? xAxisScreen : yAxisScreen;
      axisDir = glm::normalize(axisDir);

      float proj = glm::dot(screenMove, axisDir);

      // proj is pixels moved along the screen axis
      // Convert to world units: divide by zoom
      float worldMove = proj / zoom;

      // World axis matches screen axis conceptually but Y component is flipped
      // for storage? No, trans->rotation tells us the world axis direction.
      // X-Axis world vector is (c, s).
      // So we move worldMove * (c, s)

      glm::vec2 worldDir =
          (m_gizmoActiveAxis == 0) ? glm::vec2(c, s) : glm::vec2(-s, c);

      trans->position.x = m_gizmoOriginalPos.x + worldDir.x * worldMove;
      trans->position.y = m_gizmoOriginalPos.y + worldDir.y * worldMove;
    }
  } else if (m_gizmoMode == GizmoMode::Rotation) {
    float radius = handleLength * 0.8f;
    drawList->AddCircle(origin, radius, colZ, 64, handleThickness / 2);

    float dist = static_cast<float>(
        sqrt(pow(mousePos.x - origin.x, 2) + pow(mousePos.y - origin.y, 2)));
    bool hover = abs(dist - radius) < 10.0f;

    if (m_gizmoActiveAxis == 2)
      hover = true;

    if (hover)
      drawList->AddCircle(origin, radius, colHover, 64, handleThickness);

    if (mouseClicked && hover && !m_isDragging) {
      m_gizmoActiveAxis = 2;
      m_gizmoOriginalRotation = trans->rotation;
      m_gizmoClickOffset.x =
          atan2(mousePos.y - origin.y, mousePos.x - origin.x);
    }

    if (mouseDown && m_gizmoActiveAxis == 2) {
      float currentAngle = atan2(mousePos.y - origin.y, mousePos.x - origin.x);
      float delta = currentAngle - m_gizmoClickOffset.x;
      // Screen Y down means +angle is CW. World +angle is CCW.
      // So delta screen angle needs to be subtracted?
      // If I move mouse CW (angle increases), I intend to rotate CW (angle
      // decreases in standard math). So newRotation = old - delta.
      trans->rotation = m_gizmoOriginalRotation - delta;
    }
  } else if (m_gizmoMode == GizmoMode::Scale) {
    ImVec2 xEnd = {origin.x + xHandleVec.x, origin.y + xHandleVec.y};
    ImVec2 yEnd = {origin.x + yHandleVec.x, origin.y + yHandleVec.y};

    bool hoverX = distToSegment(mousePos, origin, xEnd) < interactionThresh;
    bool hoverY = distToSegment(mousePos, origin, yEnd) < interactionThresh;

    if (m_gizmoActiveAxis == 0)
      hoverX = true;
    if (m_gizmoActiveAxis == 1)
      hoverY = true;

    drawList->AddLine(origin, xEnd, hoverX ? colHover : colX, handleThickness);
    drawList->AddLine(origin, yEnd, hoverY ? colHover : colY, handleThickness);

    // Box heads
    {
      auto drawBox = [&](ImVec2 center, ImU32 col) {
        drawList->AddRectFilled({center.x - 5, center.y - 5},
                                {center.x + 5, center.y + 5}, col);
      };
      drawBox(xEnd, hoverX ? colHover : colX);
      drawBox(yEnd, hoverY ? colHover : colY);
    }

    if (mouseClicked && (hoverX || hoverY)) {
      m_gizmoActiveAxis = hoverX ? 0 : 1;
      m_gizmoOriginalScale = {trans->scale.x, trans->scale.y};
      m_gizmoClickOffset = {mousePos.x, mousePos.y};
    }

    if (mouseDown && m_gizmoActiveAxis != -1) {
      float dx = (mousePos.x - m_gizmoClickOffset.x);
      float dy = (mousePos.y - m_gizmoClickOffset.y);

      glm::vec2 screenMove(dx, dy);
      glm::vec2 axisDir = (m_gizmoActiveAxis == 0) ? xAxisScreen : yAxisScreen;
      axisDir = glm::normalize(axisDir);
      float proj = glm::dot(screenMove, axisDir);

      // Scale sensitivity
      float scaleDelta = proj * 0.01f;

      if (m_gizmoActiveAxis == 0)
        trans->scale.x = m_gizmoOriginalScale.x + scaleDelta;
      if (m_gizmoActiveAxis == 1)
        trans->scale.y = m_gizmoOriginalScale.y + scaleDelta;
    }
  }
}

} // namespace Runtime
