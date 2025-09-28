#pragma once
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace ClockWorkz::Assets
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
    void RenderText(const std::string &text, float x, float y, float scale, glm::vec3 color);

    inline const std::map<char, Character> &GetCharacters() const { return characters; }

  private:
    FT_Face face;
    FT_Library ft;
    std::map<char, Character> characters;
    unsigned int VAO, VBO;

    void SetupRenderData();
  };

} // namespace ClockWorkz::Assets
