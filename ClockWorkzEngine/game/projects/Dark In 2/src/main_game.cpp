/******************************************************************************/
/*!
\file           main_game.cpp
\project        Dark In 2
\author(s)      [Syafiq] 100%
\brief          Defines the Dark In 2 game entry point.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Core/Engine.hpp"
#include "States/MenuState.hpp"
#include <memory>
#include <iostream>

int main()
{
    std::cout << "[Main] Booting Dark In 2...\n";
    Core::Engine engine;

    engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
        return std::make_shared<Runtime::MenuState>(e.gsm(), e.input(), e.window(), e.audio());
    });

    return engine.Run();
}
