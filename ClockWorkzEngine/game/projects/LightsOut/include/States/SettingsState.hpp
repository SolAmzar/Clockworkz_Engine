/******************************************************************************/
/*!
\file           SettingsState.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Settings menu state — pushed on top of MenuState or PauseState.
                Loads SettingsMenu.json and renders audio sliders using the
                engine's in-game UISliderSystem (no ImGui). Pressing BACK or
                ESC pops this state and returns to the previous state.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "Systems/UISliderSystem.hpp"
#include "ECS/World.hpp"
#include <glm/vec2.hpp>
#include <string>
#include <vector>

namespace Platform {
class InputManager;
class WindowManager;
} // namespace Platform
namespace Runtime {
class GameStateManager;
}
namespace Audio {
class AudioManager;
}
namespace Graphics {
class RenderingManager;
class Camera2D;
} // namespace Graphics

namespace Runtime {

class SettingsState final : public GameState {
public:
    SettingsState(GameStateManager       &gsm,
                  Platform::InputManager &input,
                  Platform::WindowManager &window,
                  Audio::AudioManager    &audio);

    const char *name() const override { return "SettingsState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt)          override;
    void render(Core::Time *timer)  override;
    void renderImGui(Core::Time *)  override {}

private:
    void updateLayout(int w, int h);
    void handleAction(const std::string &action);
    void collectButtons();
    void applyHoverColors();
    void syncSlidersFromSettings();

    GameStateManager        &m_gsm;
    Platform::InputManager  &m_input;
    Platform::WindowManager &m_window;
    Audio::AudioManager     &m_audio;

    Graphics::RenderingManager *m_renderer{nullptr};
    Graphics::Camera2D         *m_cam{nullptr};

    Runtime::Scene            m_scene;
    Systems::UISliderSystem   m_sliderSystem;

    std::vector<ECS::Entity>  m_buttonEntities;
    int                       m_selected{0};

    bool m_debug{false};
};

} // namespace Runtime
