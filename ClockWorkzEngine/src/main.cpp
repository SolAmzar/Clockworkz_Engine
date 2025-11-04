#include "Core/Engine.hpp"
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

	// 2️ Create and run the engine (registers itself via Engine::Get)
	Core::Engine engine;
	return engine.Run();
}
