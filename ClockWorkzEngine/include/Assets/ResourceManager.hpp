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
