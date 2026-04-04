/******************************************************************************/
/*!
\file           MenuState.cpp
\project        Lights Out
\author(s)      [Amzar] 72% [Amirul Syafiq] 14% [Lebon] 14%
\brief          Main menu state. Loads MainMenu.json scene from the level
                editor. Button entities are identified by UIButton components;
                positions, sizes, and colors are all controlled in the editor.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "States/MenuState.hpp"
#include "States/GamePlayState.hpp"
#include "States/OptionsState.hpp"
#include "Scripting/LuaManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/AudioSettings.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "GlobalAudioSettings.hpp"
#include "IMGUI/imgui.h"
#include "Platform/GamepadManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include "States/PlayState.hpp"
#include "States/SettingsState.hpp"
#include "States/SpriteEditorState.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>

using Assets::ResourceManager;
using Graphics::Shader;

namespace {
constexpr float kVolumeStep = 0.1f;
float s_menuMasterVolume = 1.0f;
float s_menuBgmVolume = 1.0f;
float s_menuSfxVolume = 1.0f;

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

bool isOptionsEntityTag(const std::string &tag) {
  std::string lowered = tag;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return lowered.find("options") != std::string::npos;
}
} // namespace

namespace Runtime {

MenuState::MenuState(GameStateManager &gsm, Platform::InputManager &input,
                     Platform::WindowManager &window, Audio::AudioManager &audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void MenuState::onEnter() {
  m_selected = 0;
  m_optionsSelected = 0;
  syncOptionsSlidersInScene();
  m_showingHowToPlay = false;
  m_showingOptions = false;
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

  m_menuScene.Init();
  m_menuScene.LoadFromFile("assets/scenes/MainMenu.json");
  collectButtons();

  setOverlayVisible("howtoplay_overlay", false);
  setOverlayVisible("options_overlay", false);
  setMenuButtonsVisible(true);

  GlobalAudioSettings::Load();
  s_menuMasterVolume = GlobalAudioSettings::GetMasterVolume();
  s_menuBgmVolume = GlobalAudioSettings::GetBGMVolume();
  s_menuSfxVolume = GlobalAudioSettings::GetSFXVolume();
  auto &audioSettings = Audio::AudioSettings::Get();
  audioSettings.master = s_menuMasterVolume;
  audioSettings.bgm = s_menuBgmVolume;
  audioSettings.sfx = s_menuSfxVolume;
  syncOptionsSlidersInScene();
  updateOptionsOverlayText();
  GlobalAudioSettings::Apply(m_audio);

  // Scene::LoadFromFile() clears the BGM group, so start menu music after load.
  m_bgmChannel = Assets::SoundManager::Play("menu_bgm");

  updateLayout(m_window.width(), m_window.height());

  engine.events().subscribe<Core::WindowResizeEvent>(
      this, [this](const Core::WindowResizeEvent &e) {
        if (e.width > 0 && e.height > 0) {
          updateLayout(e.width, e.height);
        }
      });
}

void MenuState::updateLayout(int w, int h) {
  float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
  m_cam->setViewport(w, h);
  m_cam->setPosition({0.f, 0.f});
  m_cam->setZoom(zoom);
  glViewport(0, 0, w, h);
}

void MenuState::collectButtons() {
  m_buttonEntities.clear();
  m_menuScene.GetWorld().each<Components::UIButton>(
      [this](ECS::Entity e, Components::UIButton &) {
        m_buttonEntities.push_back(e);
      });

  std::sort(m_buttonEntities.begin(), m_buttonEntities.end(),
            [this](ECS::Entity lhs, ECS::Entity rhs) {
              const auto *lhsTransform =
                  m_menuScene.GetWorld().getComponent<Components::Transform>(lhs);
              const auto *rhsTransform =
                  m_menuScene.GetWorld().getComponent<Components::Transform>(rhs);

              const float lhsY = lhsTransform ? lhsTransform->position.y : 0.0f;
              const float rhsY = rhsTransform ? rhsTransform->position.y : 0.0f;
              return lhsY > rhsY;
            });
}

void MenuState::applyHoverColors() {
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *sr = m_menuScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
    auto *tr = m_menuScene.GetWorld().getComponent<Components::TextRenderer>(e);
    if (!btn) {
      continue;
    }

    const bool selected = (i == m_selected);
    const glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
    if (sr) {
      sr->visible = btn->showBackground;
      if (btn->showBackground) {
        sr->color = glm::vec4(c, 1.f);
      }
    }
    if (tr) {
      tr->color = c;
    }
  }
}

void MenuState::setOverlayVisible(const std::string &tag, bool visible) {
  m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
      [&](ECS::Entity, Components::Tag &entityTag,
          Components::SpriteRenderer &sr) {
        if (entityTag.tag == tag) {
          sr.visible = (tag == "options_overlay") ? false : visible;
        }
      });

  m_menuScene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &entityTag,
          Components::TextRenderer &tr) {
        if (entityTag.tag == tag) {
          tr.visible = (tag == "options_overlay") ? false : visible;
          if (tag == "options_overlay") {
            tr.fontId = "BodyText";
            tr.fontSize = 0.58f;
            tr.alignment = Components::TextAlignment::Left;
            tr.boxSize = {760.0f, 260.0f};
          }
        }
      });

  if (tag == "options_overlay") {
    m_menuScene.GetWorld().each<Components::Tag, Components::Transform>(
        [&](ECS::Entity, Components::Tag &entityTag,
            Components::Transform &transform) {
          if (entityTag.tag == tag) {
            transform.position = {-360.0f, -130.0f};
          }
        });
  }
}

void MenuState::syncOptionsSlidersInScene() {
  m_menuScene.GetWorld().each<Components::Tag, Components::UISlider>(
      [&](ECS::Entity, Components::Tag &tag, Components::UISlider &sl) {
        if (tag.tag == "options_master_bar") {
          sl.value = s_menuMasterVolume;
        } else if (tag.tag == "options_bgm_bar") {
          sl.value = s_menuBgmVolume;
        } else if (tag.tag == "options_sfx_bar") {
          sl.value = s_menuSfxVolume;
        }
      });
}

void MenuState::applyOptionsSliderSettings() {
  bool changed = false;
  auto &audioSettings = Audio::AudioSettings::Get();

  m_menuScene.GetWorld().each<Components::Tag, Components::UISlider>(
      [&](ECS::Entity, Components::Tag &tag, Components::UISlider &sl) {
        const float clamped = clampVolume(sl.value);
        if (tag.tag == "options_master_bar" && s_menuMasterVolume != clamped) {
          s_menuMasterVolume = clamped;
          GlobalAudioSettings::SetMasterVolume(clamped);
          audioSettings.master = clamped;
          changed = true;
        } else if (tag.tag == "options_bgm_bar" && s_menuBgmVolume != clamped) {
          s_menuBgmVolume = clamped;
          GlobalAudioSettings::SetBGMVolume(clamped);
          audioSettings.bgm = clamped;
          changed = true;
        } else if (tag.tag == "options_sfx_bar" && s_menuSfxVolume != clamped) {
          s_menuSfxVolume = clamped;
          GlobalAudioSettings::SetSFXVolume(clamped);
          audioSettings.sfx = clamped;
          changed = true;
        }
      });

  if (changed) {
    GlobalAudioSettings::Apply(m_audio);
    GlobalAudioSettings::Save();
    updateOptionsOverlayText();
  }
}
void MenuState::setMenuButtonsVisible(bool visible) {
  for (ECS::Entity entity : m_buttonEntities) {
    if (auto *sr =
            m_menuScene.GetWorld().getComponent<Components::SpriteRenderer>(entity)) {
      sr->visible = visible;
    }
    if (auto *tr =
            m_menuScene.GetWorld().getComponent<Components::TextRenderer>(entity)) {
      tr->visible = visible;
    }
    if (auto *btn =
            m_menuScene.GetWorld().getComponent<Components::UIButton>(entity)) {
      btn->interactable = visible;
    }
  }

  m_menuScene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        if (tag.tag == "TitleText" || tag.tag == "SubtitleText" || tag.tag == "btn_LoadLevel") {
          tr.visible = visible;
        }
      });
}

void MenuState::updateOptionsOverlayText() {
  const glm::vec3 normalColor{1.0f, 1.0f, 1.0f};
  const glm::vec3 selectedColor{1.0f, 0.8f, 0.0f};

  m_menuScene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        const std::string &entityTag = tag.tag;

        if (entityTag == "options_master_value") {
          tr.text  = std::to_string(static_cast<int>(s_menuMasterVolume * 100.0f)) + "%";
          tr.color = (m_optionsSelected == 0) ? selectedColor : normalColor;
        } else if (entityTag == "options_bgm_value") {
          tr.text  = std::to_string(static_cast<int>(s_menuBgmVolume * 100.0f)) + "%";
          tr.color = (m_optionsSelected == 1) ? selectedColor : normalColor;
        } else if (entityTag == "options_sfx_value") {
          tr.text  = std::to_string(static_cast<int>(s_menuSfxVolume * 100.0f)) + "%";
          tr.color = (m_optionsSelected == 2) ? selectedColor : normalColor;
        } else if (entityTag == "options_master_label") {
          tr.color = (m_optionsSelected == 0) ? selectedColor : normalColor;
        } else if (entityTag == "options_bgm_label") {
          tr.color = (m_optionsSelected == 1) ? selectedColor : normalColor;
        } else if (entityTag == "options_sfx_label") {
          tr.color = (m_optionsSelected == 2) ? selectedColor : normalColor;
        } else if (entityTag == "options_back_label") {
          tr.color = (m_optionsSelected == 3) ? selectedColor : normalColor;
        }
      });

  syncOptionsSlidersInScene();
}

void MenuState::openOptions() {
  m_showingOptions = true;
  m_optionsSelected = 0;
  syncOptionsSlidersInScene();
  updateOptionsOverlayText();
  setMenuButtonsVisible(false);
  setOverlayVisible("options_overlay", true);

  m_menuScene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        if (isOptionsEntityTag(tag.tag)) {
          tr.visible = true;
        }
      });

  m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::SpriteRenderer &sr) {
        if (isOptionsEntityTag(tag.tag)) {
          sr.visible = true;
        }
      });
}

void MenuState::closeOptions() {
  m_showingOptions = false;
  setOverlayVisible("options_overlay", false);
  setMenuButtonsVisible(true);

  m_menuScene.GetWorld().each<Components::Tag, Components::TextRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::TextRenderer &tr) {
        if (isOptionsEntityTag(tag.tag)) {
          tr.visible = false;
        }
      });

  m_menuScene.GetWorld().each<Components::Tag, Components::SpriteRenderer>(
      [&](ECS::Entity, Components::Tag &tag, Components::SpriteRenderer &sr) {
        if (isOptionsEntityTag(tag.tag)) {
          sr.visible = false;
        }
      });
}

void MenuState::setOptionVolume(int optionIndex, float value) {
  const float clampedValue = clampVolume(value);
  auto &audioSettings = Audio::AudioSettings::Get();

  if (optionIndex == 0) {
    if (s_menuMasterVolume == clampedValue) {
      return;
    }
    s_menuMasterVolume = clampedValue;
    GlobalAudioSettings::SetMasterVolume(s_menuMasterVolume);
    audioSettings.master = s_menuMasterVolume;
  } else if (optionIndex == 1) {
    if (s_menuBgmVolume == clampedValue) {
      return;
    }
    s_menuBgmVolume = clampedValue;
    GlobalAudioSettings::SetBGMVolume(s_menuBgmVolume);
    audioSettings.bgm = s_menuBgmVolume;
  } else if (optionIndex == 2) {
    if (s_menuSfxVolume == clampedValue) {
      return;
    }
    s_menuSfxVolume = clampedValue;
    GlobalAudioSettings::SetSFXVolume(s_menuSfxVolume);
    audioSettings.sfx = s_menuSfxVolume;
  } else {
    return;
  }

  GlobalAudioSettings::Apply(m_audio);
  GlobalAudioSettings::Save();
  updateOptionsOverlayText();
}

void MenuState::adjustOptionVolume(float delta) {
  auto stepped = [](float current, float d) {
    return std::round((current + d) * 10.0f) / 10.0f;
  };
  if (m_optionsSelected == 0) {
    setOptionVolume(0, stepped(s_menuMasterVolume, delta));
  } else if (m_optionsSelected == 1) {
    setOptionVolume(1, stepped(s_menuBgmVolume, delta));
  } else if (m_optionsSelected == 2) {
    setOptionVolume(2, stepped(s_menuSfxVolume, delta));
  }
}

void MenuState::renderVolumeBar(float centerX, float centerY, float width,
                                float height, float value, bool selected) {
  const float clampedValue = clampVolume(value);
  const float fillWidth = width * clampedValue;

  if (selected) {
    m_renderer->drawFilledRect(centerX, centerY, width + 28.0f, height + 20.0f,
                               0.95f, 0.72f, 0.22f);
  }

  m_renderer->drawFilledRect(centerX, centerY, width, height, 0.18f, 0.10f,
                             0.08f);

  const float fillCenterX = centerX - (width - fillWidth) * 0.5f;
  if (fillWidth > 0.0f) {
    m_renderer->drawFilledRect(fillCenterX, centerY, fillWidth, height - 12.0f,
                               0.87f, 0.26f, 0.16f);
  }
}

void MenuState::renderOptionsOverlay() {}


void MenuState::activateButton(const Components::UIButton &btn) {
  if (btn.uiScript.empty()) {
    std::cerr << "[MenuState] Button has no uiScript assigned\n";
    return;
  }
  Scripting::LuaManager &lua = Core::Engine::Get().lua();
  sol::state &ls = lua.GetState();
  ls["GSM"] = ls.create_table_with(
      "switch_to_gameplay", [&]() {
        m_gsm.switchTo(std::make_shared<GamePlayState>(m_gsm, m_input, m_window));
      },
      "quit", [&]() {
        Core::Engine::Get().events().publish(Core::QuitGameEvent());
      },
      "show_how_to_play", [&]() {
        m_showingHowToPlay = true;
        setMenuButtonsVisible(false);
        setOverlayVisible("howtoplay_overlay", true);
      },
      "open_options", [&]() {
        m_gsm.push(std::make_shared<OptionsState>(
            m_gsm, m_input, Core::Engine::Get().window(), m_audio));
      }
  );
  if (lua.ExecuteScript("assets/scripts/" + btn.uiScript))
    lua.CallFunction("on_click");
}

void MenuState::update(double /*dt*/) {
  int w = m_window.width();
  int h = m_window.height();

  if (m_showingHowToPlay) {
    auto& gpadHtp = Core::Engine::Get().gamepad();
    if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) ||
        m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
        m_input.wasKeyPressed(GLFW_KEY_SPACE) ||
        m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT) ||
        gpadHtp.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH) ||
        gpadHtp.isActionPressed("escape")) {
      m_showingHowToPlay = false;
      setOverlayVisible("howtoplay_overlay", false);
      setMenuButtonsVisible(true);
    }
    return;
  }

  if (m_showingOptions) {
    vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
    const bool mousePressed = m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool mouseDown = m_input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
    bool movedSelection = false;

    m_sliderSystem.update(m_menuScene.GetWorld(), m_input, *m_cam, (float)w,
                          (float)h);
    applyOptionsSliderSettings();

    auto getSpriteRectByTag = [this](const char *tagName, const RectBounds &fallback) {
      RectBounds rect = fallback;
      m_menuScene.GetWorld().each<Components::Tag, Components::Transform,
                                  Components::SpriteRenderer>(
          [&](ECS::Entity, Components::Tag &tag, Components::Transform &transform,
              Components::SpriteRenderer &sr) {
            if (tag.tag == tagName) {
              rect.centerX = transform.position.x;
              rect.centerY = transform.position.y;
              rect.width = sr.size.x;
              rect.height = sr.size.y;
            }
          });
      return rect;
    };

    auto getTextRectByTag = [this](const char *tagName, const RectBounds &fallback) {
      RectBounds rect = fallback;
      m_menuScene.GetWorld().each<Components::Tag, Components::Transform,
                                  Components::TextRenderer>(
          [&](ECS::Entity, Components::Tag &tag, Components::Transform &transform,
              Components::TextRenderer &tr) {
            if (tag.tag == tagName) {
              rect.centerX = transform.position.x;
              rect.centerY = transform.position.y;
              if (tr.boxSize.x > 0.0f) {
                rect.width = tr.boxSize.x;
              }
              if (tr.boxSize.y > 0.0f) {
                rect.height = tr.boxSize.y;
              }
            }
          });
      return rect;
    };

    const RectBounds masterRect = getSpriteRectByTag(
        "options_master_bar", RectBounds{255.0f, 200.0f, 520.0f, 40.0f});
    const RectBounds bgmRect = getSpriteRectByTag(
        "options_bgm_bar", RectBounds{255.0f, 55.0f, 520.0f, 40.0f});
    const RectBounds sfxRect = getSpriteRectByTag(
        "options_sfx_bar", RectBounds{255.0f, -90.0f, 520.0f, 40.0f});
    const RectBounds backRect = getTextRectByTag(
        "options_back_label", RectBounds{0.0f, -235.0f, 220.0f, 56.0f});

    int hoveredOption = -1;
    if (containsPoint(masterRect, mouseWorld)) {
      hoveredOption = 0;
    } else if (containsPoint(bgmRect, mouseWorld)) {
      hoveredOption = 1;
    } else if (containsPoint(sfxRect, mouseWorld)) {
      hoveredOption = 2;
    } else if (containsPoint(backRect, mouseWorld)) {
      hoveredOption = 3;
    }

    if (hoveredOption >= 0 && hoveredOption != m_optionsSelected) {
      m_optionsSelected = hoveredOption;
      Assets::SoundManager::Play("ui_move");
      movedSelection = true;
    }

    if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
      Assets::SoundManager::Play("ui_move");
      m_optionsSelected = (m_optionsSelected + 3) % 4;
      movedSelection = true;
    }
    if (m_input.wasKeyPressed(GLFW_KEY_DOWN) ||
        m_input.wasKeyPressed(GLFW_KEY_S)) {
      Assets::SoundManager::Play("ui_move");
      m_optionsSelected = (m_optionsSelected + 1) % 4;
      movedSelection = true;
    }
    auto& gp = Core::Engine::Get().gamepad();
    if (gp.isActionPressed("ui_up")) {
      Assets::SoundManager::Play("ui_move");
      m_optionsSelected = (m_optionsSelected + 3) % 4;
      movedSelection = true;
    }
    if (gp.isActionPressed("ui_down")) {
      Assets::SoundManager::Play("ui_move");
      m_optionsSelected = (m_optionsSelected + 1) % 4;
      movedSelection = true;
    }
    if (movedSelection) {
      updateOptionsOverlayText();
    }

    if (mousePressed && hoveredOption == 3) {
      Assets::SoundManager::Play("ui_select");
      closeOptions();
      return;
    }

    auto applyMouseSlider = [this, mouseWorld](const RectBounds &rect, int optionIndex) {
      const float left = rect.centerX - rect.width * 0.5f;
      const float normalized = clampVolume((mouseWorld.x - left) / rect.width);
      setOptionVolume(optionIndex, normalized);
    };

    if (mousePressed && hoveredOption >= 0 && hoveredOption <= 2) {
      Assets::SoundManager::Play("ui_move");
      if (hoveredOption == 0) {
        applyMouseSlider(masterRect, 0);
      } else if (hoveredOption == 1) {
        applyMouseSlider(bgmRect, 1);
      } else {
        applyMouseSlider(sfxRect, 2);
      }
    } else if (mouseDown) {
      if (containsPoint(masterRect, mouseWorld)) {
        applyMouseSlider(masterRect, 0);
      } else if (containsPoint(bgmRect, mouseWorld)) {
        applyMouseSlider(bgmRect, 1);
      } else if (containsPoint(sfxRect, mouseWorld)) {
        applyMouseSlider(sfxRect, 2);
      }
    }

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

    if (m_input.wasKeyPressed(GLFW_KEY_ENTER) ||
        m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) ||
        m_input.wasKeyPressed(GLFW_KEY_SPACE) ||
        gp.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
      if (m_optionsSelected == 3) {
        Assets::SoundManager::Play("ui_select");
        closeOptions();
      }
    }

    if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) ||
        gp.isActionPressed("escape")) {
      Assets::SoundManager::Play("ui_select");
      closeOptions();
    }

    return;
  }

  if (m_bgmChannel != 0 && m_bgmPaused) {
    m_audio.setPaused(m_bgmChannel, false);
    m_bgmPaused = false;
  }

  vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
  bool pressed = m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);

  int hoveredIndex = -1;
  for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
    ECS::Entity e = m_buttonEntities[i];
    auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(e);
    auto *tf = m_menuScene.GetWorld().getComponent<Components::Transform>(e);
    if (!btn || !tf || !btn->interactable) {
      continue;
    }

    float cx    = tf->position.x + btn->offset.x;
    float cy    = tf->position.y + btn->offset.y;
    float halfW = btn->size.x * 0.5f;
    float halfH = btn->size.y * 0.5f;
    bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                    mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
    if (hovered) {
      hoveredIndex = i;
      break;
    }
  }

  // Gamepad navigation
  auto& gp   = Core::Engine::Get().gamepad();
  bool gpUp      = gp.isActionPressed("ui_up");
  bool gpDown    = gp.isActionPressed("ui_down");
  bool gpConfirm = gp.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH);

  // Keyboard + gamepad navigation — selection persists across frames
  if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W) || gpUp) {
    Assets::SoundManager::Play("ui_move");
    if (--m_selected < 0) m_selected = (int)m_buttonEntities.size() - 1;
  }
  if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S) || gpDown) {
    Assets::SoundManager::Play("ui_move");
    if (++m_selected >= (int)m_buttonEntities.size()) m_selected = 0;
  }

  // Mouse hover — overrides keyboard/gamepad selection; selection persists when mouse moves away
  if (hoveredIndex >= 0) {
    if (m_selected != hoveredIndex) {
      m_selected = hoveredIndex;
      Assets::SoundManager::Play("ui_move");
    }
    if (pressed) {
      Assets::SoundManager::Play("ui_select");
      auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(
          m_buttonEntities[hoveredIndex]);
      if (btn) { activateButton(*btn); return; }
    }
  }

  // Confirm
  if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) || gpConfirm) {
    if (m_selected >= 0 && m_selected < (int)m_buttonEntities.size()) {
      Assets::SoundManager::Play("ui_select");
      auto *btn = m_menuScene.GetWorld().getComponent<Components::UIButton>(
          m_buttonEntities[m_selected]);
      if (btn) { activateButton(*btn); return; }
    }
  }

  applyHoverColors();

#ifdef _DEBUG
  if (m_input.wasKeyPressed(GLFW_KEY_P)) {
    m_debug = !m_debug;
  }
  if (m_input.wasKeyTriggered(GLFW_KEY_M)) {
    m_gsm.push(std::make_shared<Runtime::SpriteEditorState>(
        "SpriteEditor", m_input, m_gsm));
  }
#endif

  if (m_input.wasKeyTriggered(GLFW_KEY_V)) {
    m_bgmPaused = !m_bgmPaused;
    if (m_bgmChannel) {
      m_audio.setPaused(m_bgmChannel, m_bgmPaused);
    }
  }
}

void MenuState::render(Core::Time * /*timer*/) {
  int w = m_window.width();
  int h = m_window.height();
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glm::mat4 vp = m_cam->viewProj();

  m_menuScene.RenderAll(*m_renderer, vp);
  if (m_showingOptions) {
    m_renderer->beginScenePass(vp);
    renderOptionsOverlay();
    m_renderer->endScenePass();
    m_menuScene.RenderText(*m_renderer);
  }
}

void MenuState::renderImGui(Core::Time * /*timer*/) {
#ifdef _DEBUG
  if (!m_debug) {
    return;
  }
  ImGui::Begin("MenuState Debug");
  ImGui::Text("Buttons loaded: %d", (int)m_buttonEntities.size());
  ImGui::Text("Selected: %d", m_selected);
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::End();
#endif
}

void MenuState::onExit() {
  Core::Engine::Get().events().unsubscribeOwner(this);
  if (m_bgmChannel > 0) {
    m_audio.fadeOutChannel(m_bgmChannel, 0.5f);
  }
  m_bgmChannel = 0;
}

} // namespace Runtime
































