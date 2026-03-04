#include "Core/Engine.hpp"
#include "Runtime/EditorHubState.hpp"
#include "Runtime/EditorState.hpp"
#include "Runtime/EditorPlayState.hpp"
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

	// Editor factory: called by EditorHubState when opening a project.
	engine.setEditorFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorState>(engine.input(), engine.gsm(), scenePath);
	});

	// Play factory: called by EditorState when Play is pressed.
	// Creates EditorPlayState — runs the scene with all systems active.
	// Stop button returns to EditorState via Engine::editorFactory().
	engine.setPlayFactory([&engine](const std::string& scenePath) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorPlayState>(engine.input(), engine.gsm(), scenePath);
	});

	// Engine always boots to the EditorHubState (Unity-style project launcher).
	engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
		return std::make_shared<Runtime::EditorHubState>(e.gsm(), e.input());
	});

	return engine.Run();
}
