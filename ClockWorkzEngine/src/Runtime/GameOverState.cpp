/******************************************************************************/
/*!
\file           GameOverState.cpp
\project        Lights Out
\author(s)      Syafiq 100%
\brief          Implements GameOverState, the game over screen.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/GameOverState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Runtime/PlayState.hpp"
#include "Runtime/MenuState.hpp"
#include "Platform/InputManager.hpp"
#include "Core/Engine.hpp"
#include "Platform/WindowManager.hpp"
#include "IMGUI/imgui.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace Runtime {

    GameOverState::GameOverState(GameStateManager& gsm, Platform::InputManager& input, const std::string& sceneId)
        : m_gsm(gsm)
        , m_input(input)
        , m_sceneId(sceneId)
        , m_fadeAlpha(0.0f)
        , m_restartRequested(false)
        , m_menuRequested(false)
    {
    }

    GameOverState::~GameOverState() {
    }

    const char* GameOverState::name() const {
        return "GameOverState";
    }

    void GameOverState::onEnter() {
        std::cout << "[GameOverState] Entered\n";
        m_fadeAlpha = 0.0f;
        m_restartRequested = false;
        m_menuRequested = false;
    }

    void GameOverState::onExit() {
        std::cout << "[GameOverState] Exited\n";
    }

    void GameOverState::update(double dt) {
        // Fade in effect
        if (m_fadeAlpha < 1.0f) {
            m_fadeAlpha += static_cast<float>(dt * 2.0f);
            if (m_fadeAlpha > 1.0f) m_fadeAlpha = 1.0f;
        }

        // Handle input
        if (m_input.wasKeyPressed(GLFW_KEY_R) || m_input.wasKeyPressed(GLFW_KEY_ENTER)) {
            std::cout << "[GameOverState] Restart requested\n";
            m_restartRequested = true;
        }

        if (m_input.wasKeyPressed(GLFW_KEY_ESCAPE) || m_input.wasKeyPressed(GLFW_KEY_M)) {
            std::cout << "[GameOverState] Return to menu requested\n";
            m_menuRequested = true;
        }

        // Process state transitions
        if (m_restartRequested) {
            m_gsm.pop(); // Remove GameOverState
            m_gsm.pop(); // Remove PlayState
            m_gsm.push(std::make_shared<PlayState>(m_sceneId, m_input, m_gsm));
            return;
        }
        else if (m_menuRequested) {
            m_gsm.pop(); // Remove GameOverState
            m_gsm.pop(); // Remove PlayState
            return;
        }
    }

    void GameOverState::render(Core::Time* timer) {
        // The PlayState underneath will still render the game world
        // We just overlay the game over UI in renderImGui
    }

    void GameOverState::renderImGui(Core::Time* timer) {
        Core::Engine& engine = Core::Engine::Get();
        auto& window = engine.window();

        // Get window dimensions
        float windowWidth = static_cast<float>(window.width());
        float windowHeight = static_cast<float>(window.height());

        // Create a fullscreen semi-transparent overlay
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
        ImGui::Begin("GameOverOverlay", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground);

        // Draw dark overlay
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 overlayColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f * m_fadeAlpha));
        drawList->AddRectFilled(ImVec2(0, 0), ImVec2(windowWidth, windowHeight), overlayColor);

        ImGui::End();

        // Create game over text window
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // Transparent background
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));   // No border

        ImGui::SetNextWindowPos(ImVec2(windowWidth * 0.5f - 200, windowHeight * 0.35f));
        ImGui::SetNextWindowSize(ImVec2(400, 200));
        ImGui::Begin("GameOverText", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar);

        // "GAME OVER" text in red
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, m_fadeAlpha));
        ImGui::SetWindowFontScale(3.0f);
        ImGui::Text("GAME OVER");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Instructions in white
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, m_fadeAlpha * 0.9f));
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Press [R] or [ENTER] to Restart");
        ImGui::Text("Press [ESC] or [M] for Menu");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleColor(2); // Pop the window style colors
    }

} // namespace Runtime