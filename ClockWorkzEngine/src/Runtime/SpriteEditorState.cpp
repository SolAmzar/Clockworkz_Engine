/******************************************************************************/
/*!
\file           SpriteEditorState.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Implements SpriteEditorState.
*/
/******************************************************************************/

#include "Runtime/SpriteEditorState.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

// Engine Core
#include "Core/Engine.hpp"
#include "Core/Time.hpp"

// Platform
#include "Platform/WindowManager.hpp"

// Graphics
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"

// Components & Assets
#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"

// Runtime Includes
#include "Runtime/Scene.hpp"

// JSON
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)

// OpenGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace Runtime {

SpriteEditorState::SpriteEditorState(std::string initialSceneId,
                                     Platform::InputManager &input,
                                     GameStateManager &gsm)
    : m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm) {}

void SpriteEditorState::onEnter() {
  std::cout << "[SpriteEditorState] Enter\n";
  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();

  // Load shaders required for rendering sprites
  auto textureShader = Assets::ResourceManager<Graphics::Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
  Assets::ResourceManager<Graphics::Shader>::Load(
      "light", "assets/shaders/light.vert", "assets/shaders/light.frag");

  // Load Scene
  if (m_scenes.loadScene(m_initialSceneId)) {
    m_loaded = true;
    if (auto *scene = m_scenes.current()) {
      auto &world = scene->GetWorld();
      m_spriteEntity = ECS::Entity{};

      // Try to find an entity that has SpriteRenderer
      world.each<Components::SpriteRenderer>(
          [&](ECS::Entity e, Components::SpriteRenderer &sr) {
            (void)sr;
            if (!m_spriteEntity.isValid())
              m_spriteEntity = e;
          });

      if (!m_spriteEntity.isValid()) {
        m_spriteEntity = world.createEntity();
        world.addComponent(m_spriteEntity, Components::Transform{});
        world.addComponent(m_spriteEntity, Components::SpriteRenderer{});
        world.addComponent(m_spriteEntity, Components::SpriteAnimator{});
        std::cout << "[SpriteEditorState] Created new Sprite Editor Entity\n";
      } else {
        std::cout << "[SpriteEditorState] Using existing Entity: "
                  << m_spriteEntity.id << "\n";
        // Ensure it has animator
        if (!world.hasComponent<Components::SpriteAnimator>(m_spriteEntity)) {
          world.addComponent(m_spriteEntity, Components::SpriteAnimator{});
        }
      }
    }
  } else {
    std::cout << "[SpriteEditorState] Scene load FAILED.\n";
    m_shouldReturnToMenu = true;
  }
}

void SpriteEditorState::onExit() {
  std::cout << "[SpriteEditorState] Exit\n";
  if (m_loaded) {
    m_scenes.unloadCurrent();
  }
  m_loaded = false;
}

void SpriteEditorState::update(double dt) {
  if (m_shouldReturnToMenu) {
    m_gsm.pop();
    return;
  }

  if (!m_loaded)
    return;

  // Handle Exit
  if (m_input.wasKeyTriggered(GLFW_KEY_ESCAPE)) {
    m_gsm.pop();
    return;
  }

  // Toggle Debug UI with P
  if (m_input.wasKeyTriggered(GLFW_KEY_P)) {
    m_debug = !m_debug;
  }

  // Update Scene
  m_scenes.update(dt, m_input);
}

void SpriteEditorState::render(Core::Time *timer) {

  (void)timer;

  if (!m_loaded || !m_renderer)
    return;

  Core::Engine &engine = Core::Engine::Get();

  // Render Scene layer
  m_renderer->beginScenePass(engine.camera().viewProj());
  m_scenes.render(*m_renderer);

  // Custom overlay rendering for the sprite being edited
  if (m_spriteEntity.isValid()) {
    auto &world = m_scenes.current()->GetWorld();

    if (world.hasComponent<Components::Transform>(m_spriteEntity) &&
        world.hasComponent<Components::SpriteRenderer>(m_spriteEntity)) {
      auto *trans = world.getComponent<Components::Transform>(m_spriteEntity);
      auto *sr = world.getComponent<Components::SpriteRenderer>(m_spriteEntity);

      if (trans && sr && !sr->textureId.empty()) {
        auto texture =
            Assets::ResourceManager<Graphics::Texture2D>::Get(sr->textureId);
        if (texture) {
          // Render a large preview version
          // Manual conversion to avoid ambiguity between Math::vec2 and
          // glm::vec2
          glm::vec2 tPos(trans->position.x, trans->position.y);
          glm::vec2 tScale(trans->scale.x,
                           trans->scale.y);        // Scale is usually 1,1
          glm::vec2 sSize(sr->size.x, sr->size.y); // Size of sprite

          // Calculation from legacy code:
          // glm::vec2 pos = trans->position + glm::vec2(550, 100);
          // glm::vec2 scale = trans->scale * 7.0f;
          // But sr->size * scale was the 3rd arg (size on screen)

          glm::vec2 pos = tPos + glm::vec2(550.0f, 100.0f);
          glm::vec2 size = sSize * tScale * 7.0f;

          m_renderer->drawSprite(texture->id(), pos, size, trans->rotation,
                                 sr->color, glm::vec4(1.0f), false);
        }
      }
    }
  }

  m_renderer->endScenePass();
}

void SpriteEditorState::renderImGui(Core::Time *timer) {

  (void)timer;

  if (!m_debug || !m_spriteEntity.isValid())
    return;

  ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::Begin("Sprite Animator Editor", nullptr, ImGuiWindowFlags_MenuBar);

  // Help text
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Help")) {
      ImGui::MenuItem("Press 'P' to toggle this UI", nullptr, false, false);
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  auto &world = m_scenes.current()->GetWorld();
  auto *anim = world.getComponent<Components::SpriteAnimator>(m_spriteEntity);

  if (!anim) {
    ImGui::Text("No SpriteAnimatorComponent found on entity.");
    ImGui::End();
    return;
  }

  // --- Texture Browser ---
  if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
    static std::vector<std::string> texList =
        Assets::TextureManager::GetLoadedTextureNames();

    for (const auto &tex : texList) {
      bool selected = (tex == m_selectedTexture);
      if (ImGui::Selectable(tex.c_str(), selected)) {
        m_selectedTexture = tex;
        auto *sr =
            world.getComponent<Components::SpriteRenderer>(m_spriteEntity);
        if (sr) {
          sr->textureId = tex;
        }
      }
    }
  }

  // --- Animation Selection ---
  if (ImGui::CollapsingHeader("Animations", ImGuiTreeNodeFlags_DefaultOpen)) {
    std::vector<std::string> animNames;
    for (auto &[name, clip] : anim->clips) {
      animNames.push_back(name);
    }

    if (!animNames.empty()) {
      const char *current = m_selectedAnimation.empty()
                                ? animNames[0].c_str()
                                : m_selectedAnimation.c_str();
      if (ImGui::BeginCombo("Animation", current)) {
        for (auto &a : animNames) {
          bool selected = (a == m_selectedAnimation);
          if (ImGui::Selectable(a.c_str(), selected)) {
            m_selectedAnimation = a;
          }
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    }

    // Auto-play on selection change
    if (m_selectedAnimation != m_lastSelectedAnimation &&
        !m_selectedAnimation.empty()) {
      anim->play(m_selectedAnimation, true);
      m_playing = true;
      m_lastSelectedAnimation = m_selectedAnimation;
    }

    // Sync loop state
    if (auto *clip = anim->getClip(m_selectedAnimation)) {
      m_playLoop = clip->loop;
    }

    ImGui::Separator();

    // --- Playback Controls ---
    if (ImGui::Button("Play")) {
      if (!m_selectedAnimation.empty()) {
        anim->play(m_selectedAnimation, true);
        m_playing = true;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
      anim->play(m_selectedAnimation, true);
      m_playing = false;
    }
    ImGui::SameLine();
    bool loopVal = m_playLoop;
    if (ImGui::Checkbox("Loop", &loopVal)) {
      m_playLoop = loopVal;
      if (!m_selectedAnimation.empty()) {
        if (auto *clip = anim->getClip(m_selectedAnimation)) {
          clip->loop = m_playLoop;
          anim->play(m_selectedAnimation, true);
        }
      }
    }

    ImGui::Separator();

    // --- Delete Animation ---
    if (!m_selectedAnimation.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
      if (ImGui::Button("Delete Animation")) {
        ImGui::OpenPopup("ConfirmDelete");
      }
      ImGui::PopStyleColor();

      if (ImGui::BeginPopupModal("ConfirmDelete", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete animation '%s'?\nThis cannot be undone.",
                    m_selectedAnimation.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
          anim->removeClip(m_selectedAnimation);
          m_selectedAnimation.clear();
          m_lastSelectedAnimation.clear();
          m_selectedFrame = -1;
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
  }

  // --- Frame Editor ---
  if (!m_selectedAnimation.empty()) {
    Components::AnimationClip *clip = anim->getClip(m_selectedAnimation);
    if (clip) {
      ImGui::Separator();
      ImGui::Text("Editing Animation: %s (%zu frames)",
                  m_selectedAnimation.c_str(), clip->frames.size());

      // Bulk Duration
      static float bulkDuration = 0.1f;
      ImGui::DragFloat("Bulk Duration", &bulkDuration, 0.01f, 0.01f, 5.0f);
      ImGui::SameLine();
      if (ImGui::Button("Apply to All")) {
        for (auto &frame : clip->frames)
          frame.duration = bulkDuration;
      }

      ImGui::Separator();

      // Frame List
      for (size_t i = 0; i < clip->frames.size(); ++i) {
        auto &frame = clip->frames[i];
        ImGui::PushID(static_cast<int>(i));
        std::string label = "Frame " + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), (int)i == m_selectedFrame)) {
          m_selectedFrame = (int)i;
        }

        if ((int)i == m_selectedFrame) {
          ImGui::DragFloat4("UV (x,y,w,h)", &frame.uv.x, 0.001f, 0.0f, 1.0f);
          ImGui::DragFloat("Duration", &frame.duration, 0.01f, 0.01f, 2.0f);
        }
        ImGui::PopID();
      }
    }
  }

  // --- Grid-Based Generator ---
  if (ImGui::CollapsingHeader("Generate Animation From Sheet",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::InputInt("Sheet Width", &m_sheetWidth);
    ImGui::InputInt("Sheet Height", &m_sheetHeight);
    ImGui::InputInt("Cell Width", &m_cellWidth);
    ImGui::InputInt("Cell Height", &m_cellHeight);
    ImGui::InputInt("Start Frame", &m_startFrame);
    ImGui::InputInt("End Frame", &m_endFrame);
    ImGui::InputText("New Animation Name", m_newAnimName,
                     sizeof(m_newAnimName));

    if (ImGui::Button("Generate Animation")) {
      if (m_cellWidth > 0 && m_cellHeight > 0 && m_sheetWidth > 0 &&
          m_sheetHeight > 0) {
        int cols = m_sheetWidth / m_cellWidth;
        auto &clip = anim->createClip(m_newAnimName, m_playLoop);

        for (int i = m_startFrame; i <= m_endFrame; ++i) {
          int row = i / cols;
          int col = i % cols;
          float u = static_cast<float>(col * m_cellWidth) / m_sheetWidth;
          float v = 1.0f - static_cast<float>((row + 1) * m_cellHeight) /
                               m_sheetHeight;
          float w = static_cast<float>(m_cellWidth) / m_sheetWidth;
          float h = static_cast<float>(m_cellHeight) / m_sheetHeight;

          clip.frames.push_back({glm::vec4(u, v, w, h), 0.1f});
        }
        m_selectedAnimation = m_newAnimName;
      }
    }
  }

  // --- Save / Load JSON ---
  ImGui::Separator();

  // Buffer for input text
  static char jsonFileBuf[128] = "sprites.json";
  ImGui::InputText("JSON File Name", jsonFileBuf, sizeof(jsonFileBuf));

  // Use the buffer content for file operations
  std::string jsonFileName = jsonFileBuf;
  std::string folder = "assets/animations/";
  std::string path = folder + jsonFileName;

  if (ImGui::Button("Save Animation JSON")) {
    // Ensure directories exist
    if (!std::filesystem::exists(folder)) {
      std::filesystem::create_directories(folder);
    }

    nlohmann::json j = anim->serialize(jsonFileName);
    std::ofstream file(path);
    if (file.is_open()) {
      file << j.dump(4);
      std::cout << "[SpriteEditorState] Saved to " << path << "\n";
    } else {
      std::cerr << "[SpriteEditorState] Failed to save " << path << "\n";
    }
  }

  ImGui::SameLine();

  if (ImGui::Button("Reload JSON")) {
    std::ifstream file(path);
    if (file.is_open()) {
      nlohmann::json j;
      file >> j;
      anim->loadFromJson(j);
      std::cout << "[SpriteEditorState] Loaded from " << path << "\n";
    } else {
      std::cerr << "[SpriteEditorState] Failed to load " << path << "\n";
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear All")) {
    anim->clearAll();
    m_selectedAnimation.clear();
    m_lastSelectedAnimation.clear();
    m_selectedFrame = -1;
  }

  ImGui::End();
}

} // namespace Runtime
