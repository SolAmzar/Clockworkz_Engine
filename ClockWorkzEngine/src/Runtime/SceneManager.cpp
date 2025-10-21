/******************************************************************************/
/*!
\file           SceneManager.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares SceneManager, a system for handling scene switching
                and lifecycle management.
                Responsibilities include:
                - Holding the currently active Scene
                - Loading, unloading, and transitioning between scenes
                - Exposing hooks for global scene control

References:
- Game loop & scene transitions (Simillar to GSM): https://gameprogrammingpatterns.com/state.html

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Runtime/SceneManager.hpp"
#include "Runtime/Scene.hpp"
#include "Graphics/RenderingManager.hpp"
#include <iostream>


using namespace Runtime;

bool SceneManager::loadScene(const std::string& id) {
    // If not cached, create & load
    auto it = m_cache.find(id);
    if (it == m_cache.end()) {
        auto scn = std::make_shared<Scene>(id);
        if (!scn->load()) return false;
        m_cache.emplace(id, scn);
        m_current = std::move(scn);
    }
    else {
        // If already cached, switch
        m_current = it->second;
        // If you unload when leaving, you might want to re-load here. For now, assume still loaded.
    }
    std::cout << "[SceneManager] Current = " << id << "\n";
    return true;
}

void SceneManager::unloadCurrent() {
    if (!m_current) return;
    m_current->unload();
    // Keep cached (so we can reload quickly). Or erase from cache if you prefer:
    // m_cache.erase(m_current->name());
    m_current.reset();
}

void SceneManager::update(double dt) {
    if (m_current) m_current->update(dt);
}

void SceneManager::render(Graphics::RenderingManager& m_renderer) {
    if (m_current) m_current->render(m_renderer);
}

Runtime::Scene* SceneManager::current() { return m_current.get(); }
Runtime::Scene const* SceneManager::current() const { return m_current.get(); }

bool SceneManager::reloadCurrent() {
    if (!m_current) return false;

    std::string id = m_current->name();
    m_current->unload();

    if (!m_current->load()) {
        m_current.reset();
        m_cache.erase(id);
        return false;
    }
    return true;
}

