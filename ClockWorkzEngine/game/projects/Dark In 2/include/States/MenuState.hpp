#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <vector>
#include <string>

namespace Platform { class InputManager; class WindowManager; }
namespace Runtime  { class GameStateManager; }
namespace Audio    { class AudioManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class MenuState final : public GameState {
public:
    MenuState(GameStateManager& gsm, Platform::InputManager& input,
              Platform::WindowManager& window, Audio::AudioManager& audio);

    const char* name() const override { return "MenuState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&        m_gsm;
    Platform::InputManager&  m_input;
    Platform::WindowManager& m_window;
    Audio::AudioManager&     m_audio;

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_menuScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{0};
};

} // namespace Runtime
