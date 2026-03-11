/******************************************************************************/
/*!
\file           AudioSettingsPanel.cpp
\project        ClockWorkz Engine
\author         [Amzar]
\brief          ImGui panel implementation for audio volume controls.
                Reads/writes Audio::AudioSettings and applies changes to
                Audio::AudioManager group volumes immediately.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "UI/AudioSettingsPanel.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/AudioSettings.hpp"
#include "IMGUI/imgui.h"

namespace Audio {

void AudioSettingsPanel::draw(bool& visible) {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(380.f, 180.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(200.f, 200.f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Settings##AudioSettings", &visible)) {
        ImGui::End();
        return;
    }

    AudioSettings& s   = AudioSettings::Get();
    AudioManager&  mgr = AudioManager::get();

    ImGui::SeparatorText("Volume");

    if (ImGui::SliderFloat("Master##vol", &s.master, 0.f, 1.f, "%.2f")) {
        mgr.setGroupVolume(Group::Master, s.master);
    }
    if (ImGui::SliderFloat("BGM##vol",    &s.bgm,    0.f, 1.f, "%.2f")) {
        mgr.setGroupVolume(Group::BGM, s.bgm);
    }
    if (ImGui::SliderFloat("SFX##vol",    &s.sfx,    0.f, 1.f, "%.2f")) {
        mgr.setGroupVolume(Group::SFX, s.sfx);
    }

    ImGui::End();
}

} // namespace Audio
