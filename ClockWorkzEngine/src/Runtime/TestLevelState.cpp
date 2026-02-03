/******************************************************************************/
/*!
\file           TestLevelState.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Implementation of TestLevelState.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/TestLevelState.hpp"

// Must be included before other OpenGL headers
#include "Platform/WindowManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Portal.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/MenuState.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>


namespace Runtime {

TestLevelState::TestLevelState(Platform::InputManager &input,
                               GameStateManager &gsm)
    : m_input(input), m_gsm(gsm) {}

TestLevelState::~TestLevelState() = default;

void TestLevelState::onEnter() {
  m_scene.Init();

  // Load necessary shaders
  Assets::ResourceManager<Graphics::Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
  Assets::ResourceManager<Graphics::Shader>::Load(
      "light", "assets/shaders/light.vert", "assets/shaders/light.frag");

  // Load Level_Save.json
  std::string levelPath = "assets/scenes/Level_Save.json";
  std::cout << "[TestLevelState] Loading level: " << levelPath << "\n";

  if (std::filesystem::exists(levelPath)) {
    SceneSerializer::Deserialize(m_scene.GetWorld(), levelPath);
  } else {
    std::cerr << "[TestLevelState] Error: " << levelPath << " not found!\n";
  }

  // Reset viewport and camera
  auto &window = Core::Engine::Get().window();
  m_viewportSize = {static_cast<float>(window.width()),
                    static_cast<float>(window.height())};
  glViewport(0, 0, window.width(), window.height());
  Core::Engine::Get().camera().setViewport(window.width(), window.height());

  // Force camera to 0,0 or find player? Update will handle it.
}

void TestLevelState::onExit() { m_scene.Clear(); }

void TestLevelState::update(double dt) {
  // Check pending level load
  if (!m_pendingLevel.empty()) {
    loadLevel(m_pendingLevel);
    m_pendingLevel.clear();
    return;
  }

  // ESC -> MenuState
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.switchTo(std::make_shared<MenuState>(m_gsm, m_input,
                                               Core::Engine::Get().window(),
                                               Core::Engine::Get().audio()));
  }

  // Update Scene
  m_scene.Update(dt, m_input);

  // Camera Follow Player Logic
  auto &camera = Core::Engine::Get().camera();
  auto &world = m_scene.GetWorld();
  bool foundPlayer = false;

  world.each<Components::PlayerController, Components::Transform>(
      [&](ECS::Entity, Components::PlayerController &,
          Components::Transform &t) {
        camera.setPosition(glm::vec2(t.position.x, t.position.y));
        foundPlayer = true;
      });

  // If no player, maybe retain previous pos or do nothing

  // --- PORTAL LOGIC ---
  // Check collisions for portals
  auto &collisions = m_scene.GetCollisionSystem().getCollisions();
  // world is already defined above

  for (const auto &pair : collisions) {
    auto entityA = pair.first;
    auto entityB = pair.second;

    if (!world.isValid(entityA) || !world.isValid(entityB))
      continue;

    bool playerA = world.hasComponent<Components::PlayerController>(entityA);
    bool portalA = world.hasComponent<Components::Portal>(entityA);
    bool playerB = world.hasComponent<Components::PlayerController>(entityB);
    bool portalB = world.hasComponent<Components::Portal>(entityB);

    ECS::Entity portalEntity = {0, 0};

    if (playerA && portalB) {
      portalEntity = entityB;
    } else if (playerB && portalA) {
      portalEntity = entityA;
    }

    if (portalEntity.isValid()) {
      auto *portal = world.getComponent<Components::Portal>(portalEntity);
      if (portal && !portal->levelName.empty()) {
        m_pendingLevel = portal->levelName;
        break; // Only one transition per frame
      }
    }
  }
}

void TestLevelState::render(Core::Time *timer) {
  (void)timer;

  Core::Engine &engine = Core::Engine::Get();
  Graphics::RenderingManager &renderer = engine.rendering();
  Graphics::Camera2D &camera = engine.camera();

  // Clear screen
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Render Scene
  renderer.beginScenePass(camera.viewProj());
  m_scene.Render(renderer);
  renderer.endScenePass();

  // Render Torch Pass
  renderTorchPass(renderer);

  // Render HUD
  if (m_showHUD && m_viewportSize.x > 0 && m_viewportSize.y > 0) {
    glm::mat4 uiProj =
        glm::ortho(0.0f, m_viewportSize.x, 0.0f, m_viewportSize.y, -1.0f, 1.0f);

    renderer.beginScenePass(uiProj);

    auto &world = m_scene.GetWorld();
    world.each<Components::PlayerController, Components::Battery>(
        [&](ECS::Entity, Components::PlayerController &,
            Components::Battery &batt) {
          float pct = batt.charge / batt.maxCharge;

          std::string texId = "battery_empty";
          if (pct > 0.875f)
            texId = "battery_full";
          else if (pct > 0.625f)
            texId = "battery_3/4";
          else if (pct > 0.375f)
            texId = "battery_2/4";
          else if (pct > 0.125f)
            texId = "battery_1/4";

          glm::vec2 uiSize(128.0f, 64.0f);
          float margin = 20.0f;
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
}

void TestLevelState::renderImGui(Core::Time *timer) {
  (void)timer;
  // No ImGui editor
}

void TestLevelState::renderTorchPass(Graphics::RenderingManager &renderer) {
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

        vec2 torchOffsetLocal(torch.offset.x, torch.offset.y);
        vec2 torchOffsetWorld(torchOffsetLocal.x * std::cos(angle) -
                                  torchOffsetLocal.y * std::sin(angle),
                              torchOffsetLocal.x * std::sin(angle) +
                                  torchOffsetLocal.y * std::cos(angle));

        glm::vec2 originGLM =
            glm::vec2((trans.position.x + torchOffsetWorld.x),
                      (trans.position.y + torchOffsetWorld.y));

        // Create simple light polygon (Fan)
        std::vector<glm::vec2> lightPoly;
        int segments = 100;
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
                    glm::vec2 localStart =
                        RotateVec(start - obsCenter, -obsTrans.rotation);
                    glm::vec2 localDir = RotateVec(dir, -obsTrans.rotation);

                    glm::vec2 scaledSize =
                        glm::vec2(obsCol.size.x * obsTrans.scale.x,
                                  obsCol.size.y * obsTrans.scale.y);
                    glm::vec2 halfSize = scaledSize * 0.5f;

                    glm::vec2 min = -halfSize;
                    glm::vec2 max = halfSize;

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

                    if (tmax >= std::max(0.0f, tmin) && tmin < closestDist) {
                      closestDist = tmin;
                      closestPoint = start + dir * tmin;
                    }
                  } else if (obsCol.shape ==
                             Components::Collider::Shape::Circle) {
                    // --- Circle Logic ---
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

} // namespace Runtime

void Runtime::TestLevelState::loadLevel(const std::string &levelName) {
  std::string path = "assets/scenes/" + levelName;
  std::cout << "[TestLevelState] Loading level: " << path << "\n";
  m_scene.LoadFromFile(path);
}
