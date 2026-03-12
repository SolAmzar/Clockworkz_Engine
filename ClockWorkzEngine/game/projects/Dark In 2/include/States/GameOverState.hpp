#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <string>
#include <vector>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class GameOverState final : public GameState {
public:
    GameOverState(GameStateManager& gsm, Platform::InputManager& input,
                  const std::string& currentLevelPath);

    const char* name() const override { return "GameOverState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* /*timer*/) override {}

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;
    std::string             m_currentLevelPath; // level to reload on retry

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_loseScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{-1};
};

} // namespace Runtime
