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
#include <cstdlib>

namespace fs = std::filesystem;

namespace Runtime {

// Replace every occurrence of {{N}} in a template string with projectName.
static std::string scaffold(const char* tmpl, const std::string& projectName) {
    std::string s(tmpl);
    const std::string key = "{{N}}";
    std::string::size_type p = 0;
    while ((p = s.find(key, p)) != std::string::npos) {
        s.replace(p, key.size(), projectName);
        p += projectName.size();
    }
    return s;
}

static std::string getSourceRootImpl() {
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

void EditorHubState::onExit() {
    if (m_buildThread.joinable())
        m_buildThread.join();
}

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

    bool openEditModal   = false;
    bool openDeleteModal = false;

    for (int i = 0; i < static_cast<int>(m_projects.size()); ++i) {
        const bool selected = (m_selectedIdx == i);
        ImGui::PushID(i);

        if (ImGui::Selectable(m_projects[i].name.c_str(), selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            m_selectedIdx = i;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_pendingOpenIdx = i;
        }

        if (ImGui::BeginPopupContextItem("##ProjCtx")) {
            m_selectedIdx = i;
            if (ImGui::MenuItem("Edit")) {
                m_editingIdx = i;
                snprintf(m_editNameBuf, sizeof(m_editNameBuf),
                         "%s", m_projects[i].name.c_str());
                snprintf(m_editDescBuf, sizeof(m_editDescBuf),
                         "%s", m_projects[i].description.c_str());
                openEditModal = true;
            }
            if (ImGui::MenuItem("Delete")) {
                m_deletingIdx = i;
                openDeleteModal = true;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Right panel ----
    bool openNewProjectModal = false;
    ImGui::BeginChild("##RightPanel", ImVec2(0.0f, panelHeight), true);
    ImGui::Text("Actions");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("New Game", ImVec2(200.0f, 40.0f))) {
        memset(m_newProjectNameBuf, 0, sizeof(m_newProjectNameBuf));
        memset(m_newProjectDescBuf, 0, sizeof(m_newProjectDescBuf));
        openNewProjectModal = true;
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

    // Build status — mutex-free to avoid crash on some machines
    if (m_buildInProgress) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Building...");
    }

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
    if (openNewProjectModal)
        ImGui::OpenPopup("##NewProjectModal");
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("##NewProjectModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New Project");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputText("Project Name", m_newProjectNameBuf,
                         sizeof(m_newProjectNameBuf));
        // Sanitize in real-time: replace spaces/invalid CMake target chars with underscores
        for (char* c = m_newProjectNameBuf; *c; ++c)
            if (*c == ' ' || *c == '\t') *c = '_';
        ImGui::TextDisabled("No spaces allowed (use underscores or CamelCase)");
        ImGui::InputTextMultiline("Description", m_newProjectDescBuf,
                                  sizeof(m_newProjectDescBuf), ImVec2(360.0f, 80.0f));
        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120.0f, 0.0f))) {
            std::string name(m_newProjectNameBuf);
            if (!name.empty()) {
                createNewProject(name, m_newProjectDescBuf);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // ---- Edit Project Modal ----
    if (openEditModal)
        ImGui::OpenPopup("##EditProjectModal");
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##EditProjectModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Edit Project");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputText("Name##ep",        m_editNameBuf, sizeof(m_editNameBuf));
        ImGui::InputTextMultiline("Description##ep", m_editDescBuf,
                                  sizeof(m_editDescBuf), ImVec2(360.0f, 100.0f));
        ImGui::Spacing();
        if (ImGui::Button("Save##ep", ImVec2(120.0f, 0.0f))) {
            if (m_editingIdx >= 0 && m_editingIdx < static_cast<int>(m_projects.size())
                && m_editNameBuf[0] != '\0') {
                m_projects[m_editingIdx].name        = m_editNameBuf;
                m_projects[m_editingIdx].description = m_editDescBuf;
                saveRegistry();
            }
            m_editingIdx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##ep", ImVec2(120.0f, 0.0f))) {
            m_editingIdx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Delete Confirm Modal ----
    if (openDeleteModal)
        ImGui::OpenPopup("##DeleteProjectModal");
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##DeleteProjectModal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_deletingIdx >= 0 && m_deletingIdx < static_cast<int>(m_projects.size()))
            ImGui::Text("Delete \"%s\" from the list?",
                        m_projects[m_deletingIdx].name.c_str());
        ImGui::TextDisabled("(Project files on disk are not removed.)");
        ImGui::Spacing();
        if (ImGui::Button("Delete##dp", ImVec2(120.0f, 0.0f))) {
            if (m_deletingIdx >= 0 && m_deletingIdx < static_cast<int>(m_projects.size())) {
                m_projects.erase(m_projects.begin() + m_deletingIdx);
                if (m_selectedIdx >= static_cast<int>(m_projects.size()))
                    m_selectedIdx = static_cast<int>(m_projects.size()) - 1;
                saveRegistry();
            }
            m_deletingIdx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##dp", ImVec2(120.0f, 0.0f))) {
            m_deletingIdx = -1;
            ImGui::CloseCurrentPopup();
        }
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

    const std::string srcRoot = getSourceRootImpl();
    bool registryDirty = false;

    for (const auto& p : j["projects"]) {
        ProjectEntry entry;
        entry.name         = p.value("name",         "Unnamed");
        entry.path         = p.value("path",         "assets");
        entry.initialScene = p.value("initialScene", "assets/scenes/Main.json");
        entry.description  = p.value("description",  "");

        // Skip entries whose project folder no longer exists on disk
        if (!srcRoot.empty()) {
            fs::path absPath = fs::path(srcRoot) / entry.path;
            if (!fs::exists(absPath)) {
                std::cerr << "[EditorHub] Skipping missing project folder: "
                          << absPath.generic_string() << "\n";
                registryDirty = true;
                continue;
            }
        }

        m_projects.push_back(std::move(entry));
    }

    // Persist the cleaned list so stale entries don't reappear
    if (registryDirty)
        saveRegistry();
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
        fs::path srcPath =
            fs::path(srcRoot) / "assets" / "configs" / "projects_registry.json";
        fs::create_directories(srcPath.parent_path());
        std::ofstream out(srcPath.string());
        if (out)
            out << j.dump(4);
    }
}

void EditorHubState::createNewProject(const std::string& projectName, const std::string& description) {
    const std::string srcRoot = getSourceRootImpl();
    if (srcRoot.empty()) {
        m_errorMessage   = "Cannot resolve source root. Project not created.";
        m_showErrorPopup = true;
        return;
    }

    fs::path root = fs::path(srcRoot) / "projects" / projectName;

    std::error_code ec;
    auto mkdirs = [&](const fs::path& p) {
        fs::create_directories(p, ec);
        return !ec;
    };

    // --- Create directory tree ---
    if (!mkdirs(root / "src"     / "States")         ||
        !mkdirs(root / "include" / "States")         ||
        !mkdirs(root / "include" / "Components")     ||
        !mkdirs(root / "include" / "Systems")        ||
        !mkdirs(root / "assets"  / "scenes")         ||
        !mkdirs(root / "assets"  / "scripts" / "ui") ||
        !mkdirs(root / "assets"  / "configs")        ||
        !mkdirs(root / "assets"  / "textures")       ||
        !mkdirs(root / "assets"  / "prefabs")        ||
        !mkdirs(root / "assets"  / "animations")     ||
        !mkdirs(root / "assets"  / "audio")          ||
        !mkdirs(root / "assets"  / "fonts")) {
        m_errorMessage   = "Failed to create project folders: " + ec.message();
        m_showErrorPopup = true;
        return;
    }

    // Helper: write a file; stops writing on the first failure.
    bool ok = true;
    auto write = [&](const fs::path& p, std::string content) {
        if (!ok) return;
        std::ofstream out(p.string());
        if (!out) {
            m_errorMessage   = "Failed to write: " + p.filename().string();
            m_showErrorPopup = true;
            ok = false;
            return;
        }
        out << content;
    };

    const std::string& N = projectName;

    // =========================================================================
    // CMakeLists.txt  — game executable only; CWZEditor lives at engine level
    // =========================================================================
    write(root / "CMakeLists.txt", scaffold(
R"cmake(# game/projects/{{N}}/CMakeLists.txt  (auto-generated by CWZ Editor)
# Produces:
#   {{N}} — Game executable (boots to MenuState)
#
# CWZEditor is engine-level and is not redefined here.

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS     CONFIGURE_DEPENDS "include/*.hpp")

add_executable({{N}} ${ALL_SOURCES} ${HEADERS})

target_include_directories({{N}} PRIVATE include src)
target_link_libraries({{N}} PRIVATE CWZEngine)

if(MSVC)
  target_compile_options({{N}} PRIVATE
    /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj
    $<$<CONFIG:Debug>:/Od>)
  target_link_options({{N}} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
endif()

set_target_properties({{N}} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")

set(_fmod_dir "${CMAKE_SOURCE_DIR}/fmod")
if(EXISTS "${_fmod_dir}" AND NOT TARGET CopyFmod)
  add_custom_target(CopyFmod
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${_fmod_dir}" "${CMAKE_BINARY_DIR}/fmod"
    COMMENT "Copying FMOD directory" VERBATIM)
endif()
if(TARGET CopyFmod)
  add_dependencies({{N}} CopyFmod)
endif()

if(ENGINE_ENABLE_FMOD AND WIN32 AND NOT TARGET CopyFmodDll)
  add_custom_target(CopyFmodDll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<IF:$<CONFIG:Debug>,${FMOD_DLL_DEBUG},${FMOD_DLL_RELEASE}>"
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
    COMMENT "Copying FMOD runtime DLL" VERBATIM)
endif()
if(TARGET CopyFmodDll)
  add_dependencies({{N}} CopyFmodDll)
endif()

if(DEFINED GAME_SHARED_ASSETS_DIR AND EXISTS "${GAME_SHARED_ASSETS_DIR}" AND NOT TARGET CopySharedAssets)
  add_custom_target(CopySharedAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${GAME_SHARED_ASSETS_DIR}" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
    COMMENT "Copying shared game assets" VERBATIM)
endif()
if(TARGET CopySharedAssets)
  add_dependencies({{N}} CopySharedAssets)
endif()

add_custom_target(Copy{{N}}Assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/projects/{{N}}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${GAME_SHARED_ASSETS_DIR}/fonts" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/projects/{{N}}/assets/fonts"
  COMMENT "Copying {{N}} assets" VERBATIM)
if(TARGET CopySharedAssets)
  add_dependencies(Copy{{N}}Assets CopySharedAssets)
endif()
add_dependencies({{N}} Copy{{N}}Assets)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"     PREFIX "Source Files" FILES ${ALL_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})
)cmake", N));

    // =========================================================================
    // src/main_game.cpp  — game entry point
    // =========================================================================
    write(root / "src" / "main_game.cpp", scaffold(
R"cpp(#include "Core/Engine.hpp"
#include "States/MenuState.hpp"
#include <memory>
#include <iostream>

int main()
{
    std::cout << "[Main] Booting {{N}}...\n";
    Core::Engine engine;

    engine.setBootstrap([](Core::Engine& e) -> std::shared_ptr<Runtime::GameState> {
        return std::make_shared<Runtime::MenuState>(e.gsm(), e.input(), e.window(), e.audio());
    });

    return engine.Run();
}
)cpp", N));

    // =========================================================================
    // include/States/MenuState.hpp
    // =========================================================================
    write(root / "include" / "States" / "MenuState.hpp", scaffold(
R"hpp(#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <vector>
#include <string>

namespace Platform { class InputManager; class WindowManager; }
namespace Runtime  { class GameStateManager; }
namespace Audio    { class AudioManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class MenuState final : public GameState {
public:
    MenuState(GameStateManager& gsm, Platform::InputManager& input,
              Platform::WindowManager& window, Audio::AudioManager& audio);

    const char* name() const override { return "MenuState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&        m_gsm;
    Platform::InputManager&  m_input;
    Platform::WindowManager& m_window;
    Audio::AudioManager&     m_audio;

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_menuScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{0};
};

} // namespace Runtime
)hpp", N));

    // =========================================================================
    // include/States/PlayState.hpp
    // =========================================================================
    write(root / "include" / "States" / "PlayState.hpp", scaffold(
R"hpp(#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include <string>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Audio    { class AudioManager; }
namespace Core     { class Time; }

namespace Runtime {

class PlayState final : public GameState {
public:
    PlayState(GameStateManager& gsm, Platform::InputManager& input,
              Audio::AudioManager& audio, const std::string& scenePath);

    const char* name() const override { return "PlayState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;
    Audio::AudioManager&    m_audio;
    std::string             m_scenePath;
    Runtime::Scene          m_scene;
};

} // namespace Runtime
)hpp", N));

    // =========================================================================
    // include/States/PauseState.hpp
    // =========================================================================
    write(root / "include" / "States" / "PauseState.hpp", scaffold(
R"hpp(#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <glm/vec2.hpp>
#include <vector>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class PauseState final : public GameState {
public:
    PauseState(GameStateManager& gsm, Platform::InputManager& input);

    const char* name() const override { return "PauseState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* timer) override;

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_pauseScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{0};

    // Save/restore play camera so the game resumes seamlessly
    glm::vec2 m_savedCamPos{0.f, 0.f};
    float     m_savedCamZoom{1.f};
    int       m_savedViewW{800};
    int       m_savedViewH{600};
};

} // namespace Runtime
)hpp", N));

    // =========================================================================
    // include/States/GameOverState.hpp
    // =========================================================================
    write(root / "include" / "States" / "GameOverState.hpp", scaffold(
R"hpp(#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "ECS/World.hpp"
#include "Components/UIButton.hpp"
#include <string>
#include <vector>

namespace Platform { class InputManager; }
namespace Runtime  { class GameStateManager; }
namespace Core     { class Time; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class GameOverState final : public GameState {
public:
    GameOverState(GameStateManager& gsm, Platform::InputManager& input,
                  const std::string& currentLevelPath);

    const char* name() const override { return "GameOverState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* /*timer*/) override {}

private:
    void collectButtons();
    void handleButton(const Components::UIButton& btn);
    void applyHoverColors();

    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;
    std::string             m_currentLevelPath; // level to reload on retry

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_loseScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{-1};
};

} // namespace Runtime
)hpp", N));

    // =========================================================================
    // src/States/MenuState.cpp
    // =========================================================================
    write(root / "src" / "States" / "MenuState.cpp", scaffold(
R"cpp(#include "States/MenuState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

MenuState::MenuState(GameStateManager& gsm, Platform::InputManager& input,
                     Platform::WindowManager& window, Audio::AudioManager& audio)
    : m_gsm(gsm), m_input(input), m_window(window), m_audio(audio) {}

void MenuState::onEnter()
{
    // TODO: load MainMenu.json and collect buttons
    // m_menuScene.load("assets/scenes/MainMenu.json");
    // collectButtons();
}

void MenuState::onExit() {}

void MenuState::update(double /*dt*/)
{
    // TODO: keyboard nav, button hover, state transitions
}

void MenuState::render(Core::Time* /*timer*/)
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_menuScene
}

void MenuState::renderImGui(Core::Time* /*timer*/) {}

void MenuState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) to fill m_buttonEntities
}

void MenuState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void MenuState::applyHoverColors()
{
    // TODO: tint selected button's SpriteRenderer
}

} // namespace Runtime
)cpp", N));

    // =========================================================================
    // src/States/PlayState.cpp
    // =========================================================================
    write(root / "src" / "States" / "PlayState.cpp", scaffold(
R"cpp(#include "States/PlayState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include <GL/glew.h>

namespace Runtime {

PlayState::PlayState(GameStateManager& gsm, Platform::InputManager& input,
                     Audio::AudioManager& audio, const std::string& scenePath)
    : m_gsm(gsm), m_input(input), m_audio(audio), m_scenePath(scenePath) {}

void PlayState::onEnter()
{
    // TODO: load level scene
    // m_scene.load(m_scenePath);
}

void PlayState::onExit() {}

void PlayState::update(double /*dt*/)
{
    // TODO: gameplay logic, pause/game-over checks
    // Push PauseState on ESC:   m_gsm.push(std::make_shared<PauseState>(...));
    // Switch to GameOverState:  m_gsm.switchTo(std::make_shared<GameOverState>(..., m_scenePath));
}

void PlayState::render(Core::Time* /*timer*/)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_scene
}

void PlayState::renderImGui(Core::Time* /*timer*/) {}

} // namespace Runtime
)cpp", N));

    // =========================================================================
    // src/States/PauseState.cpp
    // =========================================================================
    write(root / "src" / "States" / "PauseState.cpp", scaffold(
R"cpp(#include "States/PauseState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

PauseState::PauseState(GameStateManager& gsm, Platform::InputManager& input)
    : m_gsm(gsm), m_input(input) {}

void PauseState::onEnter()
{
    // TODO: save game camera, load PauseMenu.json
    // m_pauseScene.load("assets/scenes/PauseMenu.json");
    // collectButtons();
}

void PauseState::onExit()
{
    // TODO: restore game camera
}

void PauseState::update(double /*dt*/)
{
    // TODO: ESC resumes (m_gsm.pop()), button nav
}

void PauseState::render(Core::Time* /*timer*/)
{
    // No glClear — frozen game frame bleeds through as background
    // TODO: render m_pauseScene on top
}

void PauseState::renderImGui(Core::Time* /*timer*/) {}

void PauseState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) from pause scene
}

void PauseState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void PauseState::applyHoverColors()
{
    // TODO: tint selected button
}

} // namespace Runtime
)cpp", N));

    // =========================================================================
    // src/States/GameOverState.cpp
    // =========================================================================
    write(root / "src" / "States" / "GameOverState.cpp", scaffold(
R"cpp(#include "States/GameOverState.hpp"
#include "Runtime/GameStateManager.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>

namespace Runtime {

GameOverState::GameOverState(GameStateManager& gsm, Platform::InputManager& input,
                             const std::string& currentLevelPath)
    : m_gsm(gsm), m_input(input), m_currentLevelPath(currentLevelPath) {}

void GameOverState::onEnter()
{
    // TODO: load LoseScreen.json
    // m_loseScene.load("assets/scenes/LoseScreen.json");
    // collectButtons();
}

void GameOverState::onExit() {}

void GameOverState::update(double /*dt*/)
{
    // TODO: R = retry level, ESC/M = return to main menu
    // Retry:  m_gsm.switchTo(std::make_shared<PlayState>(..., m_currentLevelPath));
    // Menu:   m_gsm.switchToAndClear(std::make_shared<MenuState>(...));
}

void GameOverState::render(Core::Time* /*timer*/)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: render m_loseScene
}

void GameOverState::collectButtons()
{
    // TODO: world.each<Components::UIButton>(...) from lose scene
}

void GameOverState::handleButton(const Components::UIButton& /*btn*/)
{
    // TODO: call btn.uiScript on_click() via Lua
}

void GameOverState::applyHoverColors()
{
    // TODO: tint selected button
}

} // namespace Runtime
)cpp", N));

    // =========================================================================
    // Scene stubs
    // =========================================================================
    const std::string emptyScene = "{\n    \"objects\": []\n}\n";
    write(root / "assets" / "scenes" / "MainMenu.json",   emptyScene);
    write(root / "assets" / "scenes" / "Level_01.json",   emptyScene);
    write(root / "assets" / "scenes" / "PauseMenu.json",  emptyScene);
    write(root / "assets" / "scenes" / "LoseScreen.json", emptyScene);

    // =========================================================================
    // Config stubs
    // =========================================================================
    write(root / "assets" / "configs" / "Textures.json", "{\n    \"textures\": []\n}\n");
    write(root / "assets" / "configs" / "fonts.json",    "{\n    \"fonts\": []\n}\n");
    write(root / "assets" / "configs" / "sounds.json",   "{\n    \"sounds\": []\n}\n");

    // Placeholder headers so VS solution shows the game-side include folders
    write(root / "include" / "Components" / "README.hpp",
        "// Place your game-specific component structs here.\n");
    write(root / "include" / "Systems" / "README.hpp",
        "// Place your game-specific system classes here.\n");

    if (!ok) return;

    // Register in the project list
    ProjectEntry entry;
    entry.name         = projectName;
    entry.path         = "projects/" + projectName;
    entry.initialScene = "assets/scenes/MainMenu.json";
    entry.description  = description.empty() ? "New project" : description;
    m_projects.push_back(entry);
    m_selectedIdx = static_cast<int>(m_projects.size()) - 1;

    saveRegistry();
    triggerProjectBuild(projectName);

    // Defer the open to update()
    m_pendingOpenIdx = m_selectedIdx;
}

void EditorHubState::triggerProjectBuild(const std::string& projectName) {
    if (m_buildInProgress)
        return;

#if !defined(CWZ_CMAKE_SOURCE_DIR) || !defined(CWZ_CMAKE_BINARY_DIR) || !defined(CWZ_BUILD_CONFIG)
    std::lock_guard<std::mutex> lk(m_buildStatusMutex);
    m_buildStatus = "Build paths unknown - reconfigure CMake manually and rebuild.";
    return;
#else
    // Join any previous finished thread before starting a new one
    if (m_buildThread.joinable())
        m_buildThread.join();

    m_buildInProgress = true;
    {
        std::lock_guard<std::mutex> lk(m_buildStatusMutex);
        m_buildStatus = "Reconfiguring CMake for " + projectName + "...";
    }

    m_buildThread = std::thread([this, projectName]() {
        const std::string srcDir = CWZ_CMAKE_SOURCE_DIR;
        const std::string binDir = CWZ_CMAKE_BINARY_DIR;
        const std::string cfg    = CWZ_BUILD_CONFIG;

        // Step 1: re-run cmake configure so file(GLOB) picks up the new project folder
        const std::string configCmd =
            "cmake -S \"" + srcDir + "\" -B \"" + binDir + "\"";
        if (std::system(configCmd.c_str()) != 0) {
            std::lock_guard<std::mutex> lk(m_buildStatusMutex);
            m_buildStatus = "CMake configure failed. Is cmake in PATH?";
            m_buildInProgress = false;
            return;
        }

        // Step 2: build only the new project target
        {
            std::lock_guard<std::mutex> lk(m_buildStatusMutex);
            m_buildStatus = "Building " + projectName + " (" + cfg + ")...";
        }
        const std::string buildCmd =
            "cmake --build \"" + binDir + "\" --target \"" + projectName +
            "\" --config " + cfg;
        if (std::system(buildCmd.c_str()) == 0) {
            std::lock_guard<std::mutex> lk(m_buildStatusMutex);
            m_buildStatus = projectName + " built successfully!";
        } else {
            std::lock_guard<std::mutex> lk(m_buildStatusMutex);
            m_buildStatus = "Build failed for " + projectName + ". Check console output.";
        }
        m_buildInProgress = false;
    });
#endif
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