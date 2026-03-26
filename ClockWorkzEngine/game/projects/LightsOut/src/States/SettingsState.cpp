/******************************************************************************/
/*!
\file           SettingsState.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Settings menu state. Loads SettingsMenu.json and renders
                interactive audio sliders using the in-game UISliderSystem.
                Pushed on the stack from MenuState ("options") or
                PauseState ("settings"); pops itself on BACK / ESC.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "States/SettingsState.hpp"
#include "Scripting/LuaManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/AudioSettings.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"
#include "Platform/GamepadManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/SceneSerializer.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <iostream>

using Assets::ResourceManager;
using Graphics::Shader;

namespace Runtime {

SettingsState::SettingsState(GameStateManager        &gsm,
                             Platform::InputManager  &input,
                             Platform::WindowManager &window,
                             Audio::AudioManager     &audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void SettingsState::onEnter() {
    m_selected = 0;
    m_buttonEntities.clear();

    Core::Engine &engine = Core::Engine::Get();
    m_renderer = &engine.rendering();
    m_cam      = &engine.camera();

    Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");
    ResourceManager<Shader>::Load("text",    "assets/shaders/text.vert",    "assets/shaders/text.frag");
    ResourceManager<Shader>::Load("texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
    Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");

    m_scene.Init();
    m_scene.LoadFromFile("assets/scenes/SettingsMenu.json");

    // Sync current AudioSettings values into slider components so they open
    // showing the actual current volume, not the default 1.0.
    syncSlidersFromSettings();

    collectButtons();
    updateLayout(m_window.width(), m_window.height());

    engine.events().subscribe<Core::WindowResizeEvent>(
        this, [this](const Core::WindowResizeEvent &e) {
            if (e.width > 0 && e.height > 0)
                updateLayout(e.width, e.height);
        });
}

void SettingsState::syncSlidersFromSettings() {
    const Audio::AudioSettings &s = Audio::AudioSettings::Get();
    m_scene.GetWorld().each<Components::UISlider>(
        [&](ECS::Entity, Components::UISlider &sl) {
            if      (sl.settingKey == "master") sl.value = s.master;
            else if (sl.settingKey == "bgm")    sl.value = s.bgm;
            else if (sl.settingKey == "sfx")    sl.value = s.sfx;
        });
}

void SettingsState::updateLayout(int w, int h) {
    float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
    m_cam->setViewport(w, h);
    m_cam->setPosition({0.f, 0.f});
    m_cam->setZoom(zoom);
    glViewport(0, 0, w, h);
}

void SettingsState::collectButtons() {
    m_buttonEntities.clear();
    m_scene.GetWorld().each<Components::UIButton>(
        [this](ECS::Entity e, Components::UIButton &) {
            m_buttonEntities.push_back(e);
        });
}

void SettingsState::applyHoverColors() {
    for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
        ECS::Entity e      = m_buttonEntities[i];
        auto *btn    = m_scene.GetWorld().getComponent<Components::UIButton>(e);
        auto *sprite = m_scene.GetWorld().getComponent<Components::SpriteRenderer>(e);
        auto *text   = m_scene.GetWorld().getComponent<Components::TextRenderer>(e);
        if (!btn) continue;
        bool selected = (m_selected >= 0 && i == m_selected);
        glm::vec3 c = selected ? btn->hoverColor : btn->normalColor;
        if (text) text->color = c;                                        // tint text only
        if (sprite && !btn->showBackground) sprite->visible = false;     // hide background if flagged off
    }
}

void SettingsState::handleButton(const Components::UIButton &btn) {
    if (!btn.uiScript.empty()) {
        Scripting::LuaManager &lua = Core::Engine::Get().lua();
        sol::state &ls = lua.GetState();
        ls["GSM"] = ls.create_table_with(
            "pop", [&]() { m_gsm.pop(); }
        );
        if (lua.ExecuteScript("assets/scripts/" + btn.uiScript)) {
            lua.CallFunction("on_click");
        }
    }
}

void SettingsState::update(double /*dt*/) {
    int w = m_window.width();
    int h = m_window.height();

    // ESC always goes back
    if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) ||
        Core::Engine::Get().gamepad().isActionPressed("escape")) {
        m_gsm.pop();
        return;
    }

    // Update sliders (mouse drag)
    m_sliderSystem.update(m_scene.GetWorld(), m_input, *m_cam, (float)w, (float)h);

    // Button input (mouse)
    vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
    bool clicked    = m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);
    bool mouseHover = false;

    for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
        ECS::Entity e = m_buttonEntities[i];
        auto *btn = m_scene.GetWorld().getComponent<Components::UIButton>(e);
        auto *tf  = m_scene.GetWorld().getComponent<Components::Transform>(e);
        if (!btn || !tf || !btn->interactable) continue;

        float cx = tf->position.x + btn->offset.x;
        float cy = tf->position.y + btn->offset.y;
        float halfW = btn->size.x * 0.5f;
        float halfH = btn->size.y * 0.5f;
        bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                        mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
        if (hovered) {
            mouseHover = true;
            m_selected = i;
            if (clicked) {
                handleButton(*btn);
                return;
            }
        }
    }

    // Deselect when mouse is not over any button
    if (!mouseHover) m_selected = -1;

    // Keyboard nav for the back button
    if (m_input.wasKeyPressed(GLFW_KEY_UP) || m_input.wasKeyPressed(GLFW_KEY_W)) {
        if (--m_selected < 0) m_selected = (int)m_buttonEntities.size() - 1;
    }
    if (m_input.wasKeyPressed(GLFW_KEY_DOWN) || m_input.wasKeyPressed(GLFW_KEY_S)) {
        if (++m_selected >= (int)m_buttonEntities.size()) m_selected = 0;
    }
    if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER)) {
        if (!m_buttonEntities.empty()) {
            auto *btn = m_scene.GetWorld().getComponent<Components::UIButton>(
                m_buttonEntities[m_selected]);
            if (btn) { handleButton(*btn); return; }
        }
    }

    applyHoverColors();
}

void SettingsState::render(Core::Time * /*timer*/) {
    int w = m_window.width();
    int h = m_window.height();
    glViewport(0, 0, w, h);
    glClearColor(0.05f, 0.05f, 0.05f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::mat4 vp = m_cam->viewProj();

    m_scene.RenderAll(*m_renderer, vp);
}

void SettingsState::onExit() {
    Core::Engine::Get().events().unsubscribeOwner(this);
    m_scene.Clear();
    std::cout << "[SettingsState] Exiting\n";
}

} // namespace Runtime
