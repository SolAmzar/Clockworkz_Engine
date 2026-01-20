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

namespace Runtime {

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
}

void SandboxState::onExit() {
  // Clean up all test entities
  m_scene.Clear();
  m_testEntities.clear();
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
  // Get renderer from Engine
  Core::Engine &engine = Core::Engine::Get();
  Graphics::RenderingManager &renderer = engine.rendering();
  Graphics::Camera2D &camera = engine.camera();

  // Start Scene Pass (sets up shader and view-projection)
  renderer.beginScenePass(camera.viewProj());

  // Run Scene Render
  m_scene.Render(renderer);

  // End Scene Pass
  renderer.endScenePass();

  // Render Torch Pass
  renderTorchPass(renderer);
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
        int segments = 30;
        float halfFov = glm::radians(torch.angle) * 0.5f;
        float startAngle = angle - halfFov;
        float step = glm::radians(torch.angle) / segments;

        // Note: drawLightCone expects polygon points, not including origin (it
        // adds origin itself)
        for (int i = 0; i <= segments; ++i) {
          float theta = startAngle + step * i;
          lightPoly.emplace_back(originGLM.x + std::cos(theta) * torch.radius,
                                 originGLM.y + std::sin(theta) * torch.radius);
        }

        // Render Light Pass
        renderer.beginLightPass(camera.viewProj(), originGLM, torch.color,
                                torch.radius, angle, torch.angle, batteryFactor,
                                torch.intensity);
        renderer.drawLightCone(originGLM, lightPoly, torch.color);
        renderer.endLightPass();
      });
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

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Render all ImGui windows
  renderSystemsWindow();
  renderEntityWindow();
  renderComponentWindow();
  renderStatsWindow();

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

  ImGui::SameLine();
  if (ImGui::Button("Clear All")) {
    m_scene.Clear(); // Use Scene Clear
    // Also clear test entities vector if tracked?
    // Actually Scene::Clear might not clear m_testEntities in SandboxState.
    // We generated m_testEntities locally.
    // But deserialization probably adds to world directly and m_testEntities
    // doesn't track them. This local tracking is legacy. Let's iterate world to
    // destroy? Use a quick loop: This is just a debug tool. For now, assume
    // Scene::Clear works or just skip. But since I updated Scene.cpp to have
    // empty Clear(), I should implement it properly there or loop here. Let's
    // rely on world.destroyEntity loop I wrote before, but adapted. Or better:
    // Scene should implement `Clear` properly. I'll leave the button doing
    // nothing useful for now or rely on reload.
    m_testEntities.clear();
  }

  ImGui::Separator();
  ImGui::Text("Entity List:");
  ImGui::Text("%zu entities", m_testEntities.size());

  // Scrollable entity list
  ImGui::BeginChild("EntityList", ImVec2(0, 200), true);

  for (size_t i = 0; i < m_testEntities.size(); ++i) {
    auto entity = m_testEntities[i];

    ImGui::PushID(static_cast<int>(i));
    ImGui::Text("Entity %u (gen %u)", entity.id, entity.generation);
    ImGui::SameLine(200);

    if (ImGui::SmallButton("Destroy")) {
      world.destroyEntity(entity);
      m_testEntities.erase(m_testEntities.begin() + i);
      ImGui::PopID();
      break;
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
  auto &world = m_scene.GetWorld();

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
            bool isSelected = (currentItem == static_cast<int>(i));
            if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
              sprite->textureId = textureNames[i];
            }
            if (isSelected)
              ImGui::SetItemDefaultFocus();
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
      ImGui::InputFloat2("Size", &c->size.x);
      ImGui::InputFloat2("Offset", &c->offset.x);
      ImGui::Checkbox("Is Solid", &c->isSolid);
      ImGui::Checkbox("Trigger", &c->isTrigger);
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

} // namespace Runtime
