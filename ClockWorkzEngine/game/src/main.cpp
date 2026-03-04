#include "Core/Engine.hpp"
#include "States/EditorHubState.hpp"
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

	// Engine always boots to the EditorHubState (Unity-style project launcher).
	// From the hub, open "Lights Out" or create new projects.
	engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorHubState>(e.gsm(), e.input());
	});

	return engine.Run();
}
