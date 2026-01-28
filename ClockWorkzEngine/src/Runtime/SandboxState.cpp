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
#include "Platform/WindowManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

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
#include "Gameplay/Components/ParticleEmitter.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_internal.h"
#include "Math/Vec2.hpp"
#include "Platform/InputManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/MenuState.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "Systems/AISystem.hpp"
#include "Systems/AnimationSystem.hpp"
#include "Systems/AudioSystem.hpp"
#include "Systems/StateMachineSystem.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace Runtime {

vec2 snapToGrid(const vec2 &pos, float gridSize) {
  float halfGrid = gridSize * 0.5f;
  return vec2(std::round((pos.x - halfGrid) / gridSize) * gridSize + halfGrid,
              std::round((pos.y - halfGrid) / gridSize) * gridSize + halfGrid);
}
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
  Assets::ResourceManager<Graphics::Shader>::Load(
      "text", "assets/shaders/text.vert", "assets/shaders/text.frag");

  // Load Debug Font for HUD
  Assets::ResourceManager<Assets::FontManager>::Load(
      "Debug", "assets/fonts/MANIC1.TTF", 24);

  // Load prefabs
  loadAvailablePrefabs();
  refreshTilemapList();

  // Init Framebuffer for Scene View
  m_framebuffer.create(800, 600);

  // Force reset dock layout on enter
  m_dockLayoutInitialized = false;
}

void SandboxState::onExit() {
  // Clean up all test entities
  m_scene.Clear();
  m_testEntities.clear();
  m_framebuffer.destroy();

  // Reset viewport to full window size to prevent menu from being squashed
  auto &window = Core::Engine::Get().window();
  glViewport(0, 0, window.width(), window.height());
  // Also reset camera projection to match window
  Core::Engine::Get().camera().setViewport(window.width(), window.height());
}

void SandboxState::update(double dt) {
  (void)dt;
  // Handle ESC to return to menu
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.switchTo(std::make_shared<MenuState>(m_gsm, m_input,
                                               Core::Engine::Get().window(),
                                               Core::Engine::Get().audio()));
  }

  // Update Scene Logic
  // Update Scene Logic
  if (!m_isPaused) {
    m_scene.Update(dt, m_input);
  }

  // Camera Logic
  auto &camera = Core::Engine::Get().camera();

  // Only follow player if we are Playing AND Follow is enabled
  if (!m_isPaused && m_cameraFollowPlayer) {
    // Camera Follow Logic
    auto &world = m_scene.GetWorld();
    world.each<Components::PlayerController, Components::Transform>(
        [&](ECS::Entity, Components::PlayerController &,
            Components::Transform &t) {
          // Simple hard lock
          camera.setPosition(glm::vec2(t.position.x, t.position.y));
          m_editorCameraPos = glm::vec2(t.position.x, t.position.y); // Sync pos
        });
  } else {
    // Free Camera Logic (Editor Mode OR Playing with Free Cam)

    // 1. Zoom Logic (Mouse Wheel)
    float zoomSpeed = 0.1f;
    float scroll = static_cast<float>(Platform::InputManager::GetScrollY());
    if (scroll != 0.0f) {
      float currentZoom = camera.zoom();
      float newZoom = currentZoom + (scroll * zoomSpeed * currentZoom);
      // Clamp zoom to reasonable values
      if (newZoom < 0.1f)
        newZoom = 0.1f;
      if (newZoom > 5.0f)
        newZoom = 5.0f;
      camera.setZoom(newZoom);
    }

    // 2. Pan Logic (MMB or Alt+LMB)
    static double lastMouseX = 0.0, lastMouseY = 0.0;
    double currMouseX, currMouseY;
    m_input.getMousePosition(currMouseX, currMouseY);

    if (m_input.isMouseDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
      // Calculate Delta
      float dx = static_cast<float>(currMouseX - lastMouseX);
      float dy = static_cast<float>(currMouseY - lastMouseY);

      // Apply to camera position (inverted for drag feeling)
      // Scale by zoom to keep panning constant relative to view
      float panScale = 1.0f / camera.zoom();

      m_editorCameraPos.x -= dx * panScale;
      m_editorCameraPos.y += dy * panScale;
    }

    lastMouseX = currMouseX;
    lastMouseY = currMouseY;

    // Focus on Selected Entity (F)
    if (m_input.wasKeyTriggered(GLFW_KEY_F)) {
      if (m_selectedEntityIdx >= 0 &&
          m_selectedEntityIdx < static_cast<int>(m_testEntities.size())) {
        auto entity = m_testEntities[m_selectedEntityIdx];
        auto *transform =
            m_scene.GetWorld().getComponent<Components::Transform>(entity);
        if (transform) {
          m_editorCameraPos =
              glm::vec2(transform->position.x, transform->position.y);
          camera.setPosition(m_editorCameraPos);
          camera.setZoom(1.0f);
        }
      }
    }

    // WASD Logic (Keep as backup or modifier)
    // User requested Ctrl + WASD to avoid conflict with Player controls
    if (m_input.isKeyDown(GLFW_KEY_LEFT_CONTROL)) {
      float camSpeed = 500.0f / camera.zoom() * static_cast<float>(dt);
      if (m_input.isKeyDown(GLFW_KEY_W))
        m_editorCameraPos.y += camSpeed;
      if (m_input.isKeyDown(GLFW_KEY_S))
        m_editorCameraPos.y -= camSpeed;
      if (m_input.isKeyDown(GLFW_KEY_A))
        m_editorCameraPos.x -= camSpeed;
      if (m_input.isKeyDown(GLFW_KEY_D))
        m_editorCameraPos.x += camSpeed;

      camera.setPosition(m_editorCameraPos);
    }
  }
}

void SandboxState::render(Core::Time *timer) {
  (void)timer;
  // We render the scene into the framebuffer for the editor view
  m_framebuffer.bind();

  // Get renderer from Engine
  Core::Engine &engine = Core::Engine::Get();
  Graphics::RenderingManager &renderer = engine.rendering();
  Graphics::Camera2D &camera = engine.camera();

  // Clear screen
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Start Scene Pass (sets up shader and view-projection)
  renderer.beginScenePass(camera.viewProj());

  // Run Scene Render
  m_scene.Render(renderer);

  // End Scene Pass
  renderer.endScenePass();

  // Render Torch Pass
  renderTorchPass(renderer);

  // Render Grid
  renderer.beginDebugPass(camera.viewProj());
  renderGrid();
  renderer.endDebugPass();

  // Debug Collider Pass
  if (m_showColliders)
    renderDebugColliders(renderer);

  // Render HUD (Battery)
  if (m_showHUD && m_viewportSize.x > 0 && m_viewportSize.y > 0) {
    // Determine Ortho Projection for UI (0,0 bottom-left, to matches GL
    // default)
    glm::mat4 uiProj =
        glm::ortho(0.0f, m_viewportSize.x, 0.0f, m_viewportSize.y, -1.0f, 1.0f);

    // Begin UI Pass (reuses Scene Pass setup but with UI projection)
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

    // --- Render Fragment HUD ---
    // Make sure we are in Scene Pass (UI Projection) which we are from Battery
    // HUD

    world.each<Components::FragmentCollector, Components::PlayerController>(
        [&](ECS::Entity, Components::FragmentCollector &col,
            Components::PlayerController &) {
          glm::vec2 uiSize(64.0f, 64.0f);
          float margin = 20.0f;

          // Position to the left of Battery (which is at right edge)
          // Battery ~ 128px wide + margin.
          float batteryWidth = 128.0f;
          float spacing = 20.0f;

          // x = ViewportW - margin - BatteryW - spacing - FragIconW
          float fragIconX =
              m_viewportSize.x - margin - batteryWidth - spacing - uiSize.x;
          float fragIconY = m_viewportSize.y - margin - uiSize.y;

          glm::vec2 uiPos(fragIconX, fragIconY);

          // 1. Draw Icon
          auto tex = Assets::TextureManager::Get("fragment_texture");
          if (tex) {
            renderer.drawSprite(tex->id(), uiPos, uiSize, 0.0f,
                                glm::vec4(0, 0, 1, 1), glm::vec4(1), true);
          }

          // 2. Draw Text (Collected Count)
          // Switch to Font Pass for text
          renderer.endScenePass(); // End Sprite Pass temporarily
          renderer.beginFontPass(uiProj);

          auto shader = Assets::ResourceManager<Graphics::Shader>::Get("text");
          auto font =
              Assets::ResourceManager<Assets::FontManager>::Get("Debug");

          if (shader && font) {
            std::string text = std::to_string(col.collectedCount) + " / " +
                               std::to_string(col.requiredFragments);

            // Rough centering vertically relative to icon
            float textY = fragIconY + (uiSize.y * 0.25f);
            float textX =
                fragIconX - (text.length() * 15.0f) - 10.0f; // Estimate width

            font->RenderText(text, textX, textY, 1.0f, glm::vec3(1.0f),
                             *shader);
          }

          // Restart Scene Pass to restore state for next operations (or clean
          // exit)
          renderer.beginScenePass(uiProj);
        });

    renderer.endScenePass();
  }

  m_framebuffer.unbind();
}

void SandboxState::savePrefabFromEntity(ECS::Entity entity,
                                        const std::string &name,
                                        const std::string &category) {
  auto &world = m_scene.GetWorld();

  // Create JSON for prefab
  nlohmann::json prefabJson;
  prefabJson["name"] = name;
  prefabJson["category"] = category;
  prefabJson["components"] = nlohmann::json::object();

  auto &components = prefabJson["components"];

  // Serialize Transform
  if (auto *trans = world.getComponent<Components::Transform>(entity)) {
    components["transform"] = {
        {"position", {{"x", trans->position.x}, {"y", trans->position.y}}},
        {"rotation", trans->rotation},
        {"scale", {{"x", trans->scale.x}, {"y", trans->scale.y}}}};
  }

  // Serialize SpriteRenderer
  if (auto *sprite = world.getComponent<Components::SpriteRenderer>(entity)) {
    components["spriteRenderer"] = {
        {"textureId", sprite->textureId},
        {"size", {{"x", sprite->size.x}, {"y", sprite->size.y}}},
        {"color",
         {{"r", sprite->color.r},
          {"g", sprite->color.g},
          {"b", sprite->color.b},
          {"a", sprite->color.a}}}};
  }

  // Serialize Collider
  if (auto *col = world.getComponent<Components::Collider>(entity)) {
    components["collider"] = {{"width", col->size.x},
                              {"height", col->size.y},
                              {"isSolid", col->isSolid},
                              {"isHazard", col->isHazard},
                              {"isTrigger", col->isTrigger},
                              {"moveable", col->moveable},
                              {"collisionCategory", col->collisionCategory},
                              {"collisionMask", col->collisionMask}};
  }

  // Serialize Health
  if (auto *health = world.getComponent<Components::Health>(entity)) {
    components["health"] = {{"hp", health->hp}, {"maxHp", health->maxHp}};
  }

  // Serialize Battery
  if (auto *battery = world.getComponent<Components::Battery>(entity)) {
    components["battery"] = {{"charge", battery->charge},
                             {"maxCharge", battery->maxCharge},
                             {"drainRate", battery->drainRate}};
  }

  // Add more component serialization as needed...

  // Save to file
  std::string filepath = "assets/prefabs/" + name + ".json";
  std::ofstream file(filepath);
  if (file.is_open()) {
    file << prefabJson.dump(4);
    file.close();
    std::cout << "[Prefab] Saved: " << filepath << "\n";

    // Reload available prefabs
    loadAvailablePrefabs();
  } else {
    std::cerr << "[Prefab] Failed to save: " << filepath << "\n";
  }
}

void SandboxState::loadAvailablePrefabs() {
  namespace fs = std::filesystem;
  m_availablePrefabs.clear();

  std::string prefabDir = "assets/prefabs/";
  if (fs::exists(prefabDir) && fs::is_directory(prefabDir)) {
    for (const auto &entry : fs::directory_iterator(prefabDir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        m_availablePrefabs.push_back(entry.path().stem().string());
      }
    }
  }

  std::cout << "[Prefab] Loaded " << m_availablePrefabs.size() << " prefabs\n";
}

void SandboxState::refreshTilemapList() {
  m_availableTilemaps.clear();
  namespace fs = std::filesystem;
  std::string path = "assets/scenes"; // Changed to scenes

  if (!fs::exists(path)) {
    fs::create_directories(path);
  }

  for (const auto &entry : fs::directory_iterator(path)) {
    if (entry.path().extension() == ".json") {
      m_availableTilemaps.push_back(entry.path().filename().string());
    }
  }

  if (m_selectedTilemapIdx >= (int)m_availableTilemaps.size()) {
    m_selectedTilemapIdx = 0;
  }
}

void SandboxState::spawnPrefab(const std::string &prefabName, vec2 position) {
  std::string filepath = "assets/prefabs/" + prefabName + ".json";
  std::ifstream file(filepath);

  if (!file.is_open()) {
    std::cerr << "[Prefab] Failed to load: " << filepath << "\n";
    return;
  }

  nlohmann::json prefabJson;
  file >> prefabJson;
  file.close();

  auto &world = m_scene.GetWorld();
  auto entity = world.createEntity();

  // Deserialize components
  if (prefabJson.contains("components")) {
    auto &components = prefabJson["components"];

    // Transform
    if (components.contains("transform")) {
      Components::Transform trans;
      trans.position = position; // Use spawn position
      trans.rotation = components["transform"].value("rotation", 0.0f);
      if (components["transform"].contains("scale")) {
        trans.scale.x = components["transform"]["scale"].value("x", 1.0f);
        trans.scale.y = components["transform"]["scale"].value("y", 1.0f);
      }
      world.addComponent(entity, trans);
    }

    // SpriteRenderer
    if (components.contains("spriteRenderer")) {
      Components::SpriteRenderer sprite;
      sprite.textureId = components["spriteRenderer"].value("textureId", "");

      if (components["spriteRenderer"].contains("size")) {
        sprite.size.x = components["spriteRenderer"]["size"].value("x", 50.0f);
        sprite.size.y = components["spriteRenderer"]["size"].value("y", 50.0f);
      }

      if (components["spriteRenderer"].contains("color")) {
        auto &color = components["spriteRenderer"]["color"];
        sprite.color.r = color.value("r", 1.0f);
        sprite.color.g = color.value("g", 1.0f);
        sprite.color.b = color.value("b", 1.0f);
        sprite.color.a = color.value("a", 1.0f);
      }
      world.addComponent(entity, sprite);
    }

    // Collider
    if (components.contains("collider")) {
      Components::Collider col;
      col.size.x = components["collider"].value("width", 50.0f);
      col.size.y = components["collider"].value("height", 50.0f);
      col.isSolid = components["collider"].value("isSolid", true);
      col.isHazard = components["collider"].value("isHazard", false);
      col.isTrigger = components["collider"].value("isTrigger", false);
      col.moveable = components["collider"].value("moveable", false);
      col.collisionCategory =
          components["collider"].value("collisionCategory", 1);
      col.collisionMask =
          components["collider"].value("collisionMask", 0xFFFFFFFF);
      world.addComponent(entity, col);
    }

    // Health
    if (components.contains("health")) {
      Components::Health health;
      health.hp = components["health"].value("hp", 100.0f);
      health.maxHp = components["health"].value("maxHp", 100.0f);
      world.addComponent(entity, health);
    }

    // Battery
    if (components.contains("battery")) {
      Components::Battery battery;
      battery.charge = components["battery"].value("charge", 100.0f);
      battery.maxCharge = components["battery"].value("maxCharge", 100.0f);
      battery.drainRate = components["battery"].value("drainRate", 1.0f);
      world.addComponent(entity, battery);
    }

    // Velocity
    if (components.contains("velocity")) {
      Components::Velocity velocity;
      velocity.velocity.x = components["velocity"].value("x", 0.0f);
      velocity.velocity.y = components["velocity"].value("y", 0.0f);
      velocity.moveSpeed = components["velocity"].value("moveSpeed", 100.0f);
      world.addComponent(entity, velocity);
    }

    // PlayerController
    if (components.contains("playerController")) {
      Components::PlayerController pc;
      pc.walkMultiplier =
          components["playerController"].value("walkMultiplier", 1.0f);
      pc.sprintMultiplier =
          components["playerController"].value("sprintMultiplier", 2.0f);
      world.addComponent(entity, pc);
    }

    // TorchComponent
    if (components.contains("torch")) {
      Components::TorchComponent torch;
      auto &tc = components["torch"];
      torch.active = tc.value("active", true);
      if (tc.contains("offset")) {
        torch.offset.x = tc["offset"].value("x", 35.0f);
        torch.offset.y = tc["offset"].value("y", -20.0f);
      }
      if (tc.contains("color")) {
        torch.color.r = tc["color"].value("r", 1.0f);
        torch.color.g = tc["color"].value("g", 0.9f);
        torch.color.b = tc["color"].value("b", 0.6f);
      }
      torch.radius = tc.value("radius", 300.0f);
      torch.angle = tc.value("angle", 60.0f);
      torch.intensity = tc.value("intensity", 1.0f);
      world.addComponent(entity, torch);
    }

    // SpriteAnimator
    if (components.contains("spriteAnimator")) {
      auto &sac = components["spriteAnimator"];
      std::string filename = sac.value("filename", "");
      if (!filename.empty()) {
        Components::SpriteAnimator anim;
        // Basic path assumption: assets/animations/ + filename
        anim.loadFromPath("assets/animations/" + filename);
        anim.filename = filename;
        world.addComponent(entity, anim);
      }
    }

    // StateMachine
    if (components.contains("stateMachine")) {
      Components::StateMachine sm;
      sm.currentState =
          components["stateMachine"].value("currentState", "idle");
      world.addComponent(entity, sm);
    }

    // Track the entity
    m_testEntities.push_back(entity);
    std::cout << "[Prefab] Spawned: " << prefabName << " at (" << position.x
              << ", " << position.y << ")\n";
  }
}

void SandboxState::deletePrefab(const std::string &prefabName) {
  std::string filepath = "assets/prefabs/" + prefabName + ".json";
  namespace fs = std::filesystem;

  if (fs::exists(filepath)) {
    fs::remove(filepath);
    std::cout << "[Prefab] Deleted: " << filepath << "\n";
    loadAvailablePrefabs();
  }
}

void SandboxState::renderPrefabEditor() {
  if (!m_showPrefabEditor)
    return;

  ImGui::Begin("Prefab Editor", &m_showPrefabEditor);

  ImGui::Text("Prefab Management");
  ImGui::Separator();

  // === SAVE AS PREFAB ===
  if (ImGui::CollapsingHeader("Save Selected Entity as Prefab",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (m_selectedEntityIdx >= 0 &&
        m_selectedEntityIdx < (int)m_testEntities.size()) {
      auto selectedEntity = m_testEntities[m_selectedEntityIdx];

      ImGui::Text("Selected Entity: %u", selectedEntity);

      // Prefab name input
      static char prefabNameBuffer[128] = "";
      ImGui::InputText("Prefab Name", prefabNameBuffer, 128);

      // Category input
      static char categoryBuffer[64] = "Custom";
      ImGui::InputText("Category", categoryBuffer, 64);

      // Save button
      if (ImGui::Button("Save as Prefab")) {
        if (strlen(prefabNameBuffer) > 0) {
          savePrefabFromEntity(selectedEntity, prefabNameBuffer,
                               categoryBuffer);
          // Clear the buffer after saving
          prefabNameBuffer[0] = '\0';
        }
      }

      ImGui::SameLine();
      ImGui::TextDisabled("(?)");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Save the selected entity as a reusable prefab");
      }
    } else {
      ImGui::TextDisabled(
          "No entity selected. Select one from Entity Management.");
    }
  }

  // === PREFAB LIBRARY ===
  if (ImGui::CollapsingHeader("Prefab Library",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button("Refresh Prefabs")) {
      loadAvailablePrefabs();
    }

    ImGui::SameLine();
    ImGui::Text("Available Prefabs: %zu", m_availablePrefabs.size());

    ImGui::Separator();

    // Display prefabs as a list
    ImGui::BeginChild("PrefabList", ImVec2(0, 200), true);

    for (int i = 0; i < (int)m_availablePrefabs.size(); ++i) {
      bool isSelected = (m_selectedPrefabIndex == i);
      if (ImGui::Selectable(m_availablePrefabs[i].c_str(), isSelected)) {
        m_selectedPrefabIndex = i;
      }

      // Drag and drop support
      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("PREFAB_NAME", m_availablePrefabs[i].c_str(),
                                  m_availablePrefabs[i].length() + 1);
        ImGui::Text("Drag: %s", m_availablePrefabs[i].c_str());
        ImGui::EndDragDropSource();
      }

      // Right-click context menu
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Spawn at Origin")) {
          spawnPrefab(m_availablePrefabs[i], vec2{0.0f, 0.0f});
        }
        if (ImGui::MenuItem("Delete Prefab")) {
          deletePrefab(m_availablePrefabs[i]);
        }
        ImGui::EndPopup();
      }
    }

    ImGui::EndChild();

    // Spawn selected prefab
    if (m_selectedPrefabIndex >= 0 &&
        m_selectedPrefabIndex < (int)m_availablePrefabs.size()) {
      ImGui::Separator();
      ImGui::Text("Spawn Position:");

      static float spawnX = 0.0f;
      static float spawnY = 0.0f;
      ImGui::DragFloat("X", &spawnX, 1.0f);
      ImGui::DragFloat("Y", &spawnY, 1.0f);

      if (ImGui::Button("Spawn Selected Prefab")) {
        spawnPrefab(m_availablePrefabs[m_selectedPrefabIndex],
                    vec2{spawnX, spawnY});
      }
    }
  }

  ImGui::End();
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
                    // Since rotation preserves distance, tmin is valid in
                    // World Space too.
                    if (tmax >= std::max(0.0f, tmin) && tmin < closestDist) {
                      closestDist = tmin;
                      closestPoint = start + dir * tmin;
                    }
                  } else if (obsCol.shape ==
                             Components::Collider::Shape::Circle) {
                    // --- Circle Logic ---
                    // Circle is rotation invariant, but we must use the
                    // correct Center (offset logic above) Apply Transform
                    // Scale to Radius
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

  if (!m_dockLayoutInitialized) {
    m_dockLayoutInitialized = true;
    ImGui::DockBuilderRemoveNode(dockspace_id); // Clear any existing layout
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_left_id = 0;
    ImGuiID dock_right_id = 0;
    ImGuiID dock_center_col_id = 0;
    ImGuiID dock_center_id = 0;
    ImGuiID dock_toolbar_id = 0;
    ImGuiID dock_actual_right_id = 0;
    ImGuiID dock_right_top_id = 0;
    ImGuiID dock_right_bottom_id = 0;

    // 1. Split Left (20%) for Entity/Systems
    ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.225f,
                                &dock_left_id, &dock_right_id);

    // 2. Split Right (25% of remainder) for Components/Stats
    ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Right, 0.25f,
                                &dock_actual_right_id, &dock_center_col_id);

    // 2.5 Split the Right Column (Components) into Top and Bottom (for
    // Assets)
    ImGui::DockBuilderSplitNode(dock_actual_right_id, ImGuiDir_Down, 0.50f,
                                &dock_right_bottom_id, &dock_right_top_id);

    // 3. Split Top of Center Column for Toolbar (small strip)
    ImGui::DockBuilderSplitNode(dock_center_col_id, ImGuiDir_Up, 0.12f,
                                &dock_toolbar_id, &dock_center_id);

    // Dock Windows
    // Left
    ImGui::DockBuilderDockWindow("Entity Management", dock_left_id);
    ImGui::DockBuilderDockWindow("Systems", dock_left_id);

    // Right Top (Components, Stats)
    ImGui::DockBuilderDockWindow("Components", dock_right_top_id);
    ImGui::DockBuilderDockWindow("ECS Stats", dock_right_top_id);

    // Right Bottom (Assets)
    ImGui::DockBuilderDockWindow("Asset Browser", dock_right_bottom_id);

    // Center Column
    ImGui::DockBuilderDockWindow("Toolbar", dock_toolbar_id);
    ImGui::DockBuilderDockWindow("Scene", dock_center_id);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  // Render all ImGui windows
  renderSystemsWindow();
  renderEntityWindow();
  renderComponentWindow();
  renderStatsWindow();
  renderSceneWindow();
  renderToolbar();
  renderAssetBrowser();
  renderPrefabEditor();
  renderTilePalette();

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

  // Dropdown for loading
  if (!m_availableTilemaps.empty()) {
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("##tilemapCombo",
                          m_availableTilemaps[m_selectedTilemapIdx].c_str())) {
      for (int i = 0; i < (int)m_availableTilemaps.size(); i++) {
        bool isSelected = (m_selectedTilemapIdx == i);
        if (ImGui::Selectable(m_availableTilemaps[i].c_str(), isSelected)) {
          m_selectedTilemapIdx = i;
          // Copy to input buffer
          strncpy_s(currentLevelName, m_availableTilemaps[i].c_str(), 256);
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
  }

  if (ImGui::Button("R")) {
    refreshTilemapList();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Refresh file list");

  ImGui::SameLine();

  ImGui::InputText("Filename", currentLevelName, 256);

  if (ImGui::Button("Save Level")) {
    // 1. Save to current runtime directory (Build folder)
    std::string path = "assets/scenes/" + std::string(currentLevelName);
    SceneSerializer::Serialize(world, path);

    // 2. Save to Project Source directory (Persistence)
    // Attempt to save to source folder assuming standard build layout
    // (bin/Debug -> root)
    // 2. Save to Project Source directory (Persistence)
    // We try multiple relative paths because the CWD can vary (e.g. 'build'
    // vs 'build/bin/Debug')
    std::string candidatePaths[] = {
        "../assets/scenes/" +
            std::string(currentLevelName), // If running from 'build'
        "../../../assets/scenes/" +
            std::string(currentLevelName) // If running from 'build/bin/Debug'
    };

    bool foundSource = false;
    for (const auto &relPath : candidatePaths) {
      std::filesystem::path srcDir =
          std::filesystem::path(relPath).parent_path();
      if (std::filesystem::exists(srcDir)) {
        SceneSerializer::Serialize(world, relPath);
        foundSource = true;
        break;
      }
    }

    if (!foundSource) {
      std::cout << "[Sandbox] Error: Could not find source 'assets/scenes' via "
                   "relative paths.\n";
      std::cout << "  CWD: " << std::filesystem::current_path() << "\n";
    }

    refreshTilemapList(); // Refresh list after saving
    ImGui::OpenPopup("Saved");
  }

  if (ImGui::BeginPopup("Saved")) {
    ImGui::Text("Level Saved Successfully!");
    ImGui::TextDisabled("(To Build & Source)");
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

  // AI Testing Buttons
  ImGui::Separator();
  if (ImGui::Button("Spawn Player (Blue)")) {
    spawnPlayer();
  }
  ImGui::SameLine();
  if (ImGui::Button("Spawn Enemy (Red)")) {
    spawnEnemy();
  }

  // ImGui::SameLine();
  if (ImGui::Button("Clear All")) {
    m_scene.Clear(); // Use Scene Clear
    // Also clear test entities vector if tracked?
    // Actually Scene::Clear might not clear m_testEntities in SandboxState.
    // We generated m_testEntities locally.
    // But deserialization probably adds to world directly and m_testEntities
    // doesn't track them. This local tracking is legacy. Let's iterate world
    // to destroy? Use a quick loop: This is just a debug tool. For now,
    // assume Scene::Clear works or just skip. But since I updated Scene.cpp
    // to have empty Clear(), I should implement it properly there or loop
    // here. Let's rely on world.destroyEntity loop I wrote before, but
    // adapted. Or better: Scene should implement `Clear` properly. I'll leave
    // the button doing nothing useful for now or rely on reload.
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

  // ParticleEmitter
  if (ImGui::CollapsingHeader("ParticleEmitter")) {
    auto *pe = world.getComponent<Components::ParticleEmitter>(entity);
    if (pe) {
      bool active = pe->active();
      if (ImGui::Checkbox("Active", &active))
        pe->setActive(active);

      // Ensure renderer is set (fix for previously created components)
      pe->setRenderer(&Core::Engine::Get().rendering());

      auto &sys = pe->system();

      ImGui::DragFloat("Rate", &sys.rate, 1.f, 0.f, 1000.f);
      ImGui::DragFloatRange2("Life", &sys.lifeMin, &sys.lifeMax, 0.05f, 0.05f,
                             5.0f);
      ImGui::DragFloatRange2("Speed", &sys.speedMin, &sys.speedMax, 1.f, 0.f,
                             500.f);
      ImGui::DragFloatRange2("Size", &sys.sizeEnd, &sys.sizeStart, 0.05f, 0.f,
                             10.f);
      ImGui::ColorEdit4("Start Color", &sys.colorStart.r);
      ImGui::ColorEdit4("End Color", &sys.colorEnd.r);

      // New controls for direction
      ImGui::DragFloat("Spread (Deg)", &sys.spreadDeg, 1.0f, 0.0f, 180.0f);
      if (ImGui::DragFloat2("Direction", &sys.dir.x, 0.05f, -1.0f, 1.0f)) {
        // Basic normalization to keep direction vector sane
        float len = std::sqrt(sys.dir.x * sys.dir.x + sys.dir.y * sys.dir.y);
        if (len > 0.001f) {
          sys.dir.x /= len;
          sys.dir.y /= len;
        }
      }

      if (ImGui::Button("Burst (50)")) {
        pe->burst(50);
      }

      if (ImGui::Button("Remove ParticleEmitter"))
        world.removeComponent<Components::ParticleEmitter>(entity);
    } else {
      if (ImGui::Button("Add ParticleEmitter")) {
        world.addComponent(entity, Components::ParticleEmitter{1000});
        auto *newPE = world.getComponent<Components::ParticleEmitter>(entity);
        if (newPE) {
          newPE->setRenderer(&Core::Engine::Get().rendering());
        }
      }
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

void SandboxState::spawnPlayer() {
  auto &world = m_scene.GetWorld();
  auto entity = world.createEntity();

  // Transform
  Components::Transform transform;
  transform.position = vec2{0.f, 0.f};
  transform.scale = vec2{1.f, 1.f};
  world.addComponent(entity, transform);

  // Sprite (Blue for Player)
  Components::SpriteRenderer sprite;
  sprite.color = {0.2f, 0.2f, 1.0f, 1.0f}; // Blue
  sprite.size = {50.f, 50.f};
  world.addComponent(entity, sprite);

  // Physics
  Components::Velocity velocity;
  world.addComponent(entity, velocity); // Needed for movement

  Components::Collider collider;
  collider.size = {50.f, 50.f};
  collider.isSolid = true;
  collider.moveable = true;
  world.addComponent(entity, collider);

  // Logic
  world.addComponent(entity, Components::PlayerController{});
  world.addComponent(entity,
                     Components::Battery{100.f, 100.f}); // Added Battery
  world.addComponent(entity, Components::Health{100.f, 100.f});

  // Add Torch
  Components::TorchComponent torch;
  torch.active = false; // Start off
  torch.offset = {35.f, -20.f};
  torch.radius = 400.f;
  torch.intensity = 1.0f;
  torch.color = {1.0f, 0.9f, 0.7f}; // Warm light
  world.addComponent(entity, torch);

  // Add Animator
  Components::SpriteAnimator anim;
  anim.loadFromPath("assets/animations/player.json");
  anim.filename = "player.json";
  anim.play("idle");
  world.addComponent(entity, anim);

  m_testEntities.push_back(entity);
  std::cout << "Spawned Player Entity: " << entity.id << "\n";
}

void SandboxState::spawnEnemy() {
  auto &world = m_scene.GetWorld();
  auto entity = world.createEntity();

  // Transform (Spawn slightly away from origin)
  Components::Transform transform;
  transform.position = vec2{200.f, 200.f};
  transform.scale = vec2{1.f, 1.f};
  world.addComponent(entity, transform);

  // Sprite (Red for Enemy)
  Components::SpriteRenderer sprite;
  sprite.color = {1.0f, 0.2f, 0.2f, 1.0f}; // Red
  sprite.size = {50.f, 50.f};
  world.addComponent(entity, sprite);

  // Physics
  Components::Velocity velocity;
  world.addComponent(entity, velocity);

  Components::Collider collider;
  collider.size = {50.f, 50.f};
  collider.isSolid = true;
  collider.moveable = true;
  world.addComponent(entity, collider);

  // Logic
  Components::EnemyAI ai;
  ai.state = Components::AIState::Patrol;
  ai.detectionRadius = 300.f;
  ai.patrolSpeed = 50.f;
  ai.chaseSpeed = 150.f;
  world.addComponent(entity, ai);

  world.addComponent(entity, Components::Health{50.f, 50.f});

  m_testEntities.push_back(entity);
  std::cout << "Spawned Enemy Entity: " << entity.id << "\n";
}

void SandboxState::cloneTestEntity(ECS::Entity sourceEntity) {
  auto &world = m_scene.GetWorld();

  // Create new entity
  auto newEntity = world.createEntity();

  // Helper lambda for copying components cleanly without C++20 template
  // lambdas
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

void Runtime::SandboxState::renderToolbar() {
  ImGui::Begin("Toolbar");

  if (m_isPaused) {
    if (ImGui::Button("Play", ImVec2(50, 0))) {
      m_isPaused = false;
    }
  } else {
    if (ImGui::Button("Pause", ImVec2(50, 0))) {
      m_isPaused = true;
    }
  }

  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();

  // TILEMAP MODE TOGGLE
  if (ImGui::Checkbox("Tilemap Mode", &m_tileMapMode)) {
    if (m_tileMapMode) {
      m_showTilePalette = true;
      // Deselect entity when entering tilemap mode
      m_selectedEntityIdx = -1;
      m_gizmoActiveAxis = -1;
    }
  }

  ImGui::SameLine();

  // Paint mode buttons (only show in tilemap mode)
  if (m_tileMapMode) {
    if (ImGui::RadioButton("Paint", m_paintMode == PaintMode::Paint)) {
      m_paintMode = PaintMode::Paint;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Erase", m_paintMode == PaintMode::Erase)) {
      m_paintMode = PaintMode::Erase;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Select", m_paintMode == PaintMode::Select)) {
      m_paintMode = PaintMode::Select;
    }
    ImGui::SameLine();
  }

  ImGui::Separator();
  ImGui::SameLine();

  ImGui::Checkbox("Show Grid", &m_showGrid);
  ImGui::SameLine();
  ImGui::DragFloat("Grid Size", &m_gridSize, 1.0f, 10.0f, 200.0f);
  ImGui::Separator();
  ImGui::Checkbox("Prefab Editor", &m_showPrefabEditor);
  ImGui::SameLine();
  ImGui::Checkbox("Camera Follow", &m_cameraFollowPlayer);

  // TILEMAP CONTROLS
  if (m_tileMapMode) {
    ImGui::Separator();
    ImGui::Checkbox("Tile Palette", &m_showTilePalette);

    if (ImGui::Button("Clear Tilemap")) {
      clearTilemap();
    }
  }

  ImGui::End();
}

void Runtime::SandboxState::renderTilePalette() {
  if (!m_showTilePalette || !m_tileMapMode)
    return;

  ImGui::Begin("Tile Palette", &m_showTilePalette);

  ImGui::Text("Select a tile to paint:");
  ImGui::Separator();

  // Display prefabs as selectable tiles
  ImGui::BeginChild("TilePaletteList", ImVec2(0, 0), true);

  for (int i = 0; i < (int)m_availablePrefabs.size(); ++i) {
    bool isSelected = (m_selectedTilePrefab == m_availablePrefabs[i]);

    if (ImGui::Selectable(m_availablePrefabs[i].c_str(), isSelected)) {
      m_selectedTilePrefab = m_availablePrefabs[i];
      m_paintMode = PaintMode::Paint; // Auto-switch to paint mode
    }

    // Optional: Show preview if you have texture info in prefab
    // You could load the prefab JSON and display a small preview icon
  }

  ImGui::EndChild();

  if (!m_selectedTilePrefab.empty()) {
    ImGui::Separator();
    ImGui::Text("Selected: %s", m_selectedTilePrefab.c_str());
  } else {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No tile selected");
  }

  ImGui::End();
}

void Runtime::SandboxState::handleTileMapInput(const vec2 &viewportMin) {
  if (!m_tileMapMode || !m_viewportHovered)
    return;

  ImVec2 mousePos = ImGui::GetMousePos();
  vec2 localMouse = {mousePos.x - viewportMin.x, mousePos.y - viewportMin.y};

  Graphics::Camera2D &cam = Core::Engine::Get().camera();
  float zoom = cam.zoom();
  vec2 camPos = vec2(cam.position().x, cam.position().y);

  vec2 centeredMouse = {localMouse.x - m_viewportSize.x * 0.5f,
                        -(localMouse.y - m_viewportSize.y * 0.5f)};

  vec2 worldPos = camPos + centeredMouse * (1.0f / zoom);

  // Always snap to grid in tilemap mode
  worldPos = snapToGrid(worldPos, m_gridSize);

  // Paint Mode
  if (m_paintMode == PaintMode::Paint && !m_selectedTilePrefab.empty()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

      // Check if tile already exists at this position
      bool tileExists = false;
      for (const auto &tile : m_tilemap) {
        float dist = magnitude(tile.position - worldPos);
        if (dist < m_gridSize * 0.1f) { // Close enough
          tileExists = true;
          break;
        }
      }

      if (!tileExists) {
        // Spawn the tile
        spawnPrefab(m_selectedTilePrefab, worldPos);

        // Record in tilemap
        m_tilemap.push_back({m_selectedTilePrefab, worldPos});
      }
    }
  }
  // Erase Mode
  else if (m_paintMode == PaintMode::Erase) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

      auto &world = m_scene.GetWorld();

      // Find and remove entities at this position
      for (int i = (int)m_testEntities.size() - 1; i >= 0; --i) {
        auto entity = m_testEntities[i];
        auto *trans = world.getComponent<Components::Transform>(entity);

        if (trans) {
          float dist =
              magnitude(vec2(trans->position.x, trans->position.y) - worldPos);
          if (dist < m_gridSize * 0.5f) {
            // Remove from tilemap data
            m_tilemap.erase(std::remove_if(m_tilemap.begin(), m_tilemap.end(),
                                           [&](const TileData &tile) {
                                             return magnitude(tile.position -
                                                              worldPos) <
                                                    m_gridSize * 0.1f;
                                           }),
                            m_tilemap.end());

            // Destroy entity
            world.destroyEntity(entity);
            m_testEntities.erase(m_testEntities.begin() + i);
            break; // Only erase one per click
          }
        }
      }
    }
  }
  // Select Mode
  else if (m_paintMode == PaintMode::Select) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      auto &world = m_scene.GetWorld();

      // Find entity at position
      for (int i = 0; i < (int)m_testEntities.size(); ++i) {
        auto entity = m_testEntities[i];
        auto *trans = world.getComponent<Components::Transform>(entity);

        if (trans) {
          float dist =
              magnitude(vec2(trans->position.x, trans->position.y) - worldPos);
          if (dist < m_gridSize * 0.5f) {
            m_selectedEntityIdx = i;
            m_tileMapMode = false; // Exit tilemap mode to edit
            return;
          }
        }
      }
    }
  }
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
    m_viewportSize = vec2(viewportPanelSize.x, viewportPanelSize.y);
    m_framebuffer.resize((int)m_viewportSize.x, (int)m_viewportSize.y);
  }

  // Always enforce camera viewport to match the panel,
  // overriding any external WindowResize events that might have reset it to
  // window size.
  if (m_viewportSize.x > 0 && m_viewportSize.y > 0) {
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
  // Determine viewport bounds

  vec2 viewportMin = {windowPos.x + contentMin.x, windowPos.y + contentMin.y};

  // Drag and Drop Target
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
      const char *textureName = (const char *)payload->Data;
      // Calculate drop position
      ImVec2 mousePos = ImGui::GetMousePos();
      vec2 localMouse = {mousePos.x - viewportMin.x,
                         mousePos.y - viewportMin.y};

      Graphics::Camera2D &cam = Core::Engine::Get().camera();
      float zoom = cam.zoom();
      vec2 camPos = vec2(cam.position().x, cam.position().y);

      vec2 centeredMouse = {localMouse.x - m_viewportSize.x * 0.5f,
                            -(localMouse.y - m_viewportSize.y * 0.5f)};

      vec2 worldPos = camPos + centeredMouse * (1.0f / zoom);

      // Snap to grid if grid is enabled
      if (m_showGrid) {
        worldPos = snapToGrid(worldPos, m_gridSize);
      }

      // Spawn Entity
      spawnTestEntity(vec2(worldPos.x, worldPos.y));
      // Set texture of last spawned entity
      if (!m_testEntities.empty()) {
        auto &world = m_scene.GetWorld();
        auto entity = m_testEntities.back();
        auto *sprite = world.getComponent<Components::SpriteRenderer>(entity);
        if (!sprite) {
          world.addComponent(entity, Components::SpriteRenderer{});
          sprite = world.getComponent<Components::SpriteRenderer>(entity);
        }
        if (sprite) {
          sprite->textureId = textureName;
          sprite->size = vec2(50, 50); // Default size
        }
      }
    }

    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("PREFAB_NAME")) {
      const char *prefabName = (const char *)payload->Data;

      // Calculate drop position
      ImVec2 mousePos = ImGui::GetMousePos();
      vec2 localMouse = {mousePos.x - viewportMin.x,
                         mousePos.y - viewportMin.y};

      Graphics::Camera2D &cam = Core::Engine::Get().camera();
      float zoom = cam.zoom();
      vec2 camPos = vec2(cam.position().x, cam.position().y);

      vec2 centeredMouse = {localMouse.x - m_viewportSize.x * 0.5f,
                            -(localMouse.y - m_viewportSize.y * 0.5f)};

      vec2 worldPos = camPos + centeredMouse * (1.0f / zoom);

      // Snap to grid if enabled
      if (m_showGrid) {
        worldPos = snapToGrid(worldPos, m_gridSize);
      }

      // Spawn the prefab at drop position
      spawnPrefab(prefabName, worldPos);
    }
    ImGui::EndDragDropTarget();
  }

  if (m_tileMapMode) {
    handleTileMapInput(viewportMin);

    // Don't do gizmo/selection in tilemap mode
  }
  // Gizmos and Picking
  renderGizmos({windowPos.x, windowPos.y},
               {viewportPanelSize.x, viewportPanelSize.y}, viewportMin);

  // Handle Object Picking (Selection)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_viewportHovered &&
      m_gizmoActiveAxis == -1) {
    // Create a dummy gizmo check context or just assume no gizmo if axis is
    // -1 Convert mouse to world space
    ImVec2 mousePos = ImGui::GetMousePos();
    vec2 localMouse = {mousePos.x - viewportMin.x, mousePos.y - viewportMin.y};

    Graphics::Camera2D &cam = Core::Engine::Get().camera();
    float zoom = cam.zoom();
    vec2 camPos = vec2(cam.position().x, cam.position().y);

    // Adjust for center origin
    vec2 centeredMouse = {
        localMouse.x - m_viewportSize.x * 0.5f,
        -(localMouse.y -
          m_viewportSize.y * 0.5f) // Flip Y because World Y is up, Screen Y
                                   // is down (pixel space)
    };

    vec2 worldPos = camPos + centeredMouse * (1.0f / zoom);

    // Iterate m_testEntities
    auto &world = m_scene.GetWorld();
    float bestDist = FLT_MAX;
    int bestIdx = -1;

    for (int i = 0; i < (int)m_testEntities.size(); ++i) {
      auto entity = m_testEntities[i];
      auto *trans = world.getComponent<Components::Transform>(entity);
      if (trans) {
        // Simple distance check for selection (assume rad 50)
        float dist =
            magnitude(vec2(trans->position.x, trans->position.y) - worldPos);
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

void SandboxState::renderGizmos(const vec2 &windowPos, const vec2 &windowSize,
                                const vec2 &viewportMin) {
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
  vec2 camPos = vec2(cam.position().x, cam.position().y);
  float zoom = cam.zoom();

  vec2 worldDelta = vec2(trans->position.x, trans->position.y) - camPos;
  vec2 screenCenter = viewportMin + m_viewportSize * 0.5f;
  // Note: World Y is up, Screen Y is down.
  vec2 objectScreenPos =
      screenCenter + vec2(worldDelta.x * zoom, -worldDelta.y * zoom);

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
  vec2 xAxisScreen = vec2(c, -s);
  vec2 yAxisScreen = vec2(-s, -c);

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
      vec2 dir = normalize(xAxisScreen);
      vec2 perp = {-dir.y, dir.x}; // 90 deg screen space
      ImVec2 p1 = {xEnd.x, xEnd.y};
      ImVec2 p2 = {xEnd.x - dir.x * 10 + perp.x * 5,
                   xEnd.y - dir.y * 10 + perp.y * 5};
      ImVec2 p3 = {xEnd.x - dir.x * 10 - perp.x * 5,
                   xEnd.y - dir.y * 10 - perp.y * 5};
      drawList->AddTriangleFilled(p1, p2, p3, hoverX ? colHover : colX);
    }
    // Y Head
    {
      vec2 dir = normalize(yAxisScreen);
      vec2 perp = {-dir.y, dir.x};
      ImVec2 p1 = {yEnd.x, yEnd.y};
      ImVec2 p2 = {yEnd.x - dir.x * 10 + perp.x * 5,
                   yEnd.y - dir.y * 10 + perp.y * 5};
      ImVec2 p3 = {yEnd.x - dir.x * 10 - perp.x * 5,
                   yEnd.y - dir.y * 10 - perp.y * 5};
      drawList->AddTriangleFilled(p1, p2, p3, hoverY ? colHover : colY);
    }

    if (mouseClicked && (hoverX || hoverY) && !m_isDragging) {
      m_gizmoActiveAxis = hoverX ? 0 : 1;
      m_gizmoOriginalPos = vec2(trans->position.x, trans->position.y);
      m_gizmoClickOffset = vec2(mousePos.x, mousePos.y);
    }

    if (mouseDown && m_gizmoActiveAxis != -1) {
      // Drag logic needs to project mouse movement onto the axis in Screen
      // Space Then apply that magnitude to World Space axis
      float dx = (mousePos.x - m_gizmoClickOffset.x);
      float dy = (mousePos.y - m_gizmoClickOffset.y);

      // Dot product with screen axis to get magnitude
      vec2 screenMove(dx, dy);
      vec2 axisDir = (m_gizmoActiveAxis == 0) ? xAxisScreen : yAxisScreen;
      axisDir = normalize(axisDir);

      float proj = dot(screenMove, axisDir);

      // proj is pixels moved along the screen axis
      // Convert to world units: divide by zoom
      float worldMove = proj / zoom;

      // World axis matches screen axis conceptually but Y component is
      // flipped for storage? No, trans->rotation tells us the world axis
      // direction. X-Axis world vector is (c, s). So we move worldMove * (c,
      // s)

      vec2 worldDir = (m_gizmoActiveAxis == 0) ? vec2(c, s) : vec2(-s, c);

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
      m_gizmoOriginalScale = vec2(trans->scale.x, trans->scale.y);
      m_gizmoClickOffset = vec2(mousePos.x, mousePos.y);
    }

    if (mouseDown && m_gizmoActiveAxis != -1) {
      float dx = (mousePos.x - m_gizmoClickOffset.x);
      float dy = (mousePos.y - m_gizmoClickOffset.y);

      vec2 screenMove(dx, dy);
      vec2 axisDir = (m_gizmoActiveAxis == 0) ? xAxisScreen : yAxisScreen;
      axisDir = normalize(axisDir);
      float proj = dot(screenMove, axisDir);

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

// void Runtime::SandboxState::renderToolbar() {
//   ImGui::Begin("Toolbar");
//
//   if (m_isPaused) {
//     if (ImGui::Button("Play", ImVec2(50, 0))) {
//       m_isPaused = false;
//     }
//   } else {
//     if (ImGui::Button("Pause", ImVec2(50, 0))) {
//       m_isPaused = true;
//     }
//   }
//
//   ImGui::SameLine();
//   ImGui::Separator();
//   ImGui::SameLine();
//
//   ImGui::Checkbox("Show Grid", &m_showGrid);
//   ImGui::SameLine();
//   ImGui::DragFloat("Grid Size", &m_gridSize, 1.0f, 10.0f, 200.0f);
//   ImGui::Separator();
//   ImGui::Checkbox("Prefab Editor", &m_showPrefabEditor);
//
//   ImGui::End();
// }

void Runtime::SandboxState::renderGrid() {
  if (!m_showGrid)
    return;

  Graphics::Camera2D &cam = Core::Engine::Get().camera();
  Graphics::RenderingManager &renderer = Core::Engine::Get().rendering();

  // Get visible world bounds from camera
  // This is an approximation. Ideally we project screen corners to world.
  // Assuming camera is at center.
  vec2 camPos = vec2(cam.position().x, cam.position().y);
  ImVec2 viewSize = ImVec2(m_viewportSize.x, m_viewportSize.y);
  float zoom = cam.zoom();

  // Calculate world width/height visible
  float worldW = viewSize.x / zoom;
  float worldH = viewSize.y / zoom;

  float left = camPos.x - worldW * 0.6f; // Add some padding
  float right = camPos.x + worldW * 0.6f;
  float top = camPos.y + worldH * 0.6f;
  float bottom = camPos.y - worldH * 0.6f;

  // Snap to grid
  float startX = std::floor(left / m_gridSize) * m_gridSize;
  float endX = std::ceil(right / m_gridSize) * m_gridSize;
  float startY = std::floor(bottom / m_gridSize) * m_gridSize;
  float endY = std::ceil(top / m_gridSize) * m_gridSize;

  // Draw Vertical Lines
  for (float x = startX; x <= endX; x += m_gridSize) {
    renderer.drawLine(x, bottom, x, top, 0.3f, 0.3f, 0.3f);
  }

  // Draw Horizontal Lines
  for (float y = startY; y <= endY; y += m_gridSize) {
    renderer.drawLine(left, y, right, y, 0.3f, 0.3f, 0.3f);
  }

  // Draw Origin
  renderer.drawLine(0, bottom, 0, top, 0.0f, 1.0f, 0.0f); // Y-Axis Green
  renderer.drawLine(left, 0, right, 0, 1.0f, 0.0f, 0.0f); // X-Axis Red
}

void Runtime::SandboxState::renderAssetBrowser() {
  ImGui::Begin("Asset Browser", &m_showAssetBrowser);

  if (ImGui::BeginTabBar("AssetTabs")) {
    if (ImGui::BeginTabItem("Textures")) {
      auto names = Assets::TextureManager::GetLoadedTextureNames();
      float thumbnailSize = 64.f;
      float padding = 8.f;
      float cellSize = thumbnailSize + padding;
      float panelWidth = ImGui::GetContentRegionAvail().x;
      int columnCount = (int)(panelWidth / cellSize);
      if (columnCount < 1)
        columnCount = 1;

      if (ImGui::BeginTable("TextureTable", columnCount)) {
        for (const auto &name : names) {
          ImGui::TableNextColumn();
          ImGui::PushID(name.c_str());

          auto tex = Assets::TextureManager::Get(name);
          if (tex && tex->id() != 0) {
            // Image Button
            if (ImGui::ImageButton("##btn", (ImTextureID)(intptr_t)tex->id(),
                                   ImVec2(thumbnailSize, thumbnailSize))) {
              m_selectedAsset = name;
            }

            // Drag and Drop Source
            if (ImGui::BeginDragDropSource()) {
              ImGui::SetDragDropPayload("TEXTURE_ASSET", name.c_str(),
                                        name.size() + 1);
              ImGui::Text("%s", name.c_str());
              ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(32, 32));
              ImGui::EndDragDropSource();
            }
          }

          ImGui::TextWrapped("%s", name.c_str());
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

void Runtime::SandboxState::saveTilemap(const std::string &filepath) {
  nlohmann::json tilemapJson;
  tilemapJson["gridSize"] = m_gridSize;
  tilemapJson["tiles"] = nlohmann::json::array();

  for (const auto &tile : m_tilemap) {
    nlohmann::json tileJson;
    tileJson["prefab"] = tile.prefabName;
    tileJson["position"] = {{"x", tile.position.x}, {"y", tile.position.y}};
    tilemapJson["tiles"].push_back(tileJson);
  }

  // Create directory if it doesn't exist
  namespace fs = std::filesystem;
  fs::path filePath(filepath);
  if (filePath.has_parent_path()) {
    fs::create_directories(filePath.parent_path());
  }

  std::ofstream file(filepath);
  if (file.is_open()) {
    file << tilemapJson.dump(4);
    file.close();
    std::cout << "[Tilemap] Saved: " << filepath << "\n";
  } else {
    std::cerr << "[Tilemap] Failed to save: " << filepath << "\n";
  }
}

void Runtime::SandboxState::loadTilemap(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "[Tilemap] Failed to load: " << filepath << "\n";
    return;
  }

  nlohmann::json tilemapJson;
  file >> tilemapJson;
  file.close();

  // Clear existing tilemap
  clearTilemap();

  // Load grid size
  if (tilemapJson.contains("gridSize")) {
    m_gridSize = tilemapJson["gridSize"].get<float>();
  }

  // Load tiles
  if (tilemapJson.contains("tiles")) {
    for (const auto &tileJson : tilemapJson["tiles"]) {
      std::string prefabName = tileJson.value("prefab", "");
      vec2 position;
      if (tileJson.contains("position")) {
        position.x = tileJson["position"].value("x", 0.0f);
        position.y = tileJson["position"].value("y", 0.0f);
      }

      // Spawn the tile
      spawnPrefab(prefabName, position);

      // Record in tilemap
      m_tilemap.push_back({prefabName, position});
    }
  }

  std::cout << "[Tilemap] Loaded: " << filepath << " (" << m_tilemap.size()
            << " tiles)\n";
}

void Runtime::SandboxState::clearTilemap() {
  auto &world = m_scene.GetWorld();

  // Destroy all tracked entities
  for (auto entity : m_testEntities) {
    world.destroyEntity(entity);
  }
  m_testEntities.clear();

  // Reset selection
  m_selectedEntityIdx = -1;
  m_gizmoActiveAxis = -1;

  m_tilemap.clear();
  std::cout << "[Tilemap] Cleared all entities and tile data\n";
}
