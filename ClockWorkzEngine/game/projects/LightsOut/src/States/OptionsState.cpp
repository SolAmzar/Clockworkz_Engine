/******************************************************************************/
/*!
\file           OptionsState.cpp
\project        Lights Out
\brief          Dedicated options menu state backed by OptionsMenu.json.
*/
/******************************************************************************/

#include "States/OptionsState.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "GlobalAudioSettings.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>

using Assets::ResourceManager;
using Graphics::Shader;

namespace {
constexpr float kVolumeStep = 0.1f;

float clampVolume(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

struct RectBounds {
  float centerX;
  float centerY;
  float width;
  float height;
};

bool containsPoint(const RectBounds &rect, const vec2 &point) {
  const float halfW = rect.width * 0.5f;
  const float halfH = rect.height * 0.5f;
  return point.x >= rect.centerX - halfW && point.x <= rect.centerX + halfW &&
         point.y >= rect.centerY - halfH && point.y <= rect.centerY + halfH;
}
} // namespace

namespace Runtime {

OptionsState::OptionsState(GameStateManager &gsm, Platform::InputManager &input,
                           Platform::WindowManager &window,
                           Audio::AudioManager &audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void OptionsState::onEnter() {
  m_selected = 0;
  m_buttonEntities.clear();

  Core::Engine &engine = Core::Engine::Get();
  m_renderer = &engine.rendering();
  m_cam = &engine.camera();

  Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");
  ResourceManager<Shader>::Load("text", "assets/shaders/text.vert",
                                "assets/shaders/text.frag");
  ResourceManager<Shader>::Load("texture", "assets/shaders/texture.vert",
                                "assets/shaders/texture.frag");
  Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");

  m_scene.Init();
  m_scene.LoadFromFile("assets/scenes/OptionsMenu.json");
  collectButtons();

  GlobalAudioSettings::Load();
  GlobalAudioSettings::Apply(m_audio);
  syncSlidersFromSettings();
  updateSceneText();

  updateLayout(m_window.width(), m_window.height());

  engine.events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        if (e.width > 0 && e.height > 0) {
          updateLayout(e.width, e.height);
        }
      });
}

void OptionsState::onExit() {
  Core::Engine::Get().events().unsubscribeOwner(this);
}

void OptionsState::updateLayout(int w, int h) {
  float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
  m_cam->setViewport(w, h);
  m_cam->setPosition({0.f, 0.f});
  m_cam->setZoom(zoom);
  glViewport(0, 0, w, h);
}

void OptionsState::collectButtons() {
  m_buttonEntities.clear();
  m_scene.GetWorld().each<Components::UIButton>(
      [this](ECS::Entity e, Components::UIButton &) {
        m_buttonEntities.push_back(e);
      });
}

void OptionsState::applyHoverColors() {
  for (ECS::Entity e : m_buttonEntities) {
    auto *btn = m_scene.GetWorld().getComponent<Components::UIButton>(e);
    auto *sr = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    auto *tr = m_scene.GetWorld().getComponent<Components::TextRenderer>(e);
    if (!btn) {
      continue;
    }

    const bool selected = (m_selected == 2);
    const glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
    if (sr) {
      sr->color = glm::vec4(c, sr->color.a);
      if (!btn->showBackground) {
        sr->visible = false;
      }
    }
    if (tr) {
      tr->color = c;
    }
  }
}

void OptionsState::handleAction(const std::string &action) {
  if (action == "back") {
    closeOptions();
  }
}

void OptionsState::updateSceneText() {
  const int bgmPercent =
      static_cast<int>(GlobalAudioSettings::GetBGMVolume() * 100.0f);
  const int sfxPercent =
      static_cast<int>(GlobalAudioSettings::GetSFXVolume() * 100.0f);

  m_scene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        if (tag.tag == "options_bgm_value") {
          tr.text = std::to_string(bgmPercent) + "%";
        } else if (tag.tag == "options_sfx_value") {
          tr.text = std::to_string(sfxPercent) + "%";
        }
      });

  applyHoverColors();
}

void OptionsState::closeOptions() {
  m_gsm.pop();
}

void OptionsState::syncSlidersFromSettings() {
  m_scene.GetWorld().each<Components::UISlider>(
      [&](ECS::Entity, Components::UISlider &sl) {
        if (sl.settingKey == "bgm") {
          sl.value = GlobalAudioSettings::GetBGMVolume();
        } else if (sl.settingKey == "sfx") {
          sl.value = GlobalAudioSettings::GetSFXVolume();
        }
      });
}

void OptionsState::applySliderSettings() {
  bool changed = false;
  m_scene.GetWorld().each<Components::UISlider>(
      [&](ECS::Entity, Components::UISlider &sl) {
        const float clamped = clampVolume(sl.value);
        if (sl.settingKey == "bgm" &&
            GlobalAudioSettings::GetBGMVolume() != clamped) {
          GlobalAudioSettings::SetBGMVolume(clamped);
          changed = true;
        } else if (sl.settingKey == "sfx" &&
                   GlobalAudioSettings::GetSFXVolume() != clamped) {
          GlobalAudioSettings::SetSFXVolume(clamped);
          changed = true;
        }
      });

  if (changed) {
    GlobalAudioSettings::Apply(m_audio);
    GlobalAudioSettings::Save();
    updateSceneText();
  }
}

void OptionsState::setOptionVolume(int optionIndex, float value) {
  const float clampedValue = clampVolume(value);

  if (optionIndex == 0) {
    GlobalAudioSettings::SetBGMVolume(clampedValue);
  } else if (optionIndex == 1) {
    GlobalAudioSettings::SetSFXVolume(clampedValue);
  } else {
    return;
  }

  GlobalAudioSettings::Apply(m_audio);
  GlobalAudioSettings::Save();
  syncSlidersFromSettings();
  updateSceneText();
}

void OptionsState::adjustOptionVolume(float delta) {
  if (m_selected == 0) {
    setOptionVolume(0, GlobalAudioSettings::GetBGMVolume() + delta);
  } else if (m_selected == 1) {
    setOptionVolume(1, GlobalAudioSettings::GetSFXVolume() + delta);
  }
}

void OptionsState::update(double /*dt*/) {
  int w = m_window.width();
  int h = m_window.height();
  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  const bool mousePressed = m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);
  bool movedSelection = false;

  m_sliderSystem.update(m_scene.GetWorld(), m_input, *m_cam, (float)w,
                        (float)h);
  applySliderSettings();

  auto getSliderRectByTag =
      [this](const char *tagName, const RectBounds &fallback) {
        RectBounds rect = fallback;
        m_scene.GetWorld().each<Components::Tag, Components::Transform,
                                Components::UISlider>(
            [&](ECS::Entity, Components::Tag &tag,
                Components::Transform &transform, Components::UISlider &sl) {
              if (tag.tag == tagName) {
                rect.centerX = transform.position.x;
                rect.centerY = transform.position.y;
                rect.width = sl.trackWidth;
                rect.height = std::max(sl.trackHeight, sl.handleSize);
              }
            });
        return rect;
      };

  const RectBounds bgmRect = getSliderRectByTag(
      "options_bgm_bar", RectBounds{200.0f, 70.0f, 500.0f, 48.0f});
  const RectBounds sfxRect = getSliderRectByTag(
      "options_sfx_bar", RectBounds{200.0f, -70.0f, 500.0f, 48.0f});

  int hoveredIndex = -1;
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_scene.GetWorld().getComponent<Components::UIButton>(e);
    auto *tf = m_scene.GetWorld().getComponent<Components::Transform>(e);
    if (!btn || !tf || !btn->interactable) {
      continue;
    }

    float cx    = tf->position.x + btn->offset.x;
    float cy    = tf->position.y + btn->offset.y;
    float halfW = btn->size.x * 0.5f;
    float halfH = btn->size.y * 0.5f;
    const bool hovered =
        mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
        mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH;
    if (hovered) {
      hoveredIndex = i;
      break;
    }
  }

  if (hoveredIndex >= 0 && m_selected != 2) {
    m_selected = 2;
    Assets::SoundManager::Play("ui_move");
    movedSelection = true;
  }

  if (hoveredIndex >= 0 && mousePressed) {
    Assets::SoundManager::Play("ui_select");
    auto *btn = m_scene.GetWorld().getComponent<Components::UIButton>(
        m_buttonEntities[hoveredIndex]);
    if (btn) {
      handleAction(btn->action);
      return;
    }
  }

  int hoveredOption = -1;
  if (containsPoint(bgmRect, mouseWorld)) {
    hoveredOption = 0;
  } else if (containsPoint(sfxRect, mouseWorld)) {
    hoveredOption = 1;
  }

  if (hoveredOption >= 0 && hoveredOption != m_selected) {
    m_selected = hoveredOption;
    Assets::SoundManager::Play("ui_move");
    movedSelection = true;
  }

  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 2) % 3;
    movedSelection = true;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) ||
      m_input.wasKeyPressed(GLFW_KEY_S)) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 1) % 3;
    movedSelection = true;
  }
  if (movedSelection) {
    updateSceneText();
  }

  if (m_input.wasKeyPressed(GLFW_KEY_LEFT) ||
      m_input.wasKeyPressed(GLFW_KEY_A)) {
    Assets::SoundManager::Play("ui_move");
    adjustOptionVolume(-kVolumeStep);
  }
  if (m_input.wasKeyPressed(GLFW_KEY_RIGHT) ||
      m_input.wasKeyPressed(GLFW_KEY_D)) {
    Assets::SoundManager::Play("ui_move");
    adjustOptionVolume(kVolumeStep);
  }

  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
      m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) ||
      m_input.wasKeyPressed(GLFW_KEY_SPACE)) {
    if (m_selected == 2) {
      Assets::SoundManager::Play("ui_select");
      handleAction("back");
      return;
    }
  }

  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE)) {
    Assets::SoundManager::Play("ui_select");
    closeOptions();
    return;
  }
}

void OptionsState::render(Core::Time * /*timer*/) {
  int w = m_window.width();
  int h = m_window.height();
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glm::mat4 vp = m_cam->viewProj();
  m_scene.RenderAll(*m_renderer, vp);
}

void OptionsState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  if (!m_debug) {
    return;
  }
  ImGui::Begin("OptionsState Debug");
  ImGui::Text("Selected: %d", m_selected);
  ImGui::Text("BGM: %.2f", GlobalAudioSettings::GetBGMVolume());
  ImGui::Text("SFX: %.2f", GlobalAudioSettings::GetSFXVolume());
  ImGui::End();
#endif
}

} // namespace Runtime
