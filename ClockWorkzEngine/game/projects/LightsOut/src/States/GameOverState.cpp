/******************************************************************************/
/*!
\file           GameOverState.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Lose screen implementation. Loads LoseScreen.json and dispatches
                UIButton clicks via Lua scripts (btn_retry, btn_main_menu).
                R key shortcuts to retry without needing a button click.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "States/GameOverState.hpp"
#include "States/GamePlayState.hpp"
#include "States/MenuState.hpp"
#include "Scripting/LuaManager.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Assets/SoundManager.hpp"
#include "Assets/TextureManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
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
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

using Assets::ResourceManager;
using Graphics::Shader;

namespace Runtime {

GameOverState::GameOverState(GameStateManager& gsm,
                             Platform::InputManager& input,
                             const std::string& currentLevelPath)
    : m_gsm(gsm), m_input(input), m_currentLevelPath(currentLevelPath) {}

void GameOverState::onEnter() {
    std::cout << "[GameOverState] Entered — level to retry: " << m_currentLevelPath << "\n";
    m_selected = -1;
    m_buttonEntities.clear();

    Core::Engine& engine = Core::Engine::Get();
    m_renderer = &engine.rendering();
    m_cam      = &engine.camera();

    Assets::FontManager::LoadFromConfig("assets/configs/fonts.json");
    ResourceManager<Shader>::Load("text",    "assets/shaders/text.vert",    "assets/shaders/text.frag");
    ResourceManager<Shader>::Load("texture", "assets/shaders/texture.vert", "assets/shaders/texture.frag");
    Assets::TextureManager::LoadFromConfig("assets/configs/Textures.json");

    m_loseScene.Init();
    m_loseScene.LoadFromFile("assets/scenes/LoseScreen.json");

    collectButtons();
    updateLayout(engine.window().width(), engine.window().height());

    engine.events().subscribe<Core::WindowResizeEvent>(
        this, [this](const Core::WindowResizeEvent& e) {
            if (e.width > 0 && e.height > 0)
                updateLayout(e.width, e.height);
        });
}

void GameOverState::updateLayout(int w, int h) {
    float zoom = std::min((float)w / 1920.f, (float)h / 1080.f);
    m_cam->setViewport(w, h);
    m_cam->setPosition({0.f, 0.f});
    m_cam->setZoom(zoom);
    glViewport(0, 0, w, h);
}

void GameOverState::collectButtons() {
    m_buttonEntities.clear();
    m_loseScene.GetWorld().each<Components::UIButton>(
        [this](ECS::Entity e, Components::UIButton&) {
            m_buttonEntities.push_back(e);
        });
}

void GameOverState::applyHoverColors() {
    for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
        ECS::Entity e    = m_buttonEntities[i];
        auto* btn    = m_loseScene.GetWorld().getComponent<Components::UIButton>(e);
        auto* sprite = m_loseScene.GetWorld().getComponent<Components::SpriteRenderer>(e);
        auto* text   = m_loseScene.GetWorld().getComponent<Components::TextRenderer>(e);
        if (!btn) continue;
        glm::vec3 c = (m_selected == i) ? btn->hoverColor : btn->normalColor;
        if (text)   text->color   = c;
        if (sprite && !btn->showBackground) sprite->visible = false;
    }
}

void GameOverState::handleButton(const Components::UIButton& btn) {
    if (!btn.uiScript.empty()) {
        Scripting::LuaManager& lua = Core::Engine::Get().lua();
        sol::state& ls = lua.GetState();
        auto& window = Core::Engine::Get().window();
        auto& audio  = Core::Engine::Get().audio();
        ls["GSM"] = ls.create_table_with(
            "retry", [&]() {
                m_gsm.switchToAndClear(std::make_shared<GamePlayState>(
                    m_gsm, m_input, window, m_currentLevelPath));
            },
            "switch_to_menu", [&]() {
                m_gsm.switchToAndClear(std::make_shared<MenuState>(
                    m_gsm, m_input, window, audio));
            }
        );
        if (lua.ExecuteScript("assets/scripts/" + btn.uiScript))
            lua.CallFunction("on_click");
    }
}

void GameOverState::update(double /*dt*/) {
    Core::Engine& engine = Core::Engine::Get();
    int w = engine.window().width();
    int h = engine.window().height();

    // R key shortcut — retry without needing a button click
    if (m_input.wasKeyPressed(GLFW_KEY_R)) {
        m_gsm.switchToAndClear(std::make_shared<GamePlayState>(
            m_gsm, m_input, engine.window(), m_currentLevelPath));
        return;
    }
    // ESC / M — return to main menu
    if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) || m_input.wasKeyPressed(GLFW_KEY_M) ||
        Core::Engine::Get().gamepad().isActionPressed("escape")) {
        m_gsm.switchToAndClear(std::make_shared<MenuState>(
            m_gsm, m_input, engine.window(), engine.audio()));
        return;
    }

    vec2 mouseWorld = m_input.getMouseWorldPos(*m_cam, (float)w, (float)h);
    bool clicked    = m_input.wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT);

    for (int i = 0; i < (int)m_buttonEntities.size(); ++i) {
        ECS::Entity e  = m_buttonEntities[i];
        auto* btn = m_loseScene.GetWorld().getComponent<Components::UIButton>(e);
        auto* tf  = m_loseScene.GetWorld().getComponent<Components::Transform>(e);
        if (!btn || !tf || !btn->interactable) continue;

        float cx    = tf->position.x + btn->offset.x;
        float cy    = tf->position.y + btn->offset.y;
        float halfW = btn->size.x * 0.5f;
        float halfH = btn->size.y * 0.5f;
        bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                        mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);
        if (hovered) {
            if (m_selected != i) {
                m_selected = i;
                Assets::SoundManager::Play("ui_move");
            }
            if (clicked) {
                Assets::SoundManager::Play("ui_select");
                handleButton(*btn);
                return;
            }
        }
    }
    // Gamepad navigation
    auto& gp   = Core::Engine::Get().gamepad();
    bool gpLeft    = gp.isActionPressed("ui_left");
    bool gpRight   = gp.isActionPressed("ui_right");
    bool gpConfirm = gp.wasButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH);

    // Keyboard + gamepad navigation — selection persists across frames
    if (m_input.wasKeyPressed(GLFW_KEY_LEFT) || m_input.wasKeyPressed(GLFW_KEY_A) || gpLeft) {
        Assets::SoundManager::Play("ui_move");
        if (--m_selected < 0) m_selected = (int)m_buttonEntities.size() - 1;
    }
    if (m_input.wasKeyPressed(GLFW_KEY_RIGHT) || m_input.wasKeyPressed(GLFW_KEY_D) || gpRight) {
        Assets::SoundManager::Play("ui_move");
        if (++m_selected >= (int)m_buttonEntities.size()) m_selected = 0;
    }

    // Confirm
    if (m_input.wasKeyPressed(GLFW_KEY_ENTER) || m_input.wasKeyPressed(GLFW_KEY_KP_ENTER) || gpConfirm) {
        if (m_selected >= 0 && m_selected < (int)m_buttonEntities.size()) {
            Assets::SoundManager::Play("ui_select");
            auto* btn = m_loseScene.GetWorld().getComponent<Components::UIButton>(
                m_buttonEntities[m_selected]);
            if (btn) { handleButton(*btn); return; }
        }
    }

    applyHoverColors();
}

void GameOverState::render(Core::Time* /*timer*/) {
    int w = (int)Core::Engine::Get().window().width();
    int h = (int)Core::Engine::Get().window().height();
    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_loseScene.RenderAll(*m_renderer, m_cam->viewProj());
}

void GameOverState::onExit() {
    Core::Engine::Get().events().unsubscribeOwner(this);
    m_loseScene.Clear();
    std::cout << "[GameOverState] Exited\n";
}

} // namespace Runtime
