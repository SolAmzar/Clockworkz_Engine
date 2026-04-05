/******************************************************************************/
/*!
\file           TextureManager.cpp
\project        Lights Out
\author(s)      [Yusuf] 99% [Sol-Amzar] 1%
\brief          Implementation of TextureManager for data-driven texture loading

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Assets/TextureManager.hpp"
#pragma warning(push, 0)
#include "Core/json.hpp" // nlohmann::json
#pragma warning(pop)
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

using json = nlohmann::json;

namespace Assets {

// Static helper to track loaded texture names (optional, for debugging)
static std::vector<std::string> loadedTextures;
static std::unordered_map<std::string, std::string> s_resolvedTexturePaths;
static std::unordered_set<std::string> s_missingTextureKeys;
static std::unordered_map<std::string, std::shared_ptr<Graphics::Texture2D>>
    s_textureAliases;
static std::unordered_map<std::string, std::shared_ptr<Graphics::Texture2D>>
    s_texturesByResolvedPath;

static std::string normalizeTextureKey(const std::string& name) {
  static const std::string suffix = "_texture";
  if (name.size() >= suffix.size() &&
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return name;
  }
  return name + suffix;
}

static std::string denormalizeTextureKey(const std::string& name) {
  static const std::string suffix = "_texture";
  if (name.size() >= suffix.size() &&
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return name.substr(0, name.size() - suffix.size());
  }
  return name;
}

static std::string findTexturePathOnDisk(const std::string& name) {
  const fs::path textureRoot("assets/textures");
  if (!fs::exists(textureRoot))
    return "";

  const std::string normalizedName = normalizeTextureKey(name);
  const std::string baseName = denormalizeTextureKey(name);
  std::error_code ec;
  for (const auto& entry : fs::recursive_directory_iterator(textureRoot, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;

    const std::string stem = entry.path().stem().string();
    if (stem == name || stem == normalizedName || stem == baseName)
      return entry.path().generic_string();
  }

  return "";
}

static std::string canonicalizeTexturePath(const std::string& path) {
  std::error_code ec;
  const fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
  if (!ec)
    return canonical.generic_string();
  return fs::path(path).generic_string();
}

bool TextureManager::LoadFromConfig(const std::string &configPath) {
  std::cout << "[TextureManager] Loading textures from: " << configPath
            << std::endl;

  try {
    // Parse the configuration
    std::vector<TextureConfig> configs = ParseConfig(configPath);

    if (configs.empty()) {
      std::cout << "[TextureManager] No textures in config file (empty project)."
                << std::endl;
      return true; // Empty config is valid
    }

    // Load each texture
    bool allSuccess = true;
    for (const auto &config : configs) {
      std::cout << "[TextureManager] Loading: " << config.name << " from "
                << config.path << std::endl;

      std::string resolvedPath = config.path;
      auto texture = LoadTexture(config.name, resolvedPath);

      if (!texture) {
        const std::string discoveredPath = findTexturePathOnDisk(config.name);
        if (!discoveredPath.empty() && discoveredPath != config.path) {
          std::cout << "[TextureManager] Retrying with discovered path: "
                    << discoveredPath << std::endl;
          resolvedPath = discoveredPath;
          texture = LoadTexture(config.name, resolvedPath);
        }
      }

      if (!texture) {
        std::cerr << "[TextureManager] Failed to load: " << config.name
                  << std::endl;
        allSuccess = false;
      } else {
        s_resolvedTexturePaths[normalize(config.name)] = resolvedPath;
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

std::string TextureManager::normalize(const std::string &name) {
  static const std::string suffix = "_texture";
  if (name.size() >= suffix.size() &&
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
    return name;
  return name + suffix;
}

std::shared_ptr<Graphics::Texture2D>
TextureManager::LoadTexture(const std::string &name, const std::string &path) {
  const std::string key = normalize(name);

  // Check if already loaded
  auto existing = ResourceManager<Graphics::Texture2D>::Get(key);
  if (existing) {
    std::cout << "[TextureManager] Texture already loaded: " << key
              << std::endl;
    return existing;
  }

  auto aliasIt = s_textureAliases.find(key);
  if (aliasIt != s_textureAliases.end()) {
    std::cout << "[TextureManager] Texture alias already loaded: " << key
              << std::endl;
    return aliasIt->second;
  }

  const std::string canonicalPath = canonicalizeTexturePath(path);
  auto byPathIt = s_texturesByResolvedPath.find(canonicalPath);
  if (byPathIt != s_texturesByResolvedPath.end()) {
    s_textureAliases[key] = byPathIt->second;
    if (std::find(loadedTextures.begin(), loadedTextures.end(), key) ==
        loadedTextures.end()) {
      loadedTextures.push_back(key);
    }
    std::cout << "[TextureManager] Aliased texture: " << key << " -> " << path
              << std::endl;
    return byPathIt->second;
  }

  auto texture = ResourceManager<Graphics::Texture2D>::Load(key, path);

  if (!texture) {
    std::cerr << "[TextureManager] Failed to load texture file: " << path
              << std::endl;
    return nullptr;
  }

  // Track loaded texture name
  if (std::find(loadedTextures.begin(), loadedTextures.end(), key) ==
      loadedTextures.end()) {
    loadedTextures.push_back(key);
  }
  s_texturesByResolvedPath[canonicalPath] = texture;

  return texture;
}

std::shared_ptr<Graphics::Texture2D>
TextureManager::Get(const std::string &name) {
  const std::string key = normalize(name);

  // Try exact key first, then normalized key as fallback so scene JSONs that
  // omit the "_texture" suffix still resolve correctly.
  auto t = ResourceManager<Graphics::Texture2D>::Get(name);
  if (!t) t = ResourceManager<Graphics::Texture2D>::Get(key);
  if (!t) {
    auto aliasIt = s_textureAliases.find(name);
    if (aliasIt != s_textureAliases.end())
      t = aliasIt->second;
  }
  if (!t) {
    auto aliasIt = s_textureAliases.find(key);
    if (aliasIt != s_textureAliases.end())
      t = aliasIt->second;
  }
  if (t)
    return t;

  auto cachedPathIt = s_resolvedTexturePaths.find(key);
  if (cachedPathIt != s_resolvedTexturePaths.end()) {
    t = LoadTexture(name, cachedPathIt->second);
    if (t) {
      s_missingTextureKeys.erase(key);
      return t;
    }
    s_resolvedTexturePaths.erase(cachedPathIt);
  }

  std::string path = GetFilePath(name);
  if (path.empty())
    path = findTexturePathOnDisk(name);

  if (!path.empty()) {
    s_resolvedTexturePaths[key] = path;
    t = LoadTexture(name, path);
    if (t) {
      s_missingTextureKeys.erase(key);
      return t;
    }
  }

  s_missingTextureKeys.insert(key);
  return t;
}

void TextureManager::Unload(const std::string &name) {
  const std::string key = normalize(name);
  auto aliasIt = s_textureAliases.find(key);
  if (aliasIt != s_textureAliases.end()) {
    s_textureAliases.erase(aliasIt);
    auto loadedIt = std::find(loadedTextures.begin(), loadedTextures.end(), key);
    if (loadedIt != loadedTextures.end())
      loadedTextures.erase(loadedIt);
    std::cout << "[TextureManager] Unloaded texture alias: " << key
              << std::endl;
    return;
  }

  auto texture = ResourceManager<Graphics::Texture2D>::Get(key);
  if (texture) {
    for (auto it = s_texturesByResolvedPath.begin();
         it != s_texturesByResolvedPath.end();) {
      if (it->second == texture) {
        it = s_texturesByResolvedPath.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = s_textureAliases.begin(); it != s_textureAliases.end();) {
      if (it->second == texture) {
        auto loadedIt = std::find(loadedTextures.begin(), loadedTextures.end(),
                                  it->first);
        if (loadedIt != loadedTextures.end())
          loadedTextures.erase(loadedIt);
        it = s_textureAliases.erase(it);
      } else {
        ++it;
      }
    }
    ResourceManager<Graphics::Texture2D>::Unload(key);

    auto it = std::find(loadedTextures.begin(), loadedTextures.end(), key);
    if (it != loadedTextures.end())
      loadedTextures.erase(it);

    std::cout << "[TextureManager] Unloaded texture: " << key << std::endl;
  }
}

void TextureManager::Clear() {
  std::cout << "[TextureManager] Clearing all textures..." << std::endl;
  ResourceManager<Graphics::Texture2D>::Clear();
  loadedTextures.clear();
  s_resolvedTexturePaths.clear();
  s_missingTextureKeys.clear();
  s_textureAliases.clear();
  s_texturesByResolvedPath.clear();
}

std::vector<std::string> TextureManager::GetLoadedTextureNames() {
  return loadedTextures;
}

bool TextureManager::IsLoaded(const std::string &name) {
  return Get(name) != nullptr;
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

bool TextureManager::ImportTexture(const std::string &sourcePath,
                                   const std::string &name,
                                   const std::string &configPath) {
  const std::string key = normalize(name);
  // 1. Validate source file exists
  if (!fs::exists(sourcePath)) {
    std::cerr << "[TextureManager] Source file not found: " << sourcePath
              << std::endl;
    return false;
  }

  // 2. Determine destination path
  fs::path source(sourcePath);
  std::string filename = source.filename().string();
  std::string destPath = "assets/textures/" + filename;

  // 3. Create destination directory if needed
  fs::create_directories("assets/textures");

  // 4. Copy file to assets folder
  if (fs::absolute(sourcePath) != fs::absolute(destPath)) {
    try {
      fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
      std::cout << "[TextureManager] Copied file to: " << destPath << std::endl;
    } catch (const fs::filesystem_error &e) {
      std::cerr << "[TextureManager] Failed to copy file: " << e.what()
                << std::endl;
      return false;
    }
  } else {
    std::cout << "[TextureManager] Source and destination are the same, skipping copy." << std::endl;
  }

  // 5. Add entry to JSON config
  std::ifstream inFile(configPath);
  json j;
  if (inFile.is_open()) {
    try {
      inFile >> j;
    } catch (...) {
      j = json::object();
    }
    inFile.close();
  }

  if (!j.contains("textures") || !j["textures"].is_array()) {
    j["textures"] = json::array();
  }

  // Check if name already exists
  for (const auto &entry : j["textures"]) {
    if (entry.value("name", "") == key) {
      std::cerr << "[TextureManager] Texture name already exists: " << key
                << std::endl;
      return false;
    }
  }

  // Add new entry
  json newEntry;
  newEntry["name"] = key;
  newEntry["path"] = destPath;
  newEntry["flipY"] = true;
  newEntry["alphaOnlyIsRed"] = false;
  j["textures"].push_back(newEntry);

  // Write back to config
  std::ofstream outFile(configPath);
  if (!outFile.is_open()) {
    std::cerr << "[TextureManager] Failed to write config: " << configPath
              << std::endl;
    return false;
  }
  outFile << std::setw(4) << j << std::endl;
  outFile.close();

  // 6. Load the texture into memory
  auto texture = LoadTexture(key, destPath);
  if (!texture) {
    std::cerr << "[TextureManager] Failed to load imported texture" << std::endl;
    return false;
  }

  std::cout << "[TextureManager] Successfully imported: " << key << std::endl;
  return true;
}

std::string TextureManager::GetFilePath(const std::string &name,
                                        const std::string &configPath) {
  const std::string key = normalize(name);
  std::ifstream file(configPath);
  if (!file.is_open())
    return "";

  json j;
  try {
    file >> j;
  } catch (...) {
    return "";
  }

  if (!j.contains("textures") || !j["textures"].is_array())
    return "";

  for (const auto &entry : j["textures"]) {
    if (entry.value("name", "") == key || entry.value("name", "") == name) {
      return entry.value("path", "");
    }
  }

  return "";
}

bool TextureManager::RemoveFromConfig(const std::string &name,
                                      const std::string &configPath) {
  std::ifstream file(configPath);
  if (!file.is_open())
    return false;

  json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }
  file.close();

  if (!j.contains("textures") || !j["textures"].is_array())
    return false;

  auto &arr = j["textures"];
  bool removed = false;
  for (auto it = arr.begin(); it != arr.end();) {
    if (it->value("name", "") == name) {
      it = arr.erase(it);
      removed = true;
    } else {
      ++it;
    }
  }

  if (!removed)
    return false;

  std::ofstream outFile(configPath);
  if (!outFile.is_open())
    return false;

  outFile << std::setw(4) << j << std::endl;
  std::cout << "[TextureManager] Removed from config: " << name << std::endl;
  return true;
}

bool TextureManager::DeleteFromDisk(const std::string &name,
                                    const std::string &configPath) {
  // 1. Get the file path before removing from config
  std::string filePath = GetFilePath(name, configPath);
  if (filePath.empty()) {
    std::cerr << "[TextureManager] Texture not found in config: " << name
              << std::endl;
    return false;
  }

  // 2. Unload from memory
  Unload(name);

  // 3. Remove from JSON config
  if (!RemoveFromConfig(name, configPath)) {
    std::cerr << "[TextureManager] Failed to remove from config: " << name
              << std::endl;
    return false;
  }

  // 4. Delete physical file from disk
  try {
    if (fs::exists(filePath)) {
      fs::remove(filePath);
      std::cout << "[TextureManager] Deleted file from disk: " << filePath
                << std::endl;
    } else {
      std::cerr << "[TextureManager] File not found on disk: " << filePath
                << std::endl;
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "[TextureManager] Failed to delete file: " << e.what()
              << std::endl;
    return false;
  }

  std::cout << "[TextureManager] Successfully deleted: " << name << std::endl;
  return true;
}

} // namespace Assets
