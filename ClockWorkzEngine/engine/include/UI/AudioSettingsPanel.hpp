/******************************************************************************/
/*!
\file           AudioSettingsPanel.hpp
\project        ClockWorkz Engine
\author         [Amzar]
\brief          Engine-level ImGui panel for audio volume controls.
                Call Audio::AudioSettingsPanel::draw(visible) between
                ImGui::NewFrame() and ImGui::Render() — e.g. inside a
                GameState::renderImGui() implementation.

                Follows the same static-draw pattern as Memory::MemoryDebugPanel.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once

#ifdef _DEBUG

namespace Audio {

class AudioSettingsPanel {
public:
    /*!
    \brief  Draw the Audio Settings panel with Master / BGM / SFX sliders.

            Must be called between ImGui::NewFrame() and ImGui::Render()
            each frame.  Pass a bool that controls visibility; the window's
            built-in close button will set it to false when dismissed.
    */
    static void draw(bool& visible);
};

} // namespace Audio

#endif // _DEBUG
