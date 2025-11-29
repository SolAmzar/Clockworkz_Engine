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
#include "Gameplay/Physics2D.hpp"
#include "Math/ps_math.hpp"
#include <GLFW/glfw3.h>   
#include <cmath>

#include "Assets/SoundManager.hpp"
#include "Core/json.hpp"
#include "Gameplay/Components/AudioSource.hpp"

#include <IMGUI/imgui.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <Core/Engine.hpp>
#include <Runtime/PlayState.hpp>
#include <Assets/ResourceManager.hpp>
#include <Assets/TextureManager.hpp>
#include <IMGUI/imgui_internal.h>
#include <Gameplay/Components/Chase.hpp>
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <Gameplay/WallTiles.hpp>
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <Gameplay/Collider.hpp>

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
        , m_gridSize(100.0f)
        , m_enableGridSnap(true)
        , m_showGrid(true)
        , m_isTransforming(false)
        , m_playModePlayer(nullptr)
        , m_playModeLeftBound(100.0f)
        , m_playModeRightBound(700.0f)
        , m_playModeBallSpeed(220.0f)
        , m_playModeBallDir(1)
        , m_playModeDistanceSinceStep(0.0f)
        , m_playModeStepDistance(60.0f)
        , m_playModeTimeSinceStep(0.0f)
        , m_playModeMinStepInterval(0.22f)
        , m_playModeLastFootPos(0.0f, 0.0f)
        , m_playModeHaveLastFootPos(false)
        , m_loadedFromPlayState(false)
        , m_editorActive(false)
    {
        m_fileDialogBuffer[0] = '\0';
        mAssetNameBuffer[0] = '\0';
        m_transformStartRotation = 0;



        // Initialize level metadata
        m_currentLevelMetadata.name = "Untitled";
        m_currentLevelMetadata.filepath = "";
        m_currentLevelMetadata.description = "New Level";
        m_currentLevelMetadata.spawnPoint = vec2(0.0f, 0.0f);

        // Initialize gizmo state
        m_showGizmos = true;
        m_gizmoMode = GizmoMode::Translate;
        m_gizmoHoveredAxis = GizmoAxis::None;
        m_gizmoActiveAxis = GizmoAxis::None;
        m_isGizmoActive = false;

        m_gizmoDragStartWorld = vec2(0.0f, 0.0f);
        m_gizmoInitialPosition = vec2(0.0f, 0.0f);
        m_gizmoInitialRotation = 0.0f;
        m_gizmoInitialScale = vec2(1.0f, 1.0f);

        // Gizmo configuration
        m_gizmoSize = 80.0f;
        m_gizmoHandleSize = 12.0f;
        m_gizmoLineThickness = 2.0f;
        m_gizmoSnapEnabled = false;
        m_gizmoSnapAngle = 15.0f;
        m_gizmoSnapScale = 0.1f;

        // Gizmo colors (RGBA)
        m_gizmoColorX = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);      // Red
        m_gizmoColorY = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);      // Green
        m_gizmoColorBoth = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);   // Yellow
        m_gizmoColorHover = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        m_gizmoColorActive = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
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
        glfwGetWindowSize(window, &width, &height);
        m_windowWidth = static_cast<float>(width);
        m_windowHeight = static_cast<float>(height);

        // Initialize editor camera with proper viewport
        m_editorCamera.setViewport(width, height);
        m_editorCamera.setPosition(glm::vec2(0.0f, 0.0f));
        m_editorCamera.setZoom(1.0f);

        // Create default scene
        m_currentScene = std::make_unique<Runtime::Scene>("EditorScene");
        if (!m_currentScene->load()) {
            return false;
        }

        m_editorCamera.setViewport(width, height);  // Initially full window
        m_editorCamera.setPosition(glm::vec2(0.0f, 0.0f));
        m_editorCamera.setZoom(1.0f);

        // Load prefabs from directory
        loadPrefabs();

        // Register built-in prefabs
        registerBuiltInPrefabs();

        // Create framebuffer for scene view
        if (!m_sceneFramebuffer.create(1280, 720)) {
            m_logger->error("Failed to create scene framebuffer");
            return false;
        }
        m_sceneViewportSize = ImVec2(1280, 720);


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
        m_sceneFramebuffer.destroy();
    }

    /******************************************************************************/
    // Main Loop Functions
    /******************************************************************************/

    void LevelEditor::update(double dt) {
        m_frameTime = dt;

        if (m_mode == EditorMode::Play) {
            updatePlayMode(dt);
            // Still allow some editor input in play mode (like stopping)
            handleKeyboardShortcuts();
            return;
        }
        else {
            // In edit mode, handle editor-specific input and camera
            handleEditorInput(dt);
            handleKeyboardShortcuts();
        }

        if (m_windowManager) {
            auto dropped = m_windowManager->consumeDroppedFiles();
            if (!dropped.empty()) {
                std::cout << "[Editor] Consuming " << dropped.size() << " dropped file(s)\n";
                handleFileDrop(dropped);
            }
        }

        // Update object count for UI
        if (m_currentScene) {
            m_objectCount = static_cast<int>(m_currentScene->objects().size());
        }
    }

    void LevelEditor::initializePlayMode() {
        if (!m_currentScene) return;

        auto* physics = m_currentScene->getPhysics();
        if (!physics) {
            std::cerr << "  No physics system - collisions won't work!\n";
        }
        else {
            std::cout << "  Physics ready\n";
        }

        // Find player
        Gameplay::GameObject* player = nullptr;
        for (auto& obj : m_currentScene->objects()) {
            if (obj && obj->type() == Gameplay::GameObjectType::Player) {
                player = obj.get();
                m_playModePlayer = player;
                break;
            }
        }

        // Add Chase components to enemies
        if (player) {
            for (auto& obj : m_currentScene->objects()) {
                if (obj && obj->type() == Gameplay::GameObjectType::Enemy && obj->active()) {
                    obj->addComponent<Gameplay::Chase>(*obj, player, 150.0f, 300.0f);
                    std::cout << "  Added Chase to: " << obj->name() << "\n";
                }
            }
        }

        std::cout << "[LevelEditor] Play Mode initialized\n";
    }


    void LevelEditor::loadSceneFromPlayState(Runtime::Scene* playStateScene) {
        if (!playStateScene) return;

        m_loadedFromPlayState = true;
        m_currentScene = std::make_unique<Runtime::Scene>(playStateScene->name());

        for (auto& objPtr : playStateScene->objects()) {
            if (!objPtr) continue;

            auto clonedObj = objPtr->clone();
            m_currentScene->addObject(std::move(clonedObj));
        }

        m_currentLevelMetadata.name = playStateScene->name() + " (from PlayState)";
        m_currentScene->registerExistingObjectsToLayers();
    }

    void LevelEditor::syncSceneBackToPlayState(Runtime::Scene* playStateScene)
    {
        if (!playStateScene || !m_currentScene || !m_loadedFromPlayState) return;

        auto& playStateObjs = playStateScene->objects();
        auto& editorObjs = m_currentScene->objects();

        std::cout << "[LevelEditor] Syncing scene changes...\n";
        std::cout << "  Editor has " << editorObjs.size() << " objects\n";
        std::cout << "  PlayState has " << playStateObjs.size() << " objects\n";

        // Step 1 & 2: Sync by INDEX (not by name)
        int minSize = static_cast<int>(std::min(editorObjs.size(), playStateObjs.size()));

        for (int i = 0; i < minSize; i++) {
            auto& editorObj = editorObjs[i];
            auto& playObj = playStateObjs[i];

            if (!editorObj || !playObj) continue;

            // Update in-place (same memory address)
            playObj->transform().position = editorObj->transform().position;
            playObj->transform().rotationRad = editorObj->transform().rotationRad;
            playObj->transform().scale = editorObj->transform().scale;
            playObj->setSize(editorObj->size());
            playObj->setLayer(editorObj->layer());
            playObj->setActive(editorObj->active());
            playObj->setTextureId(editorObj->textureId());
        }

        // Step 3: If editor has MORE objects, add them
        if (editorObjs.size() > playStateObjs.size()) {
            for (int i = minSize; i < editorObjs.size(); i++) {
                auto& editorObj = editorObjs[i];
                if (!editorObj) continue;

                auto newObj = std::make_unique<Gameplay::GameObject>(
                    editorObj->type(), editorObj->name());
                newObj->transform().position = editorObj->transform().position;
                newObj->transform().rotationRad = editorObj->transform().rotationRad;
                newObj->transform().scale = editorObj->transform().scale;
                newObj->setSize(editorObj->size());
                newObj->setLayer(editorObj->layer());
                newObj->setActive(editorObj->active());

                std::cout << "  + Adding new object: " << editorObj->name() << "\n";
                playStateScene->addObject(std::move(newObj));
            }
        }

        // Step 4: If PlayState has MORE objects, remove them
        if (playStateObjs.size() > editorObjs.size()) {
            int removeCount = static_cast<int>(playStateObjs.size() - editorObjs.size());
            for (int i = 0; i < removeCount; i++) {
                std::cout << "  - Removing extra object\n";
                playStateObjs.pop_back();
            }
        }

        std::cout << "[LevelEditor] Sync complete:\n";
        std::cout << "  Final count: " << playStateObjs.size() << " objects\n";
    }


    void LevelEditor::updatePlayMode(double dt) {
        if (!m_currentScene) {
            m_logger->error("updatePlayMode: No current scene!");
            setMode(EditorMode::Edit);
            return;
        }

        // Find player
        if (!m_playModePlayer) {
            for (auto& obj : m_currentScene->objects()) {
                if (obj && obj->type() == Gameplay::GameObjectType::Player) {
                    m_playModePlayer = obj.get();
                    break;
                }
            }
        }

        // Update scene first
        m_currentScene->update(dt);

        // ✅ PLAYER MOVEMENT (exactly like PlayState)
        if (m_playModePlayer && m_playModePlayer->active()) {
            Core::Engine& engine = Core::Engine::Get();
            auto& audio = engine.audio();
            auto& steps = engine.surfaceAudio();

            m_playModeTimeSinceStep += static_cast<float>(dt);
            auto surfaceType = getSurfaceUnder(m_playModePlayer);
            vec2 curPos = m_playModePlayer->transform().position;
            float speed = magnitude(m_playModePlayer->velocity() * 0.3f);
            bool moving = speed > 5.0f;

            if (moving) {
                if (!m_playModeHaveLastFootPos) {
                    m_playModeLastFootPos = curPos;
                    m_playModeHaveLastFootPos = true;
                }
                vec2 delta = curPos - m_playModeLastFootPos;
                float frameDist = magnitude(delta);
                m_playModeDistanceSinceStep += frameDist;
                m_playModeLastFootPos = curPos;

                if (m_playModeDistanceSinceStep >= m_playModeStepDistance &&
                    m_playModeTimeSinceStep >= m_playModeMinStepInterval) {
                    // Time for exactly ONE footstep
                    steps.playFootstep(audio, surfaceType, speed);

                    m_playModeDistanceSinceStep = 0.0f;
                    m_playModeTimeSinceStep = 0.0f;
                }
            }
            else {
                // Not moving → reset
                m_playModeDistanceSinceStep = 0.0f;
                m_playModeTimeSinceStep = 0.0f;
                m_playModeHaveLastFootPos = false;
            }

            // WASD input → movement vector
            vec2 move{ 0, 0 };
            if (m_input->isKeyDown(GLFW_KEY_W)) move.y += 1;
            if (m_input->isKeyDown(GLFW_KEY_S)) move.y -= 1;
            if (m_input->isKeyDown(GLFW_KEY_A)) move.x -= 1;
            if (m_input->isKeyDown(GLFW_KEY_D)) move.x += 1;

            // Normalize diagonal
            if (move.x != 0 || move.y != 0)
                move = normalize(move);

            // Apply speed
            float speedMove = m_playModePlayer->moveSpeed();

            // Shift to run
            bool isRunning = false;
            if (m_input->isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
                speedMove *= 2.0f;
                isRunning = true;
            }

            // Final velocity
            m_playModePlayer->velocity() = move * speedMove;

            // ✅ Player animation
            Gameplay::SpriteAnimatorComponent* playerSpriteComp = m_playModePlayer->getComponent<Gameplay::SpriteAnimatorComponent>();
            if (playerSpriteComp) {
                std::string animState = m_flashlightOn ? "idle_torch" : "idle";
                bool isMoving = (move.x != 0 || move.y != 0);

                if (isMoving) {
                    animState = m_flashlightOn ? "walk_torch" : "walk";
                    if (isRunning) {
                        animState = m_flashlightOn ? "run_torch" : "run";
                    }
                }

                playerSpriteComp->play(animState);
            }

            // ✅ Player rotation to face mouse
            vec2 mouseScreen = m_input->mousePosition();
            vec2 mouseWorld = screenToWorld(mouseScreen);
            vec2 playerPos = m_playModePlayer->transform().position;
            vec2 dir = mouseWorld - playerPos;
            float dist = magnitude(dir);

            if (dist > 10.0f) {
                dir = normalize(dir);
                float angleRad = std::atan2(dir.y, dir.x);
                m_playModePlayer->transform().rotationRad = angleRad;
            }
        }

        // Camera follow player
        if (m_playModePlayer) {
            vec2 pos = m_playModePlayer->transform().position;
            m_editorCamera.setPosition(glm::vec2(pos.x, pos.y));
        }

        // Handle flashlight toggle
        if (m_input->wasKeyPressed(GLFW_KEY_R)) {
            m_flashlightOn = !m_flashlightOn;
        }

        // Update battery
        if (m_flashlightOn) {
            m_playerBattery -= 10.0f * static_cast<float>(dt);
            if (m_playerBattery <= 0.0f) {
                m_playerBattery = 0.0f;
                m_flashlightOn = false;
            }
        }
        else {
            m_playerBattery += 5.0f * static_cast<float>(dt);
            m_playerBattery = std::min(100.0f, m_playerBattery);
        }

        // Enemy collision detection
        if (m_playModePlayer && m_playModePlayer->active()) {
            auto* scene = m_currentScene.get();
            for (auto& obj : scene->objects()) {
                if (!obj || !obj->active()) continue;

                if (obj->type() == Gameplay::GameObjectType::Enemy) {
                    vec2 playerPos = m_playModePlayer->transform().position;
                    vec2 enemyPos = obj->transform().position;

                    vec2 playerHalf = m_playModePlayer->size() * 0.5f * 2.0f;
                    vec2 enemyHalf = obj->size() * 0.5f * 2.0f;

                    if (std::abs(playerPos.x - enemyPos.x) < (playerHalf.x + enemyHalf.x) &&
                        std::abs(playerPos.y - enemyPos.y) < (playerHalf.y + enemyHalf.y)) {

                        std::cout << "Player hit by enemy: " << obj->name() << "!\n";
                        m_playModePlayer->setActive(false);
                        break;
                    }
                }
            }
        }

        // Player respawn
        if (m_playModePlayer && !m_playModePlayer->active()) {
            if (!m_playModePlayerRespawning) {
                m_playModePlayer->respawnAt(vec2(0.0f, 0.0f), true, true);
                std::cout << "[LevelEditor] Player respawned!\n";
                m_playModePlayerRespawning = true;
                resetScene();
            }
            return;
        }

        // Reset flag when player is active again
        if (m_playModePlayer && m_playModePlayer->active()) {
            m_playModePlayerRespawning = false;
        }

        // Update light exposure
        updateLightExposure(dt);
    }

    Gameplay::SurfaceType LevelEditor::getSurfaceUnder(Gameplay::GameObject* player)
    {
        auto* scene = m_currentScene.get();
        if (!scene) return Gameplay::SurfaceType::Default;

        vec2 pos = player->transform().position;

        for (auto& obj : scene->objects()) {
            if (!obj || !obj->active()) continue;

            if (auto* surf = obj->getComponent<Gameplay::SurfaceComponent>()) {
                vec2 half = obj->size() * 0.5f;
                if (std::abs(pos.x - obj->transform().position.x) < half.x &&
                    std::abs(pos.y - obj->transform().position.y) < half.y) {
                    return surf->type();
                }
            }
        }

        return Gameplay::SurfaceType::Default;
    }

void LevelEditor::resetScene()
{
    if (!m_currentScene) return;

    // Reload the scene
    m_currentScene->load();

    // Reset player reference
    m_playModePlayer = nullptr;
    for (auto& obj : m_currentScene->objects()) {
        if (obj && obj->type() == Gameplay::GameObjectType::Player) {
            m_playModePlayer = obj.get();
            break;
        }
    }

    // Re-register objects to layers
    m_currentScene->registerExistingObjectsToLayers();

    // Reinitialize enemy AI
    if (m_playModePlayer) {
        for (auto& obj : m_currentScene->objects()) {
            if (obj && obj->type() == Gameplay::GameObjectType::Enemy && obj->active()) {
                obj->addComponent<Gameplay::Chase>(*obj, m_playModePlayer, 150.0f, 300.0f);
            }
        }
    }

    std::cout << "[LevelEditor] Scene reset complete\n";
}



// ============================================================================
// Wall Auto-Tiling
// ============================================================================

    void LevelEditor::autoTileWalls() {
        if (!m_currentScene) return;

        auto& objects = m_currentScene->objects();

        // Collect all wall objects with sprite renderers using wall texture
        struct WallCell {
            Gameplay::GameObject* obj;
            glm::ivec2 cell;
        };

        std::vector<WallCell> walls;
        walls.reserve(objects.size());

        // Determine a base cell size from the first wall
        vec2 cellSize(1.0f, 1.0f);
        bool cellSizeSet = false;

        for (auto& u : objects) {
            auto* obj = u.get();
            if (!obj) continue;
            if (obj->type() != Gameplay::GameObjectType::Wall) continue;

            auto* sr = obj->getComponent<Gameplay::SpriteRendererComponent>();
            if (!sr) continue;

            if (!cellSizeSet) {
                cellSize = obj->size();
                if (cellSize.x == 0) cellSize.x = 1.0f;
                if (cellSize.y == 0) cellSize.y = 1.0f;
                cellSizeSet = true;
            }

            glm::vec2 p = glm::vec2(obj->transform().position.x,
                obj->transform().position.y);
            glm::ivec2 cell{
                static_cast<int>(std::round(p.x / cellSize.x)),
                static_cast<int>(std::round(p.y / cellSize.y))
            };

            walls.push_back({ obj, cell });
        }

        if (walls.empty()) return;

        struct Vec2Hash {
            std::size_t operator()(const glm::ivec2& v) const noexcept {
                return (std::hash<int>()(v.x) * 73856093) ^ (std::hash<int>()(v.y) * 19349663);
            }
        };

        std::unordered_map<glm::ivec2, Gameplay::GameObject*, Vec2Hash> wallMap;
        wallMap.reserve(walls.size());
        for (auto& w : walls) {
            wallMap[w.cell] = w.obj;
        }

        auto hasWallAt = [&](int x, int y) -> bool {
            return wallMap.find(glm::ivec2{ x, y }) != wallMap.end();
            };

        // Decide tile per wall based on 4-neighbour connectivity
        for (auto& w : walls) {
            glm::ivec2 c = w.cell;
            bool up = hasWallAt(c.x, c.y + 1);
            bool down = hasWallAt(c.x, c.y - 1);
            bool left = hasWallAt(c.x - 1, c.y);
            bool right = hasWallAt(c.x + 1, c.y);

            Gameplay::WallTile tile = Gameplay::WallTile::EdgeLeft;

            // Fully surrounded = center
            if (up && down && left && right) {
                tile = Gameplay::WallTile::Center;
            }
            // Edges
            else if (!up && !down && left && right) {
                tile = Gameplay::WallTile::EdgeTop;
            }
            else if (!up && !down && left && right) {
                tile = Gameplay::WallTile::EdgeBottom;
            }
            else if (up && down && !left && !right) {
                tile = Gameplay::WallTile::EdgeLeft;
            }
            else if (up && down && !left && !right) {
                tile = Gameplay::WallTile::EdgeRight;
            }
            // Corners
            else if (!up && down && !left && right) {
                tile = Gameplay::WallTile::CornerTL;
            }
            else if (!up && down && left && !right) {
                tile = Gameplay::WallTile::CornerTR;
            }
            else if (up && !down && !left && right) {
                tile = Gameplay::WallTile::CornerBL;
            }
            else if (up && !down && left && !right) {
                tile = Gameplay::WallTile::CornerBR;
            }
            else {
                //tile = Gameplay::WallTile::Center;
            }

            if (auto* sr = w.obj->getComponent<Gameplay::SpriteRendererComponent>()) {
                sr->setUV(compat::to_glm(Gameplay::GetWallTileUV(tile)));
            }
        }

        markLevelDirty();
    }


    // ============================================================================
    // Undo/redo Implementation
    // ============================================================================
    void LevelEditor::executeCommand(std::unique_ptr<Platform::EditorCommand> cmd) {
        // Execute the command
        cmd->execute();

        // Add to undo stack
        m_undoStack.push_back(std::move(cmd));

        // Limit undo stack to MAX_UNDO_LEVELS
        if (m_undoStack.size() > MAX_UNDO_LEVELS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        // Clear redo stack (can't redo after new action)
        clearRedoStack();

        // Mark level as dirty
        markLevelDirty();
    }

    void LevelEditor::undo() {
        if (m_undoStack.empty()) {
            std::cout << "Nothing to undo\n";
            return;
        }

        // 1. Take the command OUT of undo stack
        std::unique_ptr<Platform::EditorCommand> cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        // 2. Execute the undo
        cmd->undo();

        // 3. Move it to redo stack (so user can redo if they want)
        m_redoStack.push_back(std::move(cmd));

        markLevelDirty();
        std::cout << "Undo successful\n";
    }

    void LevelEditor::redo() {
        if (m_redoStack.empty()) {
            std::cout << "Nothing to redo\n";
            return;
        }

        // 1. Take the command OUT of redo stack
        std::unique_ptr<Platform::EditorCommand> cmd = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        // 2. Execute it again (calls execute(), not undo())
        cmd->execute();

        // 3. Move it back to undo stack (so user can undo again if they want)
        m_undoStack.push_back(std::move(cmd));

        markLevelDirty();
        std::cout << "Redo successful\n";
    }




    void LevelEditor::clearRedoStack() {
        m_redoStack.clear();
    }

    void LevelEditor::recordMoveCommand(Gameplay::GameObject* obj, vec2 oldPos, vec2 newPos) {
        auto cmd = std::make_unique<Platform::MoveCommand>(obj, oldPos, newPos);
        m_undoStack.push_back(std::move(cmd));

        if (m_undoStack.size() > MAX_UNDO_LEVELS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        clearRedoStack();
        markLevelDirty();
    }

    void LevelEditor::recordRotateCommand(Gameplay::GameObject* obj, float oldRot, float newRot) {
        auto cmd = std::make_unique<Platform::RotateCommand>(obj, oldRot, newRot);
        m_undoStack.push_back(std::move(cmd));

        if (m_undoStack.size() > MAX_UNDO_LEVELS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        clearRedoStack();
        markLevelDirty();
    }

    void LevelEditor::recordScaleCommand(Gameplay::GameObject* obj, vec2 oldScale, vec2 newScale) {
        auto cmd = std::make_unique<Platform::ScaleCommand>(obj, oldScale, newScale);
        m_undoStack.push_back(std::move(cmd));

        if (m_undoStack.size() > MAX_UNDO_LEVELS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        clearRedoStack();
        markLevelDirty();
    }

// ============================================================================
// gizmo Implementation
// ============================================================================




void LevelEditor::renderGizmoTranslate() {
    vec2 center = getGizmoCenterPosition();
    float scaledSize = getGizmoScaledSize();

    // Draw X axis (horizontal arrow)
    {
        glm::vec3 color = (m_gizmoActiveAxis == GizmoAxis::X) ? glm::vec3(m_gizmoColorActive) :
                         (m_gizmoHoveredAxis == GizmoAxis::X) ? glm::vec3(m_gizmoColorHover) :
                         glm::vec3(m_gizmoColorX);

        vec2 endPos = vec2(center.x + scaledSize, center.y);
        m_renderer->drawLine(center.x, center.y, endPos.x, endPos.y,
                           color.r, color.g, color.b);
        
        // Arrow head
        float arrowSize = m_gizmoHandleSize;
        m_renderer->drawLine(endPos.x, endPos.y, 
                           endPos.x - arrowSize, endPos.y - arrowSize/2,
                           color.r, color.g, color.b);
        m_renderer->drawLine(endPos.x, endPos.y,
                           endPos.x - arrowSize, endPos.y + arrowSize/2,
                           color.r, color.g, color.b);
    }

    // Draw Y axis (vertical arrow)
    {
        glm::vec3 color = (m_gizmoActiveAxis == GizmoAxis::Y) ? glm::vec3(m_gizmoColorActive) :
                         (m_gizmoHoveredAxis == GizmoAxis::Y) ? glm::vec3(m_gizmoColorHover) :
                         glm::vec3(m_gizmoColorY);

        vec2 endPos = vec2(center.x, center.y + scaledSize);
        m_renderer->drawLine(center.x, center.y, endPos.x, endPos.y,
                           color.r, color.g, color.b);
        
        // Arrow head
        float arrowSize = m_gizmoHandleSize;
        m_renderer->drawLine(endPos.x, endPos.y,
                           endPos.x - arrowSize/2, endPos.y - arrowSize,
                           color.r, color.g, color.b);
        m_renderer->drawLine(endPos.x, endPos.y,
                           endPos.x + arrowSize/2, endPos.y - arrowSize,
                           color.r, color.g, color.b);
    }

    // Draw center point
    m_renderer->drawCircle(center.x, center.y, m_gizmoHandleSize/2,
                          1.0f, 1.0f, 1.0f, 16);
}

void LevelEditor::renderGizmoRotate() {
    vec2 center = getGizmoCenterPosition();
    float scaledSize = getGizmoScaledSize();

    // Choose color based on state
    glm::vec3 color;
    if (m_isGizmoActive) {
        color = glm::vec3(m_gizmoColorActive);
    } else if (m_gizmoHoveredAxis != GizmoAxis::None) {
        color = glm::vec3(m_gizmoColorHover);
    } else {
        color = glm::vec3(m_gizmoColorBoth);
    }

    // Draw rotation circle
    m_renderer->drawCircle(center.x, center.y, scaledSize, color.r, color.g, color.b, 64);

    // Draw rotation handles (4 cardinal directions)
    const int numHandles = 4;
    const float PI = 3.14159265359f;
    for (int i = 0; i < numHandles; ++i) {
        float angle = (PI * 2.0f * i) / numHandles;
        vec2 handlePos = vec2(
            center.x + std::cos(angle) * scaledSize,
            center.y + std::sin(angle) * scaledSize
        );
        
        m_renderer->drawCircle(handlePos.x, handlePos.y, m_gizmoHandleSize, 
                             color.r, color.g, color.b, 16);
    }

    // Draw angle indicator if active
    if (m_isGizmoActive && m_selectedObject) {
        float currentRotation = m_selectedObject->transform().rotationRad;
        vec2 directionEnd = vec2(
            center.x + std::cos(currentRotation) * scaledSize * 1.5f,
            center.y + std::sin(currentRotation) * scaledSize * 1.5f
        );
        
        m_renderer->drawLine(center.x, center.y, directionEnd.x, directionEnd.y,
                           color.r, color.g, color.b);
    }
}

void LevelEditor::renderGizmoScale() {
    vec2 center = getGizmoCenterPosition();
    float scaledSize = getGizmoScaledSize();

    // Draw X axis (horizontal)
    {
        glm::vec3 color = (m_gizmoActiveAxis == GizmoAxis::X) ? glm::vec3(m_gizmoColorActive) :
                         (m_gizmoHoveredAxis == GizmoAxis::X) ? glm::vec3(m_gizmoColorHover) :
                         glm::vec3(m_gizmoColorX);

        vec2 endPos = vec2(center.x + scaledSize, center.y);
        m_renderer->drawLine(center.x, center.y, endPos.x, endPos.y, 
                           color.r, color.g, color.b);
        
        // Draw handle at the end
        m_renderer->drawRect(endPos.x - m_gizmoHandleSize/2, endPos.y - m_gizmoHandleSize/2,
                           m_gizmoHandleSize, m_gizmoHandleSize, color.r, color.g, color.b);
    }

    // Draw Y axis (vertical)
    {
        glm::vec3 color = (m_gizmoActiveAxis == GizmoAxis::Y) ? glm::vec3(m_gizmoColorActive) :
                         (m_gizmoHoveredAxis == GizmoAxis::Y) ? glm::vec3(m_gizmoColorHover) :
                         glm::vec3(m_gizmoColorY);

        vec2 endPos = vec2(center.x, center.y + scaledSize);
        m_renderer->drawLine(center.x, center.y, endPos.x, endPos.y,
                           color.r, color.g, color.b);
        
        // Draw handle at the end
        m_renderer->drawRect(endPos.x - m_gizmoHandleSize/2, endPos.y - m_gizmoHandleSize/2,
                           m_gizmoHandleSize, m_gizmoHandleSize, color.r, color.g, color.b);
    }

    // Draw center handle for uniform scaling
    {
        glm::vec3 color = (m_gizmoActiveAxis == GizmoAxis::Both) ? glm::vec3(m_gizmoColorActive) :
                         (m_gizmoHoveredAxis == GizmoAxis::Both) ? glm::vec3(m_gizmoColorHover) :
                         glm::vec3(m_gizmoColorBoth);

        m_renderer->drawRect(center.x - m_gizmoHandleSize/2, center.y - m_gizmoHandleSize/2,
                           m_gizmoHandleSize, m_gizmoHandleSize, color.r, color.g, color.b);
    }
}

// Modify updateGizmoTranslate():
void LevelEditor::updateGizmoTranslate() {
    if (!m_selectedObject) return;

    vec2 mouseScreen = m_input->mousePosition();
    vec2 mouseWorld = screenToWorld(mouseScreen);

    if (!m_isGizmoActive) {
        m_gizmoHoveredAxis = getGizmoHoveredAxis(mouseWorld);
    }

    if (m_input->wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (m_gizmoHoveredAxis != GizmoAxis::None) {
            m_isGizmoActive = true;
            m_gizmoActiveAxis = m_gizmoHoveredAxis;
            m_gizmoDragStartWorld = mouseWorld;
            m_gizmoInitialPosition = m_selectedObject->transform().position;

            // RECORD START POSITION FOR UNDO
            m_transformStartPosition = m_selectedObject->transform().position;
            m_isTransforming = true;
        }
    }

    if (m_isGizmoActive && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        vec2 dragDelta = vec2(
            mouseWorld.x - m_gizmoDragStartWorld.x,
            mouseWorld.y - m_gizmoDragStartWorld.y
        );

        if (m_gizmoActiveAxis == GizmoAxis::X) {
            dragDelta.y = 0.0f;
        }
        else if (m_gizmoActiveAxis == GizmoAxis::Y) {
            dragDelta.x = 0.0f;
        }

        vec2 newPos = vec2(
            m_gizmoInitialPosition.x + dragDelta.x,
            m_gizmoInitialPosition.y + dragDelta.y
        );

        if (m_enableGridSnap) {
            newPos = snapToHalfGrid(newPos);
        }

        m_selectedObject->transform().position = newPos;
        markLevelDirty();
    }

    if (!m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        // RECORD COMMAND WHEN RELEASING MOUSE
        if (m_isGizmoActive && m_isTransforming) {
            vec2 finalPosition = m_selectedObject->transform().position;

            // Only record if position actually changed
            if (m_transformStartPosition.x != finalPosition.x ||
                m_transformStartPosition.y != finalPosition.y) {
                recordMoveCommand(m_selectedObject, m_transformStartPosition, finalPosition);
            }

            m_isTransforming = false;
        }

        m_isGizmoActive = false;
        m_gizmoActiveAxis = GizmoAxis::None;
    }
}

// Modify updateGizmoRotate():
void LevelEditor::updateGizmoRotate() {
    if (!m_selectedObject) return;

    vec2 mouseScreen = m_input->mousePosition();
    vec2 mouseWorld = screenToWorld(mouseScreen);
    vec2 center = getGizmoCenterPosition();
    float scaledSize = getGizmoScaledSize();

    if (m_input->wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        float distToCenter = glm::length(glm::vec2(mouseWorld.x - center.x, mouseWorld.y - center.y));
        float innerRadius = scaledSize * 0.8f;
        float outerRadius = scaledSize * 1.2f;

        if (distToCenter >= innerRadius && distToCenter <= outerRadius) {
            m_isGizmoActive = true;
            m_gizmoDragStartWorld = mouseWorld;
            m_gizmoInitialRotation = m_selectedObject->transform().rotationRad;

            // RECORD START ROTATION FOR UNDO
            m_transformStartRotation = m_selectedObject->transform().rotationRad;
            m_isTransforming = true;
        }
    }

    if (m_isGizmoActive && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        vec2 startDir = vec2(
            m_gizmoDragStartWorld.x - center.x,
            m_gizmoDragStartWorld.y - center.y
        );
        vec2 currentDir = vec2(
            mouseWorld.x - center.x,
            mouseWorld.y - center.y
        );

        float startLen = std::sqrt(startDir.x * startDir.x + startDir.y * startDir.y);
        float currentLen = std::sqrt(currentDir.x * currentDir.x + currentDir.y * currentDir.y);

        if (startLen > 0.001f) startDir = vec2(startDir.x / startLen, startDir.y / startLen);
        if (currentLen > 0.001f) currentDir = vec2(currentDir.x / currentLen, currentDir.y / currentLen);

        float startAngle = std::atan2(startDir.y, startDir.x);
        float currentAngle = std::atan2(currentDir.y, currentDir.x);
        float deltaAngle = currentAngle - startAngle;

        if (m_gizmoSnapEnabled) {
            deltaAngle = snapAngleToGrid(deltaAngle);
        }

        m_selectedObject->transform().rotationRad = m_gizmoInitialRotation + deltaAngle;
        markLevelDirty();
    }

    if (!m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        // RECORD COMMAND WHEN RELEASING MOUSE
        if (m_isGizmoActive && m_isTransforming) {
            float finalRotation = m_selectedObject->transform().rotationRad;

            // Only record if rotation actually changed
            if (std::abs(m_transformStartRotation - finalRotation) > 0.001f) {
                recordRotateCommand(m_selectedObject, m_transformStartRotation, finalRotation);
            }

            m_isTransforming = false;
        }

        m_isGizmoActive = false;
    }

    if (!m_isGizmoActive) {
        float distToCenter = glm::length(glm::vec2(mouseWorld.x - center.x, mouseWorld.y - center.y));
        float innerRadius = scaledSize * 0.8f;
        float outerRadius = scaledSize * 1.2f;
        m_gizmoHoveredAxis = (distToCenter >= innerRadius && distToCenter <= outerRadius)
            ? GizmoAxis::Both : GizmoAxis::None;
    }
}

// Modify updateGizmoScale():
void LevelEditor::updateGizmoScale()
{
    if (!m_selectedObject) return;

    vec2 mouseScreen = m_input->mousePosition();
    vec2 mouseWorld = screenToWorld(mouseScreen);
    vec2 center = getGizmoCenterPosition();
    float scaledSize = getGizmoScaledSize();

    if (!m_isGizmoActive) {
        m_gizmoHoveredAxis = getGizmoHoveredAxis(mouseWorld);
    }

    if (m_input->wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (m_gizmoHoveredAxis != GizmoAxis::None) {
            m_isGizmoActive = true;
            m_gizmoActiveAxis = m_gizmoHoveredAxis;
            m_gizmoDragStartWorld = mouseWorld;
            m_gizmoInitialScale = m_selectedObject->transform().scale;
            m_transformStartScale = m_selectedObject->transform().scale;
            m_isTransforming = true;
        }
    }

    if (m_isGizmoActive && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        vec2 dragDelta = vec2(
            mouseWorld.x - m_gizmoDragStartWorld.x,
            mouseWorld.y - m_gizmoDragStartWorld.y);

        vec2 newScale = m_gizmoInitialScale;

        switch (m_gizmoActiveAxis) {
        case GizmoAxis::X: {
            float scaleChange = dragDelta.x / scaledSize;
            newScale.x = m_gizmoInitialScale.x + scaleChange;
            newScale.x = std::max(0.1f, newScale.x);
            break;
        }
        case GizmoAxis::Y: {
            float scaleChange = dragDelta.y / scaledSize;
            newScale.y = m_gizmoInitialScale.y + scaleChange;
            newScale.y = std::max(0.1f, newScale.y);
            break;
        }
        case GizmoAxis::Both: {
            float initialDist = std::sqrt(
                (m_gizmoDragStartWorld.x - center.x) * (m_gizmoDragStartWorld.x - center.x) +
                (m_gizmoDragStartWorld.y - center.y) * (m_gizmoDragStartWorld.y - center.y));

            float currentDist = std::sqrt(
                (mouseWorld.x - center.x) * (mouseWorld.x - center.x) +
                (mouseWorld.y - center.y) * (mouseWorld.y - center.y));

            float scaleFactor = currentDist / std::max(0.001f, initialDist);

            newScale = vec2(
                m_gizmoInitialScale.x * scaleFactor,
                m_gizmoInitialScale.y * scaleFactor);

            newScale.x = std::max(0.1f, newScale.x);
            newScale.y = std::max(0.1f, newScale.y);
            break;
        }
        default:
            break;
        }

        if (m_gizmoSnapEnabled) {
            newScale.x = snapScaleToGrid(newScale.x);
            newScale.y = snapScaleToGrid(newScale.y);
        }

        m_selectedObject->transform().scale = newScale;
        markLevelDirty();
    }

    if (!m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        if (m_isGizmoActive && m_isTransforming) {
            vec2 finalScale = m_selectedObject->transform().scale;

            if (std::abs(m_transformStartScale.x - finalScale.x) > 0.001f ||
                std::abs(m_transformStartScale.y - finalScale.y) > 0.001f) {
                recordScaleCommand(m_selectedObject, m_transformStartScale, finalScale);
            }

            m_isTransforming = false;
        }

        m_isGizmoActive = false;
        m_gizmoActiveAxis = GizmoAxis::None;
    }
}


// ============================================================================
// GIZMO render Implementation
// ============================================================================

    void LevelEditor::renderGizmo() {
        if (!m_selectedObject) return;

        switch (m_gizmoMode) {
        case GizmoMode::Translate:
            renderGizmoTranslate();
            break;
        case GizmoMode::Rotate:
            renderGizmoRotate();
            break;
        case GizmoMode::Scale:
            renderGizmoScale();
            break;
        default:
            break;
        }
    }

    // ============================================================================
    // Save Dialog Implementation
    // ============================================================================

    static int promptSaveDiscardCancel() {
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
// GIZMO HELPER FUNCTIONS - Add these as new methods
// ============================================================================

    vec2 LevelEditor::snapToHalfGrid(vec2 position) const {
        float halfGrid = m_gridSize * 0.5f;
        position.x = std::round(position.x / halfGrid) * halfGrid;
        position.y = std::round(position.y / halfGrid) * halfGrid;
        return position;
    }

    bool LevelEditor::isMouseOverGizmo(const vec2& mouseWorldPos) const {
        if (!m_selectedObject || !m_showGizmos) return false;

        vec2 center = getGizmoCenterPosition();
        float scaledSize = getGizmoScaledSize();
        float threshold = m_gizmoHandleSize * 1.5f;

        switch (m_gizmoMode) {
        case GizmoMode::Translate: {
            // Check X axis
            vec2 xEnd = vec2(center.x + scaledSize, center.y);
            float distToXAxis = std::abs(mouseWorldPos.y - center.y);
            if (mouseWorldPos.x >= center.x && mouseWorldPos.x <= xEnd.x &&
                distToXAxis < threshold) {
                return true;
            }

            // Check Y axis
            vec2 yEnd = vec2(center.x, center.y + scaledSize);
            float distToYAxis = std::abs(mouseWorldPos.x - center.x);
            if (mouseWorldPos.y >= center.y && mouseWorldPos.y <= yEnd.y &&
                distToYAxis < threshold) {
                return true;
            }
            break;
        }

        case GizmoMode::Rotate: {
            // Check if within rotation circle
            float distToCenter = std::sqrt(
                (mouseWorldPos.x - center.x) * (mouseWorldPos.x - center.x) +
                (mouseWorldPos.y - center.y) * (mouseWorldPos.y - center.y)
            );
            float innerRadius = scaledSize * 0.8f;
            float outerRadius = scaledSize * 1.2f;
            if (distToCenter >= innerRadius && distToCenter <= outerRadius) {
                return true;
            }
            break;
        }

        case GizmoMode::Scale: {
            // Check center handle
            float distToCenter = std::sqrt(
                (mouseWorldPos.x - center.x) * (mouseWorldPos.x - center.x) +
                (mouseWorldPos.y - center.y) * (mouseWorldPos.y - center.y)
            );
            if (distToCenter < m_gizmoHandleSize) {
                return true;
            }

            // Check X handle
            vec2 xHandle = vec2(center.x + scaledSize, center.y);
            float distToXHandle = std::sqrt(
                (mouseWorldPos.x - xHandle.x) * (mouseWorldPos.x - xHandle.x) +
                (mouseWorldPos.y - xHandle.y) * (mouseWorldPos.y - xHandle.y)
            );
            if (distToXHandle < threshold) {
                return true;
            }

            // Check Y handle
            vec2 yHandle = vec2(center.x, center.y + scaledSize);
            float distToYHandle = std::sqrt(
                (mouseWorldPos.x - yHandle.x) * (mouseWorldPos.x - yHandle.x) +
                (mouseWorldPos.y - yHandle.y) * (mouseWorldPos.y - yHandle.y)
            );
            if (distToYHandle < threshold) {
                return true;
            }
            break;
        }

        default:
            break;
        }

        return false;
    }


    GizmoAxis LevelEditor::getGizmoHoveredAxis(const vec2& mouseWorldPos) const {
        if (!m_selectedObject) return GizmoAxis::None;

        vec2 center = getGizmoCenterPosition();
        float scaledSize = getGizmoScaledSize();
        float threshold = m_gizmoHandleSize * 1.5f;

        if (m_gizmoMode == GizmoMode::Scale) {
            // Check center handle for uniform scaling
            float distToCenter = std::sqrt(
                (mouseWorldPos.x - center.x) * (mouseWorldPos.x - center.x) +
                (mouseWorldPos.y - center.y) * (mouseWorldPos.y - center.y)
            );
            if (distToCenter < m_gizmoHandleSize) {
                return GizmoAxis::Both;
            }

            // Check X axis handle
            vec2 xHandle = vec2(center.x + scaledSize, center.y);
            float distToXHandle = std::sqrt(
                (mouseWorldPos.x - xHandle.x) * (mouseWorldPos.x - xHandle.x) +
                (mouseWorldPos.y - xHandle.y) * (mouseWorldPos.y - xHandle.y)
            );
            if (distToXHandle < threshold) {
                return GizmoAxis::X;
            }

            // Check Y axis handle
            vec2 yHandle = vec2(center.x, center.y + scaledSize);
            float distToYHandle = std::sqrt(
                (mouseWorldPos.x - yHandle.x) * (mouseWorldPos.x - yHandle.x) +
                (mouseWorldPos.y - yHandle.y) * (mouseWorldPos.y - yHandle.y)
            );
            if (distToYHandle < threshold) {
                return GizmoAxis::Y;
            }
        }
        else if (m_gizmoMode == GizmoMode::Translate) {
            // Check distance to X axis line
            vec2 xEnd = vec2(center.x + scaledSize, center.y);
            float distToXAxis = std::abs(mouseWorldPos.y - center.y);
            if (mouseWorldPos.x >= center.x && mouseWorldPos.x <= xEnd.x &&
                distToXAxis < threshold) {
                return GizmoAxis::X;
            }

            // Check distance to Y axis line
            vec2 yEnd = vec2(center.x, center.y + scaledSize);
            float distToYAxis = std::abs(mouseWorldPos.x - center.x);
            if (mouseWorldPos.y >= center.y && mouseWorldPos.y <= yEnd.y &&
                distToYAxis < threshold) {
                return GizmoAxis::Y;
            }
        }

        return GizmoAxis::None;
    }

    vec2 LevelEditor::getGizmoCenterPosition() const {
        if (!m_selectedObject) return vec2(0.0f, 0.0f);
        return m_selectedObject->transform().position;
    }

    float LevelEditor::getGizmoScaledSize() const {
        // Make gizmo size consistent regardless of zoom
        // Adjust this based on your camera zoom implementation
        return m_gizmoSize;
    }

    float LevelEditor::snapAngleToGrid(float angle) const {
        if (!m_gizmoSnapEnabled) return angle;

        const float PI = 3.14159265359f;
        float snapRadians = m_gizmoSnapAngle * (PI / 180.0f);
        return std::round(angle / snapRadians) * snapRadians;
    }

    float LevelEditor::snapScaleToGrid(float scale) const {
        if (!m_gizmoSnapEnabled) return scale;

        return std::round(scale / m_gizmoSnapScale) * m_gizmoSnapScale;
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

    /******************************************************************************/
    // Grid Rendering
    /******************************************************************************/

    void LevelEditor::renderGrid() {
        if (!m_showGrid || !m_renderer) return;

        // Calculate visible area in world space
        vec2 topLeft = screenToWorld(vec2(0, 0));
        vec2 bottomRight = screenToWorld(vec2(m_windowWidth, m_windowHeight));

        // Ensure topLeft is actually top-left and bottomRight is bottom-right
        if (topLeft.x > bottomRight.x) std::swap(topLeft.x, bottomRight.x);
        if (topLeft.y > bottomRight.y) std::swap(topLeft.y, bottomRight.y);

        // Expand the grid bounds to span much further (5x viewport size)
        float width = bottomRight.x - topLeft.x;
        float height = bottomRight.y - topLeft.y;
        topLeft.x -= width * 2.0f;
        topLeft.y -= height * 2.0f;
        bottomRight.x += width * 2.0f;
        bottomRight.y += height * 2.0f;

        // Calculate grid boundaries (snap to grid)
        float startX = std::floor(topLeft.x / m_gridSize) * m_gridSize;
        float startY = std::floor(topLeft.y / m_gridSize) * m_gridSize;
        float endX = std::ceil(bottomRight.x / m_gridSize) * m_gridSize;
        float endY = std::ceil(bottomRight.y / m_gridSize) * m_gridSize;

        float gridR = 1.0f, gridG = 0.0f, gridB = 0.0f;
        float axisR = 0.8f, axisG = 0.9f, axisB = 1.0f;

        // Draw vertical lines
        for (float x = startX; x <= endX; x += m_gridSize) {
            // Use axis color for origin line (x = 0)
            bool isOrigin = std::abs(x) < 0.01f;
            float r = isOrigin ? axisR : gridR;
            float g = isOrigin ? axisG : gridG;
            float b = isOrigin ? axisB : gridB;

            m_renderer->drawLine(x, topLeft.y, x, bottomRight.y, r, g, b);
        }

        // Draw horizontal lines
        for (float y = startY; y <= endY; y += m_gridSize) {
            // Use axis color for origin line (y = 0)
            bool isOrigin = std::abs(y) < 0.01f;
            float r = isOrigin ? axisR : gridR;
            float g = isOrigin ? axisG : gridG;
            float b = isOrigin ? axisB : gridB;

            m_renderer->drawLine(topLeft.x, y, bottomRight.x, y, r, g, b);
        }
    }

    /******************************************************************************/
    // Scene Rendering
    /******************************************************************************/

    void LevelEditor::render() {
        if (!m_currentScene) {
            std::cout << "ERROR: No scene to render!\n";
            return;
        }

        //std::cout << "Rendering to framebuffer...\n";
        m_sceneFramebuffer.bind();

        // Clear the framebuffer (this is your blue background!)
        glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Get camera's view-projection matrix
        glm::mat4 viewProj = m_editorCamera.viewProj();

        // Render sprites/objects using your Scene's render method
        m_renderer->beginScenePass(viewProj);
        m_currentScene->render(*m_renderer); 
        m_renderer->endScenePass();

        if (m_mode == EditorMode::Play) {
            renderFlashlight();
        }

        // Render debug/editor elements (only in Edit mode)
        if (m_mode == EditorMode::Edit) {
            m_renderer->beginDebugPass(viewProj);

            // Render grid
            if (m_showGrid) {
                renderGrid();
            }

            // Render gizmo if object is selected
            if (m_selectedObject && m_showGizmos) {
                renderGizmo();
            }

            m_renderer->endDebugPass();
        }

        m_sceneFramebuffer.unbind();
            //std::cout << "Framebuffer render complete\n";

        if (m_windowManager) {
            glViewport(0, 0, m_windowManager->width(), m_windowManager->height());
        }
        else {
            glViewport(0, 0, static_cast<int>(m_windowWidth),
                static_cast<int>(m_windowHeight));
        }
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
        if(ImGui::IsDragDropActive())renderSceneViewDropZone();

        // Always show scene view
        if (m_showSceneView) renderSceneView();

        renderAssetBrowser();

        if (showReplaceDialog) {
            renderReplace();
        }
        renderSaveDialog();
        renderLoadDialog();
        renderErrorPopup();
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
            initializePlayMode();
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

        m_undoStack.clear();
        m_redoStack.clear();
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

            std::cout << "[LevelEditor] Successfully saved level: " << std::filesystem::absolute(filepath) << "\n";
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
        m_undoStack.clear();
        m_redoStack.clear();
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

        // Create delete command (this will handle the soft delete via setActive(false))
        auto cmd = std::make_unique<Platform::DeleteCommand>(m_selectedObject, m_currentScene.get());
        executeCommand(std::move(cmd));

        // Deselect after command is executed
        m_selectedObject = nullptr;

        markLevelDirty();

        if (Core::Engine::IsInitialized()) {
            Core::Engine::Get().logger().info("Deleted object from scene");
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

        std::cout << "[LevelEditor] Creating: " << prefabName << " at ("
            << position.x << ", " << position.y << ")\n";

        // Create the object
        auto obj = Gameplay::PrefabFactory::get().createAt(prefabName, position);
        if (!obj) {
            std::cout << "[LevelEditor] Failed to create prefab\n";
            return;
        }

        Gameplay::GameObject* objPtr = obj.get();
        m_currentScene->addObject(std::move(obj));

        // Record the creation as an undo command
        auto cmd = std::make_unique<Platform::CreateCommand>(objPtr, m_currentScene.get());
        executeCommand(std::move(cmd));

        markLevelDirty();
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

      /*  m_prefabPalette.push_back({
           "Wall", "Wall", "Terrain",
           "", Gameplay::GameObjectType::WallCornerBL
            });*/

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
    // Drag-and-Drop Support
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
            else if (filePath.extension() == ".wav"
                || filePath.extension() == ".ogg"
                || filePath.extension() == ".mp3")
            {
                using nlohmann::json;

                // logical sound id based on filename (e.g. "door_open")
                std::string name = filePath.stem().string();

                // Decide where to store inside your project
                fs::path dstDir = fs::path("assets") / "audio" / "imported";
                fs::create_directories(dstDir);

                fs::path dstPath = dstDir / filePath.filename();  // e.g. assets/audio/imported/door_open.ogg

                // Copy from original location into assets folder
                std::error_code ec;
                fs::copy_file(filePath, dstPath,
                    fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    std::cout << "[LevelEditor] Failed to copy audio file: "
                        << filePath << " -> " << dstPath
                        << " (" << ec.message() << ")\n";
                    continue;
                }

                // Build the path string as used by the engine (forward slashes)
                std::string enginePath = dstPath.generic_string();

                // Simple heuristic: stream long formats (mp3/ogg), sample wav
                bool stream = (filePath.extension() == ".mp3" || filePath.extension() == ".ogg");
                bool loop = false;
                bool is3D = false;

                // 4a) Load immediately into SoundManager/AudioManager
                auto clip = Assets::SoundManager::LoadSound(
                    name,
                    enginePath,
                    loop,
                    stream,
                    is3D
                );

                if (!clip) {
                    std::cout << "[LevelEditor] Failed to load audio clip: " << name << "\n";
                    continue;
                }

                std::cout << "[LevelEditor] Imported audio '" << name
                    << "' from '" << path
                    << "' to '" << enginePath << "'\n";

                // 4b) Append entry to assets/configs/sounds.json so it persists
                const std::string configPath = "assets/configs/sounds.json";

                json j;
                {
                    std::ifstream in(configPath);
                    if (in) {
                        try {
                            in >> j;
                        }
                        catch (...) {
                            // if file is corrupt, fall back to empty
                            j = json::object();
                        }
                    }
                    else {
                        // file did not exist yet
                        j = json::object();
                    }
                }

                if (!j.contains("sounds") || !j["sounds"].is_array()) {
                    j["sounds"] = json::array();
                }

                json entry;
                entry["name"] = name;
                entry["path"] = enginePath;
                entry["loop"] = loop;
                entry["stream"] = stream;
                entry["is3D"] = is3D;

                j["sounds"].push_back(entry);

                {
                    std::ofstream out(configPath);
                    if (!out) {
                        std::cout << "[LevelEditor] Failed to open sounds.json for writing\n";
                    }
                    else {
                        out << j.dump(4);
                        std::cout << "[LevelEditor] Updated sounds.json with entry '" << name << "'\n";
                    }
                }

                // optional: mark editor dirty if you treat audio as part of level data
                // markLevelDirty();
            }
            else {
                // This is where you satisfy the rubric example: dropping wrong things like .txt
                // when trying to use the audio pipeline.
                queueErrorPopup(
                    "Unsupported File Type",
                    "The file:\n\n" + path +
                    "\n\nis not a supported asset type for this editor.\n"
                    "Supported audio formats: .ogg, .wav, .mp3\n"
                    "Supported image formats: .png, .jpg\n"
                    "Level files: .json\n"
                    "Prefab files: /prefabs/*.json"
                );
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
        ImGui::Begin("Toolbar");

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

        // Grid controls
        ImGui::Checkbox("Grid Snap", &m_enableGridSnap);
        ImGui::SameLine();
        ImGui::Checkbox("Show Grid", &m_showGrid);
        ImGui::SameLine();

        // Gizmo mode
        ImGui::Separator();
        ImGui::Text("Gizmo Tools");

        // Mode buttons
        if (ImGui::RadioButton("Translate (Q)", m_gizmoMode == GizmoMode::Translate)) {
            m_gizmoMode = GizmoMode::Translate;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate (W)", m_gizmoMode == GizmoMode::Rotate)) {
            m_gizmoMode = GizmoMode::Rotate;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale (E)", m_gizmoMode == GizmoMode::Scale)) {
            m_gizmoMode = GizmoMode::Scale;
        }

        // Snap settings
        ImGui::Checkbox("Enable Snap (Ctrl)", &m_gizmoSnapEnabled);

        if (m_gizmoMode == GizmoMode::Rotate) {
            ImGui::SliderFloat("Snap Angle", &m_gizmoSnapAngle, 1.0f, 90.0f, "%.0f°");
        }

        if (m_gizmoMode == GizmoMode::Scale) {
            ImGui::SliderFloat("Snap Scale", &m_gizmoSnapScale, 0.01f, 1.0f, "%.2f");
        }

        ImGui::Checkbox("Show Gizmos", &m_showGizmos);
        ImGui::Separator();
        ImGui::Text("History");

        // Undo button
        if (ImGui::Button("Undo (Ctrl+Z)")) {
            undo();
        }
        if (!m_undoStack.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu)", m_undoStack.size());
        }

        // Redo button
        if (ImGui::Button("Redo (Ctrl+Y)")) {
            redo();
        }
        if (!m_redoStack.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu)", m_redoStack.size());
        }

        // Show undo stack
        if (ImGui::CollapsingHeader("Undo History")) {
            ImGui::Text("Recent Actions:");
            ImGui::Indent();

            // Show last 10 actions
            int showCount = std::min(10, static_cast<int>(m_undoStack.size()));
            for (int i = 0; i < showCount; ++i) {
                int index = static_cast<int>(m_undoStack.size()) - 1 - i;
                ImGui::BulletText("%s", m_undoStack[index]->getName().c_str());
            }

            if (m_undoStack.size() > 10) {
                ImGui::BulletText("... and %zu more", m_undoStack.size() - 10);
            }

            ImGui::Unindent();
        }

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

                if (!obj->active()) {
                    continue;
                }

                // Apply search filter
                if (searchBuffer[0] != '\0') {
                    if (obj->name().find(searchBuffer) == std::string::npos) {
                        continue;
                    }
                }

                // Determine icon based on type
                const char* icon = "?";
                switch (obj->type()) {
                case Gameplay::GameObjectType::Floor:  icon = "F"; break;
                case Gameplay::GameObjectType::Wall:   icon = "W"; break;
                case Gameplay::GameObjectType::Player: icon = "P"; break;
                case Gameplay::GameObjectType::Enemy:  icon = "E"; break;
                case Gameplay::GameObjectType::Trap:   icon = "T"; break;
                case Gameplay::GameObjectType::Ball:   icon = "B"; break;
                case Gameplay::GameObjectType::Box:    icon = "B"; break;
                default:                               icon = "?"; break;
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
        const char* typeNames[] = { "Floor", "Wall", "Player", "Enemy", "Trap", "Unknown", "Ball"};
        int typeIndex = static_cast<int>(m_selectedObject->type());
        ImGui::Text("Type: %s", typeNames[typeIndex]);

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& transform = m_selectedObject->transform();

            // Position
            vec2 pos = transform.position;
            if (ImGui::DragFloat2("Position", &pos.x, 1.0f)) {
                m_selectedObject->setPosition(pos);
                markLevelDirty();
            }

            // Rotation (convert to degrees for UI display)
            float rotationDeg = transform.rotationRad * (180.0f / 3.14159265359f);
            if (ImGui::DragFloat("Rotation", &rotationDeg, 1.0f, -360.0f, 360.0f, "%.1f°")) {
                transform.rotationRad = rotationDeg * (3.14159265359f / 180.0f);
                markLevelDirty();
            }

            // Scale
            vec2 scale = transform.scale;
            if (ImGui::DragFloat2("Scale", &scale.x, 0.01f, 0.01f, 10.0f)) {
                transform.scale = scale;
                markLevelDirty();
            }

            // Size (collider size)
            vec2 size = m_selectedObject->size();
            if (ImGui::DragFloat2("Size", &size.x, 1.0f, 1.0f, 1000.0f)) {
                m_selectedObject->setSize(size);
                markLevelDirty();
            }

            ImGui::Separator();

            // Quick reset buttons
            if (ImGui::Button("Reset Rotation")) {
                m_selectedObject->transform().rotationRad = 0.0f;
                markLevelDirty();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Scale")) {
                m_selectedObject->transform().scale = vec2(1.0f, 1.0f);
                markLevelDirty();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Position")) {
                m_selectedObject->transform().position = vec2(0.0f, 0.0f);
                markLevelDirty();
            }
        }
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
        if (ImGui::CollapsingHeader("Rendering")) {
            // Texture ID
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

            // Drag-and-drop target for texture thumbnails
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_ID")) {
                    const char* droppedName = (const char*)payload->Data;
                    m_selectedObject->setTextureId(droppedName);
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
        if (ImGui::CollapsingHeader("Components")) {
            //ImGui::TextDisabled("Component list would appear here");
            // In a real implementation, list and edit components
                using Gameplay::AudioSourceComponent;

                // Try to get an existing AudioSourceComponent on the selected object
                auto* audio = m_selectedObject->getComponent<AudioSourceComponent>();

                // If there is no audio component yet, allow adding one
                if (!audio) {
                    if (ImGui::Button("Add Audio Source Component")) {
                        audio = m_selectedObject->addComponent<AudioSourceComponent>();
                        markLevelDirty();
                    }
                }

                // If we now have an audio component, show its controls
                if (audio) {
                    if (ImGui::TreeNode("Audio Source")) {

                        // --- Sound ID dropdown using SoundManager ---
                        auto soundNames = Assets::SoundManager::GetLoadedSoundNames();
                        if (!soundNames.empty()) {
                            // Build an array of const char* for ImGui
                            std::vector<const char*> items;
                            items.reserve(soundNames.size());
                            int currentIndex = 0;

                            for (std::size_t i = 0; i < soundNames.size(); ++i) {
                                items.push_back(soundNames[i].c_str());
                                if (soundNames[i] == audio->soundId) {
                                    currentIndex = static_cast<int>(i);
                                }
                            }

                            if (ImGui::Combo("Sound", &currentIndex,
                                items.data(),
                                static_cast<int>(items.size()))) {
                                audio->soundId = soundNames[currentIndex];
                                markLevelDirty();
                            }
                        }
                        else {
                            ImGui::TextDisabled("No sounds loaded. Check sounds.json.");
                        }

                        // --- Volume slider ---
                        float vol = audio->volume;
                        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
                            audio->volume = vol;
                            markLevelDirty();
                        }

                        // --- Loop checkbox ---
                        bool loop = audio->loop;
                        if (ImGui::Checkbox("Loop", &loop)) {
                            audio->loop = loop;
                            markLevelDirty();
                        }

                        // --- Play on start checkbox ---
                        bool playOnStart = audio->playOnStart;
                        if (ImGui::Checkbox("Play On Start", &playOnStart)) {
                            audio->playOnStart = playOnStart;
                            markLevelDirty();
                        }

                        // --- Optional: group selection (SFX/BGM) as ints or radio buttons ---
                        int group = static_cast<int>(audio->group);
                        if (ImGui::RadioButton("SFX", group == static_cast<int>(Audio::Group::SFX))) {
                            group = static_cast<int>(Audio::Group::SFX);
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("BGM", group == static_cast<int>(Audio::Group::BGM))) {
                            group = static_cast<int>(Audio::Group::BGM);
                        }
                        if (group != static_cast<int>(audio->group)) {
                            audio->group = static_cast<Audio::Group>(group);
                            markLevelDirty();
                        }

                        // --- Preview button to test sound immediately ---
                        if (ImGui::Button("Preview")) {
                            if (!audio->soundId.empty()) {
                                Core::Engine::Get().audio().playOneShot(
                                    audio->soundId,
                                    audio->group,
                                    audio->volume
                                );
                            }
                        }
                        ImGui::TreePop();
                    }
                }
                using Gameplay::SpriteRendererComponent;

                // Sprite Renderer inspector
                if (auto* sr = m_selectedObject->getComponent<SpriteRendererComponent>()) {
                    if (ImGui::TreeNode("Sprite Renderer")) {
                        // Show / edit texture alias
                        char texAlias[256];
                        std::strncpy(texAlias, sr->textureAlias().c_str(), sizeof(texAlias));
                        if (ImGui::InputText("Texture Alias", texAlias, sizeof(texAlias))) {
                            sr->setTexture(texAlias);
                            m_selectedObject->setTextureId(texAlias);
                            markLevelDirty();
                        }

                        // Show current UV
                        glm::vec4 uv = sr->uv();
                        ImGui::Text("UV: (%.3f, %.3f, %.3f, %.3f)", uv.x, uv.y, uv.z, uv.w);

                        // --- Wall tile dropdown, only if this is a wall ---
                        if (m_selectedObject->type() == Gameplay::GameObjectType::Wall) {
                            static const char* tileNames[] = {
                                "CornerTL", "EdgeTop", "CornerTR",
                                "EdgeLeft", "Center", "EdgeRight",
                                "CornerBL", "EdgeBottom", "CornerBR"
                            };

                            // Try to detect current tile from UV
                            int currentIndex = 4; // default Center
                            auto epsEqual = [](float a, float b) {
                                return std::abs(a - b) < 0.0005f;
                                };
                            auto uvEquals = [&](const glm::vec4& a, const glm::vec4& b) {
                                return epsEqual(a.x, b.x) && epsEqual(a.y, b.y) &&
                                    epsEqual(a.z, b.z) && epsEqual(a.w, b.w);
                                };

                            Gameplay::WallTile allTiles[] = {
                                Gameplay::WallTile::CornerTL,
                                Gameplay::WallTile::EdgeTop,
                                Gameplay::WallTile::CornerTR,
                                Gameplay::WallTile::EdgeLeft,
                                Gameplay::WallTile::Center,
                                Gameplay::WallTile::EdgeRight,
                                Gameplay::WallTile::CornerBL,
                                Gameplay::WallTile::EdgeBottom,
                                Gameplay::WallTile::CornerBR
                            };

                            for (int i = 0; i < 9; ++i) {
                                if (uvEquals(uv, compat::to_glm(Gameplay::GetWallTileUV(allTiles[i])))) {
                                    currentIndex = i;
                                    break;
                                }
                            }

                            int selectedIndex = currentIndex;
                            if (ImGui::Combo("Wall Tile", &selectedIndex, tileNames, IM_ARRAYSIZE(tileNames))) {
                                Gameplay::WallTile chosen = allTiles[selectedIndex];
                                sr->setUV(compat::to_glm(Gameplay::GetWallTileUV(chosen)));
                                markLevelDirty();
                            }

                            if (ImGui::Button("Auto-tile all walls")) {
                                autoTileWalls();
                            }
                        }

                        ImGui::TreePop();
                    }
                }
                else {
                    ImGui::TextDisabled("No SpriteRendererComponent on this object");
                }
        }

        

        ImGui::Separator();
        if (ImGui::Button("Duplicate")) {
            //duplicateSelectedObject();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            deleteSelectedObject();
        }

        ImGui::End();
    }

    void LevelEditor::copyComponents(Gameplay::GameObject* source, Gameplay::GameObject* dest) {
        if (!source || !dest) return;

        // Copy SpriteRenderer FIRST
        Gameplay::SpriteRendererComponent* destRenderer = nullptr;
        if (auto* srcRenderer = source->getComponent<Gameplay::SpriteRendererComponent>()) {
            destRenderer = dest->addComponent<Gameplay::SpriteRendererComponent>();
            destRenderer->setTint(srcRenderer->tint());
            destRenderer->setUV(srcRenderer->uv());

            if (!srcRenderer->textureAlias().empty()) {
                destRenderer->setTexture(srcRenderer->textureAlias());
            }
        }

        // Copy SpriteAnimator
        if (auto* srcAnimator = source->getComponent<Gameplay::SpriteAnimatorComponent>()) {
            std::string filename = srcAnimator->getFilename();

            if (!filename.empty()) {
                auto* destAnimator = dest->addComponent<Gameplay::SpriteAnimatorComponent>();

                std::ifstream file(filename);
                if (file.is_open()) {
                    nlohmann::json j;
                    file >> j;
                    destAnimator->loadFromJson(j);

                    if (destRenderer) {
                        destAnimator->setSpriteRenderer(destRenderer);
                    }
                }
            }
        }
    }

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

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PREFAB_NAME",
                    prefab.name.c_str(),
                    prefab.name.length() + 1);
                ImGui::Text("Drag: %s", prefab.displayName.c_str());
                ImGui::EndDragDropSource();
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
        // Remove padding for clean scene view

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Scene View");
        ImGui::PopStyleVar();

        m_sceneViewHovered = ImGui::IsWindowHovered();
        m_sceneViewFocused = ImGui::IsWindowFocused();

        ImVec2 winPos = ImGui::GetCursorScreenPos();
        m_sceneViewPosition = winPos;

        // Get available content region (space inside window)
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

        // Resize framebuffer if window size changed
        if (viewportPanelSize.x != m_sceneViewportSize.x ||
            viewportPanelSize.y != m_sceneViewportSize.y) {

            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                // 1. Update the Framebuffer size
                m_sceneFramebuffer.resize((int)viewportPanelSize.x, (int)viewportPanelSize.y);

                // 2. Update your tracking variables for Input Math
                m_sceneViewportSize = viewportPanelSize;
                m_windowWidth = viewportPanelSize.x;
                m_windowHeight = viewportPanelSize.y;

                // [IMPORTANT] 3. Update the Camera Projection to match!
                // This prevents the "Displacement" drift when resizing.
                m_editorCamera.setViewport((int)m_windowWidth, (int)m_windowHeight);
            }
        }

        // ⭐ THIS IS THE CRITICAL PART - Display framebuffer as image
        GLuint texID = m_sceneFramebuffer.getTextureID();
        if (texID != 0) {
            ImGui::Image(
                (void*)(intptr_t)texID,      // Framebuffer texture
                viewportPanelSize,            // Size to display
                ImVec2(0, 1),                 // UV top-left (flipped Y)
                ImVec2(1, 0)                  // UV bottom-right
            );
        }
        else {
            ImGui::Text("ERROR: Framebuffer texture is invalid!");
        }

        // Handle drag-drop of prefabs into scene view
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_NAME")) {
                const char* prefabName = (const char*)payload->Data;
                ImVec2 mouseImGui = ImGui::GetMousePos();
                vec2 mouseScreen(mouseImGui.x, mouseImGui.y);
                vec2 mouseWorld = screenToWorld(mouseScreen);
                vec2 snappedWorld = snapToGrid(mouseWorld);
                float halfGrid = 0.5f * m_gridSize;
                vec2 finalPos = snappedWorld + vec2(halfGrid, halfGrid);

                createObjectFromPrefab(prefabName, finalPos);
            }
            ImGui::EndDragDropTarget();
        }

        // Optional: Display scene info overlay
        ImGui::SetCursorPos(ImVec2(10, 30));
        ImGui::BeginChild("SceneInfo", ImVec2(250, 80), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
        ImGui::Text("Camera: (%.1f, %.1f)",
            m_editorCamera.position().x,
            m_editorCamera.position().y);
        ImGui::Text("Zoom: %.2f", m_editorCamera.zoom());
        ImGui::Text("Objects: %d", m_objectCount);
        ImGui::EndChild();

        ImGui::End();
    }

    //void LevelEditor::renderSceneView() {
    //    ImGui::Begin("Scene View");

    //    // Display scene statistics
    //    ImGui::Text("Camera Position: (%.1f, %.1f)",
    //        m_editorCamera.position().x,
    //        m_editorCamera.position().y);
    //    ImGui::Text("Zoom: %.2f", m_editorCamera.zoom());
    //    ImGui::Text("Objects: %d", m_objectCount);

    //    ImGui::Separator();

    //    // Camera controls
    //    if (ImGui::Button("Reset Camera")) {
    //        resetCamera();
    //    }

    //    ImGui::SameLine();

    //    if (ImGui::Button("Frame Selected")) {
    //        if (m_selectedObject) {
    //            glm::vec2 selObjectPos = glm::vec2(m_selectedObject->transform().position.x, m_selectedObject->transform().position.y);
    //            m_editorCamera.setPosition(selObjectPos);
    //        }
    //    }

    //    ImGui::End();
    //}

    void LevelEditor::renderSceneViewDropZone() {
        ImVec2 scenePos = ImVec2(0, 0);
        ImVec2 sceneSize = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(scenePos);
        ImGui::SetNextWindowSize(sceneSize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        bool opened = ImGui::Begin("SceneDropOverlay", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();

        if (opened) {
            ImGui::InvisibleButton("DropZone", sceneSize);

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_NAME")) {
                    const char* prefabName = (const char*)payload->Data;
                    ImVec2 mouseImGui = ImGui::GetMousePos();
                    vec2 mouseScreen(mouseImGui.x, mouseImGui.y);
                    vec2 mouseWorld = screenToWorld(mouseScreen);
                    vec2 snappedWorld = snapToGrid(mouseWorld);
                    float halfGrid = 0.5f * m_gridSize;
                    vec2 finalPos = snappedWorld + vec2(halfGrid, halfGrid);
                    createObjectFromPrefab(prefabName, finalPos);
                }
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::End(); // Make sure this always executes
    }




    void LevelEditor::renderAssetBrowser()
    {
        ImGui::Begin("Assets Browser");

        // Create tabs for different asset types
        if (ImGui::BeginTabBar("AssetTabs")) {

            // Texture Assets Tab
            if (ImGui::BeginTabItem("Textures")) {
                renderTextureAssets();
                ImGui::EndTabItem();
            }

            // Audio Assets Tab
            if (ImGui::BeginTabItem("Audio")) {
                renderAudioAssets();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void LevelEditor::renderTextureAssets()
    {
        std::vector<std::string> names = Assets::TextureManager::GetLoadedTextureNames();
        static std::string selectedTextureName;

        for (const auto& name : names) {
            ImGui::PushID(name.c_str());
            bool isSelected = (selectedTextureName == name);

            if (ImGui::Selectable(name.c_str(), isSelected)) {
                selectedTextureName = name;
            }

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
                if (ImGui::ImageButton("##thumb", (ImTextureID)(intptr_t)tex->id(), ImVec2(64, 64))) {
                    // Click-to-select or preview
                }

                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("TEXTURE_ID", name.c_str(), name.length() + 1);
                    ImGui::Text("Drag: %s", name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
            ImGui::PopID();
        }
    }

    void LevelEditor::renderAudioAssets()
    {
        std::vector<std::string> names = Assets::SoundManager::GetLoadedSoundNames();
        static std::string selectedSoundName;

        for (const auto& name : names) {
            ImGui::PushID(name.c_str());
            bool isSelected = (selectedSoundName == name);

            // Selectable - show name
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                selectedSoundName = name;
            }

            // Context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Play")) {
                    auto clip = Assets::SoundManager::Get(name);
                    if (clip) {
                        // Play the sound (you'll need access to AudioManager)
                        // audioManager->PlaySound(clip);
                    }
                }
                if (ImGui::MenuItem("Stop")) {
                    // Stop the sound if playing
                }
                if (ImGui::MenuItem("Delete")) {
                    Assets::SoundManager::Unload(name);
                }
                ImGui::EndPopup();
            }

            // Show audio icon or info
            auto clip = Assets::SoundManager::Get(name);
            if (clip) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Audio)");

                // Optionally show audio properties
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Name: %s", name.c_str());
                    // Add more properties if available
                    ImGui::EndTooltip();
                }
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("AUDIO_CLIP", name.c_str(), name.length() + 1);
                ImGui::Text("Drag: %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();
        }

        // Button to load new audio
        if (ImGui::Button("Load Audio File...")) {
            // Open file dialog and load new audio
            // This would be similar to your texture loading logic
        }
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

    void LevelEditor::handleEditorInput(double dt) {
        if (!m_input) return;

        if (m_selectedObject && m_showGizmos && m_mode == EditorMode::Edit) {
            switch (m_gizmoMode) {
            case GizmoMode::Translate:
                updateGizmoTranslate();
                break;
            case GizmoMode::Rotate:
                updateGizmoRotate();
                break;
            case GizmoMode::Scale:
                updateGizmoScale();
                break;
            default:
                break;
            }
        }

        // Handle object selection and dragging
        if (m_currentTool == EditorTool::Select && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
            if (!isMouseOverUI()) {
                vec2 mouseWorld = screenToWorld(m_input->mousePosition());

                // On first click
                if (m_input->wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
                    if (m_selectedObject && isMouseOverGizmo(mouseWorld)) {
                        // Gizmo will handle this - do nothing here
                        return;
                    }

                    Gameplay::GameObject* obj = getObjectAtPosition(mouseWorld);
                    if (obj) {
                        selectObject(obj);
                        m_isDraggingObject = true;
                    }
                    else {
                        selectObject(nullptr);
                        m_isDraggingObject = false;
                    }
                }
                // Continue dragging on subsequent frames (only if gizmo isn't active)
                else if (m_selectedObject && m_isDraggingObject && !m_isGizmoActive) {
                    handleObjectDragging();
                }
            }
        }

        // STOP dragging when mouse is released
        if (m_input->wasMouseReleased(GLFW_MOUSE_BUTTON_LEFT)) {
            m_isDraggingObject = false;
        }

        // Handle object placement
        if (m_currentTool == EditorTool::Place && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
            handleObjectPlacement();
        }

        // Handle object deletion
        if (m_currentTool == EditorTool::Erase && m_input->isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
            handleObjectDeletion();
        }

        if (m_currentTool == EditorTool::Pan) {
            handleCameraInput(dt);
        }
    }


    void LevelEditor::handleCameraInput(double dt) {
        if (!m_input) return;

        if (!m_sceneViewFocused) return;

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
        if (m_input->isKeyDown(GLFW_KEY_Q))
        {
            float newZoom = m_editorCamera.zoom() * (1.0f + m_cameraZoomSpeed * static_cast<float>(dt));
            m_editorCamera.setZoom(newZoom);
        }

        if (m_input->isKeyDown(GLFW_KEY_R))
        {
            float newZoom = m_editorCamera.zoom() * (1.0f - m_cameraZoomSpeed * static_cast<float>(dt));
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

        //if (m_input->wasKeyPressed(GLFW_KEY_Q)) {
        //    m_currentTool = EditorTool::Select;
        //}

        //if (m_input->wasKeyPressed(GLFW_KEY_W)) {
        //    m_currentTool = EditorTool::Place;
        //}

        //if (m_input->wasKeyPressed(GLFW_KEY_E)) {
        //    m_currentTool = EditorTool::Erase;
        //}

        if (m_input->wasKeyPressed(GLFW_KEY_Z)) {
            m_gizmoMode = GizmoMode::Translate;
        }
        if (m_input->wasKeyPressed(GLFW_KEY_X)) {
            undo();
            //m_gizmoMode = GizmoMode::Rotate;
        }
        if (m_input->wasKeyPressed(GLFW_KEY_C)) {
            redo();
            //m_gizmoMode = GizmoMode::Scale;
        }

        // Undo with Ctrl+Z
        if ((m_input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || m_input->isKeyDown(GLFW_KEY_RIGHT_CONTROL)) &&
            m_input->wasKeyPressed(GLFW_KEY_Z)) {
            undo();
        }

        // Redo with Ctrl+Y (or Ctrl+Shift+Z)
        if ((m_input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || m_input->isKeyDown(GLFW_KEY_RIGHT_CONTROL)) &&
            m_input->wasKeyPressed(GLFW_KEY_Y)) {
            redo();
        }

        // Toggle snapping with Ctrl
        m_gizmoSnapEnabled = m_input->isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
            m_input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    }

    void LevelEditor::handleObjectSelection() {
        if (!m_input->wasMouseClicked(GLFW_MOUSE_BUTTON_LEFT)) return;
        if (isMouseOverUI()) return;

        vec2 mouseScreen = m_input->mousePosition();
        vec2 localPos = mouseScreen - vec2(m_sceneViewPosition.x, m_sceneViewPosition.y);

        if (localPos.x < 0 || localPos.y < 0 ||
            localPos.x > m_sceneViewportSize.x || localPos.y > m_sceneViewportSize.y) {
            return;
        }

        vec2 mouseWorld = screenToWorld(mouseScreen);

        if (m_selectedObject && isMouseOverGizmo(mouseWorld)) {
            return; // Let gizmo handle the click
        }

        Gameplay::GameObject* obj = getObjectAtPosition(mouseWorld);

        //// Show the conversion steps
        //vec2 centered = mouseScreen - vec2(m_windowWidth * 0.5f, m_windowHeight * 0.5f);
        //vec2 scaled = centered / m_editorCamera.zoom();
        //vec2 m_editorCameraPos = compat::to_vec2(m_editorCamera.position());
        //vec2 final = scaled + m_editorCameraPos;

        //std::cout << "\nConversion Steps:\n";
        //std::cout << "  1. Centered: (" << centered.x << ", " << centered.y << ")\n";
        //std::cout << "  2. Scaled by zoom: (" << scaled.x << ", " << scaled.y << ")\n";
        //std::cout << "  3. Add camera pos: (" << final.x << ", " << final.y << ")\n";
        //std::cout << "  4. Final world: (" << mouseWorld.x << ", " << mouseWorld.y << ")\n";

        //std::cout << "\nObject found: " << (obj ? obj->name() : "nullptr") << "\n";

        //if (m_currentScene) {
        //    std::cout << "\nChecking " << m_currentScene->objects().size() << " objects:\n";
        //    for (auto& o : m_currentScene->objects()) {
        //        auto aabb = o->aabbWorld();
        //        auto pos = o->transform().position;
        //        auto size = o->size();

        //        // Calculate distance from mouse to object center
        //        f32 distToCenter = magnitude(vec2(pos.x, pos.y) - mouseWorld);

        //        // Check if mouse is inside AABB
        //        bool isInside = (mouseWorld.x >= aabb.min.x && mouseWorld.x <= aabb.max.x &&
        //            mouseWorld.y >= aabb.min.y && mouseWorld.y <= aabb.max.y);

        //        std::cout << "  - " << o->name() << "\n";
        //        std::cout << "    Position: (" << pos.x << ", " << pos.y << ")\n";
        //        std::cout << "    Size: (" << size.x << ", " << size.y << ")\n";
        //        std::cout << "    AABB: [" << aabb.min.x << "," << aabb.min.y
        //            << " to " << aabb.max.x << "," << aabb.max.y << "]\n";
        //        std::cout << "    Distance from mouse: " << distToCenter << " units\n";
        //        std::cout << "    Mouse inside AABB: " << (isInside ? "YES " : "NO ") << "\n";

        //        // Additional check: is the AABB calculation correct?
        //        vec2 expectedMin = vec2(pos.x, pos.y) - vec2(size.x, size.y) * 0.5f;
        //        vec2 expectedMax = vec2(pos.x, pos.y) + vec2(size.x, size.y) * 0.5f;

        //        if (std::abs(aabb.min.x - expectedMin.x) > 0.1f ||
        //            std::abs(aabb.min.y - expectedMin.y) > 0.1f) {
        //            std::cout << "    AABB MIN MISMATCH! Expected: ("
        //                << expectedMin.x << ", " << expectedMin.y << ")\n";
        //        }
        //        if (std::abs(aabb.max.x - expectedMax.x) > 0.1f ||
        //            std::abs(aabb.max.y - expectedMax.y) > 0.1f) {
        //            std::cout << "    AABB MAX MISMATCH! Expected: ("
        //                << expectedMax.x << ", " << expectedMax.y << ")\n";
        //        }
        //    }
        //}

        selectObject(obj);
    }

    void LevelEditor::handlePlayerInput(Gameplay::GameObject* player, double dt) {
        if (!player || !m_input) return;

        vec2 mouseScreen = m_input->mousePosition();
        vec2 mouseWorld = screenToWorld(mouseScreen);

        float moveSpeed = player->moveSpeed();
        float maxSpeed = moveSpeed;
        float maxRunSpeed = maxSpeed * 2.0f;
        bool isRunning = m_input->isKeyDown(GLFW_KEY_LEFT_SHIFT);

        if (isRunning) moveSpeed *= 2.0f;

        // Vertical
        if (m_input->isKeyDown(GLFW_KEY_W)) {
            player->velocity().y += moveSpeed * (float)dt;
            player->velocity().y = std::min(player->velocity().y, isRunning ? maxRunSpeed : maxSpeed);
        }
        else if (m_input->isKeyDown(GLFW_KEY_S)) {
            player->velocity().y -= moveSpeed * (float)dt;
            player->velocity().y = std::max(player->velocity().y, isRunning ? -maxRunSpeed : -maxSpeed);
        }
        else {
            player->velocity().y = 0.0f;
        }

        // Horizontal
        if (m_input->isKeyDown(GLFW_KEY_A)) {
            player->velocity().x -= moveSpeed * (float)dt;
            player->velocity().x = std::max(player->velocity().x, isRunning ? -maxRunSpeed : -maxSpeed);
        }
        else if (m_input->isKeyDown(GLFW_KEY_D)) {
            player->velocity().x += moveSpeed * (float)dt;
            player->velocity().x = std::min(player->velocity().x, isRunning ? maxRunSpeed : maxSpeed);
        }
        else {
            player->velocity().x = 0.0f;
        }

        // Mouse look
        vec2 dir = vec2(mouseWorld.x - player->transform().position.x,
            mouseWorld.y - player->transform().position.y);
        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dist > 10.0f) {
            player->transform().rotationRad = std::atan2(dir.y, dir.x);
        }
    }

    void LevelEditor::handleObjectPlacement() {
        if (isMouseOverUI()) {
            return;
        }
        if (m_selectedPrefab.empty()) {
            return;
        }

        vec2 mouseScreen = m_input->mousePosition();
        vec2 localPos = mouseScreen - vec2(m_sceneViewPosition.x, m_sceneViewPosition.y);

        if (localPos.x < 0 || localPos.y < 0 ||
            localPos.x > m_sceneViewportSize.x || localPos.y > m_sceneViewportSize.y) {
            return;
        }
        vec2 mouseWorld = screenToWorld(mouseScreen);

        // Apply grid snapping - this snaps to the grid corner
        vec2 snappedWorld = snapToGrid(mouseWorld);

        // Check if we're trying to place at the same grid position
        if (snappedWorld.x == m_lastPlacedGridPos.x && snappedWorld.y == m_lastPlacedGridPos.y) {
            return; // Already placed here, don't place again
        }

        // Update last placed position
        m_lastPlacedGridPos = snappedWorld;

        // Center the object in the grid cell
        float halfGrid = 0.5f * m_gridSize;
        vec2 offsetPos = snappedWorld + vec2(halfGrid, halfGrid);

        //std::cout << "\n=== PLACEMENT ===\n";
        //std::cout << "Screen: (" << mouseScreen.x << ", " << mouseScreen.y << ")\n";
        //std::cout << "World: (" << mouseWorld.x << ", " << mouseWorld.y << ")\n";
        //std::cout << "Snapped: (" << snappedWorld.x << ", " << snappedWorld.y << ")\n";
        //std::cout << "Camera: (" << m_editorCamera.position().x << ", "
        //    << m_editorCamera.position().y << ")\n";
        //std::cout << "Zoom: " << m_editorCamera.zoom() << "\n";
        //std::cout << "Grid Size: " << m_gridSize << "\n";
        //std::cout << "Half Grid: " << halfGrid << "\n";
        //std::cout << "Final Position: (" << offsetPos.x << ", " << offsetPos.y << ")\n\n";

        createObjectFromPrefab(m_selectedPrefab, offsetPos);
    }


    void LevelEditor::handleObjectDeletion() {
        if (isMouseOverUI()) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());
        Gameplay::GameObject* obj = getObjectAtPosition(mouseWorld);

        if (obj && obj->active() && m_currentScene) {
            selectObject(obj);
            deleteSelectedObject();
        }
    }

    void LevelEditor::handleObjectDragging() {
        if (!m_selectedObject) return;

        vec2 mouseWorld = screenToWorld(m_input->mousePosition());
        vec2 snappedWorld = snapToGrid(mouseWorld);

        float halfGrid = 0.5f * m_gridSize;
        vec2 offsetPos = snappedWorld + vec2(halfGrid, halfGrid);

        m_selectedObject->setPosition(offsetPos);
        markLevelDirty();
    }


    void LevelEditor::moveSelectedObject(vec2 delta) {
        if (!m_selectedObject) return;

        vec2 newPos = m_selectedObject->transform().position + delta;

        // Apply grid snapping
        newPos = snapToGrid(newPos);

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
            m_currentScene->registerExistingObjectsToLayers();

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

        auto& objects = m_currentScene->objects();
        for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
            auto& obj = *it;
            if (!obj || !obj->active()) continue;

            // Use the object's own AABB but make it larger for easier selection
            Gameplay::AABB aabb = obj->aabbWorld();

            // Expand the AABB by 50% on all sides for easier clicking
            vec2 expand = (aabb.max - aabb.min) * 0.5f * 0.5f;
            aabb.min -= expand;
            aabb.max += expand;

            if (worldPos.x >= aabb.min.x && worldPos.x <= aabb.max.x &&
                worldPos.y >= aabb.min.y && worldPos.y <= aabb.max.y) {
                return obj.get();
            }
        }
        return nullptr;
    }



    vec2 LevelEditor::screenToWorld(vec2 screenPos) const
    {
        // 1. Get the mouse position relative to the Top-Left of the Scene View panel
        vec2 localPos = screenPos - vec2(m_sceneViewPosition.x, m_sceneViewPosition.y);

        // 2. Convert to coordinates relative to the CENTER of the scene viewport
        // ← Use m_sceneViewportSize, NOT m_windowWidth/m_windowHeight
        vec2 centered = localPos - vec2(m_sceneViewportSize.x * 0.5f, m_sceneViewportSize.y * 0.5f);

        // 3. Scale by Camera Zoom to get World Units
        vec2 worldRel = centered / m_editorCamera.zoom();

        // 4. Flip Y: Screen Y increases downward, World Y increases upward
        worldRel.y = -worldRel.y;

        // 5. Add the Camera Position to get the final World Coordinate
        vec2 meditorCameraPos = compat::to_vec2(m_editorCamera.position());
        return worldRel + meditorCameraPos;
    }




    vec2 LevelEditor::worldToScreen(vec2 worldPos) const {
        vec2 m_editorCameraPos = compat::to_vec2(m_editorCamera.position());

        // 1. Subtract camera position
        vec2 relative = worldPos - m_editorCameraPos;

        // 2. Multiply by zoom
        relative *= m_editorCamera.zoom();

  
        relative.y = -relative.y;

        // 4. Add window center
        vec2 screenPos = relative + vec2(m_windowWidth * 0.5f, m_windowHeight * 0.5f);

        return screenPos;
    }

    bool LevelEditor::isMouseOverUI() const {
        bool anyWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
        bool anyItemHovered = ImGui::IsAnyItemHovered();

        // If we are hovering the Scene View, we do NOT consider it "UI" 
        // that blocks gameplay, so we subtract it from the check.
        if (m_sceneViewHovered) {
            return false;
        }
        return anyWindowHovered || anyItemHovered;;
    }

    vec2 LevelEditor::snapToGrid(vec2 position) const {
        if (!m_enableGridSnap) {
            return position;
        }

        // Snap to nearest grid point
        float snappedX = std::floor(position.x / m_gridSize) * m_gridSize;
        float snappedY = std::floor(position.y / m_gridSize) * m_gridSize;

        return vec2(snappedX, snappedY);
    }

    void LevelEditor::setActive(bool active) {
        m_editorActive = active;

        if (active) {
            std::cout << "[LevelEditor] Editor activated\n";
        }
        else {
            // ⭐ RESTORE FULL WINDOW VIEWPORT
            if (m_windowManager) {
                int fullWidth = m_windowManager->width();
                int fullHeight = m_windowManager->height();
                glViewport(0, 0, fullWidth, fullHeight);

                std::cout << "[LevelEditor] Viewport restored to "
                    << fullWidth << "x" << fullHeight << "\n";
            }

            m_selectedObject = nullptr;
        }
    }

    bool LevelEditor::isPointInPolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon) {
        int intersections = 0;
        size_t n = polygon.size();
        for (size_t i = 0; i < n; i++) {
            glm::vec2 p1 = polygon[i];
            glm::vec2 p2 = polygon[(i + 1) % n];
            if ((p1.y > point.y) != (p2.y > point.y)) {
                float xIntersect = p1.x + (point.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
                if (point.x < xIntersect) intersections++;
            }
        }
        return (intersections % 2) == 1;
    }

    void LevelEditor::updateLightExposure(double dt) {
        if (m_mode != EditorMode::Play || !m_currentScene) return;

        auto* physics = m_currentScene->getPhysics();
        if (!physics || !m_flashlightOn || !m_playModePlayer || m_playerBattery <= 0.0f) {
            for (auto& obj : m_currentScene->objects()) {
                if (obj && obj->type() == Gameplay::GameObjectType::Enemy) {
                    obj->resetLightExposure();
                }
            }
            return;
        }

        // Calculate flashlight position
        vec2 origin = m_playModePlayer->transform().position;
        float angle = m_playModePlayer->transform().rotationRad;
        glm::vec2 torchOffsetLocal(35.0f, -20.0f);
        glm::vec2 torchOffsetWorld(
            torchOffsetLocal.x * std::cos(angle) - torchOffsetLocal.y * std::sin(angle),
            torchOffsetLocal.x * std::sin(angle) + torchOffsetLocal.y * std::cos(angle)
        );
        glm::vec2 originGLM(origin.x + torchOffsetWorld.x, origin.y + torchOffsetWorld.y);

        // Get visibility polygon
        vec2 originVec2{ originGLM.x, originGLM.y };
        std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
            originVec2, angle, glm::radians(60.0f), 300.0f, *physics, m_currentScene->objects()
        );

        std::vector<glm::vec2> lightPolyGLM;
        for (const auto& p : lightPoly) lightPolyGLM.emplace_back(p.x, p.y);

        // Check enemies
        for (auto& obj : m_currentScene->objects()) {
            if (obj && obj->active() && obj->type() == Gameplay::GameObjectType::Enemy) {
                glm::vec2 enemyPos(obj->transform().position.x, obj->transform().position.y);
                if (isPointInPolygon(enemyPos, lightPolyGLM)) {
                    obj->addLightExposure(static_cast<float>(dt));
                    if (obj->lightExposureTime() >= 2.0f) {
                        obj->setActive(false);
                    }
                }
                else {
                    obj->resetLightExposure();
                }
            }
        }
    }

    void LevelEditor::renderFlashlight() {
        if (!m_flashlightOn || !m_playModePlayer || m_playerBattery <= 0.0f) return;

        auto* physics = m_currentScene->getPhysics();
        if (!physics) return;

        // Calculate position
        vec2 origin = m_playModePlayer->transform().position;
        float angle = m_playModePlayer->transform().rotationRad;
        glm::vec2 torchOffsetLocal(35.0f, -20.0f);
        glm::vec2 torchOffsetWorld(
            torchOffsetLocal.x * std::cos(angle) - torchOffsetLocal.y * std::sin(angle),
            torchOffsetLocal.x * std::sin(angle) + torchOffsetLocal.y * std::cos(angle)
        );
        glm::vec2 originGLM(origin.x + torchOffsetWorld.x, origin.y + torchOffsetWorld.y);

        // Get polygon
        vec2 originVec2{ originGLM.x, originGLM.y };
        std::vector<vec2> lightPoly = m_renderer->computeVisibilityPolygon(
            originVec2, angle, glm::radians(60.0f), 300.0f, *physics, m_currentScene->objects()
        );

        std::vector<glm::vec2> lightPolyGLM;
        for (const auto& p : lightPoly) lightPolyGLM.emplace_back(p.x, p.y);

        // Render
        m_renderer->beginLightPass(m_editorCamera.viewProj(), originGLM,
            glm::vec3(1.0f, 0.9f, 0.6f), 300.0f, angle, glm::radians(60.0f));
        m_renderer->drawLightCone(originGLM, lightPolyGLM, { 1.0f, 0.9f, 0.6f });
        m_renderer->endLightPass();
    }

	/******************************************************************************/
    // Error Handling
    /******************************************************************************/
    void LevelEditor::queueErrorPopup(const std::string& title, const std::string& message)
    {
        m_errorPopupTitle = title;
        m_errorPopupMessage = message;
        m_showErrorPopup = true;
    }

    void LevelEditor::renderErrorPopup()
    {
        // Plan (pseudocode):
        // 1. If there is no error to show, return early.
        // 2. Open the ImGui popup with the same name as the modal.
        // 3. Compute the center of the main viewport.
        // 4. Use ImGui::SetNextWindowPos(...) with a 0.5,0.5 pivot BEFORE BeginPopupModal
        //    because SetWindowPos does not accept a pivot parameter and will not match the
        //    intended overload. SetNextWindowPos is the correct call when specifying a pivot.
        // 5. Begin the modal with BeginPopupModal and render title, message and OK button.
        // 6. Close the popup and clear the flag when OK is pressed.
        //
        // Rationale:
        // - Replaces incorrect ImGui::SetWindowPos(center, cond, pivot) call with
        //   ImGui::SetNextWindowPos(center, cond, pivot) placed before BeginPopupModal.
        // - Keeps the rest of the modal intact.

        if (!m_showErrorPopup)
            return;

        ImGui::OpenPopup("Audio Error");

        // Center the popup in the main viewport using SetNextWindowPos with pivot (0.5,0.5)
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Audio Error", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::TextWrapped("%s", m_errorPopupTitle.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_errorPopupMessage.c_str());
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                m_showErrorPopup = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    //void LevelEditor::setupDefaultDockLayout() {
    //    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");

    //    // Clear existing layout
    //    ImGui::DockBuilderRemoveNode(dockspaceId);
    //    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);

    //    // Split the dockspace
    //    ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Up, 0.05f, nullptr, &dockspaceId);
    //    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, nullptr, &dockspaceId);
    //    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &dockspaceId);
    //    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.3f, nullptr, &dockspaceId);

    //    // Dock windows to specific locations
    //    ImGui::DockBuilderDockWindow("Toolbar", dockTop);
    //    ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
    //    ImGui::DockBuilderDockWindow("Prefab Palette", dockLeft);
    //    ImGui::DockBuilderDockWindow("Properties", dockRight);
    //    ImGui::DockBuilderDockWindow("Scene View", dockspaceId);  // Center
    //    ImGui::DockBuilderDockWindow("Texture Assets Browser", dockBottom);

    //    ImGui::DockBuilderFinish(dockspaceId);
    //}
} // namespace Editor