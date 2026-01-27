/******************************************************************************/
/*!
\file           TextureManager.cpp
\project        Lights Out
\author(s)      [Yusuf], 100%
\brief          Implementation of TextureManager for data-driven texture loading

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Assets/TextureManager.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

namespace Assets {

// Static helper to track loaded texture names (optional, for debugging)
static std::vector<std::string> loadedTextures;

bool TextureManager::LoadFromConfig(const std::string &configPath) {
  std::cout << "[TextureManager] Loading textures from: " << configPath
            << std::endl;

  try {
    // Parse the configuration
    std::vector<TextureConfig> configs = ParseConfig(configPath);

    if (configs.empty()) {
      std::cerr << "[TextureManager] Warning: No textures found in config file"
                << std::endl;
      return false;
    }

    // Load each texture
    bool allSuccess = true;
    for (const auto &config : configs) {
      std::cout << "[TextureManager] Loading: " << config.name << " from "
                << config.path << std::endl;

      auto texture = LoadTexture(config.name, config.path);

      if (!texture) {
        std::cerr << "[TextureManager] Failed to load: " << config.name
                  << std::endl;
        allSuccess = false;
      } else {
        std::cout << "[TextureManager] Successfully loaded: " << config.name
                  << " (" << texture->width() << "x" << texture->height() << ")"
                  << std::endl;
      }
    }

    return allSuccess;
  } catch (const std::exception &e) {
    std::cerr << "[TextureManager] Error loading config: " << e.what()
              << std::endl;
    return false;
  }
}

std::shared_ptr<Graphics::Texture2D>
TextureManager::LoadTexture(const std::string &name, const std::string &path) {
  // Check if already loaded
  auto existing = ResourceManager<Graphics::Texture2D>::Get(name);
  if (existing) {
    std::cout << "[TextureManager] Texture already loaded: " << name
              << std::endl;
    return existing;
  }

  // Load using ResourceManager - it will create the Texture2D and call
  // Load(path)
  auto texture = ResourceManager<Graphics::Texture2D>::Load(name, path);

  if (!texture) {
    std::cerr << "[TextureManager] Failed to load texture file: " << path
              << std::endl;
    return nullptr;
  }

  // Track loaded texture name
  if (std::find(loadedTextures.begin(), loadedTextures.end(), name) ==
      loadedTextures.end()) {
    loadedTextures.push_back(name);
  }

  return texture;
}

std::shared_ptr<Graphics::Texture2D>
TextureManager::Get(const std::string &name) {
  return ResourceManager<Graphics::Texture2D>::Get(name);
}

void TextureManager::Unload(const std::string &name) {
  auto texture = ResourceManager<Graphics::Texture2D>::Get(name);
  if (texture) {
    ResourceManager<Graphics::Texture2D>::Unload(
        name); // Fully remove from ResourceManager

    // Remove from tracking
    auto it = std::find(loadedTextures.begin(), loadedTextures.end(), name);
    if (it != loadedTextures.end()) {
      loadedTextures.erase(it);
    }

    std::cout << "[TextureManager] Unloaded texture: " << name << std::endl;
  }
}

void TextureManager::Clear() {
  std::cout << "[TextureManager] Clearing all textures..." << std::endl;
  ResourceManager<Graphics::Texture2D>::Clear();
  loadedTextures.clear();
}

std::vector<std::string> TextureManager::GetLoadedTextureNames() {
  return loadedTextures;
}

bool TextureManager::IsLoaded(const std::string &name) {
  return ResourceManager<Graphics::Texture2D>::Get(name) != nullptr;
}

std::vector<TextureConfig>
TextureManager::ParseConfig(const std::string &configPath) {
  std::vector<TextureConfig> configs;

  // Open and parse JSON file
  std::ifstream file(configPath);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open texture config file: " +
                             configPath);
  }

  json j;
  try {
    file >> j;
  } catch (const json::exception &e) {
    throw std::runtime_error("JSON parse error: " + std::string(e.what()));
  }

  // Validate JSON structure
  if (!j.contains("textures")) {
    throw std::runtime_error("Config file missing 'textures' array");
  }

  if (!j["textures"].is_array()) {
    throw std::runtime_error("'textures' must be an array");
  }

  // Parse each texture entry
  for (const auto &item : j["textures"]) {
    TextureConfig config;

    // Required fields
    if (!item.contains("name") || !item.contains("path")) {
      std::cerr << "[TextureManager] Warning: Skipping texture entry missing "
                   "'name' or 'path'"
                << std::endl;
      continue;
    }

    config.name = item["name"].get<std::string>();
    config.path = item["path"].get<std::string>();

    // Optional fields with defaults
    if (item.contains("flipY")) {
      config.flipY = item["flipY"].get<bool>();
    }

    if (item.contains("alphaOnlyIsRed")) {
      config.alphaOnlyIsRed = item["alphaOnlyIsRed"].get<bool>();
    }

    configs.push_back(config);
  }

  return configs;
}

} // namespace Assets