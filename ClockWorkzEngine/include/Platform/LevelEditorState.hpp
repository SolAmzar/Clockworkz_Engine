/******************************************************************************/
/*!
\file           LevelEditorState.hpp
\project        Lights Out - Level Editor
\author(s)      Yusuf,[100%]
\brief          Game state for the level editor, integrating it into the
                game's state machine. Compatible with existing GameState interface.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once

#include "Runtime/GameState.hpp"
#include <memory>

namespace Platform {
    class LevelEditor;
    class InputManager;
    class WindowManager;
}

namespace Core {
    class Time;
}

namespace Graphics {
    class RenderingManager;
    class ImGuiManager;
}

namespace Runtime {
    class GameStateManager;

    /******************************************************************************/
    /*!
    \class  EditorState
    \brief  Game state that runs the level editor
    */
    /******************************************************************************/
    class EditorState : public GameState {
    public:
        // Constructor matching PlayState pattern
        explicit EditorState(Platform::InputManager& input, GameStateManager& gsm);
        ~EditorState() override;

        // GameState interface (matching your existing GameState.hpp)
        void onEnter() override;
        void onExit() override;

        void update(double dt) override;
        void render(Core::Time* timer) override;
        void renderImGui(Core::Time* timer) override;

        // State name
        const char* name() const override { return "EditorState"; }

        // Access to editor
        Platform::LevelEditor* getEditor() { return m_editor.get(); }

    private:
        std::unique_ptr<Platform::LevelEditor> m_editor;

        // System references (stored as references like PlayState)
        Platform::InputManager& m_input;
        GameStateManager& m_gsm;

        Graphics::RenderingManager* m_renderer;
        Platform::WindowManager* m_windowManager;
        Graphics::ImGuiManager* m_imguiManager;

        bool m_initialized;
    };

} // namespace Runtime