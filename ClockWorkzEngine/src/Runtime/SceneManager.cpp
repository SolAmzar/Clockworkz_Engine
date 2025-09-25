#include "Runtime/SceneManager.hpp"
#include "Runtime/Scene.hpp"
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

void SceneManager::render() {
    if (m_current) m_current->render();
}

Runtime::Scene* SceneManager::current() { return m_current.get(); }
Runtime::Scene const* SceneManager::current() const { return m_current.get(); }

