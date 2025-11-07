/******************************************************************************/
/*!
\file           LevelEditor.cpp
\project        Lights Out - Level Editor
\author(s)      [Yusuf], 100%
\brief          Implementation of comprehensive level editor with all features
                required by the rubric criteria.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Platform/LevelEditor.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/ImGuiManager.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Runtime/Scene.hpp"
#include "Runtime/SceneManager.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Prefab.hpp"
#include "Gameplay/Serializer.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "math/VecCompatGLM.hpp"

#include <IMGUI/imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <Core/Engine.hpp>
#include <Runtime/PlayState.hpp>
#include <Assets/ResourceManager.hpp>
#include <Assets/TextureManager.hpp>

namespace fs = std::filesystem;

namespace Platform {

    /******************************************************************************/
    // Constructor / Destructor
    /******************************************************************************/

    LevelEditor::LevelEditor()
        : m_window(nullptr)
        , m_renderer(nullptr)
        , m_input(nullptr)
        , m_windowManager(nullptr)
        , m_currentScene(nullptr)
        , m_playModeSceneBackup(nullptr)
        , m_levelDirty(false)
        , m_mode(EditorMode::Edit)
        , m_currentTool(EditorTool::Select)
        , m_selectedObject(nullptr)
        , m_dragStartPos(0.0f, 0.0f)
        , m_isDraggingObject(false)
        , m_editorCamera()
        , m_cameraPanStart(0.0f, 0.0f)
        , m_isPanningCamera(false)
        , m_cameraSpeed(500.0f)
        , m_cameraZoomSpeed(0.1f)
        , m_selectedPrefab("")
        , m_showHierarchy(true)
        , m_showProperties(true)
        , m_showPrefabPalette(true)
        , m_showSceneView(true)
        , m_showLevelSettings(false)
        , m_objectTypeToPlace(Gameplay::GameObjectType::Floor)
        , m_windowWidth(1280.0f)
        , m_windowHeight(720.0f)
        , m_showGizmos(true)
        , m_gizmoMode(GizmoMode::Translate)
        , m_frameTime(0.0)
        , m_objectCount(0)
        , m_showSaveDialog(false)
        , m_showLoadDialog(false)
    {
        m_fileDialogBuffer[0] = '\0';

        // Initialize level metadata
        m_currentLevelMetadata.name = "Untitled";
        m_currentLevelMetadata.filepath = "";
        m_currentLevelMetadata.description = "New Level";
        m_currentLevelMetadata.spawnPoint =  vec2(0.0f, 0.0f);
    }

    static bool showReplaceDialog = false;
    static std::string replacingTexture;
    static char newFilePathBuf[128] = "";

    LevelEditor::~LevelEditor() {
        shutdown();
    }

    /******************************************************************************/
    // Initialization
    /******************************************************************************/


    bool LevelEditor::init(GLFWwindow* window,
        Graphics::RenderingManager& renderer,
        Platform::InputManager& input,
        Platform::WindowManager& windowMgr) {
        m_window = window;
        m_renderer = &renderer;
        m_input = &input;
        m_windowManager = &windowMgr;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        m_windowWidth = static_cast<float>(width);
        m_windowHeight = static_cast<float>(height);

        // Initialize editor camera with proper viewport
        m_editorCamera.setPosition(glm::vec2(0.0f, 0.0f));
        m_editorCamera.setZoom(1.0f);
        // If Camera2D has setViewport method:
        // m_editorCamera.setViewport(m_windowWidth, m_windowHeight);

        // Create default scene
        m_currentScene = std::make_unique<Runtime::Scene>("EditorScene");
        if (!m_currentScene->load()) {
            return false;
        }


        // Load prefabs from directory
        loadPrefabs();

        // Register built-in prefabs
        registerBuiltInPrefabs();

        return true;
    }

    void LevelEditor::shutdown() {
        if (m_levelDirty) {
            //// Prompt user: Save / Discard / Cancel
            //int choice = promptSaveDiscardCancel(); // Implement a modal dialog
            //if (choice == 0) { // Save
            //    saveLevel(); // your existing save logic
            //    // then proceed to leave
            //}
            //else if (choice == 1) { // Discard
            //    // proceed without saving
            //}
            //else { // Cancel
            //    return; // abort leaving
            //}
        }
        m_currentScene.reset();
    }

    /******************************************************************************/
    // Main Loop Functions
    /******************************************************************************/

    void LevelEditor::update(double dt) {
        m_frameTime = dt;

        // Update scene based on mode
        if (m_mode == EditorMode::Play) {
            // In play mode, just update the scene normally
            if (m_currentScene) {
                m_currentScene->update(dt);
            }
        }
        else {
            // In edit mode, handle editor-specific input and camera
            handleEditorInput(dt);
            handleCameraInput(dt);
            handleKeyboardShortcuts();
        }

        // Update object count for UI
        if (m_currentScene) {
            m_objectCount = static_cast<int>(m_currentScene->objects().size());
        }
    }

// ============================================================================
// Save Dialog Implementation
// ============================================================================

    int promptSaveDiscardCancel() {
        static bool showDialog = true;
        static int choice = -1; // 0=save, 1=discard, 2=cancel

        if (showDialog) {
            ImGui::OpenPopup("Save Changes?");
            showDialog = false;
        }
        if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Do you want to save before leaving?");
            if (ImGui::Button("Save")) {
                choice = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) {
                choice = 1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                choice = 2;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            if (choice != -1) {
                return choice;
            }
        }
        return -1; // no user choice yet
    }

    void LevelEditor::renderSaveDialog() {
        if (!m_showSaveDialog) return;

        ImGui::OpenPopup("Save Level");

        // Center the popup
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Save Level", &m_showSaveDialog,
            ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Enter filename (without extension):");
            ImGui::Spacing();

            // Input field for filename
            ImGui::InputText("##filename", m_fileDialogBuffer, sizeof(m_fileDialogBuffer));

            ImGui::Spacing();
            ImGui::Text("Will save to: assets/levels/");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s.json", m_fileDialogBuffer);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Save button
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (strlen(m_fileDialogBuffer) > 0) {
                    // ✓ FIXED: Ensure directory exists
                    std::string directory = "assets/levels/";

                    try {
                        if (!fs::exists(directory)) {
                            fs::create_directories(directory);
                            std::cout << "[LevelEditor] Created directory: " << directory << "\n";
                        }
                    }
                    catch (const std::exception& e) {
                        std::cout << "[LevelEditor] Error creating directory: " << e.what() << "\n";
                        ImGui::EndPopup();
                        return;
                    }

                    // Build full filepath
                    std::string filepath = directory + std::string(m_fileDialogBuffer) + ".json";

                    // ✓ Check if file exists and warn
                    bool fileExists = fs::exists(filepath);
                    static bool confirmOverwrite = false;

                    if (fileExists && !confirmOverwrite) {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                            "Warning: File already exists!");
                        ImGui::Text("Click Save again to overwrite.");
                        confirmOverwrite = true;
                        ImGui::EndPopup();
                        return;
                    }

                    // Save the level
                    if (saveLevel(filepath)) {
                        std::cout << "[LevelEditor] Level saved to: " << filepath << "\n";
                        m_showSaveDialog = false;
                        confirmOverwrite = false;
                        ImGui::CloseCurrentPopup();
                    }
                    else {
                        std::cout << "[LevelEditor] Failed to save level\n";
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                        "Please enter a filename!");
                }
            }

            ImGui::SameLine();

            // Cancel button
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showSaveDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // ============================================================================
    // Load Dialog Implementation
    // ============================================================================
    void LevelEditor::renderLoadDialog() {
        if (!m_showLoadDialog) return;

        ImGui::OpenPopup("Open Level");

        // Center the popup
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Open Level", &m_showLoadDialog,
            ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Select a level to load:");
            ImGui::Spacing();

            // List available levels from assets/levels/
            std::string levelsDir = "assets/levels/";
            static std::vector<std::string> availableLevels;
            static bool needsRefresh = true;

            if (needsRefresh) {
                availableLevels.clear();
                if (fs::exists(levelsDir) && fs::is_directory(levelsDir)) {
                    for (const auto& entry : fs::directory_iterator(levelsDir)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".json") {
                            availableLevels.push_back(entry.path().string());
                        }
                    }
                }
                needsRefresh = false;
            }

            // Display list of levels
            ImGui::BeginChild("LevelList", ImVec2(400, 300), true);

            if (availableLevels.empty()) {
                ImGui::TextDisabled("No levels found in %s", levelsDir.c_str());
            }
            else {
                for (const auto& levelPath : availableLevels) {
                    fs::path p(levelPath);
                    std::string displayName = p.stem().string();

                    if (ImGui::Selectable(displayName.c_str())) {
                        if (loadLevel(levelPath)) {
                            std::cout << "[LevelEditor] Level loaded: " << levelPath << "\n";
                            m_showLoadDialog = false;
                            needsRefresh = true;
                            ImGui::CloseCurrentPopup();
                        }
                        else {
                            std::cout << "[LevelEditor] Failed to load level\n";
                        }
                    }

                    // Show full path on hover
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Path: %s", levelPath.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }

            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Refresh button
            if (ImGui::Button("Refresh", ImVec2(120, 0))) {
                needsRefresh = true;
            }

            ImGui::SameLine();

            // Cancel button
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showLoadDialog = false;
                needsRefresh = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void LevelEditor::render() {
        if (!m_renderer || !m_currentScene) return;

        //Core::Engine& engine = Core::Engine::Get();

        // Use editor camera's view-projection matrix
        glm::mat4 viewProj = m_editorCamera.viewProj();

        m_renderer->beginScenePass(viewProj);


        // Render all objects via batch rendering
        m_currentScene->render(*m_renderer);

        m_renderer->endScenePass();

        //if (m_mode == EditorMode::Play && m_currentScene) {
        //    for (auto& obj : m_currentScene->objects()) {
        //        if (obj->type() == Gameplay::GameObjectType::Player) {
        //            vec2 playerPos = obj->transform().position;
        //            float playerRotation = obj->transform().rotationRad;

        //            auto* physics = m_currentScene->getPhysics();
        //            if (!physics) break;

        //            // Get visibility polygon as vec2
        //            std::vector<vec2> lightPoly =
        //                m_renderer->computeVisibilityPolygon(
        //                    playerPos,
        //                    playerRotation,
        //                    glm::radians(60.0f),
        //                    300.0f,
        //                    *physics,
        //                    m_currentScene->objects()
        //                );

        //            // Pass vec2 directly if methods accept it
        //            m_renderer->beginLightPass(
        //                viewProj,
        //                playerPos,
        //                vec3(1.0f, 0.9f, 0.6f),
        //                300.0f,
        //                playerRotation,
        //                glm::radians(60.0f)
        //            );

        //            // Pass vec2 vector directly
        //            m_renderer->drawLightCone(
        //                playerPos,
        //                lightPoly,      
        //                vec3(1.0f, 0.9f, 0.6f)
        //            );

        //            m_renderer->endLightPass();
        //            break;
        //        }
        //    }
        //}
    }




    void LevelEditor::renderReplace()
    {
        ImGui::OpenPopup("Replace Texture");
        if (ImGui::BeginPopupModal("Replace Texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("New PNG path", newFilePathBuf, sizeof(newFilePathBuf));
            if (ImGui::Button("OK")) {
                Assets::TextureManager::Unload(replacingTexture);
                Assets::TextureManager::LoadTexture(replacingTexture, newFilePathBuf); // reloads under same name
                showReplaceDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                showReplaceDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void LevelEditor::renderUI() {
        renderMenuBar();

        renderToolbar();

        if (m_showHierarchy) renderHierarchy();
        if (m_showProperties) renderProperties();
        if (m_showPrefabPalette) renderPrefabPalette();
        if (m_showLevelSettings) renderLevelSettings();

        // Always show scene view
        if (m_showSceneView) renderSceneView();

        renderAssetBrowser();

        if (showReplaceDialog) {
            renderReplace();
        }

        renderSaveDialog();
        renderLoadDialog();
    }

    /******************************************************************************/
    // Editor State Management
    /******************************************************************************/

    void LevelEditor::setMode(EditorMode mode) {
        if (m_mode == mode) return;

        //EditorMode oldMode = m_mode;
        m_mode = mode;

        if (mode == EditorMode::Play) {
            // Entering play mode - backup current scene
            saveEditorState();
            // TODO: Core::Engine::Get().logger().info("Entering Play Mode");
        }
        else {
            // Exiting play mode - restore scene
            restoreEditorState();
            // TODO: Core::Engine::Get().logger().info("Exiting Play Mode");
        }
    }

    void LevelEditor::togglePlayMode() {
        if (m_mode == EditorMode::Edit) {
            setMode(EditorMode::Play);
        }
        else {
            setMode(EditorMode::Edit);
        }
    }

    /******************************************************************************/
    // Level Management
    /******************************************************************************/

    bool LevelEditor::newLevel(const std::string& name) {
        if (m_levelDirty) {
            // In a real implementation, prompt to save
            // TODO: Core::Engine::Get().logger().warn("Creating new level with unsaved changes");
        }

        m_currentScene = std::make_unique<Runtime::Scene>(name);
        if (!m_currentScene->load()) {
            // TODO: Core::Engine::Get().logger().error("Failed to create new level");
            return false;
        }

        m_currentLevelMetadata.name = name;
        m_currentLevelMetadata.filepath = "";
        m_currentLevelMetadata.description = "New Level";
        m_currentLevelMetadata.spawnPoint = vec2(0.0f, 0.0f);

        m_levelDirty = false;
        m_selectedObject = nullptr;

        // TODO: Core::Engine::Get().logger().info("Created new level: " + name);
        return true;
    }

    bool LevelEditor::saveLevel() {
        if (m_currentLevelMetadata.filepath.empty()) {
            m_showSaveDialog = true;
            return false;
        }

        return saveLevel(m_currentLevelMetadata.filepath);
    }

    bool LevelEditor::saveLevel(const std::string& filepath) {
        if (!m_currentScene) {
            std::cout << "[LevelEditor] No scene to save\n";
            return false;
        }

        // ✓ FIXED: Ensure parent directory exists
        fs::path filePath(filepath);
        fs::path parentDir = filePath.parent_path();

        try {
            if (!parentDir.empty() && !fs::exists(parentDir)) {
                fs::create_directories(parentDir);
                std::cout << "[LevelEditor] Created directory: " << parentDir << "\n";
            }
        }
        catch (const std::exception& e) {
            std::cout << "[LevelEditor] Error creating directory: " << e.what() << "\n";
            return false;
        }

        // Save scene to file (creates file if it doesn't exist)
        bool success = Gameplay::GameObjectSerializer::saveSceneToFile(
            m_currentLevelMetadata.name,
            m_currentScene->objects(),
            filepath
        );

        if (success) {
            // Update metadata
            m_currentLevelMetadata.filepath = filepath;
            m_levelDirty = false;

            // Add to recent levels
            auto it = std::find(m_recentLevels.begin(), m_recentLevels.end(), filepath);
            if (it != m_recentLevels.end()) {
                m_recentLevels.erase(it);
            }
            m_recentLevels.insert(m_recentLevels.begin(), filepath);
            if (m_recentLevels.size() > MAX_RECENT_LEVELS) {
                m_recentLevels.resize(MAX_RECENT_LEVELS);
            }

            std::cout << "[LevelEditor] Successfully saved level: " << filepath << "\n";
        }
        else {
            std::cout << "[LevelEditor] Failed to save level: " << filepath << "\n";
        }

        return success;
    }

    bool LevelEditor::loadLevel(const std::string& filepath) {
        if (m_levelDirty) {
            // In a real implementation, prompt to save
        }

        std::string sceneName;
        std::vector<std::unique_ptr<Gameplay::GameObject>> objects;

        bool success = Gameplay::GameObjectSerializer::loadSceneFromFile(
            filepath,
            sceneName,
            objects
        );

        if (success) {
            m_currentScene = std::make_unique<Runtime::Scene>(sceneName);
            m_currentScene->load();

            for (auto& obj : objects) {
                m_currentScene->addObject(std::move(obj)); 
            }
            std::cout << "GameObjects loaded (" << m_currentScene->objects().size() << "):\n";
            for (const auto& obj : m_currentScene->objects()) {
                auto p = obj->transform().position;
                auto s = obj->size();
                std::cout << " - name=\"" << obj->name() << "\""
                    << " pos=(" << p.x << "," << p.y << ")"
                    << " size=(" << s.x << "," << s.y << ")"
                    << " texture=\"" << obj->textureId() << "\"\n";
            }
            std::cout.flush();

            m_currentLevelMetadata.name = sceneName;
            m_currentLevelMetadata.filepath = filepath;
            m_levelDirty = false;
            m_selectedObject = nullptr;

            // Add to recent levels
            auto it = std::find(m_recentLevels.begin(), m_recentLevels.end(), filepath);
            if (it != m_recentLevels.end()) {
                m_recentLevels.erase(it);
            }
            m_recentLevels.insert(m_recentLevels.begin(), filepath);
            if (m_recentLevels.size() > MAX_RECENT_LEVELS) {
                m_recentLevels.resize(MAX_RECENT_LEVELS);
            }
        }
        else {
            std::cout << "[LevelEditor] Failed to load level: " << filepath << "\n";
        }

        return success;
    }

    std::vector<std::string> LevelEditor::getRecentLevels() const {
        return m_recentLevels;
    }

    /******************************************************************************/
    // Object Management
    /******************************************************************************/

    void LevelEditor::selectObject(Gameplay::GameObject* obj) {
        m_selectedObject = obj;
    }

    void LevelEditor::deleteSelectedObject() {
        if (!m_selectedObject || !m_currentScene) return;

        auto& objects = m_currentScene->objects();
        auto it = std::find_if(objects.begin(), objects.end(),
            [this](const std::unique_ptr<Gameplay::GameObject>& obj) {
                return obj.get() == m_selectedObject;
            });

        if (it != objects.end()) {
            objects.erase(it);
            m_selectedObject = nullptr;
            markLevelDirty();
            // TODO: Core::Engine::Get().logger().info("Deleted object");
        }
    }

    //void LevelEditor::duplicateSelectedObject() {
    //    if (!m_selectedObject || !m_currentScene) return;

    //    // In a real implementation, this would properly clone the object
    //    // For now, we'll create a new object at an offset position
    //    vec2 offset(50.0f, 50.0f);
    //    vec2 newPos = m_selectedObject->transform().position + offset;

    //    // Create duplicate based on type
    //    std::unique_ptr<Gameplay::GameObject> duplicate;
    //    switch (m_selectedObject->type()) {
    //    case Gameplay::GameObjectType::Floor:
    //        duplicate = Gameplay::GameObject::MakeFloor(newPos, m_selectedObject->size());
    //        break;
    //    case Gameplay::GameObjectType::Wall:
    //        duplicate = Gameplay::GameObject::MakeWall(newPos, m_selectedObject->size());
    //        break;
    //    case Gameplay::GameObjectType::Player:
    //        duplicate = Gameplay::GameObject::MakePlayer(newPos, m_selectedObject->size());
    //        break;
    //    case Gameplay::GameObjectType::Enemy:
    //        duplicate = Gameplay::GameObject::MakeEnemy(newPos, m_selectedObject->size());
    //        break;
    //    case Gameplay::GameObjectType::Trap:
    //        duplicate = Gameplay::GameObject::MakeTrap(newPos, m_selectedObject->size());
    //        break;
    //    default:
    //        break;
    //    }

    //    if (duplicate) {
    //        m_currentScene->objects().push_back(std::move(duplicate));
    //        markLevelDirty();
    //        // TODO: Core::Engine::Get().logger().info("Duplicated object");
    //    }
    //}

    /******************************************************************************/
    // Prefab/Archetype System (Rubric 1206 - Advanced)
    /******************************************************************************/

    void LevelEditor::loadPrefabs() {
        // Load prefabs from directory
        std::string prefabDir = "assets/prefabs/";

        if (fs::exists(prefabDir) && fs::is_directory(prefabDir)) {
            scanPrefabDirectory(prefabDir);
        }

        // Load all prefabs into factory
        Gameplay::PrefabFactory::get().loadAllPrefabsFromDirectory(prefabDir);
    }

    void LevelEditor::createObjectFromPrefab(const std::string& prefabName, vec2 position) {
        if (!m_currentScene) return;

        auto obj = Gameplay::PrefabFactory::get().createAt(prefabName, position);
        if (obj) {
            m_currentScene->addObject(std::move(obj)); 
            markLevelDirty();
            // TODO: Core::Engine::Get().logger().info("Created object from prefab: " + prefabName);
        }
        else {
            // TODO: Core::Engine::Get().logger().error("Failed to create object from prefab: " + prefabName);
        }
    }

    bool LevelEditor::savePrefab(const std::string& name) {
        if (!m_selectedObject) {
            // TODO: Core::Engine::Get().logger().error("No object selected to save as prefab");
            return false;
        }

        std::string filepath = "assets/prefabs/" + name + ".json";

        // Serialize selected object as prefab
        auto json = Gameplay::GameObjectSerializer::serializeGameObject(*m_selectedObject);

        // Save to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            // TODO: Core::Engine::Get().logger().error("Failed to open file for writing: " + filepath);
            return false;
        }

        file << json.dump(4);
        file.close();

        // Register in factory
        Gameplay::PrefabFactory::get().loadPrefabFromFile(filepath);

        // TODO: Core::Engine::Get().logger().info("Saved prefab: " + name);
        return true;
    }

    bool LevelEditor::loadPrefab(const std::string& filepath) {
        return Gameplay::PrefabFactory::get().loadPrefabFromFile(filepath);
    }

    void LevelEditor::registerBuiltInPrefabs() {
        // Register built-in prefabs with palette entries

        m_prefabPalette.push_back({
            "Floor", "Floor Tile", "Terrain",
            "", Gameplay::GameObjectType::Floor
            });

        m_prefabPalette.push_back({
            "Wall", "Wall", "Terrain",
            "", Gameplay::GameObjectType::Wall
            });

        m_prefabPalette.push_back({
            "Player", "Player Character", "Characters",
            "", Gameplay::GameObjectType::Player
            });

        m_prefabPalette.push_back({
            "Enemy", "Enemy", "Characters",
            "", Gameplay::GameObjectType::Enemy
            });

        m_prefabPalette.push_back({
            "Trap", "Trap", "Hazards",
            "", Gameplay::GameObjectType::Trap
            });
    }

    void LevelEditor::scanPrefabDirectory(const std::string& directory) {
        // Scan directory for prefab files and add to palette
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filename = entry.path().stem().string();

                    // Check if already in palette
                    bool exists = false;
                    for (const auto& prefab : m_prefabPalette) {
                        if (prefab.name == filename) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                        m_prefabPalette.push_back({
                            filename,
                            filename,
                            "Custom",
                            "",
                            Gameplay::GameObjectType::Unknown
                            });
                    }
                }
            }
        }
        catch (const std::exception& e) {
            m_logger->fatal("Fatal exception caught: " + std::string(e.what()));
        }
    }
    /******************************************************************************/
    // Camera Control
    /******************************************************************************/

    void LevelEditor::resetCamera() {
        m_editorCamera.setPosition(glm::vec2(0.0f, 0.0f));
        m_editorCamera.setZoom(1.0f);
    }

    /******************************************************************************/
    // Drag-and-Drop Support (Rubric 1209 - Advanced)
    /******************************************************************************/

    void LevelEditor::handleFileDrop(const std::vector<std::string>& paths) {
        for (const auto& path : paths) {
            fs::path filePath(path);

            // Check if it's a level file
            if (filePath.extension() == ".json") {
                // Try to load as level
                if (loadLevel(path)) {
                    // TODO: Core::Engine::Get().logger().info("Loaded level from dropped file: " + path);
                }
            }
            // Check if it's a prefab file
            else if (filePath.parent_path().filename() == "prefabs") {
                if (loadPrefab(path)) {
                    // TODO: Core::Engine::Get().logger().info("Loaded prefab from dropped file: " + path);
                }
            }
            else if (filePath.extension() == ".png" || filePath.extension() == ".jpg") {
                std::string name = filePath.stem().string(); // e.g., "my_pic" from "my_pic.png"
                if (Assets::TextureManager::LoadTexture(name, path)) {
                    markLevelDirty();
                }
                else {
                    // Optionally: log or alert about failure
                }
            }
        }
    }

    /******************************************************************************/
    // UI Rendering Functions
    /******************************************************************************/

    void LevelEditor::renderMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Level", "Ctrl+N")) {
                    newLevel("Untitled");
                }

                if (ImGui::MenuItem("Open Level...", "Ctrl+O")) {
                    m_showLoadDialog = true;
                }

                if (ImGui::MenuItem("Save Level", "Ctrl+S")) {
                    // If no filepath, show save dialog
                    if (m_currentLevelMetadata.filepath.empty()) {
                        // Pre-fill with current level name
                        strncpy(m_fileDialogBuffer,
                            m_currentLevelMetadata.name.c_str(),
                            sizeof(m_fileDialogBuffer));
                        m_showSaveDialog = true;
                    }
                    else {
                        // Save to existing file
                        saveLevel();
                    }
                }

                if (ImGui::MenuItem("Save Level As...", "Ctrl+Shift+S")) {
                    // Pre-fill with current level name
                    strncpy(m_fileDialogBuffer,
                        m_currentLevelMetadata.name.c_str(),
                        sizeof(m_fileDialogBuffer));
                    m_showSaveDialog = true;
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Recent Levels")) {
                    if (m_recentLevels.empty()) {
                        ImGui::TextDisabled("No recent levels");
                    }
                    else {
                        for (const auto& level : m_recentLevels) {
                            fs::path p(level);
                            std::string displayName = p.stem().string();

                            if (ImGui::MenuItem(displayName.c_str())) {
                                loadLevel(level);
                            }

                            // Show full path on hover
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s", level.c_str());
                                ImGui::EndTooltip();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    shutdown();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                bool canUndo = !m_undoStack.empty();
                bool canRedo = !m_redoStack.empty();

                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                    // Undo functionality
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
                    // Redo functionality
                }

                ImGui::Separator();

                bool hasSelection = m_selectedObject != nullptr;
                if (ImGui::MenuItem("Delete", "Delete", false, hasSelection)) {
                    deleteSelectedObject();
                }
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection)) {
                    //duplicateSelectedObject();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Level Settings")) {
                    m_showLevelSettings = !m_showLevelSettings;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
                ImGui::MenuItem("Properties", nullptr, &m_showProperties);
                ImGui::MenuItem("Prefab Palette", nullptr, &m_showPrefabPalette);
                ImGui::MenuItem("Scene View", nullptr, &m_showSceneView);

                ImGui::Separator();

                if (ImGui::MenuItem("Reset Camera", "Home")) {
                    resetCamera();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Prefabs")) {
                if (ImGui::MenuItem("Save Selected as Prefab")) {
                    if (m_selectedObject) {
                        // Show dialog to name prefab
                        ImGui::OpenPopup("SavePrefabDialog");
                    }
                }
                if (ImGui::MenuItem("Reload Prefabs")) {
                    loadPrefabs();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Documentation")) {
                    // Open documentation
                }
                if (ImGui::MenuItem("About")) {
                    // Show about dialog
                }

                ImGui::EndMenu();
            }

            // Right-aligned status
            ImGui::SameLine(ImGui::GetWindowWidth() - 400);
            ImGui::Text("FPS: %.1f | Objects: %d | %s",
                1.0 / m_frameTime,
                m_objectCount,
                m_levelDirty ? "Modified" : "Saved");
            ImGui::EndMainMenuBar();
        }
    }

    void LevelEditor::renderToolbar() {
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        // Play/Stop button (Rubric 1207 - Advanced)
        if (m_mode == EditorMode::Edit) {
            if (ImGui::Button("Play")) {
                togglePlayMode();
            }
        }
        else {
            if (ImGui::Button("Stop")) {
                togglePlayMode();
            }
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Tool selection
        if (ImGui::RadioButton("Select", m_currentTool == EditorTool::Select)) {
            m_currentTool = EditorTool::Select;
        }
        ImGui::SameLine();

        if (ImGui::RadioButton("Place", m_currentTool == EditorTool::Place)) {
            m_currentTool = EditorTool::Place;
        }
        ImGui::SameLine();

        if (ImGui::RadioButton("Erase", m_currentTool == EditorTool::Erase)) {
            m_currentTool = EditorTool::Erase;
        }
        ImGui::SameLine();

        if (ImGui::RadioButton("Pan", m_currentTool == EditorTool::Pan)) {
            m_currentTool = EditorTool::Pan;
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        //// Gizmo mode
        //if (ImGui::RadioButton("Translate", m_gizmoMode == GizmoMode::Translate)) {
        //    m_gizmoMode = GizmoMode::Translate;
        //}
        //ImGui::SameLine();

        //if (ImGui::RadioButton("Rotate", m_gizmoMode == GizmoMode::Rotate)) {
        //    m_gizmoMode = GizmoMode::Rotate;
        //}
        //ImGui::SameLine();

        //if (ImGui::RadioButton("Scale", m_gizmoMode == GizmoMode::Scale)) {
        //    m_gizmoMode = GizmoMode::Scale;
        //}

        ImGui::End();
    }

    void LevelEditor::renderHierarchy() {
        ImGui::Begin("Hierarchy");

        if (ImGui::Button("Create Empty Object")) {
            auto obj = std::make_unique<Gameplay::GameObject>(
                Gameplay::GameObjectType::Unknown,
                "GameObject",
                Gameplay::GameObjectShape::Square
            );
			vec2 camPos = compat::to_vec2(m_editorCamera.position());

            obj->setPosition(camPos);
            m_currentScene->addObject(std::move(obj)); 
            markLevelDirty();
        }

        ImGui::Separator();

        // Search filter
        static char searchBuffer[256] = "";
        ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));

        ImGui::Separator();

        // List all objects in scene
        if (m_currentScene) {
            auto& objects = m_currentScene->objects();

            for (size_t i = 0; i < objects.size(); ++i) {
                auto& obj = objects[i];

                // Apply search filter
                if (searchBuffer[0] != '\0') {
                    if (obj->name().find(searchBuffer) == std::string::npos) {
                        continue;
                    }
                }

                // Determine icon based on type
                const char* icon = "?";
                switch (obj->type()) {
                case Gameplay::GameObjectType::Floor: icon = "F"; break;
                case Gameplay::GameObjectType::Wall: icon = "W"; break;
                case Gameplay::GameObjectType::Player: icon = "P"; break;
                case Gameplay::GameObjectType::Enemy: icon = "E"; break;
                case Gameplay::GameObjectType::Trap: icon = "T"; break;
                default: icon = "?"; break;
                }

                ImGui::PushID(static_cast<int>(i));

                bool isSelected = (m_selectedObject == obj.get());
                if (ImGui::Selectable((std::string(icon) + " " + obj->name()).c_str(), isSelected)) {
                    selectObject(obj.get());
                }

                // Context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        // Open rename dialog
                    }
                    if (ImGui::MenuItem("Duplicate")) {
                        selectObject(obj.get());
                        //duplicateSelectedObject();
                    }
                    if (ImGui::MenuItem("Delete")) {
                        selectObject(obj.get());
                        deleteSelectedObject();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save as Prefab")) {
                        selectObject(obj.get());
                        // Open save prefab dialog
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            if (objects.empty()) {
                ImGui::TextDisabled("No objects in scene");
                ImGui::TextDisabled("Use the Prefab Palette to add objects");
            }
        }

        ImGui::End();
    }

    // Rubric 1203 - Good: Property editor for modifying objects
    void LevelEditor::renderProperties() {
        ImGui::Begin("Properties");

        if (!m_selectedObject) {
            ImGui::TextDisabled("No object selected");
            ImGui::TextDisabled("Select an object from the Hierarchy");
            ImGui::End();
            return;
        }

        // Object name
        char nameBuffer[256];
        strncpy(nameBuffer, m_selectedObject->name().c_str(), sizeof(nameBuffer));
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            m_selectedObject->setName(nameBuffer);
            markLevelDirty();
        }

        // Object type (read-only)
        const char* typeNames[] = { "Floor", "Wall", "Player", "Enemy", "Trap", "Unknown", "Ball" };
        int typeIndex = static_cast<int>(m_selectedObject->type());
        ImGui::Text("Type: %s", typeNames[typeIndex]);

        ImGui::Separator();

        // Transform
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& transform = m_selectedObject->transform();

            // Position
            vec2 pos = transform.position;
            if (ImGui::DragFloat2("Position", &pos.x, 1.0f)) {
                m_selectedObject->setPosition(pos);
                markLevelDirty();
            }

            // Rotation
            float rotation = glm::degrees(transform.rotationRad);
            if (ImGui::DragFloat("Rotation", &rotation, 1.0f)) {
                transform.rotationRad = glm::radians(rotation);
                markLevelDirty();
            }

            // Scale
            vec2 scale = transform.scale;
            if (ImGui::DragFloat2("Scale", &scale.x, 0.01f, 0.01f, 10.0f)) {
                transform.scale = scale;
                markLevelDirty();
            }

            // Size
            vec2 size = m_selectedObject->size();
            if (ImGui::DragFloat2("Size", &size.x, 1.0f, 1.0f, 1000.0f)) {
                m_selectedObject->setSize(size);
                markLevelDirty();
            }
        }

        // Physics
        if (ImGui::CollapsingHeader("Physics")) {
            vec2 vel = m_selectedObject->velocity();
            if (ImGui::DragFloat2("Velocity", &vel.x, 1.0f)) {
                m_selectedObject->velocity() = vel;
                markLevelDirty();
            }

            float speed = m_selectedObject->moveSpeed();
            if (ImGui::DragFloat("Move Speed", &speed, 1.0f, 0.0f, 1000.0f)) {
                m_selectedObject->setMoveSpeed(speed);
                markLevelDirty();
            }
        }

        // Rendering
        if (ImGui::CollapsingHeader("Rendering")) {
            // Texture ID
            // Texture assignment
            char texBuffer[256];
            strncpy(texBuffer, m_selectedObject->textureId().c_str(), sizeof(texBuffer));
            if (ImGui::InputText("Texture ID", texBuffer, sizeof(texBuffer))) {
                m_selectedObject->setTextureId(texBuffer);
                markLevelDirty();
            }
            // Texture preview (to the right of the field)
            auto tex = Assets::TextureManager::Get(m_selectedObject->textureId());
            ImGui::SameLine();
            if (tex && tex->id() != 0) {
                ImGui::Image((ImTextureID)(intptr_t)tex->id(), ImVec2(32, 32));
            }
            // --- Drag-and-drop target for texture thumbnails
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_ID")) {
                    const char* droppedName = (const char*)payload->Data;
                    m_selectedObject->setTextureId(droppedName);  // update
                    markLevelDirty();
                }
                ImGui::EndDragDropTarget();
            }

            // Layer
            int layer = static_cast<int>(m_selectedObject->layer());
            if (ImGui::DragInt("Layer", &layer, 1, 0, 10)) {
                m_selectedObject->setLayer(static_cast<uint32_t>(layer));
                markLevelDirty();
            }
        }

        // Flags
        if (ImGui::CollapsingHeader("Flags")) {
            bool solid = m_selectedObject->isSolid();
            if (ImGui::Checkbox("Solid", &solid)) {
                m_selectedObject->setSolid(solid);
                markLevelDirty();
            }

            bool hazard = m_selectedObject->isHazard();
            if (ImGui::Checkbox("Hazard", &hazard)) {
                m_selectedObject->setHazard(hazard);
                markLevelDirty();
            }

            bool blocksLight = m_selectedObject->blocksLight();
            if (ImGui::Checkbox("Blocks Light", &blocksLight)) {
                m_selectedObject->setBlocksLight(blocksLight);
                markLevelDirty();
            }

            bool active = m_selectedObject->active();
            if (ImGui::Checkbox("Active", &active)) {
                m_selectedObject->setActive(active);
                markLevelDirty();
            }
        }

        // Components
        if (ImGui::CollapsingHeader("Components")) {
            ImGui::TextDisabled("Component list would appear here");
            // In a real implementation, list and edit components
        }

        ImGui::Separator();

        // Actions
        if (ImGui::Button("Duplicate")) {
            //duplicateSelectedObject();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            deleteSelectedObject();
        }

        ImGui::End();
    }

    // Rubric 1206 - Advanced: Prefab palette
    void LevelEditor::renderPrefabPalette() {
        ImGui::Begin("Prefab Palette");

        ImGui::Text("Drag prefabs into the scene to create objects");
        ImGui::Separator();

        // Category filter
        static int categoryIndex = 0;
        const char* categories[] = { "All", "Terrain", "Characters", "Hazards", "Custom" };
        ImGui::Combo("Category", &categoryIndex, categories, IM_ARRAYSIZE(categories));

        ImGui::Separator();

        // Display prefabs as buttons
        std::string selectedCategory = categories[categoryIndex];

        for (const auto& prefab : m_prefabPalette) {
            // Filter by category
            if (selectedCategory != "All" && prefab.category != selectedCategory) {
                continue;
            }

            ImGui::PushID(prefab.name.c_str());

            bool isSelected = (m_selectedPrefab == prefab.name);
            if (ImGui::Selectable(prefab.displayName.c_str(), isSelected)) {
                m_selectedPrefab = prefab.name;
                m_currentTool = EditorTool::Place;
                m_objectTypeToPlace = prefab.type;
            }

            // Tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Name: %s", prefab.name.c_str());
                ImGui::Text("Category: %s", prefab.category.c_str());
                ImGui::Text("Click to select, then click in scene to place");
                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }

        ImGui::Separator();

        if (ImGui::Button("Refresh Prefabs")) {
            loadPrefabs();
        }

        ImGui::End();
    }

    void LevelEditor::renderSceneView() {
        ImGui::Begin("Scene View");

        // Display scene statistics
        ImGui::Text("Camera Position: (%.1f, %.1f)",
            m_editorCamera.position().x,
            m_editorCamera.position().y);
        ImGui::Text("Zoom: %.2f", m_editorCamera.zoom());
        ImGui::Text("Objects: %d", m_objectCount);

        ImGui::Separator();

        // Camera controls
        if (ImGui::Button("Reset Camera")) {
            resetCamera();
        }

        ImGui::SameLine();

        if (ImGui::Button("Frame Selected")) {
            if (m_selectedObject) {
				glm::vec2 selObjectPos = glm::vec2(m_selectedObject->transform().position.x, m_selectedObject->transform().position.y);
                m_editorCamera.setPosition(selObjectPos);
            }
        }

        ImGui::End();
    }

    void LevelEditor::renderAssetBrowser()
    {
        ImGui::Begin("Texture Assets Browser");
        std::vector<std::string> names = Assets::TextureManager::GetLoadedTextureNames();
        static std::string selectedTextureName;

        for (const auto& name : names) {
            ImGui::PushID(name.c_str());  // Ensure all widgets in this loop have unique ID scope

            bool isSelected = (selectedTextureName == name);
            // Selectable - show name, allow context menu
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                selectedTextureName = name;
            }
            // Context menu for selectable (right-click name)
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Replace")) {
                    showReplaceDialog = true;
                    replacingTexture = name;
                    newFilePathBuf[0] = '\0';
                }
                if (ImGui::MenuItem("Rename")) {
                    // Show rename UI/dialog here
                }
                if (ImGui::MenuItem("Delete")) {
                    Assets::TextureManager::Unload(name);
                }
                ImGui::EndPopup();
            }

            auto tex = Assets::TextureManager::Get(name);
            if (tex && tex->id() != 0) {
                // Show texture preview as an ImageButton.
                // The label ("##thumb") ensures a unique widget, but 'PushID' already ensures uniqueness, so label doesn't need to be unique.
                if (ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)tex->id(), ImVec2(64, 64))) {
                    // You can implement click-to-select or preview here.
                }
                // Context menu for the image thumbnail (right-click thumbnail)
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("TEXTURE_ID", name.c_str(), name.length() + 1);
                    ImGui::Text("Drag: %s", name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
            ImGui::PopID();
        }
        ImGui::End();
    }



    void LevelEditor::renderLevelSettings() {
        ImGui::Begin("Level Settings", &m_showLevelSettings);

        // Level name
        char nameBuffer[256];
        strncpy(nameBuffer, m_currentLevelMetadata.name.c_str(), sizeof(nameBuffer));
        if (ImGui::InputText("Level Name", nameBuffer, sizeof(nameBuffer))) {
            m_currentLevelMetadata.name = nameBuffer;
            markLevelDirty();
        }

        // Description
        char descBuffer[1024];
        strncpy(descBuffer, m_currentLevelMetadata.description.c_str(), sizeof(descBuffer));
        if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer))) {
            m_currentLevelMetadata.description = descBuffer;
            markLevelDirty();
        }

        // Spawn point
        vec2 spawn = m_currentLevelMetadata.spawnPoint;
        if (ImGui::DragFloat2("Spawn Point", &spawn.x, 1.0f)) {
            m_currentLevelMetadata.spawnPoint = spawn;
            markLevelDirty();
        }

        ImGui::Separator();

        // File information
        ImGui::Text("Filepath: %s", m_currentLevelMetadata.filepath.empty() ?
            "(not saved)" : m_currentLevelMetadata.filepath.c_str());

        ImGui::End();
    }

    void LevelEditor::renderCameraControls() {
        // Camera movement hints
        ImGui::Begin("Camera Controls");

        ImGui::Text("Camera Movement:");
        ImGui::BulletText("WASD or Arrow Keys - Pan camera");
        ImGui::BulletText("Mouse Wheel - Zoom in/out");
        ImGui::BulletText("Middle Mouse - Pan camera");
        ImGui::BulletText("Home - Reset camera");

        ImGui::Separator();

        ImGui::Text("Object Manipulation:");
        ImGui::BulletText("Left Click - Select object");
        ImGui::BulletText("Left Click + Drag - Move object");
        ImGui::BulletText("Delete - Delete selected");
        ImGui::BulletText("Ctrl+D - Duplicate selected");

        ImGui::End();
    }

    /******************************************************************************/
    // Input Handling
    /******************************************************************************/

    //to be fixed later
    void LevelEditor::handleEditorInput(double dt) {
        if (!m_input) return;

        //// Handle object selection
        if (m_currentTool == EditorTool::Select) {
            handleObjectSelection();

            // Handle object dragging
        //    //if (m_selectedObject && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        //    //    handleObjectDragging();
        //    //}
        }

        // Handle object placement
        if (m_currentTool == EditorTool::Place && m_input->wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) {
            handleObjectPlacement();
        }

        //// Handle object deletion
        if (m_currentTool == EditorTool::Erase && m_input->wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) {
            handleObjectDeletion();
        }
        (void)dt;
    }

    void LevelEditor::handleCameraInput(double dt) {
        if (!m_input) return;

        vec2 cameraMove(0.0f, 0.0f);

        // WASD movement
        if (m_input->isKeyDown(GLFW_KEY_W)) cameraMove.y += 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_S)) cameraMove.y -= 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_A)) cameraMove.x -= 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_D)) cameraMove.x += 1.0f;

        // Arrow key movement
        if (m_input->isKeyDown(GLFW_KEY_UP)) cameraMove.y += 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_DOWN)) cameraMove.y -= 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_LEFT)) cameraMove.x -= 1.0f;
        if (m_input->isKeyDown(GLFW_KEY_RIGHT)) cameraMove.x += 1.0f;

        // Apply movement
        if (magnitude(cameraMove) > 0.0f) {
            cameraMove = normalize(cameraMove);
			vec2 cameraPos = compat::to_vec2(m_editorCamera.position());

            vec2 newPos = cameraPos + cameraMove * m_cameraSpeed * static_cast<float>(dt);
			glm::vec2 Posnew = glm::vec2(newPos.x, newPos.y);

            m_editorCamera.setPosition(Posnew);
        }

        // Zoom with mouse wheel (would need scroll callback)
        // For now, use keys
        if (m_input->isKeyDown(GLFW_KEY_Q)) {
            float newZoom = m_editorCamera.zoom() * (1.0f + m_cameraZoomSpeed);
            m_editorCamera.setZoom(newZoom);
        }
        if (m_input->isKeyDown(GLFW_KEY_R)) {
            float newZoom = m_editorCamera.zoom() * (1.0f - m_cameraZoomSpeed);
            m_editorCamera.setZoom(std::max(0.1f, newZoom));
        }

        // Reset camera
        if (m_input->wasKeyPressed(GLFW_KEY_HOME)) {
            resetCamera();
        }
    }

    void LevelEditor::handleKeyboardShortcuts() {
        if (!m_input) return;

        bool ctrl = m_input->isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
            m_input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
        bool shift = m_input->isKeyDown(GLFW_KEY_LEFT_SHIFT) ||
            m_input->isKeyDown(GLFW_KEY_RIGHT_SHIFT);

        if (ctrl && m_input->wasKeyPressed(GLFW_KEY_N)) {
            newLevel("Untitled");
        }

        if (ctrl && !shift && m_input->wasKeyPressed(GLFW_KEY_S)) {
            if (m_currentLevelMetadata.filepath.empty()) {
                // Show save dialog if no filepath
                strncpy(m_fileDialogBuffer,
                    m_currentLevelMetadata.name.c_str(),
                    sizeof(m_fileDialogBuffer));
                m_showSaveDialog = true;
            }
            else {
                saveLevel();
            }
        }

        if (ctrl && shift && m_input->wasKeyPressed(GLFW_KEY_S)) {
            strncpy(m_fileDialogBuffer,
                m_currentLevelMetadata.name.c_str(),
                sizeof(m_fileDialogBuffer));
            m_showSaveDialog = true;
        }

        if (ctrl && m_input->wasKeyPressed(GLFW_KEY_O)) {
            m_showLoadDialog = true;
        }

        // Ctrl+D - Duplicate
        //if (ctrl && m_input->wasKeyPressed(GLFW_KEY_D)) {
        //    duplicateSelectedObject();
        //}

        // Delete - Delete selected
        if (m_input->wasKeyPressed(GLFW_KEY_DELETE)) {
            deleteSelectedObject();
        }

        // F5 - Play/Stop
        if (m_input->wasKeyPressed(GLFW_KEY_F5)) {
            togglePlayMode();
        }

        if (m_input->wasKeyPressed(GLFW_KEY_Q)) {
            m_currentTool = EditorTool::Select;
        }

        if (m_input->wasKeyPressed(GLFW_KEY_W)) {
            m_currentTool = EditorTool::Place;
        }

        if (m_input->wasKeyPressed(GLFW_KEY_E)) {
            m_currentTool = EditorTool::Erase;
        }
    }

    void LevelEditor::handleObjectSelection() {
        if (!m_input->wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) return;
        if (isMouseOverUI()) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());
        Gameplay::GameObject* obj = getObjectAtPosition(mouseWorld);

        selectObject(obj);
    }

    void LevelEditor::handleObjectPlacement() {
        if (isMouseOverUI()) return;
        if (m_selectedPrefab.empty()) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());

        createObjectFromPrefab(m_selectedPrefab, mouseWorld);
    }

    void LevelEditor::handleObjectDeletion() {
        if (isMouseOverUI()) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());
        Gameplay::GameObject* obj = getObjectAtPosition(mouseWorld);

        if (obj && m_currentScene) {
            selectObject(obj);
            deleteSelectedObject();
        }
    }

    void LevelEditor::handleObjectDragging() {
        if (!m_selectedObject) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());

        m_selectedObject->setPosition(mouseWorld);
        markLevelDirty();
    }

    void LevelEditor::moveSelectedObject(vec2 delta) {
        if (!m_selectedObject) return;

        vec2 newPos = m_selectedObject->transform().position + delta;

        m_selectedObject->setPosition(newPos);
        markLevelDirty();
    }

    /******************************************************************************/
    // State Management
    /******************************************************************************/

    void LevelEditor::saveEditorState() {
        if (!m_currentScene) return;

        // Save current scene to JSON for play mode backup
        std::string tempFile = "temp_editor_state.json";
        Gameplay::GameObjectSerializer::saveSceneToFile(
            m_currentScene->name(),
            m_currentScene->objects(),
            tempFile
        );
    }

    void LevelEditor::restoreEditorState() {
        // Restore scene from backup
        std::string tempFile = "temp_editor_state.json";

        std::string sceneName;
        std::vector<std::unique_ptr<Gameplay::GameObject>> objects;

        if (Gameplay::GameObjectSerializer::loadSceneFromFile(tempFile, sceneName, objects)) {
            m_currentScene = std::make_unique<Runtime::Scene>(sceneName);
            m_currentScene->load();
            m_currentScene->objects().clear();
            for (auto& obj : objects) {
                m_currentScene->addObject(std::move(obj)); 
            }

            m_selectedObject = nullptr;
        }
    }

    void LevelEditor::markLevelDirty() {
        m_levelDirty = true;
    }

    /******************************************************************************/
    // Utility Functions
    /******************************************************************************/

    Gameplay::GameObject* LevelEditor::getObjectAtPosition(vec2 worldPos) {
        if (!m_currentScene) return nullptr;

        // Check objects in reverse order (top to bottom)
        auto& objects = m_currentScene->objects();
        for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
            auto& obj = *it;

            // Simple AABB check
            auto aabb = obj->aabbWorld();
            if (worldPos.x >= aabb.min.x && worldPos.x <= aabb.max.x &&
                worldPos.y >= aabb.min.y && worldPos.y <= aabb.max.y) {
                return obj.get();
            }
        }

        return nullptr;
    }

    vec2 LevelEditor::screenToWorld(vec2 screenPos) const {
        // In a real implementation, this would use the camera's projection matrix
        // to properly convert screen coordinates to world coordinates

        // Simple implementation: adjust for camera position and zoom
        vec2 worldPos = screenPos;
		vec2 m_editorCameraPos = compat::to_vec2(m_editorCamera.position());
        worldPos -= vec2(m_windowWidth * 0.5f, m_windowHeight * 0.5f);
        worldPos /= m_editorCamera.zoom();
		worldPos += m_editorCameraPos;

        // Flip Y coordinate (screen space is top-down, world space is bottom-up)
        worldPos.y = -worldPos.y;

        return worldPos;
    }

    vec2 LevelEditor::worldToScreen(vec2 worldPos) const {
        // Inverse of screenToWorld
        vec2 screenPos = worldPos;
        vec2 m_editorCameraPos = compat::to_vec2(m_editorCamera.position());
        screenPos -= m_editorCameraPos;
        screenPos *= m_editorCamera.zoom();
        screenPos += vec2(m_windowWidth * 0.5f, m_windowHeight * 0.5f);
        screenPos.y = -screenPos.y;

        return screenPos;
    }

    bool LevelEditor::isMouseOverUI() const {
        // Check if mouse is over any ImGui window
        return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
            ImGui::IsAnyItemHovered();
    }

} // namespace Editor