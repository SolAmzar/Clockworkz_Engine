/******************************************************************************/
/*!
\file           FontManager.cpp
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
#include "Assets/FontManager.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>

namespace Assets
{

    FontManager::FontManager() : face(nullptr), VAO(3), VBO(3)
    {
        if (FT_Init_FreeType(&ft))
        {
            std::cerr << "ERROR::FREETYPE: Could not init FreeType library\n";
        }
    }

    FontManager::~FontManager()
    {
        Unload();             // free GPU + FreeType resources
        FT_Done_FreeType(ft); // finally shut down FreeType library
    }

    bool FontManager::Load(const std::string &filePath, unsigned int fontSize)
    {
        std::cerr << "[FontManager] Trying to load font: " << filePath << std::endl;
        /*
        For Debugging
        FILE* f = fopen(filePath.c_str(), "rb");
        if (!f) {
            std::cerr << "File not found at runtime path!" << std::endl;
        }
        else {
            std::cerr << "File exists, FreeType still failing." << std::endl;
            fclose(f);
        }*/
        if (FT_New_Face(ft, filePath.c_str(), 0, &face))
        {
            std::cerr << "ERROR::FREETYPE: Failed to load font: " << filePath << "\n";
            return false;
        }
        FT_Set_Pixel_Sizes(face, 0, fontSize);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++)
        {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            {
                std::cerr << "ERROR::FREETYPE: Failed to load Glyph\n";
                continue;
            }
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)};

            characters.insert(std::pair<char, Character>(c, character));
        }

        SetupRenderData();
        return true;
    }

    void FontManager::Unload()
    {
        // Delete glyph textures
        for (auto &kv : characters)
        {
            glDeleteTextures(1, &kv.second.TextureID);
        }
        characters.clear();

        // Delete buffers
        if (VBO)
            glDeleteBuffers(1, &VBO);
        if (VAO)
            glDeleteVertexArrays(1, &VAO);

        VBO = VAO = 0;

        // Delete FreeType face
        if (face)
        {
            FT_Done_Face(face);
            face = nullptr;
        }
    }

    void FontManager::SetupRenderData()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void FontManager::RenderText(const std::string &text, float x, float y, float scale, glm::vec3 color, Graphics::Shader &shader)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);
        shader.setVec3("textColor", color);
        for (auto c : text)
        {
            Character ch = characters[c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                {xpos, ypos + h, 0.0f, 0.0f},
                {xpos, ypos, 0.0f, 1.0f},
                {xpos + w, ypos, 1.0f, 1.0f},

                {xpos, ypos + h, 0.0f, 0.0f},
                {xpos + w, ypos, 1.0f, 1.0f},
                {xpos + w, ypos + h, 1.0f, 0.0f}};

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            // advance is in 1/64 pixels, so shift by 6
            // 2^6 is 64, this is to offset the next character x position
            x += (ch.Advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

    float FontManager::MeasureTextWidth(const std::string &text, float scale) const
    {
        float width = 0.0f;
        for (auto c : text)
        {
            auto it = characters.find(c);
            if (it != characters.end())
            {
                width += (it->second.Advance >> 6) * scale;
            }
        }
        return width;
    }

    float FontManager::GetLineHeight(float scale) const
    {
        // Use the tallest glyph (capital H is a good reference) as line height
        auto it = characters.find('H');
        if (it != characters.end())
        {
            return it->second.Size.y * scale * 1.4f; // 1.4x for line spacing
        }
        return 24.0f * scale; // fallback
    }

    std::vector<std::string> FontManager::WrapText(const std::string &text, float maxWidth, float scale) const
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string paragraph;

        // Split by newlines first
        while (std::getline(stream, paragraph))
        {
            if (paragraph.empty())
            {
                lines.push_back("");
                continue;
            }

            // Word-wrap each paragraph
            std::istringstream wordStream(paragraph);
            std::string word;
            std::string currentLine;

            while (wordStream >> word)
            {
                std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
                float testWidth = MeasureTextWidth(testLine, scale);

                if (testWidth > maxWidth && !currentLine.empty())
                {
                    lines.push_back(currentLine);
                    currentLine = word;
                }
                else
                {
                    currentLine = testLine;
                }
            }

            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
            }
        }

        return lines;
    }

    void FontManager::RenderTextInBox(const std::string &text, float x, float y,
                                       float boxWidth, float boxHeight, float scale,
                                       glm::vec3 color, Graphics::Shader &shader, int alignment)
    {
        std::vector<std::string> lines = WrapText(text, boxWidth, scale);
        float lineHeight = GetLineHeight(scale);

        // Start rendering from top of box, going downward
        // y is the top-left of the box in world space (OpenGL: y increases upward)
        float cursorY = y - lineHeight;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);
        shader.setVec3("textColor", color);

        for (const auto &line : lines)
        {
            // Stop rendering if we've gone past the bottom of the box
            if (cursorY < y - boxHeight)
                break;

            // Calculate x offset based on alignment
            float lineWidth = MeasureTextWidth(line, scale);
            float lineX = x; // Left aligned by default

            if (alignment == 1) // Center
            {
                lineX = x + (boxWidth - lineWidth) * 0.5f;
            }
            else if (alignment == 2) // Right
            {
                lineX = x + boxWidth - lineWidth;
            }

            // Render each character in the line
            float penX = lineX;
            for (auto c : line)
            {
                Character ch = characters[c];

                float xpos = penX + ch.Bearing.x * scale;
                float ypos = cursorY - (ch.Size.y - ch.Bearing.y) * scale;

                float w = ch.Size.x * scale;
                float h = ch.Size.y * scale;

                float vertices[6][4] = {
                    {xpos, ypos + h, 0.0f, 0.0f},
                    {xpos, ypos, 0.0f, 1.0f},
                    {xpos + w, ypos, 1.0f, 1.0f},

                    {xpos, ypos + h, 0.0f, 0.0f},
                    {xpos + w, ypos, 1.0f, 1.0f},
                    {xpos + w, ypos + h, 1.0f, 0.0f}};

                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                penX += (ch.Advance >> 6) * scale;
            }

            cursorY -= lineHeight;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

} // namespace ClockWorkz::Assets
