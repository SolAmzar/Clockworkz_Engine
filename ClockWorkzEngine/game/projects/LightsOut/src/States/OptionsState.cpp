/******************************************************************************/
/*!
\file           OptionsState.cpp
\project        Lights Out
\author(s)      [Amzar] 100%
\brief          Implements the options/settings menu state for Lights Out,
                backed by OptionsMenu.json. Matches the MainMenu overlay style.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
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
#include "Components/UISlider.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "GlobalAudioSettings.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "IMGUI/imgui.h"
#include "Platform/GamepadManager.hpp"
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

static const glm::vec3 kNormalColor   = {1.0f, 1.0f, 1.0f};
static const glm::vec3 kSelectedColor = {1.0f, 0.8f, 0.0f};

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

void OptionsState::updateSceneText() {
  const int masterPercent =
      static_cast<int>(GlobalAudioSettings::GetMasterVolume() * 100.0f);
  const int bgmPercent =
      static_cast<int>(GlobalAudioSettings::GetBGMVolume() * 100.0f);
  const int sfxPercent =
      static_cast<int>(GlobalAudioSettings::GetSFXVolume() * 100.0f);

  m_scene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        const std::string &t = tag.tag;
        if (t == "options_master_value") {
          tr.text = std::to_string(masterPercent) + "%";
          tr.color = (m_selected == 0) ? kSelectedColor : kNormalColor;
        } else if (t == "options_bgm_value") {
          tr.text = std::to_string(bgmPercent) + "%";
          tr.color = (m_selected == 1) ? kSelectedColor : kNormalColor;
        } else if (t == "options_sfx_value") {
          tr.text = std::to_string(sfxPercent) + "%";
          tr.color = (m_selected == 2) ? kSelectedColor : kNormalColor;
        } else if (t == "options_master_label") {
          tr.color = (m_selected == 0) ? kSelectedColor : kNormalColor;
        } else if (t == "options_bgm_label") {
          tr.color = (m_selected == 1) ? kSelectedColor : kNormalColor;
        } else if (t == "options_sfx_label") {
          tr.color = (m_selected == 2) ? kSelectedColor : kNormalColor;
        } else if (t == "options_back_label") {
          tr.color = (m_selected == 3) ? kSelectedColor : kNormalColor;
        }
      });
}

void OptionsState::closeOptions() {
  m_gsm.pop();
}

void OptionsState::syncSlidersFromSettings() {
  m_scene.GetWorld().each<Components::UISlider>(
      [&](ECS::Entity, Components::UISlider &sl) {
        if (sl.settingKey == "master") {
          sl.value = GlobalAudioSettings::GetMasterVolume();
        } else if (sl.settingKey == "bgm") {
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
        if (sl.settingKey == "master" &&
            GlobalAudioSettings::GetMasterVolume() != clamped) {
          GlobalAudioSettings::SetMasterVolume(clamped);
          changed = true;
        } else if (sl.settingKey == "bgm" &&
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
    GlobalAudioSettings::SetMasterVolume(clampedValue);
  } else if (optionIndex == 1) {
    GlobalAudioSettings::SetBGMVolume(clampedValue);
  } else if (optionIndex == 2) {
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
  auto stepped = [](float current, float d) {
    return std::round((current + d) * 10.0f) / 10.0f;
  };
  if (m_selected == 0) {
    setOptionVolume(0, stepped(GlobalAudioSettings::GetMasterVolume(), delta));
  } else if (m_selected == 1) {
    setOptionVolume(1, stepped(GlobalAudioSettings::GetBGMVolume(), delta));
  } else if (m_selected == 2) {
    setOptionVolume(2, stepped(GlobalAudioSettings::GetSFXVolume(), delta));
  }
}

void OptionsState::update(double /*dt*/) {
  int w = m_window.width();
  int h = m_window.height();
  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  const bool mousePressed = m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool mouseDown    = m_input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
  bool movedSelection = false;
  auto& gp = Core::Engine::Get().gamepad();

  m_sliderSystem.update(m_scene.GetWorld(), m_input, *m_cam, (float)w, (float)h);
  applySliderSettings();

  // Build hit rects from scene transforms
  auto getSpriteRectByTag = [this](const char *tagName, const RectBounds &fallback) {
    RectBounds rect = fallback;
    m_scene.GetWorld().each<Components::Tag, Components::Transform,
                            Components::SpriteRenderer>(
        [&](ECS::Entity, Components::Tag &tag, Components::Transform &tf,
            Components::SpriteRenderer &sr) {
          if (tag.tag == tagName) {
            rect.centerX = tf.position.x;
            rect.centerY = tf.position.y;
            rect.width   = sr.size.x;
            rect.height  = sr.size.y;
          }
        });
    return rect;
  };

  auto getTextRectByTag = [this](const char *tagName, const RectBounds &fallback) {
    RectBounds rect = fallback;
    m_scene.GetWorld().each<Components::Tag, Components::Transform,
                            Components::TextRenderer>(
        [&](ECS::Entity, Components::Tag &tag, Components::Transform &tf,
            Components::TextRenderer &tr) {
          if (tag.tag == tagName) {
            rect.centerX = tf.position.x;
            rect.centerY = tf.position.y;
            if (tr.boxSize.x > 0.0f) rect.width  = tr.boxSize.x;
            if (tr.boxSize.y > 0.0f) rect.height = tr.boxSize.y;
          }
        });
    return rect;
  };

  const RectBounds masterRect = getSpriteRectByTag(
      "options_master_bar", RectBounds{10.0f,  145.0f,  520.0f, 40.0f});
  const RectBounds bgmRect = getSpriteRectByTag(
      "options_bgm_bar",    RectBounds{10.0f,   31.0f,  520.0f, 40.0f});
  const RectBounds sfxRect = getSpriteRectByTag(
      "options_sfx_bar",    RectBounds{10.0f,  -90.0f,  520.0f, 40.0f});
  const RectBounds backRect = getTextRectByTag(
      "options_back_label", RectBounds{0.0f,  -179.0f,  240.0f, 60.0f});

  // Mouse hover detection
  int hoveredOption = -1;
  if (containsPoint(masterRect, mouseWorld))     hoveredOption = 0;
  else if (containsPoint(bgmRect, mouseWorld))   hoveredOption = 1;
  else if (containsPoint(sfxRect, mouseWorld))   hoveredOption = 2;
  else if (containsPoint(backRect, mouseWorld))  hoveredOption = 3;

  if (hoveredOption >= 0 && hoveredOption != m_selected) {
    m_selected = hoveredOption;
    Assets::SoundManager::Play("ui_move");
    movedSelection = true;
  }

  // Mouse click on back label
  if (mousePressed && hoveredOption == 3) {
    Assets::SoundManager::Play("ui_select");
    closeOptions();
    return;
  }

  // Mouse slider interaction (click snap + drag)
  auto applyMouseSlider = [this, &mouseWorld](const RectBounds &rect, int idx) {
    const float left       = rect.centerX - rect.width * 0.5f;
    const float normalized = clampVolume((mouseWorld.x - left) / rect.width);
    setOptionVolume(idx, normalized);
  };

  if (mousePressed && hoveredOption >= 0 && hoveredOption <= 2) {
    Assets::SoundManager::Play("ui_move");
    applyMouseSlider(hoveredOption == 0 ? masterRect : (hoveredOption == 1 ? bgmRect : sfxRect),
                     hoveredOption);
  } else if (mouseDown) {
    if (containsPoint(masterRect, mouseWorld))   applyMouseSlider(masterRect, 0);
    else if (containsPoint(bgmRect, mouseWorld)) applyMouseSlider(bgmRect,    1);
    else if (containsPoint(sfxRect, mouseWorld)) applyMouseSlider(sfxRect,    2);
  }

  // Keyboard UP / DOWN navigation
  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 3) % 4;
    movedSelection = true;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 1) % 4;
    movedSelection = true;
  }

  // Gamepad UP / DOWN navigation
  if (gp.isActionPressed("ui_up")) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 3) % 4;
    movedSelection = true;
  }
  if (gp.isActionPressed("ui_down")) {
    Assets::SoundManager::Play("ui_move");
    m_selected = (m_selected + 1) % 4;
    movedSelection = true;
  }

  if (movedSelection) {
    updateSceneText();
  }

  // Keyboard + gamepad LEFT / RIGHT volume adjust
  if (m_input.wasKeyPressed(GLFW_KEY_LEFT) || m_input.wasKeyPressed(GLFW_KEY_A) ||
      gp.isActionPressed("ui_left")) {
    Assets::SoundManager::Play("ui_move");
    adjustOptionVolume(-kVolumeStep);
  }
  if (m_input.wasKeyPressed(GLFW_KEY_RIGHT) || m_input.wasKeyPressed(GLFW_KEY_D) ||
      gp.isActionPressed("ui_right")) {
    Assets::SoundManager::Play("ui_move");
    adjustOptionVolume(kVolumeStep);
  }

  // Confirm — only triggers on BACK
  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
      m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) ||
      m_input.wasKeyPressed(GLFW_KEY_SPACE) ||
      gp.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
    if (m_selected == 3) {
      Assets::SoundManager::Play("ui_select");
      closeOptions();
      return;
    }
  }

  // Escape always closes
  if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) || gp.isActionPressed("escape")) {
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
  ImGui::Text("Master: %.2f", GlobalAudioSettings::GetMasterVolume());
  ImGui::Text("BGM: %.2f", GlobalAudioSettings::GetBGMVolume());
  ImGui::Text("SFX: %.2f", GlobalAudioSettings::GetSFXVolume());
  ImGui::End();
#endif
}

} // namespace Runtime
