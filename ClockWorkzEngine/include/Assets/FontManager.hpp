/******************************************************************************/
/*!
\file           FontManager.hpp
\project        Lights Out
\author(s)      [Lebon], 100% 
\brief          FontManager handles loading, storing, and rendering fonts using
                the FreeType library. Each font type will be loaded as a font 
                manager using ResourceManager
                It provides:
                - Load() to initialize font data from file at a given size
                - RenderText() to draw strings onto the screen with OpenGL
                - Access to glyph metrics and textures via GetCharacters()
                - Automatic cleanup of allocated resources with Unload()

References:
- FreeType Tutorial: https://freetype.org/freetype2/docs/tutorial/step1.html
- LearnOpenGL Text Rendering Guide: https://learnopengl.com/In-Practice/Text-Rendering
- OpenGL glTexImage2D Reference: https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
- OpenGL Vertex Specification (VAO/VBO): https://www.khronos.org/opengl/wiki/Vertex_Specification


All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <ft2build.h> // Free Type Library
#include "Graphics/Shader.hpp"
#include FT_FREETYPE_H

namespace Assets
{

  struct Character
  {
    unsigned int TextureID; // OpenGL texture ID
    glm::ivec2 Size;        // Glyph size
    glm::ivec2 Bearing;     // Offset from baseline
    unsigned int Advance;   // Horizontal offset to next glyph
  };

  class FontManager
  {
  public:
    FontManager();
    ~FontManager();

    // Load font from file
    bool Load(const std::string &filePath, unsigned int fontSize);
    void Unload();
    // Render text with this font
    void RenderText(const std::string &text, float x, float y, float scale, glm::vec3 color, Graphics::Shader& s);

    inline const std::map<char, Character> &GetCharacters() const { return characters; }

  private:
    FT_Face face;
    FT_Library ft;
    std::map<char, Character> characters;
    unsigned int VAO, VBO;

    void SetupRenderData();
  };

} // namespace ClockWorkz::Assets
