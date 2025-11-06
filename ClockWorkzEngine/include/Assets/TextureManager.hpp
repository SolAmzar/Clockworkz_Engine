/******************************************************************************/
/*!
\file           TextureManager.hpp
\project        Lights Out
\author(s)      [Yusuf], 100%
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

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
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

    private:
        /**
         * @brief Parse texture configuration from JSON
         *
         * @param configPath Path to JSON config file
         * @return Vector of TextureConfig structures
         */
        static std::vector<TextureConfig> ParseConfig(const std::string& configPath);
    };

} // namespace Assets