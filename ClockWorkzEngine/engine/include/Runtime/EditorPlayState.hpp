/******************************************************************************/
/*!
\file           EditorPlayState.hpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Editor play-mode state.
                Loaded by EditorState when the Play button is pressed.
                Runs the scene with all engine systems active (no editor UI).
                "Stop" returns to EditorState via Engine::editorFactory().

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include <string>

namespace Platform { class InputManager; }
namespace Core    { class Time; }
namespace Runtime { class GameStateManager; }

namespace Runtime {

class EditorPlayState final : public GameState {
public:
    EditorPlayState(Platform::InputManager& input, GameStateManager& gsm,
                    std::string scenePath);

    const char* name() const override { return "EditorPlay"; }

    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    Platform::InputManager& m_input;
    GameStateManager&       m_gsm;
    std::string             m_sceneFilePath;
    Scene                   m_scene{"EditorPlay"};
    bool                    m_pendingStop{false};
};

} // namespace Runtime
