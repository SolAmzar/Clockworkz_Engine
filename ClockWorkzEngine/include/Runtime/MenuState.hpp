/******************************************************************************/
/*!
\file           MenuState.hpp
\project        Lights Out
\author(s)      [Lebon], 70%, [Amirul Syafiq] 30%
\brief          Declares MenuState, the state representing the game’s main menu.
                Responsibilities include:
                - Displaying menu options such as Start, Options, Credits, Exit
                - Handling keyboard and mouse input for menu navigation
                - Triggering state transitions (e.g., moving from menu to PlayState)
                - Rendering UI text using FontManager and Shader resources

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include <vector>
#include <string>
#include <functional>
#include <memory>
namespace ClockWorkz::Assets { class FontManager; }

namespace Platform { class InputManager; class WindowManager; }
namespace Runtime { class GameStateManager; }
namespace Audio { class AudioManager; }
namespace Core { class Time; }  // forward declare

namespace Runtime {

    class MenuState final : public GameState {
    public:
        // quitFn is called when "Exit Game" is selected (Engine sets a flag / stops loop)
        MenuState(GameStateManager& gsm, Platform::InputManager& input, Platform::WindowManager& window, Audio::AudioManager& audio, std::function<void()> quitFn);                

        const char* name() const override{ return "MenuState"; }
        void onEnter() override;
        void onExit()  override;
        void update(double dt) override;
        void render(Core::Time* timer) override;


    private:
        void printMenu();
        void moveUp();
        void moveDown();
        void select();

        GameStateManager& m_gsm;
        Platform::InputManager& m_input;
        Platform::WindowManager& m_window;
        Audio::AudioManager& m_audio;
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
