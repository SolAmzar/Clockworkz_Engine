#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <glm/vec2.hpp>
#include <vector>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class PauseState final : public GameState {
public:
    PauseState(GameStateManager& gsm, Platform::InputManager& input);

    const char* name() const override { return "PauseState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_pauseScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{0};

    // Save/restore play camera so the game resumes seamlessly
    glm::vec2 m_savedCamPos{0.f, 0.f};
    float     m_savedCamZoom{1.f};
    int       m_savedViewW{800};
    int       m_savedViewH{600};
};

} // namespace Runtime
