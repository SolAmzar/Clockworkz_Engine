/******************************************************************************/
/*!
\file           ResourceManager.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          ResourceManager is a generic templated class for managing
                reusable game assets. 
                It provides:
                - Load(): Load and store resources (e.g., fonts, textures, shaders)
                  under a given name, avoiding duplicate loads
                - Get(): Retrieve already loaded resources by name
                - Clear(): Release and unload all managed resources from memory
                - Centralized storage using std::unordered_map keyed by string
                - Smart pointer management (std::shared_ptr) to ensure safe reuse

                Designed for integration with asset types that implement
                Load() and Unload() methods.

                cpp not required as its generic, too many definition to add for each template resource type.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace ClockWorkz::Assets {

template <typename T>
class ResourceManager {
public:
    // Load and store asset under a name
    template <typename... Args>
    static std::shared_ptr<T> Load(const std::string& name, Args&&... args) {
        auto it = resources.find(name);
        if (it != resources.end()) {
            return it->second;
        }

        auto resource = std::make_shared<T>();
        if (!resource->Load(std::forward<Args>(args)...)) {
            throw std::runtime_error("Failed to load resource: " + name);
        }

        resources[name] = resource;
        return resource;
    }

    // Get already loaded asset
    static std::shared_ptr<T> Get(const std::string& name) {
        auto it = resources.find(name);
        if (it == resources.end()) {
            throw std::runtime_error("Resource not found: " + name);
        }
        return it->second;
    }

    // Clear all resources
    static void Clear() {
        for (auto& kv : resources) {
            kv.second->Unload();   // free GPU resources
        }
        resources.clear();
    }

private:
    static std::unordered_map<std::string, std::shared_ptr<T>> resources;
};

template <typename T>
std::unordered_map<std::string, std::shared_ptr<T>> ResourceManager<T>::resources;

} // namespace ClockWorkz::Assets
