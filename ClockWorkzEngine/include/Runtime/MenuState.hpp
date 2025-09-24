#pragma once
#include "Runtime/GameState.hpp"
#include <vector>
#include <string>
#include <functional>

namespace Platform { class InputManager; }
namespace Runtime { class GameStateManager; }

namespace Runtime {

    class MenuState final : public GameState {
    public:
        // quitFn is called when "Exit Game" is selected (Engine sets a flag / stops loop)
        MenuState(GameStateManager& gsm, Platform::InputManager& input, std::function<void()> quitFn);

        const char* name() const override{ return "MenuState"; }
        void onEnter() override;
        void onExit()  override {}
        void update(double dt) override;
        void render() override {}

    private:
        void printMenu();
        void moveUp();
        void moveDown();
        void select();

        GameStateManager& m_gsm;
        Platform::InputManager& m_input;
        std::function<void()>   m_quitFn;

        std::vector<std::string> m_items{
          "Start Game",
          "Load Game",
          "Option",
          "Credits",
          "Exit Game"
        };

        int  m_selected{ 0 };
        bool m_dirty{ true };
    };

} // namespace Runtime
