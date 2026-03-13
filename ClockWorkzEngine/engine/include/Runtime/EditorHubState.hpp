/******************************************************************************/
/*!
\file           EditorHubState.hpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          EditorHubState: Unity-style project launcher hub.
                Engine entry point. Lists projects, creates new ones, and
                opens them in the registered editor via Core::Engine::editorFactory().

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Platform { class InputManager; }
namespace Core    { class Time; }
namespace Runtime { class GameStateManager; }

namespace Runtime {

class EditorHubState final : public GameState {
public:
    EditorHubState(GameStateManager& gsm, Platform::InputManager& input);

    const char* name() const override { return "EditorHub"; }

    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    struct ProjectEntry {
        std::string name;
        std::string path;         // project root, relative to getSourceRoot()
        std::string initialScene; // scene path, relative to project root
        std::string description;
    };

    void loadRegistry();
    void saveRegistry();
    void openProject(const ProjectEntry& project);
    void createNewProject(const std::string& projectName, const std::string& description);
    void triggerProjectBuild(const std::string& projectName);
    std::string resolveAbsoluteScenePath(const ProjectEntry& project) const;
    static std::string getSourceRoot();

    GameStateManager&         m_gsm;
    Platform::InputManager&   m_input;
    std::vector<ProjectEntry> m_projects;
    int                       m_selectedIdx{-1};
    char                      m_newProjectNameBuf[128]{};
    char                      m_newProjectDescBuf[512]{};
    bool                      m_showErrorPopup{false};
    std::string               m_errorMessage;

    // NOTE: Deferred project open: set in renderImGui(),
    // consumed in update(),  to avoid switching state mid-ImGui-frame.
    int                       m_pendingOpenIdx{-1};

    // Right-click edit/delete state
    int                       m_editingIdx{-1};
    char                      m_editNameBuf[128]{};
    char                      m_editDescBuf[512]{};
    int                       m_deletingIdx{-1};

    // Background cmake build state
    std::thread               m_buildThread;
    std::atomic<bool>         m_buildInProgress{false};
    std::mutex                m_buildStatusMutex;
    std::string               m_buildStatus;
};

} // namespace Runtime
