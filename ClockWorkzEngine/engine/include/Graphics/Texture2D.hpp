/******************************************************************************/
/*!
\file           Texture2D.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the Texture2D class responsible for loading, managing,
                and binding 2D textures in OpenGL using the stb_image library.

                Responsibilities:
                - LoadFromFile(): Reads image files (RGB, RGBA, grayscale) and
                  uploads data to the GPU as a 2D texture.
                - Provides bind() to activate textures in a specific texture slot.
                - Supports flipping and single-channel (alpha-only) textures.
                - Handles proper cleanup of GPU resources through RAII semantics.
                - Non-copyable but fully movable for safe resource transfer.

                Used by Renderer2D, FontManager, and other graphics subsystems
                that require texture management.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <cstdint>

namespace Graphics
{
  class Texture2D
  {
  public:
    //Texture2D(const std::string &path) { loadFromFile(path); }
    Texture2D() = default;
    explicit Texture2D(const std::string& path) { LoadFromFile(path); }
    ~Texture2D();                           
    

    // Non-copyable, movable
    Texture2D(const Texture2D &) = delete;
    Texture2D &operator=(const Texture2D &) = delete;
    Texture2D(Texture2D &&) noexcept;
    Texture2D &operator=(Texture2D &&) noexcept;
    bool Load(const std::string& path);
    bool LoadFromFile(const std::string& path, bool flipY = true, bool alphaOnlyIsRed = false);
    void Unload();

 /*   bool loadFromFile(const std::string &path, bool flipY = true, bool alphaOnlyIsRed = false);
    void unload();*/

    void bind(unsigned slot = 0) const;

    inline unsigned id() const { return m_id; }
    inline int width() const { return m_width; }
    inline int height() const { return m_height; }

  private:
    unsigned m_id = 0;
    int m_width = 0, m_height = 0, m_channels = 0;
  };
}
