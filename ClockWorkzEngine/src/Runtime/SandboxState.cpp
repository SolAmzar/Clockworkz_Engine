/******************************************************************************/
/*!
\file           SandboxState.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon][Yusuf][Syafiq]
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
#include <algorithm>
#include <cctype>
#include <glm/gtc/matrix_transform.hpp>

#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/ActivationZone.hpp"
#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/TextRenderer.hpp"

#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/Portal.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
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

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

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

  // Initialize texture file path mapping for hot-reload
  initTextureFilePaths();

  // Initialize layer visibility
  m_layerVisible.resize(m_numLayers, true);

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
  // Check pending level load (from Editor UI or Scene Logic/Portals)
  std::string levelToLoad = m_pendingLevel;
  if (levelToLoad.empty()) {
    levelToLoad = m_scene.GetPendingLevel();
    if (!levelToLoad.empty()) {
      m_scene.ClearPendingLevel(); // Prevent Scene from loading it again
    }
  }

  if (!levelToLoad.empty()) {
    loadLevel(levelToLoad);
    m_pendingLevel.clear();
    return;
  }

  // Asset hot-reload polling
  m_hotReloadTimer += static_cast<float>(dt);
  if (m_hotReloadTimer >= HOT_RELOAD_INTERVAL) {
    m_hotReloadTimer = 0.f;
    checkAssetHotReload();
  }

  // Handle OS file drops (textures/audio dragged onto the window)
  {
    auto &window = Core::Engine::Get().window();
    auto dropped = window.consumeDroppedFiles();
    for (const auto &path : dropped) {
      namespace fs = std::filesystem;
      fs::path filePath(path);
      std::string ext = filePath.extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return (char)std::tolower(c); });

      auto makeUniqueName = [](const std::string &base,
                               auto isLoadedFn) -> std::string {
        if (!isLoadedFn(base))
          return base;
        for (int i = 1; i < 10000; ++i) {
          std::string candidate = base + "_" + std::to_string(i);
          if (!isLoadedFn(candidate))
            return candidate;
        }
        return base + "_copy";
      };

      const bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                            ext == ".bmp" || ext == ".tga");
      const bool isAudio = (ext == ".wav" || ext == ".mp3" || ext == ".ogg" ||
                            ext == ".flac" || ext == ".aiff" || ext == ".aif");

      if (isImage) {
        std::string baseName = filePath.stem().string();
        std::string name = makeUniqueName(baseName, [](const std::string &n) {
          return Assets::TextureManager::IsLoaded(n);
        });

        auto loaded = Assets::TextureManager::LoadTexture(name, path);
        if (loaded) {
          m_textureFilePaths[name] = path;
          if (fs::exists(path)) {
            m_assetFileTimestamps[name] = fs::last_write_time(path);
          }
          std::cout << "[Sandbox] Loaded dropped texture: " << name << " from "
                    << path << std::endl;
        }
      } else if (isAudio) {
        std::string baseName = filePath.stem().string();
        std::string name = makeUniqueName(baseName, [](const std::string &n) {
          return Assets::SoundManager::IsLoaded(n);
        });

        auto clip = Assets::SoundManager::LoadSound(name, path);
        if (clip) {
          std::cout << "[Sandbox] Loaded dropped sound: " << name << " from "
                    << path << std::endl;
        } else {
          std::cerr << "[Sandbox] Failed to load dropped sound: " << path
                    << std::endl;
        }
      } else {
        std::cout << "[Sandbox] Dropped file ignored (unsupported type): "
                  << path << std::endl;
        m_showUnsupportedDropPopup = true;
        m_unsupportedDropPath = path;
        m_unsupportedDropExt = ext.empty() ? "(no extension)" : ext;
      }
    }
  }

  // Handle ESC to return to menu
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.switchTo(std::make_shared<MenuState>(m_gsm, m_input,
                                               Core::Engine::Get().window(),
                                               Core::Engine::Get().audio()));
  }

  // Undo/Redo shortcuts (Ctrl+Z / Ctrl+Y)
  if (m_input.isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
      m_input.isKeyDown(GLFW_KEY_RIGHT_CONTROL)) {
    if (m_input.wasKeyTriggered(GLFW_KEY_Z)) {
      undo();
    }
    if (m_input.wasKeyTriggered(GLFW_KEY_Y)) {
      redo();
    }
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
    // Always consume scroll value to prevent stale values carrying over
    float scroll = static_cast<float>(Platform::InputManager::GetScrollY());
    if (m_viewportHovered && scroll != 0.0f) {
      float zoomSpeed = 0.1f;
      float currentZoom = camera.zoom();
      float newZoom = currentZoom + (scroll * zoomSpeed * currentZoom);
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

  // Render Activation Zones
  renderActivationZones(renderer);

  // End Scene Pass
  renderer.endScenePass();

  // Render Text Entities (Font Pass in world space)
  if (m_scene.m_textRenderSystemEnabled) {
    auto shader = Assets::ResourceManager<Graphics::Shader>::Get("text");
    if (shader) {
      renderer.beginFontPass(camera.viewProj());

      auto &world = m_scene.GetWorld();
      world.each<Components::Transform, Components::TextRenderer>(
          [&](ECS::Entity, Components::Transform &transform,
              Components::TextRenderer &text) {
            if (!text.visible || text.text.empty())
              return;

            auto font =
                Assets::ResourceManager<Assets::FontManager>::Get(text.fontId);
            if (!font)
              return;

            float x = transform.position.x;
            float y = transform.position.y;
            float scale = text.fontSize * transform.scale.x;
            float boxW = text.boxSize.x * transform.scale.x;
            float boxH = text.boxSize.y * transform.scale.y;

            font->RenderTextInBox(text.text, x, y, boxW, boxH, scale,
                                  text.color, *shader,
                                  static_cast<int>(text.alignment));
          });

      renderer.endFontPass();
    }
  }

  // Draw bounding boxes for text entities (debug overlay)
  renderer.beginDebugPass(camera.viewProj());
  {
    auto &world = m_scene.GetWorld();
    world.each<Components::Transform, Components::TextRenderer>(
        [&](ECS::Entity, Components::Transform &transform,
            Components::TextRenderer &text) {
          if (!text.visible)
            return;

          float x = transform.position.x;
          float y = transform.position.y;
          float boxW = text.boxSize.x * transform.scale.x;
          float boxH = text.boxSize.y * transform.scale.y;

          // drawRect expects (centerX, centerY, w, h)
          // Text renders from (x, y) as top-left, extending right and down
          float centerX = x + boxW * 0.5f;
          float centerY = y - boxH * 0.5f;
          renderer.drawRect(centerX, centerY, boxW, boxH, 1.0f, 1.0f, 0.0f);
        });
  }
  renderer.endDebugPass();

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
    // Make sure we are in Scene Pass (UI Projection) which we are from
    // Battery HUD

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

  // Serialize ActivationZone
  if (auto *zone = world.getComponent<Components::ActivationZone>(entity)) {
    components["activationZone"] = {{"active", zone->active},
                                    {"actionType", (int)zone->actionType},
                                    {"payload", zone->payload},
                                    {"oneShot", zone->oneShot},
                                    {"triggered", zone->triggered}};
  }

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

    // Default Tag from Prefab Name
    world.addComponent(entity, Components::Tag{prefabName});

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

    // ActivationZone
    if (components.contains("activationZone")) {
      Components::ActivationZone zone;
      auto &azc = components["activationZone"];
      zone.active = azc.value("active", true);
      zone.actionType =
          (Components::ActivationZone::ActionType)azc.value("actionType", 0);
      zone.payload = azc.value("payload", "");
      zone.oneShot = azc.value("oneShot", false);
      zone.triggered = azc.value("triggered", false);
      world.addComponent(entity, zone);
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

    // Add ParticleEmitter for death effects
    Components::ParticleEmitter emitter;
    emitter.system().colorStart = {255.f, 255.f, 255.f, 1.f}; // Black particles
    emitter.system().colorEnd = {0.f, 0.f, 0.f, 0.f};         // Fade out
    emitter.system().sizeStart = 5.0f;
    emitter.system().sizeEnd = 1.0f;
    emitter.system().lifeMin = 0.5f;
    emitter.system().lifeMax = 1.0f;
    world.addComponent(entity, emitter);

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

    // Display prefabs as a thumbnail grid
    float thumbnailSize = 64.f;
    float padding = 8.f;
    float cellSize = thumbnailSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1)
      columnCount = 1;

    ImGui::BeginChild("PrefabList", ImVec2(0, 250), true);

    if (ImGui::BeginTable("PrefabGrid", columnCount)) {
      for (int i = 0; i < (int)m_availablePrefabs.size(); ++i) {
        ImGui::TableNextColumn();
        ImGui::PushID(i);

        const auto &prefabName = m_availablePrefabs[i];
        bool isSelected = (m_selectedPrefabIndex == i);

        // Try to read texture from prefab JSON
        std::string textureId;
        {
          std::string prefabPath = "assets/prefabs/" + prefabName + ".json";
          std::ifstream pf(prefabPath);
          if (pf.is_open()) {
            try {
              nlohmann::json pj;
              pf >> pj;
              if (pj.contains("components") &&
                  pj["components"].contains("spriteRenderer") &&
                  pj["components"]["spriteRenderer"].contains("textureId")) {
                textureId = pj["components"]["spriteRenderer"]["textureId"]
                                .get<std::string>();
              }
            } catch (...) {
            }
          }
        }

        // Render thumbnail or fallback
        auto tex = textureId.empty() ? nullptr
                                     : Assets::TextureManager::Get(textureId);
        if (tex && tex->id() != 0) {
          if (ImGui::ImageButton("##prefab_thumb",
                                 (ImTextureID)(intptr_t)tex->id(),
                                 ImVec2(thumbnailSize, thumbnailSize))) {
            m_selectedPrefabIndex = i;
          }
        } else {
          // Colored fallback button
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.4f, 1.f));
          if (ImGui::Button("##prefab_fallback",
                            ImVec2(thumbnailSize, thumbnailSize))) {
            m_selectedPrefabIndex = i;
          }
          ImGui::PopStyleColor();
        }

        // Highlight selected
        if (isSelected) {
          ImVec2 rmin = ImGui::GetItemRectMin();
          ImVec2 rmax = ImGui::GetItemRectMax();
          ImGui::GetWindowDrawList()->AddRect(
              rmin, rmax, IM_COL32(255, 200, 0, 255), 0.f, 0, 2.f);
        }

        // Drag and drop support
        if (ImGui::BeginDragDropSource()) {
          ImGui::SetDragDropPayload("PREFAB_NAME", prefabName.c_str(),
                                    prefabName.length() + 1);
          ImGui::Text("Drag: %s", prefabName.c_str());
          ImGui::EndDragDropSource();
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Spawn at Origin")) {
            spawnPrefab(prefabName, vec2{0.0f, 0.0f});
            m_tilemap.push_back(
                {prefabName, vec2{0.0f, 0.0f}, 0, m_testEntities.back()});
          }
          if (ImGui::MenuItem("Delete Prefab")) {
            deletePrefab(prefabName);
          }
          ImGui::EndPopup();
        }

        // Label below thumbnail
        ImGui::TextWrapped("%s", prefabName.c_str());
        ImGui::PopID();
      }
      ImGui::EndTable();
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
        m_tilemap.push_back({m_availablePrefabs[m_selectedPrefabIndex],
                             vec2{spawnX, spawnY}, 0, m_testEntities.back()});
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

        // Correct Offset Logic: Effective = Offset - Pivot
        glm::vec2 effectiveOffset =
            glm::vec2(c.offset.x - t.pivot.x, c.offset.y - t.pivot.y);
        glm::vec2 offsetRot = RotateVec(effectiveOffset, t.rotation);
        glm::vec2 pos(t.position.x + offsetRot.x, t.position.y + offsetRot.y);

        glm::vec3 color = c.isTrigger ? glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow
                                      : glm::vec3(0.0f, 1.0f, 0.0f); // Green

        if (c.shape == Components::Collider::Shape::Box) {
          // Apply Scale to Size
          glm::vec2 scaledSize =
              glm::vec2(c.size.x * t.scale.x, c.size.y * t.scale.y);
          glm::vec2 half = scaledSize * 0.5f;

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
          // Apply Scale to Radius (Max Scaling)
          float scaledRadius = c.radius * std::max(t.scale.x, t.scale.y);
          renderer.drawCircle(pos.x, pos.y, scaledRadius, color.r, color.g,
                              color.b);
        }
      });
  renderer.endDebugPass();
}

void SandboxState::loadLevel(const std::string &levelName) {
  std::string path = "assets/scenes/" + levelName;
  std::cout << "[Sandbox] Loading level: " << path << "\n";

  m_testEntities.clear();
  m_tilemap.clear();
  m_entityGroups.clear();
  m_selectedGroupIdx = -1;
  m_editingGroup = false;
  m_selectedEntities.clear();
  m_lastClickedEntityIdx = -1;

  if (m_scene.LoadFromFile(path)) {
    // Populate m_testEntities
    auto &world = m_scene.GetWorld();
    world.each<Components::Transform>(
        [&](ECS::Entity entity, Components::Transform &trans) {
          m_testEntities.push_back(entity);
          // Restore tilemap data
          int layer = 0;
          auto *spr = world.getComponent<Components::SpriteRenderer>(entity);
          if (spr)
            layer = spr->sortOrder;
          m_tilemap.push_back({"", trans.position, layer, entity});
        });
    rebuildGroupsFromTilemap();
  } else {
    // std::cerr << "[Sandbox] Failed to load level: " << path << "\n";
    ImGui::OpenPopup(
        "LoadError"); // Logic has to be handled where ImGui is active, this
                      // might not work here if called from Update.
    // For Update call, we can't open popup easily.
    // We'll just log content.
    std::cout << "[Sandbox] Failed to load level: " << path << "\n";
  }
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
    ImGui::DockBuilderDockWindow("Audio Browser", dock_right_bottom_id);

    // Center Column
    ImGui::DockBuilderDockWindow("Toolbar", dock_toolbar_id);
    ImGui::DockBuilderDockWindow("Undo / Redo", dock_toolbar_id);
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
  renderTextureReplaceDialog();
  renderAudioBrowser();
  renderPrefabEditor();
  renderTilePalette();
  renderUndoRedoPanel();
  renderUnsupportedDropPopup();

  // End DockSpace
  Core::Engine::Get().imguiManager().endDockspace();

  (void)timer;
}

void SandboxState::renderUnsupportedDropPopup() {
  if (!m_showUnsupportedDropPopup)
    return;

  ImGui::OpenPopup("Unsupported File Type");
  if (ImGui::BeginPopupModal("Unsupported File Type", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("This file type isn't accepted.");
    ImGui::Separator();
    ImGui::TextWrapped("File: %s", m_unsupportedDropPath.c_str());
    ImGui::Text("Extension: %s", m_unsupportedDropExt.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      m_showUnsupportedDropPopup = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
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
    m_testEntities.clear();
    m_tilemap.clear();
    m_entityGroups.clear();
    m_selectedGroupIdx = -1;
    m_editingGroup = false;
    m_selectedEntities.clear();
    m_lastClickedEntityIdx = -1;

    std::string path = "assets/scenes/" + std::string(currentLevelName);
    if (SceneSerializer::Deserialize(world, path)) {
      // Success - populate m_testEntities with loaded entities
      world.each<Components::Transform>(
          [&](ECS::Entity entity, Components::Transform &trans) {
            m_testEntities.push_back(entity);
            // Use sprite sortOrder as layer if available
            int layer = 0;
            auto *spr = world.getComponent<Components::SpriteRenderer>(entity);
            if (spr)
              layer = spr->sortOrder;
            m_tilemap.push_back({"", trans.position, layer, entity});
          });
      rebuildGroupsFromTilemap();
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
    m_scene.Clear();
    m_testEntities.clear();
    m_entityGroups.clear();
    m_tilemap.clear();
    m_selectedGroupIdx = -1;
    m_editingGroup = false;
    m_selectedEntities.clear();
    m_lastClickedEntityIdx = -1;
  }

  ImGui::Separator();
  ImGui::Text("Entity List:");
  ImGui::Text("%zu entities, %zu groups", m_testEntities.size(),
              m_entityGroups.size());

  // New Group button
  ImGui::SameLine();
  if (ImGui::Button("+ Group")) {
    m_entityGroups.push_back(
        {"Group " + std::to_string(m_entityGroups.size()), {}, false});
  }

  // Scrollable entity list with groups
  ImGui::BeginChild("EntityList", ImVec2(0, 300), true);

  // Lambda: build entity label (include both id and generation for uniqueness)
  auto makeLabel = [&](ECS::Entity entity, char *buf, size_t bufSize) {
    auto *tag = world.getComponent<Components::Tag>(entity);
    if (tag && !tag->tag.empty())
      snprintf(buf, bufSize, "%s (id %u.%u)", tag->tag.c_str(), entity.id, entity.generation);
    else
      snprintf(buf, bufSize, "Entity %u (gen %u)", entity.id,
               entity.generation);
  };

  // Lambda: render a single entity row (returns entity to clone/delete via
  // out-params)
  auto renderEntityRow = [&](ECS::Entity entity, int testIdx, int &outClone,
                             int &outDelete) {
    ImGui::PushID(static_cast<int>(entity.id) * 1000 +
                  static_cast<int>(entity.generation));

    char label[64];
    makeLabel(entity, label, sizeof(label));

    auto *tag = world.getComponent<Components::Tag>(entity);

    // Renaming
    if (m_renamingEntityIdx == testIdx && testIdx >= 0) {
      ImGui::SetKeyboardFocusHere();
      if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                           ImGuiInputTextFlags_EnterReturnsTrue |
                               ImGuiInputTextFlags_AutoSelectAll)) {
        if (tag) {
          tag->tag = std::string(m_renameBuffer);
        } else {
          world.addComponent(entity, Components::Tag{m_renameBuffer});
        }
        m_renamingEntityIdx = -1;
      }
      if (!ImGui::IsItemActive() &&
          (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
        if (tag)
          tag->tag = std::string(m_renameBuffer);
        m_renamingEntityIdx = -1;
      }
    } else {
      bool isMultiSelected = m_selectedEntities.count(testIdx) > 0;
      bool isSelected =
          isMultiSelected ||
          (!m_editingGroup && m_selectedEntityIdx == testIdx && testIdx >= 0);
      float availWidth = ImGui::GetContentRegionAvail().x;

      if (ImGui::Selectable(label, isSelected, 0, ImVec2(availWidth - 60, 0))) {
        m_editingGroup = false;
        m_selectedGroupIdx = -1;

        bool shift = ImGui::GetIO().KeyShift;
        bool ctrl = ImGui::GetIO().KeyCtrl;

        if (shift && m_lastClickedEntityIdx >= 0 && testIdx >= 0) {
          // Shift+Click: select range from last clicked to this one
          int lo = std::min(m_lastClickedEntityIdx, testIdx);
          int hi = std::max(m_lastClickedEntityIdx, testIdx);
          if (!ctrl)
            m_selectedEntities.clear();
          for (int r = lo; r <= hi; ++r)
            m_selectedEntities.insert(r);
          m_selectedEntityIdx = testIdx;
        } else if (ctrl) {
          // Ctrl+Click: toggle this entity in/out of selection
          if (m_selectedEntities.count(testIdx))
            m_selectedEntities.erase(testIdx);
          else
            m_selectedEntities.insert(testIdx);
          m_selectedEntityIdx = testIdx;
          m_lastClickedEntityIdx = testIdx;
        } else {
          // Normal click: single select
          m_selectedEntities.clear();
          m_selectedEntities.insert(testIdx);
          m_selectedEntityIdx = testIdx;
          m_lastClickedEntityIdx = testIdx;
        }
      }

      // Drag source: drag all selected entities (or just this one)
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        // Make sure the dragged entity is part of the selection
        if (m_selectedEntities.empty() || !m_selectedEntities.count(testIdx)) {
          m_selectedEntities.clear();
          m_selectedEntities.insert(testIdx);
          m_selectedEntityIdx = testIdx;
        }
        // Payload: send count of selected entities as marker
        // (actual entities read from m_selectedEntities on drop)
        int selCount = static_cast<int>(m_selectedEntities.size());
        ImGui::SetDragDropPayload("ENTITY_MULTI_DND", &selCount, sizeof(int));
        if (selCount == 1) {
          ImGui::Text("Move: %s", label);
        } else {
          ImGui::Text("Move %d entities", selCount);
        }
        ImGui::EndDragDropSource();
      }

      // Right click context menu
      if (ImGui::BeginPopupContextItem()) {
        // If right-clicking an unselected entity, select just it
        if (!m_selectedEntities.count(testIdx)) {
          m_selectedEntities.clear();
          m_selectedEntities.insert(testIdx);
          m_selectedEntityIdx = testIdx;
          m_lastClickedEntityIdx = testIdx;
        }

        int selCount = static_cast<int>(m_selectedEntities.size());
        if (selCount > 1) {
          ImGui::TextDisabled("%d entities selected", selCount);
          ImGui::Separator();
        }

        if (selCount <= 1 && ImGui::Selectable("Rename")) {
          m_renamingEntityIdx = testIdx;
          memset(m_renameBuffer, 0, sizeof(m_renameBuffer));
          if (tag)
            strncpy_s(m_renameBuffer, tag->tag.c_str(), sizeof(m_renameBuffer));
          ImGui::CloseCurrentPopup();
        }
        if (selCount <= 1 && ImGui::Selectable("Clone")) {
          outClone = testIdx;
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable(selCount > 1 ? "Delete All Selected"
                                           : "Delete")) {
          outDelete = testIdx; // handled below for multi-select
          ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        // Move to group submenu (operates on all selected)
        if (ImGui::BeginMenu("Move to Group")) {
          for (int g = 0; g < (int)m_entityGroups.size(); ++g) {
            if (ImGui::MenuItem(m_entityGroups[g].name.c_str())) {
              for (int si : m_selectedEntities) {
                if (si >= 0 && si < (int)m_testEntities.size()) {
                  removeEntityFromGroups(m_testEntities[si]);
                  m_entityGroups[g].entities.push_back(m_testEntities[si]);
                }
              }
            }
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Remove from Group")) {
            for (int si : m_selectedEntities) {
              if (si >= 0 && si < (int)m_testEntities.size())
                removeEntityFromGroups(m_testEntities[si]);
            }
          }
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("C")) {
      outClone = testIdx;
    }
    ImGui::SameLine();
    if (ImGui::Button("X")) {
      outDelete = testIdx;
    }
    ImGui::PopID();
  };

  int entityToClone = -1;
  int entityToDelete = -1;
  int groupToDelete = -1;

  // Collect entities that are in any group
  std::vector<bool> inGroup(m_testEntities.size(), false);
  for (const auto &group : m_entityGroups) {
    for (const auto &ge : group.entities) {
      for (size_t i = 0; i < m_testEntities.size(); ++i) {
        if (m_testEntities[i].id == ge.id &&
            m_testEntities[i].generation == ge.generation) {
          inGroup[i] = true;
        }
      }
    }
  }

  // --- Render Groups ---
  for (int g = 0; g < (int)m_entityGroups.size(); ++g) {
    auto &group = m_entityGroups[g];
    ImGui::PushID(g + 100000); // Unique ID range for groups

    // Group header
    bool groupSelected = (m_editingGroup && m_selectedGroupIdx == g);
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap;
    if (groupSelected)
      flags |= ImGuiTreeNodeFlags_Selected;
    if (!group.collapsed)
      flags |= ImGuiTreeNodeFlags_DefaultOpen;

    // Renaming group
    if (m_renamingGroupIdx == g) {
      ImGui::SetKeyboardFocusHere();
      if (ImGui::InputText("##grename", m_groupRenameBuffer,
                           sizeof(m_groupRenameBuffer),
                           ImGuiInputTextFlags_EnterReturnsTrue |
                               ImGuiInputTextFlags_AutoSelectAll)) {
        group.name = std::string(m_groupRenameBuffer);
        m_renamingGroupIdx = -1;
      }
      if (!ImGui::IsItemActive() &&
          (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
        group.name = std::string(m_groupRenameBuffer);
        m_renamingGroupIdx = -1;
      }
    } else {
      char groupLabel[128];
      snprintf(groupLabel, sizeof(groupLabel), "%s (%zu)", group.name.c_str(),
               group.entities.size());

      bool nodeOpen = ImGui::TreeNodeEx(groupLabel, flags);
      group.collapsed = !nodeOpen;

      // Click on group header (not arrow) selects the group for bulk editing
      if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        m_selectedGroupIdx = g;
        m_editingGroup = true;
        m_selectedEntityIdx = -1;
        m_selectedEntities.clear();
      }

      // Drop target: drop selected entities into this group
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("ENTITY_MULTI_DND")) {
          // Parse "Layer N" once for layer sync
          int layerNum = -1;
          sscanf(group.name.c_str(), "Layer %d", &layerNum);

          // Move all selected entities into this group
          for (int si : m_selectedEntities) {
            if (si < 0 || si >= (int)m_testEntities.size())
              continue;
            ECS::Entity dropped = m_testEntities[si];
            removeEntityFromGroups(dropped);
            group.entities.push_back(dropped);

            // If this is a "Layer N" group, update entity layer to match
            if (layerNum >= 0 && layerNum < m_numLayers) {
              auto *spr =
                  world.getComponent<Components::SpriteRenderer>(dropped);
              if (spr)
                spr->sortOrder = layerNum;
              for (auto &tile : m_tilemap) {
                if (tile.entity.id == dropped.id &&
                    tile.entity.generation == dropped.generation) {
                  tile.layer = layerNum;
                  break;
                }
              }
            }
          }
        }
        ImGui::EndDragDropTarget();
      }

      // Right click on group header
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Rename Group")) {
          m_renamingGroupIdx = g;
          memset(m_groupRenameBuffer, 0, sizeof(m_groupRenameBuffer));
          strncpy_s(m_groupRenameBuffer, group.name.c_str(),
                    sizeof(m_groupRenameBuffer));
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Delete Group (ungroup entities)")) {
          groupToDelete = g;
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (nodeOpen) {
        // Render entities inside this group
        for (size_t ei = 0; ei < group.entities.size(); ++ei) {
          auto ge = group.entities[ei];
          // Find index in m_testEntities
          int testIdx = -1;
          for (int ti = 0; ti < (int)m_testEntities.size(); ++ti) {
            if (m_testEntities[ti].id == ge.id &&
                m_testEntities[ti].generation == ge.generation) {
              testIdx = ti;
              break;
            }
          }
          if (testIdx < 0)
            continue;

          renderEntityRow(ge, testIdx, entityToClone, entityToDelete);
        }
        ImGui::TreePop();
      }
    }

    ImGui::PopID();
  }

  // --- Render Ungrouped entities ---
  bool hasUngrouped = false;
  for (size_t i = 0; i < m_testEntities.size(); ++i) {
    if (inGroup[i])
      continue;
    if (!hasUngrouped) {
      ImGui::Separator();
      ImGui::TextDisabled("Ungrouped");

      // Drop target on "Ungrouped" header to remove from group
      if (ImGui::BeginDragDropTarget()) {
        if (ImGui::AcceptDragDropPayload("ENTITY_MULTI_DND")) {
          for (int si : m_selectedEntities) {
            if (si >= 0 && si < (int)m_testEntities.size())
              removeEntityFromGroups(m_testEntities[si]);
          }
        }
        ImGui::EndDragDropTarget();
      }

      hasUngrouped = true;
    }
    renderEntityRow(m_testEntities[i], (int)i, entityToClone, entityToDelete);
  }

  // Handle deferred actions
  if (entityToClone != -1) {
    if (entityToClone >= 0 && entityToClone < (int)m_testEntities.size())
      cloneTestEntity(m_testEntities[entityToClone]);
  }

  if (entityToDelete != -1) {
    if (m_selectedEntities.size() > 1) {
      // Multi-delete: delete all selected entities (highest index first)
      std::vector<int> sorted(m_selectedEntities.begin(),
                              m_selectedEntities.end());
      std::sort(sorted.rbegin(), sorted.rend());
      for (int si : sorted) {
        if (si >= 0 && si < (int)m_testEntities.size()) {
          ECS::Entity toRemove = m_testEntities[si];
          removeEntityFromGroups(toRemove);
          auto cmd = std::make_unique<Sandbox::DeleteEntityCommand>(
              m_scene, m_testEntities, toRemove, static_cast<size_t>(si));
          executeCommand(std::move(cmd));
        }
      }
      m_selectedEntities.clear();
      m_selectedEntityIdx = -1;
    } else if (entityToDelete >= 0 &&
               entityToDelete < (int)m_testEntities.size()) {
      ECS::Entity toRemove = m_testEntities[entityToDelete];
      removeEntityFromGroups(toRemove);

      auto cmd = std::make_unique<Sandbox::DeleteEntityCommand>(
          m_scene, m_testEntities, toRemove,
          static_cast<size_t>(entityToDelete));
      executeCommand(std::move(cmd));

      m_selectedEntities.erase(entityToDelete);
      if (m_selectedEntityIdx == entityToDelete)
        m_selectedEntityIdx = -1;
      else if (m_selectedEntityIdx > entityToDelete)
        m_selectedEntityIdx--;
    }
  }

  if (groupToDelete != -1 && groupToDelete < (int)m_entityGroups.size()) {
    // Ungroup: just remove the group, entities stay in m_testEntities
    m_entityGroups.erase(m_entityGroups.begin() + groupToDelete);
    if (m_selectedGroupIdx == groupToDelete) {
      m_selectedGroupIdx = -1;
      m_editingGroup = false;
    } else if (m_selectedGroupIdx > groupToDelete) {
      m_selectedGroupIdx--;
    }
  }

  // Clean up stale entity references from all tracking lists
  // This prevents duplicate IDs and ghost entries
  m_testEntities.erase(
      std::remove_if(m_testEntities.begin(), m_testEntities.end(),
                     [&](const ECS::Entity &e) {
                       return !world.isValid(e);
                     }),
      m_testEntities.end());

  m_tilemap.erase(
      std::remove_if(m_tilemap.begin(), m_tilemap.end(),
                     [&](const TileData &tile) {
                       return !world.isValid(tile.entity);
                     }),
      m_tilemap.end());

  for (auto &group : m_entityGroups) {
    group.entities.erase(
        std::remove_if(group.entities.begin(), group.entities.end(),
                       [&](const ECS::Entity &e) { return !world.isValid(e); }),
        group.entities.end());
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

  // --- Group-level bulk editing ---
  if (m_editingGroup && m_selectedGroupIdx >= 0 &&
      m_selectedGroupIdx < (int)m_entityGroups.size()) {
    auto &group = m_entityGroups[m_selectedGroupIdx];
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                       "Editing Group: %s (%zu entities)", group.name.c_str(),
                       group.entities.size());
    ImGui::Separator();

    if (group.entities.empty()) {
      ImGui::TextDisabled("Group is empty");
      ImGui::End();
      return;
    }

    // Layer slider - apply to all entities in group
    {
      // Read layer from first entity as reference
      auto *firstSprite =
          world.getComponent<Components::SpriteRenderer>(group.entities[0]);
      int groupLayer = firstSprite ? firstSprite->sortOrder : 0;

      if (ImGui::SliderInt("Layer (all)", &groupLayer, 0, m_numLayers - 1)) {
        // Get or create the target layer group
        auto *targetGroup = getOrCreateLayerGroup(groupLayer);

        for (auto &ge : group.entities) {
          auto *spr = world.getComponent<Components::SpriteRenderer>(ge);
          if (spr)
            spr->sortOrder = groupLayer;
          for (auto &tile : m_tilemap) {
            if (tile.entity.id == ge.id &&
                tile.entity.generation == ge.generation) {
              tile.layer = groupLayer;
              break;
            }
          }
          // Move to target layer group if different from current
          if (targetGroup != &group) {
            targetGroup->entities.push_back(ge);
          }
        }
        // If we moved entities to a different group, clear this group
        if (targetGroup != &group) {
          group.entities.clear();
        }
      }
    }

    ImGui::Separator();

    // Visibility toggle for all
    if (ImGui::CollapsingHeader("SpriteRenderer (Group)")) {
      // Visible toggle
      bool allVisible = true;
      for (auto &ge : group.entities) {
        auto *spr = world.getComponent<Components::SpriteRenderer>(ge);
        if (spr && !spr->visible)
          allVisible = false;
      }
      if (ImGui::Checkbox("Visible (all)", &allVisible)) {
        for (auto &ge : group.entities) {
          auto *spr = world.getComponent<Components::SpriteRenderer>(ge);
          if (spr)
            spr->visible = allVisible;
        }
      }

      // Color for all
      auto *firstSprite =
          world.getComponent<Components::SpriteRenderer>(group.entities[0]);
      if (firstSprite) {
        glm::vec4 color = firstSprite->color;
        if (ImGui::ColorEdit4("Color (all)", &color.r)) {
          for (auto &ge : group.entities) {
            auto *spr = world.getComponent<Components::SpriteRenderer>(ge);
            if (spr)
              spr->color = color;
          }
        }
      }

      // Add SpriteRenderer to all that don't have it
      if (ImGui::Button("Add SpriteRenderer to All")) {
        for (auto &ge : group.entities) {
          if (!world.getComponent<Components::SpriteRenderer>(ge))
            world.addComponent(ge, Components::SpriteRenderer{});
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Remove SpriteRenderer from All")) {
        for (auto &ge : group.entities) {
          if (world.getComponent<Components::SpriteRenderer>(ge))
            world.removeComponent<Components::SpriteRenderer>(ge);
        }
      }
    }

    // Collider for all
    if (ImGui::CollapsingHeader("Collider (Group)")) {
      auto *firstCol =
          world.getComponent<Components::Collider>(group.entities[0]);
      if (firstCol) {
        bool solid = firstCol->isSolid;
        if (ImGui::Checkbox("Solid (all)", &solid)) {
          for (auto &ge : group.entities) {
            auto *col = world.getComponent<Components::Collider>(ge);
            if (col)
              col->isSolid = solid;
          }
        }
        bool trigger = firstCol->isTrigger;
        if (ImGui::Checkbox("Trigger (all)", &trigger)) {
          for (auto &ge : group.entities) {
            auto *col = world.getComponent<Components::Collider>(ge);
            if (col)
              col->isTrigger = trigger;
          }
        }
      }

      if (ImGui::Button("Add Collider to All")) {
        for (auto &ge : group.entities) {
          if (!world.getComponent<Components::Collider>(ge))
            world.addComponent(ge, Components::Collider{});
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Remove Collider from All")) {
        for (auto &ge : group.entities) {
          if (world.getComponent<Components::Collider>(ge))
            world.removeComponent<Components::Collider>(ge);
        }
      }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Click an entity in the group to edit individually");

    ImGui::End();
    return;
  }

  // --- Multi-select bulk editing ---
  if (!m_editingGroup && m_selectedEntities.size() > 1) {
    int selCount = static_cast<int>(m_selectedEntities.size());
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%d entities selected",
                       selCount);
    ImGui::Separator();

    // Collect valid selected entities
    std::vector<ECS::Entity> selEntities;
    for (int si : m_selectedEntities) {
      if (si >= 0 && si < (int)m_testEntities.size())
        selEntities.push_back(m_testEntities[si]);
    }

    if (!selEntities.empty()) {
      // Layer slider
      auto *firstSpr =
          world.getComponent<Components::SpriteRenderer>(selEntities[0]);
      int layer = firstSpr ? firstSpr->sortOrder : 0;
      if (ImGui::SliderInt("Layer (all)", &layer, 0, m_numLayers - 1)) {
        auto *targetGroup = getOrCreateLayerGroup(layer);
        for (auto &se : selEntities) {
          auto *spr = world.getComponent<Components::SpriteRenderer>(se);
          if (spr)
            spr->sortOrder = layer;
          for (auto &tile : m_tilemap) {
            if (tile.entity.id == se.id &&
                tile.entity.generation == se.generation) {
              tile.layer = layer;
              break;
            }
          }
          // Move to the target layer group
          removeEntityFromGroups(se);
          targetGroup->entities.push_back(se);
        }
      }

      ImGui::Separator();

      // Visibility
      if (ImGui::CollapsingHeader("SpriteRenderer (Selection)")) {
        bool allVis = true;
        for (auto &se : selEntities) {
          auto *spr = world.getComponent<Components::SpriteRenderer>(se);
          if (spr && !spr->visible)
            allVis = false;
        }
        if (ImGui::Checkbox("Visible (all)", &allVis)) {
          for (auto &se : selEntities) {
            auto *spr = world.getComponent<Components::SpriteRenderer>(se);
            if (spr)
              spr->visible = allVis;
          }
        }

        if (firstSpr) {
          glm::vec4 color = firstSpr->color;
          if (ImGui::ColorEdit4("Color (all)", &color.r)) {
            for (auto &se : selEntities) {
              auto *spr = world.getComponent<Components::SpriteRenderer>(se);
              if (spr)
                spr->color = color;
            }
          }
        }
      }

      // Collider
      if (ImGui::CollapsingHeader("Collider (Selection)")) {
        auto *firstCol =
            world.getComponent<Components::Collider>(selEntities[0]);
        if (firstCol) {
          bool solid = firstCol->isSolid;
          if (ImGui::Checkbox("Solid (all)", &solid)) {
            for (auto &se : selEntities) {
              auto *col = world.getComponent<Components::Collider>(se);
              if (col)
                col->isSolid = solid;
            }
          }
        }
      }

      // Move selection to group
      ImGui::Separator();
      if (ImGui::BeginCombo("Move to Group", "Select...")) {
        for (int g = 0; g < (int)m_entityGroups.size(); ++g) {
          if (ImGui::Selectable(m_entityGroups[g].name.c_str())) {
            for (auto &se : selEntities) {
              removeEntityFromGroups(se);
              m_entityGroups[g].entities.push_back(se);
            }
          }
        }
        ImGui::EndCombo();
      }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Click a single entity to edit individually");
    ImGui::End();
    return;
  }

  // --- Individual entity editing (existing behavior) ---

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

  // Tag / Name
  auto *tag = world.getComponent<Components::Tag>(entity);
  if (tag) {
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy_s(buffer, tag->tag.c_str(), sizeof(buffer));
    if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
      tag->tag = std::string(buffer);
    }

  } else {
    if (ImGui::Button("Add Tag")) {
      world.addComponent(entity, Components::Tag{"Entity"});
    }
  }

  ImGui::Separator();

  // Layer (render order: 0 = behind, higher = on top)
  {
    auto *sprite = world.getComponent<Components::SpriteRenderer>(entity);
    if (sprite) {
      int layer = sprite->sortOrder;
      if (ImGui::SliderInt("Layer", &layer, 0, m_numLayers - 1)) {
        sprite->sortOrder = layer;
        // Sync tilemap tile data
        for (auto &tile : m_tilemap) {
          if (tile.entity.id == entity.id &&
              tile.entity.generation == entity.generation) {
            tile.layer = layer;
            break;
          }
        }
        // Move entity to the corresponding "Layer N" group
        removeEntityFromGroups(entity);
        auto *layerGroup = getOrCreateLayerGroup(layer);
        layerGroup->entities.push_back(entity);
      }
      ImGui::Text("Layer %d of %d (0 = back, %d = front)", layer,
                  m_numLayers - 1, m_numLayers - 1);

      // Show current group membership
      int groupIdx = findGroupForEntity(entity);
      if (groupIdx >= 0) {
        ImGui::TextDisabled("Group: %s", m_entityGroups[groupIdx].name.c_str());
      } else {
        ImGui::TextDisabled("Group: (ungrouped)");
      }
    } else {
      ImGui::TextDisabled("Add SpriteRenderer to set layer");
    }
  }

  ImGui::Separator();

  // Transform Component
  if (ImGui::CollapsingHeader("Transform")) {
    auto *transform = world.getComponent<Components::Transform>(entity);

    if (transform) {
      ImGui::InputFloat2("Position", &transform->position.x);

      // Rotation (Radians -> Degrees for Editor)
      float deg = glm::degrees(transform->rotation);
      if (ImGui::DragFloat("Rotation (Deg)", &deg)) {
        transform->rotation = glm::radians(deg);
      }

      ImGui::InputFloat2("Scale", &transform->scale.x);
      ImGui::InputFloat2("Pivot", &transform->pivot.x);
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

  // Door Component
  if (ImGui::CollapsingHeader("Door")) {
    auto *door = world.getComponent<Components::Door>(entity);
    if (door) {
      ImGui::InputInt("Required Fragments", &door->requiredFragments);
      ImGui::Checkbox("Is Open", &door->isOpen);

      // Open Texture Dropdown
      if (ImGui::BeginCombo("Open Texture", door->openTexture.c_str())) {
        for (const auto &name :
             Assets::TextureManager::GetLoadedTextureNames()) {
          bool isSelected = (door->openTexture == name);
          if (ImGui::Selectable(name.c_str(), isSelected)) {
            door->openTexture = name;
          }
          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      // Closed Texture Dropdown
      if (ImGui::BeginCombo("Closed Texture", door->closedTexture.c_str())) {
        for (const auto &name :
             Assets::TextureManager::GetLoadedTextureNames()) {
          bool isSelected = (door->closedTexture == name);
          if (ImGui::Selectable(name.c_str(), isSelected)) {
            door->closedTexture = name;
          }
          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if (ImGui::Button("Remove Door")) {
        world.removeComponent<Components::Door>(entity);
      }
    } else {
      if (ImGui::Button("Add Door")) {
        world.addComponent(entity, Components::Door{});
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

      // Mass (Always show, even if not moveable, so we can configure it)
      if (c->moveable) {
        ImGui::InputFloat("Mass", &c->mass);
      } else {
        // Optional: Display as disabled or just normal?
        // User asked for "update mass", implying they want to edit it.
        // Let's just show it.
        ImGui::InputFloat("Mass", &c->mass);
      }

      if (ImGui::Button("Fit to Sprite")) {
        auto *spr = world.getComponent<Components::SpriteRenderer>(entity);
        if (spr) {
          if (c->shape == Components::Collider::Shape::Box) {
            c->size = spr->size;
          } else {
            // Radius is half the max dimension
            c->radius = std::max(spr->size.x, spr->size.y) * 0.5f;
          }

          // Also apply pivot offset if Transform exists
          auto *t = world.getComponent<Components::Transform>(entity);
          if (t) {
            // Collider offset should be zero because CollisionSystem now
            // automatically subtracts the pivot from the offset to align with
            // the sprite.
            c->offset = {0.f, 0.f};
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

  // Portal
  if (ImGui::CollapsingHeader("Portal")) {
    auto *portal = world.getComponent<Components::Portal>(entity);
    if (portal) {
      char buf[256];
      strncpy(buf, portal->levelName.c_str(), sizeof(buf));
      buf[sizeof(buf) - 1] = 0;
      if (ImGui::InputText("Level Name", buf, sizeof(buf))) {
        portal->levelName = buf;
      }
      if (ImGui::Button("Remove Portal")) {
        world.removeComponent<Components::Portal>(entity);
      }
    } else {
      if (ImGui::Button("Add Portal")) {
        world.addComponent(entity, Components::Portal{});
      }
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

      ImGui::Separator();
      ImGui::Text("Visibility");
      ImGui::Checkbox("Requires Light", &f->requiresLight);
      ImGui::DragFloat("Fade Speed", &f->fadeSpeed, 0.1f, 0.1f, 20.0f);
      ImGui::Text("Current Alpha: %.2f", f->currentAlpha);

      ImGui::Separator();
      ImGui::Text("Animation");
      ImGui::DragFloat("Bob Speed", &f->bobSpeed, 0.1f);
      ImGui::DragFloat("Bob Amount", &f->bobAmount, 0.1f);

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
      auto loadedSounds = Assets::SoundManager::GetLoadedSoundNames();
      if (ImGui::BeginCombo("Sound ID", a->soundId.c_str())) {
        for (const auto &soundName : loadedSounds) {
          bool isSelected = (a->soundId == soundName);
          if (ImGui::Selectable(soundName.c_str(), isSelected)) {
            a->soundId = soundName;
          }
          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
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

  // DoorSwing
  if (ImGui::CollapsingHeader("DoorSwing")) {
    auto *ds = world.getComponent<Components::DoorSwing>(entity);
    if (ds) {
      ImGui::Checkbox("Is Open", &ds->isOpen);
      ImGui::DragFloat("Swing Angle", &ds->swingAngle, 1.0f, 0.0f, 360.0f);
      ImGui::Checkbox("Clockwise", &ds->clockwise);
      ImGui::DragFloat("Speed", &ds->speed, 1.0f, 0.0f, 720.0f);

      ImGui::Text("Debug Info:");
      ImGui::Text("Initialized: %s", ds->initialized ? "Yes" : "No");
      ImGui::Text("Initial Rot: %.2f", ds->initialRotation);

      if (ImGui::Button("Reset Init")) {
        // Force re-initialization of rotation
        ds->initialized = false;
      }

      if (ImGui::Button("Remove DoorSwing"))
        world.removeComponent<Components::DoorSwing>(entity);
    } else {
      if (ImGui::Button("Add DoorSwing"))
        world.addComponent(entity, Components::DoorSwing{});
    }
  }

  // ActivationZone
  if (ImGui::CollapsingHeader("ActivationZone")) {
    auto *zone = world.getComponent<Components::ActivationZone>(entity);
    if (zone) {
      bool active = zone->active;
      if (ImGui::Checkbox("Active", &active))
        zone->active = active;

      bool loop = !zone->oneShot;
      if (ImGui::Checkbox("Loop (Not OneShot)", &loop))
        zone->oneShot = !loop;

      ImGui::Text("Has Triggered: %s", zone->triggered ? "Yes" : "No");
      if (ImGui::Button("Reset Trigger"))
        zone->triggered = false;

      // Action Type
      const char *actionItems[] = {"None", "PlayAudio", "SpawnEnemy",
                                   "ChangeVariable"};
      int currentAction = (int)zone->actionType;
      if (ImGui::Combo("Action", &currentAction, actionItems, 4)) {
        zone->actionType =
            (Components::ActivationZone::ActionType)currentAction;
      }

      // Payload
      char buffer[128] = "";
      if (!zone->payload.empty()) {
        strncpy_s(buffer, zone->payload.c_str(), sizeof(buffer));
      }

      if (ImGui::InputText("Payload", buffer, sizeof(buffer))) {
        zone->payload = std::string(buffer);
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Audio Name, Prefab Name, etc.");

      // UI Hint for AudioSource Link
      if (zone->actionType ==
              Components::ActivationZone::ActionType::PlayAudio &&
          zone->payload.empty()) {
        auto *audio = world.getComponent<Components::AudioSource>(entity);
        if (audio && !audio->soundId.empty()) {
          ImGui::TextDisabled("Using AudioSource: %s", audio->soundId.c_str());
        } else {
          ImGui::TextColored({1.0f, 0.5f, 0.0f, 1.0f},
                             "Warning: No Audio Provided");
        }
      }

      if (ImGui::Button("Remove ActivationZone"))
        world.removeComponent<Components::ActivationZone>(entity);
    } else {
      if (ImGui::Button("Add ActivationZone"))
        world.addComponent(entity, Components::ActivationZone{});
    }
  }

  // ParticleEmitter
  // Ensure renderer is set (fix for previously created components)
  if (ImGui::CollapsingHeader("ParticleEmitter")) {
    auto *pe = world.getComponent<Components::ParticleEmitter>(entity);
    if (pe) {
      bool active = pe->active();
      if (ImGui::Checkbox("Active", &active))
        pe->setActive(active);
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

  // TextRenderer Component
  if (ImGui::CollapsingHeader("TextRenderer")) {
    auto *tr = world.getComponent<Components::TextRenderer>(entity);
    if (tr) {
      // Text content input (256 char buffer)
      static char textBuf[256] = {};
      strncpy(textBuf, tr->text.c_str(), sizeof(textBuf) - 1);
      textBuf[sizeof(textBuf) - 1] = '\0';
      if (ImGui::InputTextMultiline("Text", textBuf, sizeof(textBuf),
                                    ImVec2(-1, 80))) {
        tr->text = textBuf;
      }

      ImGui::DragFloat("Font Size", &tr->fontSize, 0.05f, 0.1f, 10.0f);
      ImGui::ColorEdit3("Text Color", &tr->color.x);
      ImGui::Checkbox("Text Visible", &tr->visible);

      // Bounding box size
      ImGui::DragFloat2("Box Size", &tr->boxSize.x, 1.0f, 10.0f, 2000.0f);

      // Text alignment
      static const char *alignOptions[] = {"Left", "Center", "Right"};
      int alignIdx = static_cast<int>(tr->alignment);
      if (ImGui::Combo("Alignment", &alignIdx, alignOptions,
                       IM_ARRAYSIZE(alignOptions))) {
        tr->alignment = static_cast<Components::TextAlignment>(alignIdx);
      }

      // Font selection
      static const char *fontOptions[] = {"Debug"};
      int fontIdx = 0;
      if (ImGui::Combo("Font", &fontIdx, fontOptions,
                       IM_ARRAYSIZE(fontOptions))) {
        tr->fontId = fontOptions[fontIdx];
      }

      if (ImGui::Button("Remove TextRenderer")) {
        world.removeComponent<Components::TextRenderer>(entity);
      }
    } else {
      if (ImGui::Button("Add TextRenderer")) {
        world.addComponent(entity, Components::TextRenderer{});
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

  m_tilemap.push_back({"", position, 0, entity});

  // Default Name
  world.addComponent(entity,
                     Components::Tag{"Entity " + std::to_string(entity.id)});

  // Track via undo system (adds to m_testEntities)
  auto cmd = std::make_unique<Sandbox::CreateEntityCommand>(
      m_scene, m_testEntities, entity);
  executeCommand(std::move(cmd));
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

  m_tilemap.push_back({"Player", transform.position, 0, entity});
  world.addComponent(entity, Components::Tag{"Player"});

  // Track via undo system (adds to m_testEntities)
  auto cmd = std::make_unique<Sandbox::CreateEntityCommand>(
      m_scene, m_testEntities, entity);
  executeCommand(std::move(cmd));
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

  // Sprite (White for Enemy with Texture)
  // Sprite
  Components::SpriteRenderer sprite;
  sprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
  sprite.size = {50.f, 50.f};
  sprite.textureId = "monster_texture";
  sprite.uv = {0.75f, 0.5f, 0.25f, 0.25f};
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
  world.addComponent(
      entity, Components::LightExposure{}); // Needed for light interaction

  // Animation
  Components::SpriteAnimator anim;
  anim.loadFromPath("assets/animations/enemy.json");
  anim.filename = "enemy.json";
  anim.play("walk");
  world.addComponent(entity, anim);

  auto *enemyTrans = world.getComponent<Components::Transform>(entity);
  vec2 enemyPos = enemyTrans ? enemyTrans->position : vec2{0.f, 0.f};
  m_tilemap.push_back({"Enemy", enemyPos, 0, entity});
  world.addComponent(entity, Components::Tag{"Enemy"});

  // Track via undo system (adds to m_testEntities)
  auto cmd = std::make_unique<Sandbox::CreateEntityCommand>(
      m_scene, m_testEntities, entity);
  executeCommand(std::move(cmd));
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
  copyXYZ(Identity<Components::Door>{});
  copyXYZ(Identity<Components::Door>{});
  copyXYZ(Identity<Components::DoorSwing>{});
  copyXYZ(Identity<Components::Tag>{}); // Copy tag (will be same name)

  // Append (Clone) to name if it exists
  if (auto *tag = world.getComponent<Components::Tag>(newEntity)) {
    tag->tag += " (Clone)";
  } else {
    world.addComponent(newEntity, Components::Tag{"Entity (Clone)"});
  }

  // Transform (Offset slightly so we see it)
  auto *trans = world.getComponent<Components::Transform>(sourceEntity);
  if (trans) {
    Components::Transform newTrans = *trans;
    newTrans.position.x += 20.f;
    newTrans.position.y -= 20.f;
    world.addComponent(newEntity, newTrans);
  }

  // Add tilemap data for cloned entity
  auto *cloneTrans = world.getComponent<Components::Transform>(newEntity);
  vec2 clonePos = cloneTrans ? cloneTrans->position : vec2{0.f, 0.f};
  // Find source tile data to inherit prefab name and layer
  int cloneLayer = 0;
  std::string clonePrefab;
  for (const auto &tile : m_tilemap) {
    if (tile.entity.id == sourceEntity.id &&
        tile.entity.generation == sourceEntity.generation) {
      cloneLayer = tile.layer;
      clonePrefab = tile.prefabName;
      break;
    }
  }
  m_tilemap.push_back({clonePrefab, clonePos, cloneLayer, newEntity});

  // Inherit group membership from source entity
  int srcGroupIdx = findGroupForEntity(sourceEntity);
  if (srcGroupIdx >= 0) {
    m_entityGroups[srcGroupIdx].entities.push_back(newEntity);
  }

  // Track via undo system (adds to m_testEntities)
  auto cmd = std::make_unique<Sandbox::CreateEntityCommand>(
      m_scene, m_testEntities, newEntity);
  executeCommand(std::move(cmd));

  // Select the new one
  m_selectedEntityIdx = (int)m_testEntities.size() - 1;
  m_editingGroup = false;
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

    // Layer system
    ImGui::Separator();
    ImGui::Text("Layers");
    ImGui::SameLine();
    if (ImGui::Button("+##addlayer")) {
      m_numLayers++;
      m_layerVisible.push_back(true);
    }
    ImGui::SameLine();
    if (m_numLayers > 1 && ImGui::Button("-##remlayer")) {
      // Don't remove layer if current layer would become invalid
      if (m_currentLayer >= m_numLayers - 1) {
        m_currentLayer = m_numLayers - 2;
      }
      m_numLayers--;
      m_layerVisible.pop_back();
    }

    for (int i = 0; i < m_numLayers; i++) {
      ImGui::PushID(i);
      bool vis = m_layerVisible[i];
      if (ImGui::Checkbox("##vis", &vis)) {
        m_layerVisible[i] = vis;
      }
      ImGui::SameLine();
      bool selected = (m_currentLayer == i);
      char label[32];
      snprintf(label, sizeof(label), "Layer %d", i);
      if (ImGui::Selectable(label, selected)) {
        m_currentLayer = i;
      }
      ImGui::PopID();
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
  // Reset paint flag each frame
  m_paintedThisFrame = false;

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

      // Check if clicking on an existing entity - if so, skip painting to allow selection
      auto &world = m_scene.GetWorld();
      bool clickedOnEntity = false;
      for (int i = 0; i < (int)m_testEntities.size(); ++i) {
        auto entity = m_testEntities[i];
        auto *trans = world.getComponent<Components::Transform>(entity);
        if (trans) {
          float dist = magnitude(vec2(trans->position.x, trans->position.y) - worldPos);
          if (dist < m_gridSize * 0.5f) {
            clickedOnEntity = true;
            break;
          }
        }
      }

      // Only paint if not clicking on an entity and cell is not occupied
      if (!clickedOnEntity && !isCellOccupied(worldPos, m_currentLayer)) {
        // Spawn the tile
        spawnPrefab(m_selectedTilePrefab, worldPos);

        // Record in tilemap with layer and entity reference
        ECS::Entity spawned = m_testEntities.back();
        // Set sprite sortOrder to match layer for correct render order
        auto *spr = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(
            spawned);
        if (spr)
          spr->sortOrder = m_currentLayer;
        m_tilemap.push_back(
            {m_selectedTilePrefab, worldPos, m_currentLayer, spawned});
        // Auto-group into layer group (check for duplicates)
        auto *layerGroup = getOrCreateLayerGroup(m_currentLayer);
        bool alreadyInGroup = std::any_of(
            layerGroup->entities.begin(), layerGroup->entities.end(),
            [&](const ECS::Entity &e) {
              return e.id == spawned.id && e.generation == spawned.generation;
            });
        if (!alreadyInGroup) {
          layerGroup->entities.push_back(spawned);
        }

        // Mark that we painted this frame to block selection
        m_paintedThisFrame = true;
      }
    }
  }
  // Erase Mode
  else if (m_paintMode == PaintMode::Erase) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

      auto &world = m_scene.GetWorld();

      // Find tile on the current layer at this position
      for (auto it = m_tilemap.begin(); it != m_tilemap.end(); ++it) {
        if (it->layer != m_currentLayer)
          continue;
        float dist = magnitude(it->position - worldPos);
        if (dist < m_gridSize * 0.1f) {
          // Remove entity from m_testEntities
          auto entIt =
              std::find_if(m_testEntities.begin(), m_testEntities.end(),
                           [&](const ECS::Entity &e) {
                             return e.id == it->entity.id &&
                                    e.generation == it->entity.generation;
                           });
          if (entIt != m_testEntities.end()) {
            m_testEntities.erase(entIt);
          }

          // Remove from groups
          removeEntityFromGroups(it->entity);

          // Destroy entity from ECS
          world.destroyEntity(it->entity);

          // Remove from tilemap
          m_tilemap.erase(it);

          // Mark that we erased this frame to block selection
          m_paintedThisFrame = true;
          break; // Only erase one per click
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
  ImGui::Checkbox("TextRenderSystem", &m_scene.m_textRenderSystemEnabled);

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
      m_tilemap.push_back({prefabName, worldPos, 0, m_testEntities.back()});
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
  // Skip selection if we painted/erased this frame (clicked on blank space)
  // Allow selection if we clicked on an entity (didn't paint) or in Select mode
  bool allowSelection = !m_paintedThisFrame;
  if (allowSelection && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_viewportHovered &&
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

  // Disable gizmo when we painted/erased this frame to prevent accidental transforms
  if (m_paintedThisFrame)
    return;

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

  int releasedAxis = -1;
  if (mouseReleased && m_gizmoActiveAxis != -1) {
    releasedAxis = m_gizmoActiveAxis;
    m_gizmoActiveAxis = -1;
  } else if (mouseReleased) {
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

  // Create undo command when gizmo drag finishes
  if (releasedAxis != -1) {
    if (m_gizmoMode == GizmoMode::Translation) {
      vec2 newPos = trans->position;
      if (m_gizmoOriginalPos.x != newPos.x ||
          m_gizmoOriginalPos.y != newPos.y) {
        auto cmd = Sandbox::makePropertyCommand(
            world, entity, &Components::Transform::position, m_gizmoOriginalPos,
            newPos, "Move Entity");
        executeCommand(std::move(cmd));
      }
    } else if (m_gizmoMode == GizmoMode::Rotation) {
      float newRot = trans->rotation;
      if (m_gizmoOriginalRotation != newRot) {
        auto cmd = Sandbox::makePropertyCommand(
            world, entity, &Components::Transform::rotation,
            m_gizmoOriginalRotation, newRot, "Rotate Entity");
        executeCommand(std::move(cmd));
      }
    } else if (m_gizmoMode == GizmoMode::Scale) {
      vec2 newScale = trans->scale;
      if (m_gizmoOriginalScale.x != newScale.x ||
          m_gizmoOriginalScale.y != newScale.y) {
        auto cmd = Sandbox::makePropertyCommand(
            world, entity, &Components::Transform::scale, m_gizmoOriginalScale,
            newScale, "Scale Entity");
        executeCommand(std::move(cmd));
      }
    }
  }
}

void Runtime::SandboxState::renderAudioBrowser() {
  if (!m_showAudioBrowser)
    return;

  ImGui::Begin("Audio Browser", &m_showAudioBrowser);

  // Stop All Button
  if (ImGui::Button("Stop All Sounds")) {
    Core::Engine::Get().audio().stopAll();
  }

  ImGui::Separator();

  // List all loaded sounds
  auto soundNames = Assets::SoundManager::GetLoadedSoundNames();

  if (soundNames.empty()) {
    ImGui::TextDisabled("No sounds loaded.");
  } else {
    static ImGuiTextFilter filter;
    filter.Draw("Search");

    ImGui::BeginChild("SoundList");
    for (const auto &name : soundNames) {
      if (!filter.PassFilter(name.c_str()))
        continue;

      ImGui::PushID(name.c_str());

      if (ImGui::Button("Play")) {
        Assets::SoundManager::Play(name);
      }
      ImGui::SameLine();
      if (ImGui::Button("Stop")) {
        Core::Engine::Get().audio().stop(name);
      }
      ImGui::SameLine();
      ImGui::Text("%s", name.c_str());

      ImGui::PopID();
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

// namespace Runtime

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

  // Hot-reload toggle at the top
  ImGui::Checkbox("Hot Reload", &m_hotReloadEnabled);
  ImGui::SameLine();

  // "Add Asset" button
  if (ImGui::Button("Add Asset")) {
    ImGui::OpenPopup("AddAssetPopup");
    m_newAssetNameBuffer[0] = '\0';
    m_newAssetPathBuffer[0] = '\0';
  }

  // Add Asset popup
  if (ImGui::BeginPopupModal("AddAssetPopup", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Load a new texture asset");
    ImGui::Separator();
    ImGui::InputText("Asset Name", m_newAssetNameBuffer,
                     sizeof(m_newAssetNameBuffer));
    ImGui::InputText("File Path", m_newAssetPathBuffer,
                     sizeof(m_newAssetPathBuffer));
    if (ImGui::Button("Load")) {
      if (strlen(m_newAssetNameBuffer) > 0 &&
          strlen(m_newAssetPathBuffer) > 0) {
        auto loaded = Assets::TextureManager::LoadTexture(m_newAssetNameBuffer,
                                                          m_newAssetPathBuffer);
        if (loaded) {
          // Update hot-reload tracking
          m_textureFilePaths[m_newAssetNameBuffer] = m_newAssetPathBuffer;
          namespace fs = std::filesystem;
          if (fs::exists(m_newAssetPathBuffer)) {
            m_assetFileTimestamps[m_newAssetNameBuffer] =
                fs::last_write_time(m_newAssetPathBuffer);
          }
          std::cout << "[Sandbox] Loaded new asset: " << m_newAssetNameBuffer
                    << " from " << m_newAssetPathBuffer << std::endl;
        } else {
          std::cerr << "[Sandbox] Failed to load: " << m_newAssetPathBuffer
                    << std::endl;
        }
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::Separator();

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

            // Context menu (right-click) - Replace / Rename / Delete
            if (ImGui::BeginPopupContextItem()) {
              if (ImGui::MenuItem("Replace")) {
                m_showReplaceDialog = true;
                m_replacingTexture = name;
                m_replacePathBuffer[0] = '\0';
              }
              if (ImGui::MenuItem("Rename")) {
                // Rename placeholder - could be extended later
              }
              if (ImGui::MenuItem("Delete")) {
                Assets::TextureManager::Unload(name);
              }
              ImGui::EndPopup();
            }

            // Tooltip on hover - dependency info (only if no popup is open)
            if (ImGui::IsItemHovered() &&
                !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
              auto &world = m_scene.GetWorld();
              int usageCount = 0;
              std::vector<std::string> entityNames;
              world.each<Components::SpriteRenderer>(
                  [&](ECS::Entity e, Components::SpriteRenderer &sr) {
                    if (sr.textureId == name) {
                      ++usageCount;
                      auto *tag = world.getComponent<Components::Tag>(e);
                      if (tag) {
                        entityNames.push_back(tag->tag);
                      } else {
                        entityNames.push_back("Entity " + std::to_string(e.id));
                      }
                    }
                  });
              ImGui::BeginTooltip();
              ImGui::Text("Used by %d entit%s", usageCount,
                          usageCount == 1 ? "y" : "ies");
              for (const auto &ename : entityNames) {
                ImGui::BulletText("%s", ename.c_str());
              }
              ImGui::EndTooltip();
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

      // Dependencies section for selected asset
      if (!m_selectedAsset.empty()) {
        ImGui::Separator();
        renderAssetDependencies(m_selectedAsset);
      }

      // Drop zone hint at bottom of Textures tab
      ImGui::Separator();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1.f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.25f, 0.25f, 0.35f, 1.f));
      ImGui::Button("Drop image files onto the window or use Add Asset",
                    ImVec2(-1, 30));
      ImGui::PopStyleColor(2);

      // Accept internal drag-drop payloads (for future use)
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("ASSET_FILE_PATH")) {
          const char *droppedPath = (const char *)payload->Data;
          namespace fs = std::filesystem;
          fs::path filePath(droppedPath);
          std::string name = filePath.stem().string();
          auto loaded = Assets::TextureManager::LoadTexture(name, droppedPath);
          if (loaded) {
            m_textureFilePaths[name] = droppedPath;
            if (fs::exists(droppedPath)) {
              m_assetFileTimestamps[name] = fs::last_write_time(droppedPath);
            }
            std::cout << "[Sandbox] Loaded asset from drop: " << name
                      << std::endl;
          }
        }
        ImGui::EndDragDropTarget();
      }

      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

// Feature 1: Texture Replace Dialog
void Runtime::SandboxState::renderTextureReplaceDialog() {
  if (!m_showReplaceDialog)
    return;

  ImGui::OpenPopup("Replace Texture");
  if (ImGui::BeginPopupModal("Replace Texture", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Replacing: %s", m_replacingTexture.c_str());
    ImGui::InputText("New PNG path", m_replacePathBuffer,
                     sizeof(m_replacePathBuffer));
    if (ImGui::Button("OK")) {
      Assets::TextureManager::Unload(m_replacingTexture);
      Assets::TextureManager::LoadTexture(m_replacingTexture,
                                          m_replacePathBuffer);
      // Update file path mapping for hot-reload
      m_textureFilePaths[m_replacingTexture] = m_replacePathBuffer;
      if (std::filesystem::exists(m_replacePathBuffer)) {
        m_assetFileTimestamps[m_replacingTexture] =
            std::filesystem::last_write_time(m_replacePathBuffer);
      }
      std::cout << "[Sandbox] Replaced texture: " << m_replacingTexture
                << " -> " << m_replacePathBuffer << std::endl;
      m_showReplaceDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_showReplaceDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// Feature 2: Asset Hot-Reloading
void Runtime::SandboxState::initTextureFilePaths() {
  m_textureFilePaths.clear();
  m_assetFileTimestamps.clear();

  // Parse the textures config to build name -> path mapping
  const std::string configPath = "assets/configs/textures.json";
  std::ifstream file(configPath);
  if (!file.is_open())
    return;

  try {
    nlohmann::json j;
    file >> j;
    if (j.contains("textures") && j["textures"].is_array()) {
      for (const auto &item : j["textures"]) {
        if (item.contains("name") && item.contains("path")) {
          std::string name = item["name"].get<std::string>();
          std::string path = item["path"].get<std::string>();
          m_textureFilePaths[name] = path;

          // Record initial modification time
          if (std::filesystem::exists(path)) {
            m_assetFileTimestamps[name] =
                std::filesystem::last_write_time(path);
          }
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[Sandbox] Failed to parse texture config for hot-reload: "
              << e.what() << std::endl;
  }
}

void Runtime::SandboxState::checkAssetHotReload() {
  if (!m_hotReloadEnabled)
    return;

  for (auto &[name, path] : m_textureFilePaths) {
    if (!std::filesystem::exists(path))
      continue;

    auto currentTime = std::filesystem::last_write_time(path);
    auto it = m_assetFileTimestamps.find(name);
    if (it != m_assetFileTimestamps.end() && currentTime != it->second) {
      // File has been modified — reload
      Assets::TextureManager::Unload(name);
      Assets::TextureManager::LoadTexture(name, path);
      it->second = currentTime;
      std::cout << "[Sandbox] Hot-reloaded texture: " << name << std::endl;
    }
  }
}

// Feature 3: Asset Dependency Tracking
void Runtime::SandboxState::renderAssetDependencies(
    const std::string &textureName) {
  if (ImGui::CollapsingHeader("Dependencies", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto &world = m_scene.GetWorld();
    int usageCount = 0;
    std::vector<std::pair<unsigned int, std::string>> entities;

    world.each<Components::SpriteRenderer>([&](ECS::Entity e,
                                               Components::SpriteRenderer &sr) {
      if (sr.textureId == textureName) {
        ++usageCount;
        auto *tag = world.getComponent<Components::Tag>(e);
        std::string label = tag ? tag->tag : ("Entity " + std::to_string(e.id));
        entities.emplace_back(e.id, label);
      }
    });

    ImGui::Text("Texture: %s", textureName.c_str());
    ImGui::Text("Used by %d entit%s", usageCount,
                usageCount == 1 ? "y" : "ies");

    if (!entities.empty()) {
      ImGui::Indent();
      for (const auto &[id, label] : entities) {
        ImGui::BulletText("[%u] %s", id, label.c_str());
      }
      ImGui::Unindent();
    }
  }
}

void Runtime::SandboxState::saveTilemap(const std::string &filepath) {
  nlohmann::json tilemapJson;
  tilemapJson["gridSize"] = m_gridSize;
  tilemapJson["numLayers"] = m_numLayers;
  tilemapJson["tiles"] = nlohmann::json::array();

  for (const auto &tile : m_tilemap) {
    nlohmann::json tileJson;
    tileJson["prefab"] = tile.prefabName;
    tileJson["position"] = {{"x", tile.position.x}, {"y", tile.position.y}};
    tileJson["layer"] = tile.layer;
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

  // Load layer count
  if (tilemapJson.contains("numLayers")) {
    m_numLayers = tilemapJson["numLayers"].get<int>();
  }
  m_layerVisible.resize(m_numLayers, true);

  // Load tiles
  if (tilemapJson.contains("tiles")) {
    for (const auto &tileJson : tilemapJson["tiles"]) {
      std::string prefabName = tileJson.value("prefab", "");
      int layer = tileJson.value("layer", 0);
      vec2 position;
      if (tileJson.contains("position")) {
        position.x = tileJson["position"].value("x", 0.0f);
        position.y = tileJson["position"].value("y", 0.0f);
      }

      // Spawn the tile
      spawnPrefab(prefabName, position);

      // Record in tilemap with layer and entity
      ECS::Entity spawned = m_testEntities.back();
      // Set sprite sortOrder to match layer for correct render order
      auto *spr =
          m_scene.GetWorld().getComponent<Components::SpriteRenderer>(spawned);
      if (spr)
        spr->sortOrder = layer;
      m_tilemap.push_back({prefabName, position, layer, spawned});
    }
  }

  // Rebuild groups from loaded tilemap layer data
  rebuildGroupsFromTilemap();

  std::cout << "[Tilemap] Loaded: " << filepath << " (" << m_tilemap.size()
            << " tiles)\n";
}

bool Runtime::SandboxState::isCellOccupied(vec2 worldPos, int layer) const {
  for (const auto &tile : m_tilemap) {
    if (tile.layer == layer && tile.entity.isValid()) {
      float dist = magnitude(tile.position - worldPos);
      if (dist < m_gridSize * 0.1f) {
        return true;
      }
    }
  }
  return false;
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
  m_selectedGroupIdx = -1;
  m_editingGroup = false;
  m_selectedEntities.clear();
  m_lastClickedEntityIdx = -1;
  m_gizmoActiveAxis = -1;

  m_tilemap.clear();
  m_entityGroups.clear();
  std::cout << "[Tilemap] Cleared all entities and tile data\n";
}

int SandboxState::findGroupForEntity(ECS::Entity entity) const {
  for (int g = 0; g < (int)m_entityGroups.size(); ++g) {
    for (const auto &e : m_entityGroups[g].entities) {
      if (e.id == entity.id && e.generation == entity.generation)
        return g;
    }
  }
  return -1;
}

SandboxState::EntityGroup *SandboxState::getOrCreateLayerGroup(int layer) {
  std::string groupName = "Layer " + std::to_string(layer);
  for (auto &group : m_entityGroups) {
    if (group.name == groupName)
      return &group;
  }
  m_entityGroups.push_back({groupName, {}, false});
  return &m_entityGroups.back();
}

void SandboxState::removeEntityFromGroups(ECS::Entity entity) {
  for (auto &group : m_entityGroups) {
    auto it = std::find_if(group.entities.begin(), group.entities.end(),
                           [&](const ECS::Entity &e) {
                             return e.id == entity.id &&
                                    e.generation == entity.generation;
                           });
    if (it != group.entities.end()) {
      group.entities.erase(it);
      return;
    }
  }
}

void SandboxState::rebuildGroupsFromTilemap() {
  m_entityGroups.clear();
  for (const auto &tile : m_tilemap) {
    if (!tile.entity.isValid())
      continue;
    auto *group = getOrCreateLayerGroup(tile.layer);
    // Check for duplicates before adding
    bool alreadyInGroup = std::any_of(
        group->entities.begin(), group->entities.end(),
        [&](const ECS::Entity &e) {
          return e.id == tile.entity.id && e.generation == tile.entity.generation;
        });
    if (!alreadyInGroup) {
      group->entities.push_back(tile.entity);
    }
  }
}

} // namespace Runtime

void Runtime::SandboxState::renderActivationZones(
    Graphics::RenderingManager &renderer) {
  auto &world = m_scene.GetWorld();
  world.each<Components::Transform, Components::Collider,
             Components::ActivationZone>(
      [&](ECS::Entity, Components::Transform &t, Components::Collider &c,
          Components::ActivationZone &zone) {
        glm::vec4 color(0.0f, 1.0f, 0.0f, 0.3f); // Semi-transparent Green
        if (zone.triggered)
          color = glm::vec4(1.0f, 0.5f, 0.0f, 0.4f); // Orange if triggered
        if (!zone.active)
          color = glm::vec4(0.5f, 0.5f, 0.5f, 0.3f); // Gray if inactive

        // Calculate position with offset
        // Rotate offset by t.rotation
        float cosR = cos(t.rotation);
        float sinR = sin(t.rotation);
        glm::vec2 rotatedOffset = {c.offset.x * cosR - c.offset.y * sinR,
                                   c.offset.x * sinR + c.offset.y * cosR};
        glm::vec2 pos = {t.position.x + rotatedOffset.x,
                         t.position.y + rotatedOffset.y};

        if (c.shape == Components::Collider::Shape::Box) {
          // Draw filled box (Sprite)
          renderer.drawSprite(0, pos, {c.size.x, c.size.y}, t.rotation,
                              glm::vec4(0, 0, 1, 1), color);

          // Draw border (Debug Line) - Use rendering manager's drawRect if
          // available for rotated? drawRect only supports axis aligned in
          // helper, but manual lines needed for rotated. For now, filled sprite
          // is good enough as per request.
        } else {
          // Circle
          renderer.drawSprite(0, pos, {c.radius * 2.0f, c.radius * 2.0f},
                              t.rotation, glm::vec4(0, 0, 1, 1), color);
        }
      });
}

// =============================================================================
// Undo / Redo
// =============================================================================

void Runtime::SandboxState::executeCommand(
    std::unique_ptr<Sandbox::SandboxCommand> cmd) {
  cmd->execute();
  m_undoStack.push_back(std::move(cmd));

  // Trim to max undo levels
  while (m_undoStack.size() > MAX_UNDO_LEVELS) {
    m_undoStack.erase(m_undoStack.begin());
  }

  // Any new action invalidates the redo history
  m_redoStack.clear();
}

void Runtime::SandboxState::undo() {
  if (m_undoStack.empty())
    return;

  auto cmd = std::move(m_undoStack.back());
  m_undoStack.pop_back();

  cmd->undo();
  m_redoStack.push_back(std::move(cmd));
}

void Runtime::SandboxState::redo() {
  if (m_redoStack.empty())
    return;

  auto cmd = std::move(m_redoStack.back());
  m_redoStack.pop_back();

  cmd->execute();
  m_undoStack.push_back(std::move(cmd));
}

void Runtime::SandboxState::renderUndoRedoPanel() {
  ImGui::Begin("Undo / Redo");

  // Undo / Redo buttons
  {
    bool canUndo = !m_undoStack.empty();
    bool canRedo = !m_redoStack.empty();

    if (!canUndo)
      ImGui::BeginDisabled();
    if (ImGui::Button("Undo (Ctrl+Z)")) {
      undo();
    }
    if (!canUndo)
      ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canRedo)
      ImGui::BeginDisabled();
    if (ImGui::Button("Redo (Ctrl+Y)")) {
      redo();
    }
    if (!canRedo)
      ImGui::EndDisabled();
  }

  ImGui::Separator();

  // History list
  ImGui::Text("History (%zu / %zu)", m_undoStack.size(), MAX_UNDO_LEVELS);

  if (ImGui::BeginChild("UndoHistory", ImVec2(0, 0), true)) {
    // Show redo stack (future actions) in grey, top-to-bottom
    for (int i = static_cast<int>(m_redoStack.size()) - 1; i >= 0; --i) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      ImGui::Text("  %s", m_redoStack[i]->getName().c_str());
      ImGui::PopStyleColor();
    }

    // Current position marker
    if (!m_undoStack.empty() || !m_redoStack.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      ImGui::Text(">> Current");
      ImGui::PopStyleColor();
    }

    // Show undo stack (past actions), most recent first
    for (int i = static_cast<int>(m_undoStack.size()) - 1; i >= 0; --i) {
      ImGui::Text("  %s", m_undoStack[i]->getName().c_str());
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

// void Runtime::SandboxState::loadLevel(const std::string &levelName) {
//   std::string path = "assets/scenes/" + levelName;
//   std::cout << "[Sandbox] Loading level: " << path << "\n";
//
//   m_testEntities.clear();
//   m_tilemap.clear();
//   m_entityGroups.clear();
//   m_selectedGroupIdx = -1;
//   m_editingGroup = false;
//   m_selectedEntities.clear();
//   m_lastClickedEntityIdx = -1;
//
//   if (m_scene.LoadFromFile(path)) {
//     // Populate m_testEntities
//     auto &world = m_scene.GetWorld();
//     world.each<Components::Transform>(
//         [&](ECS::Entity entity, Components::Transform &trans) {
//           m_testEntities.push_back(entity);
//           // Restore tilemap data
//           int layer = 0;
//           auto *spr = world.getComponent<Components::SpriteRenderer>(entity);
//           if (spr)
//             layer = spr->sortOrder;
//           m_tilemap.push_back({"", trans.position, layer, entity});
//         });
//     rebuildGroupsFromTilemap();
//   } else {
//     // std::cerr << "[Sandbox] Failed to load level: " << path << "\n";
//     ImGui::OpenPopup(
//         "LoadError"); // Logic has to be handled where ImGui is active, this
//                       // might not work here if called from Update.
//     // For Update call, we can't open popup easily.
//     // We'll just log content.
//     std::cout << "[Sandbox] Failed to load level: " << path << "\n";
//   }
// }
