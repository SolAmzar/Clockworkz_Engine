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
