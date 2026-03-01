#include "Core/Engine.hpp"
#include "States/MenuState.hpp"
#include "States/SandboxState.hpp"
#include <iostream>
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#ifndef DBG_NEW
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DBG_NEW
#endif
#endif

int main()
{
	std::cout << "[Main] Booting ClockWorkz Engine...\n";
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Core::Engine engine;

	// Register a factory that is called inside Run() after init() completes,
	// so that config and all subsystems are available when the state is created.
	engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
		if (e.startState() == "Sandbox")
			return std::make_shared<Runtime::SandboxState>(e.input(), e.gsm());
		return std::make_shared<Runtime::MenuState>(e.gsm(), e.input(), e.window(), e.audio());
	});

	return engine.Run();
}
