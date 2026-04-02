/******************************************************************************/
/*!
\file           SpriteState.cpp
\project        Lights Out
\author(s)      [Lebon] 99% [Syafiq] 0% [Yusuf] 1%
\brief          Implements the Lights Out sprite preview state.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#if 0
/******************************************************************************/
/*!
\file           SpriteState.cpp
\project        Lights Out
\author(s)      [Lebon] 90%, [Wei An] 10%
\brief          Provides the Sprite Editor state for previewing and editing
animations. Supports loading scenes, manipulating sprite sheets, and
saving/loading animation JSON files.


All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "States/SpriteState.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Core/Profiler.hpp"
#include "Gameplay/Components/chase.hpp"
#include "Gameplay/GameObject.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "States/MenuState.hpp"
#include "Runtime/Scene.hpp"
#include "Runtime/SceneManager.hpp"
#include <Assets/TextureManager.hpp>
#include <Core/Engine.hpp>
#include <Core/Time.hpp>
#include <GLFW/glfw3.h>
#include <Gameplay/Prefab.hpp>
#include <Graphics/Camera2D.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Graphics/Renderer2D.hpp>
#include <Platform/LevelEditorState.hpp>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace Components;
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Website to see how sprite work
// https://codeshack.io/images-sprite-sheet-generator/
// https://ezgif.com/sprite-cutter/ezgif-4d563db71b384c6a.png.html

// UV
// 0,0 is at bottom left.
// 1,1 is at top right

using namespace Graphics;
using namespace Assets;

namespace Runtime {

// std::unordered_map<std::string, std::vector<glm::vec4>> playerSprite;
// const float changeFrameTime = 0.3f;
// float frameTimer = 0.f;
// size_t frameCount = 0;

// void ResetFrame()
//{
//	frameTimer = 0.f;
//	frameCount = 0;
// }

// void SpriteState::rebindLightsAndPlayerAfterReload()
//{
//	auto* scene = m_scenes.current();
//
// }

/*std::vector<std::string> SpriteState::GetTextureList(const std::string&
folder)
{

        std::vector<std::string> textures;
        for (auto& entry : std::filesystem::directory_iterator(folder))
        {
                if (!entry.is_regular_file()) continue;
                auto path = entry.path().string();

                auto hasExtension = [](const std::string& str, const
std::string& ext) { if (str.size() < ext.size()) return false; return
std::equal(ext.rbegin(), ext.rend(), str.rbegin(),
                                [](char a, char b) { return std::tolower(a) ==
std::tolower(b); });
                        };

                if (hasExtension(path, ".png") || hasExtension(path, ".jpg") ||
hasExtension(path, ".jpeg")) textures.push_back(path);
        }
        return textures;
}*/

void SpriteState::InitPlayerSpriteUVs() {
  if (m_spriteEntity == ECS::INVALID_ENTITY)
    return;
  auto *world = &m_scenes.current()->GetWorld();
  auto *anim = world->getComponent<Components::SpriteAnimator>(m_spriteEntity);
  if (!anim)
    return;

  const float frameW = 1.0f / 8.0f;
  const float frameH = 1.0f / 8.0f;

  auto &deathClip = anim->createClip("death", true);
  for (int i = 0; i < 21; ++i) {
    int row = i / 8;
    int col = i % 8;
    glm::vec4 uv = {col * frameW, (7 - row) * frameH, frameW, frameH};
    deathClip.frames.push_back({uv, 0.2f});
  }

  auto &idleClip = anim->createClip("idle", true);
  for (int i = 21; i < 25; ++i) {
    int row = i / 8;
    int col = i % 8;
    glm::vec4 uv = {col * frameW, (7 - row) * frameH, frameW, frameH};
    idleClip.frames.push_back({uv, 0.1f});
  }

  anim->play("death");
}

SpriteState::SpriteState(std::string initialSceneId,
                         Platform::InputManager &input, GameStateManager &gsm)
    : m_initialSceneId(std::move(initialSceneId)), m_input(input), m_gsm(gsm),
      m_sprite_obj(nullptr) {}

const char *SpriteState::name() const { return "SpriteEditor"; }

void SpriteState::onEnter() {
  std::cout << "[SpriteState] Enter\n";
  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();

  auto textureShader = ResourceManager<Shader>::Load(
      "texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
  ResourceManager<Shader>::Load("light", "assets/shaders/light.vert",
                                "assets/shaders/light.frag");

  if (m_scenes.loadScene(m_initialSceneId)) {
    m_loaded = true;
    if (auto *scene = m_scenes.current()) {
      auto &world = scene->GetWorld();
      // Find first entity with SpriteRenderer or create one?
      // For now, let's look for valid entities.
      // Since we can't easily iterate ALL entities without a view, let's hope
      // the scene has entities. Or just spawn a new test entity if none exists?
      // Actually, let's create a dedicated "SpriteEditorEntity" if not found.

      m_spriteEntity = ECS::INVALID_ENTITY;
      // Try to find an entity that looks like a player or has sprite renderer
      world.each<Components::SpriteRenderer>(
          [&](ECS::Entity e, Components::SpriteRenderer &sr) {
            if (m_spriteEntity == ECS::INVALID_ENTITY)
              m_spriteEntity = e;
          });

      if (m_spriteEntity == ECS::INVALID_ENTITY) {
        m_spriteEntity = world.createEntity();
        world.addComponent(m_spriteEntity, Components::Transform{});
        world.addComponent(m_spriteEntity, Components::SpriteRenderer{});
        world.addComponent(m_spriteEntity, Components::SpriteAnimator{});
        std::cout << "[SpriteState] Created new Sprite Editor Entity\n";
      } else {
        std::cout << "[SpriteState] Using existing Entity: " << m_spriteEntity
                  << "\n";
        if (!world.hasComponent<Components::SpriteAnimator>(m_spriteEntity)) {
          world.addComponent(m_spriteEntity, Components::SpriteAnimator{});
        }
      }
    }
  } else {
    std::cout << "[SpriteState] Scene load FAILED.\n";
    m_shouldReturnToMenu = true;
  }
  m_debug = true;
}

void SpriteState::onExit() {
  // Only unload if a scene was actually loaded successfully
  if (m_loaded) {
    m_scenes.unloadCurrent();
  }
  m_loaded = false;
}

void SpriteState::update(double dt) {

  Core::Engine &engine = Core::Engine::Get();
  // Platform::InputManager &input = engine.input();
  Graphics::Camera2D &cam = engine.camera();
  Platform::WindowManager &window = engine.window();
  vec2 mouseWorld =
      m_input.getMouseWorldPos(cam, static_cast<float>(window.width()),
                               static_cast<float>(window.height()));

  if (m_shouldReturnToMenu) {
    m_gsm.pop();
    return;
  }

  if (!m_loaded)
    return;

  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    m_gsm.pop();
    return;
  }

  // if (m_input.wasKeyPressed(GLFW_KEY_P))
  //{
  //	m_debug = !m_debug;
  //	if (m_debug)
  //	{
  //		std::cout << "Debug is pressed\n";
  //	}
  //	return;
  // }

  //// Syafiq - Player movement
  // if (auto *scene = m_scenes.current())
  //{

  //	// Syafiq - need find a way to cache the player.or do a unordered_map
  // with object unique name to array of objects.
  //	// objMap["Player"][0] <- since only 1 player. index 0 will work.
  //	for (auto &obj : scene->objects())
  //	{

  //
  //	}
  //}

  // if (m_input.isKeyDown(GLFW_KEY_Q))
  //{
  //	cam.setZoom(cam.zoom() * 1.02f);
  // }
  // if (m_input.isKeyDown(GLFW_KEY_E))
  //{
  //	cam.setZoom(cam.zoom() / 1.02f);
  // }

  // m_scenes.update(dt) -> m_scenes.update(dt, m_input)
  m_scenes.update(dt, m_input);
}

void SpriteState::render(Core::Time *timer) {
  if (!m_loaded || !m_renderer)
    return;

  Core::Engine &engine = Core::Engine::Get();

  m_renderer->beginScenePass(engine.camera().viewProj());
  m_scenes.render(*m_renderer);

  // Custom render overlay for the editing sprite
  if (m_spriteEntity != ECS::INVALID_ENTITY) {
    auto &world = m_scenes.current()->GetWorld();
    auto *trans = world.getComponent<Components::Transform>(m_spriteEntity);
    auto *sr = world.getComponent<Components::SpriteRenderer>(m_spriteEntity);

    if (trans && sr && !sr->textureId.empty()) {
      auto texture = ResourceManager<Texture2D>::Get(sr->textureId);
      if (texture) {
        glm::vec2 pos = trans->position + glm::vec2(550, 100);
        glm::vec2 scale = trans->scale * 7.0f;
        m_renderer->drawSprite(texture->id(), pos, sr->size * scale,
                               trans->rotation, sr->color, glm::vec4(1), false);
      }
    }
  }

  m_renderer->endScenePass();
  (void)timer;
}

void SpriteState::renderImGui(Core::Time *timer) {

  if (!m_debug || m_spriteEntity == ECS::INVALID_ENTITY)
    return;
  using namespace Gameplay; // Should probably remove this using soon
  ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::Begin("Sprite Animator Editor", nullptr, ImGuiWindowFlags_MenuBar);

  auto &world = m_scenes.current()->GetWorld();
  auto *anim = world.getComponent<Components::SpriteAnimator>(m_spriteEntity);

  if (!anim) {
    ImGui::Text("No SpriteAnimatorComponent found.");
    ImGui::End();
    return;
  }

  // --- Texture Browser ---
  if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
    static std::vector<std::string> texList =
        Assets::TextureManager::GetLoadedTextureNames();
    for (auto &tex : texList) {
      if (ImGui::Selectable(tex.c_str(), tex == m_selectedTexture)) {
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
    for (auto &[name, clip] : anim->clips)
      animNames.push_back(name);

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
        }
        ImGui::EndCombo();
      }
    }
    if (m_selectedAnimation != m_lastSelectedAnimation &&
        !m_selectedAnimation.empty()) {
      anim->play(m_selectedAnimation, true);
      m_playing = true;
      m_lastSelectedAnimation = m_selectedAnimation;
    }
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
    bool loopValue = m_playLoop;
    if (ImGui::Checkbox("Loop", &loopValue)) {
      m_playLoop = loopValue;
      if (!m_selectedAnimation.empty()) {
        if (auto *clip = anim->getClip(m_selectedAnimation)) {
          clip->loop = m_playLoop;
          anim->play(m_selectedAnimation, true);
        }
      }
    }

    ImGui::Separator();
    if (!m_selectedAnimation.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
      if (ImGui::Button("Delete Animation")) {
        ImGui::OpenPopup("ConfirmDelete");
      }
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
      ImGui::PopStyleColor();
    }

    (void)timer;
  }

  // --- Frame Editor ---
  if (!m_selectedAnimation.empty()) {
    Components::AnimationClip *clip = anim->getClip(m_selectedAnimation);
    if (clip) {
      ImGui::Separator();
      ImGui::Text("Editing Animation: %s (%zu frames)",
                  m_selectedAnimation.c_str(), clip->frames.size());

      // --- Bulk Duration Editor ---
      static float bulkDuration = 0.1f;
      ImGui::DragFloat("Bulk Duration", &bulkDuration, 0.01f, 0.01f, 5.0f);
      ImGui::SameLine();
      if (ImGui::Button("Apply to All Frames")) {
        for (auto &frame : clip->frames)
          frame.duration = bulkDuration;
      }
      ImGui::Separator();

      for (size_t i = 0; i < clip->frames.size(); ++i) {
        auto &frame = clip->frames[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(("Frame " + std::to_string(i)).c_str(),
                              i == m_selectedFrame))
          m_selectedFrame = (int)i;

        if (i == m_selectedFrame) {
          ImGui::DragFloat4("UV (x,y,w,h)", &frame.uv.x, 0.001f, 0.0f, 1.0f);
          ImGui::DragFloat("Duration", &frame.duration, 0.01f, 0.01f, 2.0f);
        }
        ImGui::PopID();
      }
    }
  }

  // --- Grid-Based Animation Creator ---
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
      int cols = m_sheetWidth / m_cellWidth;
      // int rows = m_sheetHeight / m_cellHeight;
      auto &clip = anim->createClip(m_newAnimName, m_playLoop);

      for (int i = m_startFrame; i <= m_endFrame; ++i) {
        int row = i / cols;
        int col = i % cols;
        float u = static_cast<float>(col * m_cellWidth) / m_sheetWidth;
        float v =
            1.0f - static_cast<float>((row + 1) * m_cellHeight) / m_sheetHeight;
        float w = static_cast<float>(m_cellWidth) / m_sheetWidth;
        float h = static_cast<float>(m_cellHeight) / m_sheetHeight;
        clip.frames.push_back({glm::vec4(u, v, w, h), 0.1f});
      }

      m_selectedAnimation = m_newAnimName;
    }
  }

  // --- Save / Load JSON Section ---
  static std::string jsonFileName = "";

  ImGuiInputTextFlags flags =
      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize;
  if (ImGui::InputText(
          "JSON File Name", jsonFileName.data(), jsonFileName.capacity() + 1,
          flags,
          [](ImGuiInputTextCallbackData *data) -> int {
            std::string *str = reinterpret_cast<std::string *>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
              str->resize(data->BufTextLen);
              data->Buf = str->data();
            }
            return 0;
          },
          &jsonFileName)) {
    // Optionally react to enter key here
  }

  std::string folder = "assets/animations/";
  std::string folder2 = "../assets/animations/";
  std::string path = folder + jsonFileName;
  std::string path2 = folder2 + jsonFileName;
  if (ImGui::Button("Save Animation JSON")) {
    std::filesystem::create_directories(folder);
    std::filesystem::create_directories(folder2);
    if (std::filesystem::exists(path)) {
      ImGui::OpenPopup("ConfirmOverwrite");
    } else {
      nlohmann::json j = anim->serialize(jsonFileName);
      std::ofstream(path) << j.dump(4);
      std::ofstream(path2) << j.dump(4);
      std::cout << "[SpriteState] Saved animation JSON to: "
                << std::filesystem::absolute(path) << std::endl;
    }
  }

  if (ImGui::BeginPopupModal("ConfirmOverwrite", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("File '%s' already exists.\nDo you want to overwrite it?",
                jsonFileName.c_str());
    ImGui::Separator();
    if (ImGui::Button("Yes, Overwrite", ImVec2(150, 0))) {
      nlohmann::json j = anim->serialize(jsonFileName);
      std::ofstream(path) << j.dump(4);
      std::ofstream(path2) << j.dump(4);
      std::cout << "[SpriteState] Overwrote: "
                << std::filesystem::absolute(path) << std::endl;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(150, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine();

  if (ImGui::Button("Reset JSON")) {
    anim->clearAll();
    jsonFileName = "";
    auto *spriteImageRenderer =
        world.getComponent<Components::SpriteRenderer>(m_spriteEntity);
    if (spriteImageRenderer) {
      // spriteImageRenderer->resetAll(); // Custom reset if needed, for now
      // just clear texture? spriteImageRenderer->textureId = "";
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Reload JSON")) {
    std::string folder1 = "assets/animations/";
    std::string path1 = folder1 + std::string(jsonFileName);
    std::ifstream f(path1);
    if (f.is_open()) {
      nlohmann::json j;
      f >> j;
      anim->loadFromJson(j);
      std::cout << "[SpriteState] Reloaded animation from: "
                << std::filesystem::absolute(path1) << std::endl;
    }
  }

  ImGui::End();
}

void SpriteState::resetScene() {}

} // namespace Runtime
#endif