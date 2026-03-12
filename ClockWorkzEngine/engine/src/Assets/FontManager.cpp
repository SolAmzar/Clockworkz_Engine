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
#include "Assets/ResourceManager.hpp"
#include "Core/json.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

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
                                       glm::vec3 color, Graphics::Shader &shader,
                                       int hAlign, int vAlign,
                                       bool wordWrap, float lineSpacing)
    {
        // --- Build line list ---
        std::vector<std::string> lines;
        if (wordWrap) {
            lines = WrapText(text, boxWidth, scale);
        } else {
            std::istringstream ss(text);
            std::string ln;
            while (std::getline(ss, ln)) lines.push_back(ln);
            if (lines.empty()) lines.push_back(text);
        }

        float lineH      = GetLineHeight(scale) * lineSpacing;
        float totalTextH = static_cast<float>(lines.size()) * lineH;

        // --- Compute vertical start (cursor = baseline of first line) ---
        // textBlockTop = y-coordinate of the top of the first line (OpenGL y-up)
        float textBlockTop;
        if (vAlign == 1)      // Middle
            textBlockTop = y - (boxHeight - totalTextH) * 0.5f;
        else if (vAlign == 2) // Bottom
            textBlockTop = y - boxHeight + totalTextH;
        else                  // Top (default)
            textBlockTop = y;

        float cursorY = textBlockTop - lineH;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);
        shader.setVec3("textColor", color);

        for (const auto &line : lines)
        {
            // Clip lines that fall outside the box bottom
            if (cursorY < y - boxHeight)
                break;

            // Horizontal alignment
            float lineWidth = MeasureTextWidth(line, scale);
            float lineX = x;
            if (hAlign == 1)       lineX = x + (boxWidth - lineWidth) * 0.5f;
            else if (hAlign == 2)  lineX = x + boxWidth - lineWidth;

            float penX = lineX;
            for (auto c : line)
            {
                auto it = characters.find(c);
                if (it == characters.end()) continue;
                const Character &ch = it->second;

                float xpos = penX + ch.Bearing.x * scale;
                float ypos = cursorY - (ch.Size.y - ch.Bearing.y) * scale;
                float w    = ch.Size.x * scale;
                float h    = ch.Size.y * scale;

                float vertices[6][4] = {
                    {xpos,     ypos + h, 0.0f, 0.0f},
                    {xpos,     ypos,     0.0f, 1.0f},
                    {xpos + w, ypos,     1.0f, 1.0f},

                    {xpos,     ypos + h, 0.0f, 0.0f},
                    {xpos + w, ypos,     1.0f, 1.0f},
                    {xpos + w, ypos + h, 1.0f, 0.0f}
                };

                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                penX += (ch.Advance >> 6) * scale;
            }

            cursorY -= lineH;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

bool FontManager::LoadFromConfig(const std::string &configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[FontManager] Could not open config: " << configPath << "\n";
        return false;
    }

    nlohmann::json j;
    try { file >> j; }
    catch (const std::exception &e) {
        std::cerr << "[FontManager] JSON parse error in " << configPath << ": " << e.what() << "\n";
        return false;
    }

    if (!j.contains("fonts") || !j["fonts"].is_array()) {
        std::cerr << "[FontManager] Config missing 'fonts' array: " << configPath << "\n";
        return false;
    }

    bool allOk = true;
    for (const auto &entry : j["fonts"]) {
        if (!entry.contains("name") || !entry.contains("path") || !entry.contains("size")) {
            std::cerr << "[FontManager] Skipping entry missing name/path/size\n";
            continue;
        }
        std::string name = entry["name"].get<std::string>();
        std::string path = entry["path"].get<std::string>();
        unsigned int size = entry["size"].get<unsigned int>();

        // Resolve path relative to the config file's directory so it works
        // regardless of the current working directory.
        fs::path resolved = fs::path(configPath).parent_path() / path;
        path = resolved.generic_string();

        auto font = ResourceManager<FontManager>::Load(name, path, size);
        if (!font) {
            std::cerr << "[FontManager] Failed to load font: " << name << " from " << path << "\n";
            allOk = false;
        } else {
            std::cout << "[FontManager] Loaded: " << name << " (" << path << ", " << size << "pt)\n";
        }
    }
    return allOk;
}

bool FontManager::ImportFont(const std::string &sourcePath, const std::string &name,
                             unsigned int size, const std::string &configPath) {
    if (!fs::exists(sourcePath)) {
        std::cerr << "[FontManager] Source file not found: " << sourcePath << "\n";
        return false;
    }

    fs::create_directories("assets/fonts");
    std::string filename = fs::path(sourcePath).filename().string();
    std::string destPath = "assets/fonts/" + filename;
    try {
        fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error &e) {
        std::cerr << "[FontManager] Failed to copy font file: " << e.what() << "\n";
        return false;
    }

    // Add to JSON config
    std::ifstream inFile(configPath);
    nlohmann::json j;
    if (inFile.is_open()) {
        try { inFile >> j; } catch (...) { j = nlohmann::json::object(); }
        inFile.close();
    }
    if (!j.contains("fonts") || !j["fonts"].is_array())
        j["fonts"] = nlohmann::json::array();

    for (const auto &entry : j["fonts"]) {
        if (entry.value("name", "") == name) {
            std::cerr << "[FontManager] Font name already exists: " << name << "\n";
            return false;
        }
    }
    nlohmann::json newEntry;
    newEntry["name"] = name;
    newEntry["path"] = destPath;
    newEntry["size"] = size;
    j["fonts"].push_back(newEntry);

    std::ofstream outFile(configPath);
    if (!outFile.is_open()) {
        std::cerr << "[FontManager] Failed to write config: " << configPath << "\n";
        return false;
    }
    outFile << std::setw(4) << j << "\n";
    outFile.close();

    // Load into memory
    auto font = ResourceManager<FontManager>::Load(name, destPath, size);
    if (!font) {
        std::cerr << "[FontManager] Failed to load imported font: " << name << "\n";
        return false;
    }
    std::cout << "[FontManager] Imported font: " << name << " from " << destPath << "\n";
    return true;
}

std::string FontManager::GetFilePath(const std::string &name, const std::string &configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) return "";
    nlohmann::json j;
    try { file >> j; } catch (...) { return ""; }
    if (!j.contains("fonts") || !j["fonts"].is_array()) return "";
    for (const auto &entry : j["fonts"]) {
        if (entry.value("name", "") == name)
            return entry.value("path", "");
    }
    return "";
}

void FontManager::Unload(const std::string &name) {
    ResourceManager<FontManager>::Unload(name);
}

bool FontManager::DeleteFont(const std::string &name, const std::string &configPath) {
    std::string filePath = GetFilePath(name, configPath);

    // Unload from memory
    Unload(name);

    // Remove from JSON config
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        std::cerr << "[FontManager] Cannot open config for delete: " << configPath << "\n";
        return false;
    }
    nlohmann::json j;
    try { inFile >> j; } catch (...) { return false; }
    inFile.close();

    if (!j.contains("fonts") || !j["fonts"].is_array()) return false;
    auto &arr = j["fonts"];
    bool removed = false;
    for (auto it = arr.begin(); it != arr.end();) {
        if (it->value("name", "") == name) { it = arr.erase(it); removed = true; }
        else ++it;
    }
    if (!removed) return false;

    std::ofstream outFile(configPath);
    if (!outFile.is_open()) return false;
    outFile << std::setw(4) << j << "\n";
    outFile.close();

    // Delete physical file (only if it's in assets/fonts/ — never delete system fonts)
    if (!filePath.empty()) {
        fs::path p(filePath);
        if (p.string().find("assets/fonts") != std::string::npos && fs::exists(p)) {
            std::error_code ec;
            fs::remove(p, ec);
            if (ec) std::cerr << "[FontManager] Failed to delete file: " << ec.message() << "\n";
        }
    }
    std::cout << "[FontManager] Deleted font: " << name << "\n";
    return true;
}

} // namespace ClockWorkz::Assets
