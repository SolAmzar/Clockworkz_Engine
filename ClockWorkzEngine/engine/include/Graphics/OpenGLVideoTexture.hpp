#pragma once
#include <cstdint>

namespace Graphics {

class OpenGLVideoTexture {
public:
  OpenGLVideoTexture();
  ~OpenGLVideoTexture();

  void init(int width, int height);
  void updatePixels(const uint8_t *pixels); // Uploads RGBA frame to GPU

  unsigned int getRendererId() const { return m_textureId; }
  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }

private:
  void cleanup();

  unsigned int m_textureId{0};
  int m_width{0};
  int m_height{0};
  // Future optimization: PBO handles here
};

} // namespace Graphics
