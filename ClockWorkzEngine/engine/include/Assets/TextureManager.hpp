/******************************************************************************/
/*!
\file           TextureManager.hpp
\project        Lights Out
 \author(s)      [Yusuf] 96% [Amzar] 4%
\brief          TextureManager provides a data-driven approach to loading and
                managing textures using JSON configuration files.
                It provides:
                - LoadFromConfig(): Load all textures from a JSON configuration file
                - LoadTexture(): Manually load a single texture (backwards compatible)
                - Get(): Retrieve a loaded texture by name
                - Unload(): Release a specific texture
                - Clear(): Release all textures
                - Uses ResourceManager<Texture2D> internally for resource management

Example JSON format (textures.json):
{
  "textures": [
    {
      "name": "player_texture",
      "path": "assets/textures/player.png",
      "flipY": true,
      "alphaOnlyIsRed": false
    }
  ]
}

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <memory>
#include <vector>
#include "Graphics/Texture2D.hpp"
#include "Assets/ResourceManager.hpp"

namespace Assets {

    /**
     * @brief Holds configuration data for a single texture
     */
    struct TextureConfig {
        std::string name;           // Unique identifier for the texture
        std::string path;           // File path to the texture
        bool flipY = true;          // Flip texture vertically on load
        bool alphaOnlyIsRed = false; // Treat alpha channel as red channel
    };

    /**
     * @brief TextureManager - Data-driven texture loading and management
     *
     * This class wraps ResourceManager<Texture2D> to provide:
     * - JSON-based configuration for bulk texture loading
     * - Centralized texture management
     * - Easy access to loaded textures
     */
    class TextureManager {
    public:
        /**
         * @brief Load all textures from a JSON configuration file
         *
         * @param configPath Path to JSON file (e.g., "assets/configs/textures.json")
         * @return true if all textures loaded successfully, false otherwise
         *
         * Example usage:
         *   TextureManager::LoadFromConfig("assets/configs/textures.json");
         */
        static bool LoadFromConfig(const std::string& configPath);

        /**
         * @brief Manually load a single texture (backwards compatible)
         *
         * @param name Unique name/alias for the texture
         * @param path File path to the texture
         * @param flipY Flip texture vertically (default: true)
         * @param alphaOnlyIsRed Treat alpha as red channel (default: false)
         * @return Shared pointer to the loaded texture, or nullptr on failure
         *
         * Example usage:
         *   auto tex = TextureManager::LoadTexture("hero", "assets/hero.png");
         */
        static std::shared_ptr<Graphics::Texture2D> LoadTexture(
            const std::string& name,
            const std::string& path
        );

        /**
         * @brief Get a previously loaded texture by name
         *
         * @param name Texture name/alias
         * @return Shared pointer to the texture, or nullptr if not found
         *
         * Example usage:
         *   auto playerTex = TextureManager::Get("player_texture");
         *   if (playerTex) {
         *       playerTex->bind();
         *   }
         */
        static std::shared_ptr<Graphics::Texture2D> Get(const std::string& name);

        /**
         * @brief Unload a specific texture by name
         *
         * @param name Texture name to unload
         */
        static void Unload(const std::string& name);

        /**
         * @brief Clear all loaded textures
         */
        static void Clear();

        /**
         * @brief Get list of all loaded texture names
         *
         * @return Vector of texture names currently loaded
         */
        static std::vector<std::string> GetLoadedTextureNames();

        /**
         * @brief Check if a texture is loaded
         *
         * @param name Texture name to check
         * @return true if loaded, false otherwise
         */
        static bool IsLoaded(const std::string& name);

        /**
         * @brief Import a texture file into the assets folder and register it
         *
         * @param sourcePath Path to the source texture file (e.g., "C:/Downloads/hero.png")
         * @param name Unique name for the texture
         * @param configPath Path to JSON config file (default: "assets/configs/Textures.json")
         * @param destinationDir Destination folder inside the project
         * @return true if import successful, false otherwise
         *
         * This will:
         * 1. Copy the file to the requested assets/textures subfolder
         * 2. Add an entry to the JSON config
         * 3. Load the texture into memory
         */
        static bool ImportTexture(const std::string& sourcePath,
                                  const std::string& name,
                                  const std::string& configPath = "assets/configs/Textures.json",
                                  const std::string& destinationDir = "assets/textures");

        /**
         * @brief Remove texture entry from JSON config (does not delete file)
         *
         * @param name Texture name to remove
         * @param configPath Path to JSON config file
         * @return true if removed, false otherwise
         */
        static bool RemoveFromConfig(const std::string& name,
                                     const std::string& configPath = "assets/configs/Textures.json");

        /**
         * @brief Delete texture from memory, config, AND physical file from disk
         *
         * @param name Texture name to delete
         * @param configPath Path to JSON config file
         * @return true if fully deleted, false otherwise
         *
         * WARNING: This permanently deletes the file from disk!
         */
        static bool DeleteFromDisk(const std::string& name,
                                   const std::string& configPath = "assets/configs/Textures.json");

        /**
         * @brief Update a texture entry to point to a new file path after a move
         *
         * @param name Texture name
         * @param newPath New file path on disk
         * @param configPath Path to JSON config file
         * @return true if updated, false otherwise
         */
        static bool UpdateConfigPath(const std::string& name,
                                     const std::string& newPath,
                                     const std::string& configPath = "assets/configs/Textures.json");

        /**
         * @brief Get the file path for a texture from config
         *
         * @param name Texture name
         * @param configPath Path to JSON config file
         * @return File path string, or empty if not found
         */
        static std::string GetFilePath(const std::string& name,
                                       const std::string& configPath = "assets/configs/Textures.json");

    private:
        static std::vector<TextureConfig> ParseConfig(const std::string& configPath);

        // Returns name unchanged if it already ends with "_texture",
        // otherwise appends "_texture". Applied at load and lookup time so
        // all registered keys are consistent.
        static std::string normalize(const std::string& name);
    };

} // namespace Assets
