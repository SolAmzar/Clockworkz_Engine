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
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <ft2build.h> // Free Type Library
#include <freetype/freetype.h>
#include "Graphics/Shader.hpp"

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

    // Render text constrained to a bounding box.
    // (x, y) = top-left corner of box (y-up OpenGL space, so y is the higher value).
    // hAlign: 0=Left, 1=Center, 2=Right
    // vAlign: 0=Top,  1=Middle, 2=Bottom
    // wordWrap: wrap at box width; if false splits only on '\n'
    // lineSpacing: multiplier on line height (1.0 = normal)
    void RenderTextInBox(const std::string &text, float x, float y,
                         float boxWidth, float boxHeight, float scale,
                         glm::vec3 color, Graphics::Shader &s,
                         int hAlign, int vAlign,
                         bool wordWrap, float lineSpacing);

    // Measure the width of a text string in pixels (before scaling)
    float MeasureTextWidth(const std::string &text, float scale) const;

    // Get the line height for this font at the given scale
    float GetLineHeight(float scale) const;

    inline const std::map<char, Character> &GetCharacters() const { return characters; }

    // Load all fonts listed in a JSON config file (same pattern as TextureManager)
    // Each entry: { "name": "Title", "path": "assets/fonts/MANIC1.ttf", "size": 48 }
    static bool LoadFromConfig(const std::string &configPath);

    // Import a font from an external path: copies it to assets/fonts/, adds to config, loads into memory.
    static bool ImportFont(const std::string &sourcePath, const std::string &name,
                           unsigned int size,
                           const std::string &configPath = "assets/configs/fonts.json");

    // Delete a font: unloads from memory, removes from config, deletes file from disk.
    static bool DeleteFont(const std::string &name,
                           const std::string &configPath = "assets/configs/fonts.json");

    // Returns the file path stored in the config for a named font (empty if not found).
    static std::string GetFilePath(const std::string &name,
                                   const std::string &configPath = "assets/configs/fonts.json");

    // Unload a named font from memory (ResourceManager).
    static void Unload(const std::string &name);

  private:
    FT_Face face;
    FT_Library ft;
    std::map<char, Character> characters;
    unsigned int VAO, VBO;

    void SetupRenderData();
    // Break text into lines that fit within maxWidth
    std::vector<std::string> WrapText(const std::string &text, float maxWidth, float scale) const;
  };

} // namespace ClockWorkz::Assets
