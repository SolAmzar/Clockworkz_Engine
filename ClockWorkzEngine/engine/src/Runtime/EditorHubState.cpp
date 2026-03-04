/******************************************************************************/
/*!
\file           EditorHubState.cpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Engine-level project launcher hub.
                Opens projects via Core::Engine::editorFactory() so engine
                code stays free of game-specific state types.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Runtime/EditorHubState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Core/Time.hpp"
#include "IMGUI/imgui.h"

#pragma warning(push, 0)
#include "Core/json.hpp"
#pragma warning(pop)

#include <GL/glew.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Runtime {

static std::string getSourceRootImpl() {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(__FILE__).parent_path();
    for (int i = 0; i < 10 && !dir.empty(); ++i) {
        // Engine file lives in engine/src/Runtime/ — walk up to ClockWorkzEngine/
        // and return the game/ subfolder as the project root.
        if (fs::exists(dir / "game" / "CMakeLists.txt") && fs::exists(dir / "game" / "assets"))
            return (dir / "game").generic_string();
        // Fallback: file is already inside game/ (e.g. during migration)
        if (fs::exists(dir / "CMakeLists.txt") && fs::exists(dir / "assets"))
            return dir.generic_string();
        dir = dir.parent_path();
    }
    return "";
}

// ---------------------------------------------------------------------------
// Constructor / lifecycle
// ---------------------------------------------------------------------------

EditorHubState::EditorHubState(GameStateManager& gsm, Platform::InputManager& input)
    : m_gsm(gsm), m_input(input) {}

void EditorHubState::onEnter() {
    loadRegistry();
}

void EditorHubState::onExit() {}

void EditorHubState::update(double dt) {
    (void)dt;
    // Process deferred project open
    if (m_pendingOpenIdx >= 0 &&
        m_pendingOpenIdx < static_cast<int>(m_projects.size())) {
        int idx = m_pendingOpenIdx;
        m_pendingOpenIdx = -1;
        openProject(m_projects[idx]);
    }
}

void EditorHubState::render(Core::Time* timer) {
    (void)timer;
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}


void EditorHubState::renderImGui(Core::Time* timer) {
    (void)timer;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowBgAlpha(1.0f);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##EditorHub", nullptr, kFlags);

    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("ClockWorkz Engine");
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("Editor Hub");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    const float panelHeight = vp->Size.y - 80.0f;
    const float leftWidth   = vp->Size.x * 0.65f;

    // ---- Project List (left panel) ----
    ImGui::BeginChild("##ProjectList", ImVec2(leftWidth, panelHeight), true);
    ImGui::Text("Projects");
    ImGui::Separator();
    ImGui::Spacing();

    for (int i = 0; i < static_cast<int>(m_projects.size()); ++i) {
        const bool selected = (m_selectedIdx == i);
        if (ImGui::Selectable(m_projects[i].name.c_str(), selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            m_selectedIdx = i;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_pendingOpenIdx = i;
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Right panel ----
    ImGui::BeginChild("##RightPanel", ImVec2(0.0f, panelHeight), true);
    ImGui::Text("Actions");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("New Game", ImVec2(200.0f, 40.0f))) {
        memset(m_newProjectNameBuf, 0, sizeof(m_newProjectNameBuf));
        ImGui::OpenPopup("##NewProjectModal");
    }

    ImGui::Spacing();

    const bool hasSelection =
        (m_selectedIdx >= 0 && m_selectedIdx < static_cast<int>(m_projects.size()));

    if (!hasSelection)
        ImGui::BeginDisabled();
    if (ImGui::Button("Open Game", ImVec2(200.0f, 40.0f)) && hasSelection)
        m_pendingOpenIdx = m_selectedIdx;
    if (!hasSelection)
        ImGui::EndDisabled();

    if (hasSelection) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Project: %s", m_projects[m_selectedIdx].name.c_str());
        ImGui::TextWrapped("Path:    %s", m_projects[m_selectedIdx].path.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_projects[m_selectedIdx].description.c_str());
    }

    ImGui::EndChild();

    // ---- New Project Modal ----
    ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("##NewProjectModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New Project");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputText("Project Name", m_newProjectNameBuf,
                         sizeof(m_newProjectNameBuf));
        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120.0f, 0.0f))) {
            std::string name(m_newProjectNameBuf);
            if (!name.empty()) {
                createNewProject(name);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // ---- Error Popup ----
    if (m_showErrorPopup) {
        m_showErrorPopup = false;
        ImGui::OpenPopup("##HubError");
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##HubError", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", m_errorMessage.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string EditorHubState::getSourceRoot() {
    return getSourceRootImpl();
}

std::string EditorHubState::resolveAbsoluteScenePath(const ProjectEntry& project) const {
    const std::string srcRoot = getSourceRootImpl();
    if (srcRoot.empty())
        return {};
    namespace fs = std::filesystem;
    return (fs::path(srcRoot) / project.path / project.initialScene).generic_string();
}

void EditorHubState::loadRegistry() {
    using nlohmann::json;
    m_projects.clear();

    std::ifstream f("assets/configs/projects_registry.json");
    if (!f) {
        std::cerr << "[EditorHub] projects_registry.json not found.\n";
        return;
    }

    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        std::cerr << "[EditorHub] Failed to parse registry: " << e.what() << "\n";
        return;
    }

    if (!j.contains("projects") || !j["projects"].is_array())
        return;

    for (const auto& p : j["projects"]) {
        ProjectEntry entry;
        entry.name         = p.value("name",         "Unnamed");
        entry.path         = p.value("path",         "assets");
        entry.initialScene = p.value("initialScene", "assets/scenes/Main.json");
        entry.description  = p.value("description",  "");
        m_projects.push_back(std::move(entry));
    }
}

void EditorHubState::saveRegistry() {
    using nlohmann::json;

    json j;
    j["projects"] = json::array();
    for (const auto& p : m_projects) {
        json entry;
        entry["name"]         = p.name;
        entry["path"]         = p.path;
        entry["initialScene"] = p.initialScene;
        entry["description"]  = p.description;
        j["projects"].push_back(entry);
    }

    // Write to build-output directory
    {
        std::ofstream out("assets/configs/projects_registry.json");
        if (out)
            out << j.dump(4);
    }

    // Write to source directory
    const std::string srcRoot = getSourceRootImpl();
    if (!srcRoot.empty()) {
        namespace fs = std::filesystem;
        fs::path srcPath =
            fs::path(srcRoot) / "assets" / "configs" / "projects_registry.json";
        fs::create_directories(srcPath.parent_path());
        std::ofstream out(srcPath);
        if (out)
            out << j.dump(4);
    }
}

void EditorHubState::createNewProject(const std::string& projectName) {
    namespace fs = std::filesystem;

    const std::string srcRoot = getSourceRootImpl();
    if (srcRoot.empty()) {
        m_errorMessage   = "Cannot resolve source root. Project not created.";
        m_showErrorPopup = true;
        return;
    }

    // Project lives under game/projects/<Name>/
    fs::path projectRoot = fs::path(srcRoot) / "projects" / projectName;

    // Create required subfolders
    std::error_code ec;
    fs::create_directories(projectRoot / "assets" / "scenes",   ec);
    fs::create_directories(projectRoot / "assets" / "scripts",  ec);
    fs::create_directories(projectRoot / "assets" / "configs",  ec);
    fs::create_directories(projectRoot / "assets" / "textures", ec);
    fs::create_directories(projectRoot / "assets" / "prefabs",  ec);

    if (ec) {
        m_errorMessage   = "Failed to create project folders: " + ec.message();
        m_showErrorPopup = true;
        return;
    }

    // Write starter empty scene
    fs::path mainScene = projectRoot / "assets" / "scenes" / "Main.json";
    {
        std::ofstream out(mainScene);
        if (!out) {
            m_errorMessage   = "Failed to write starter scene Main.json.";
            m_showErrorPopup = true;
            return;
        }
        out << "{\n    \"objects\": []\n}\n";
    }

    // Register in the project list
    ProjectEntry entry;
    entry.name         = projectName;
    entry.path         = "projects/" + projectName;
    entry.initialScene = "assets/scenes/Main.json";
    entry.description  = "New project";
    m_projects.push_back(entry);
    m_selectedIdx = static_cast<int>(m_projects.size()) - 1;

    saveRegistry();

    // Defer the open to update()
    m_pendingOpenIdx = m_selectedIdx;
}

void EditorHubState::openProject(const ProjectEntry& project) {
    std::string scenePath = resolveAbsoluteScenePath(project);
    if (scenePath.empty()) {
        m_errorMessage   = "Could not resolve scene path for: " + project.name;
        m_showErrorPopup = true;
        return;
    }

    const auto& factory = Core::Engine::Get().editorFactory();
    if (!factory) {
        m_errorMessage   = "No editor factory registered. Call engine.setEditorFactory() in main.cpp.";
        m_showErrorPopup = true;
        return;
    }

    auto editorState = factory(scenePath);
    if (editorState)
        m_gsm.switchToAndClear(editorState);
}

} // namespace Runtime