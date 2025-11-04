// Lebon for Resource Manager to Load in Textures
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
