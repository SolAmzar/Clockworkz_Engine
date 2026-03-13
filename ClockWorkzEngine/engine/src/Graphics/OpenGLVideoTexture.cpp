#include "Graphics/OpenGLVideoTexture.hpp"
#include <GL/glew.h>
#include <iostream>

namespace Graphics {

OpenGLVideoTexture::OpenGLVideoTexture() {}

OpenGLVideoTexture::~OpenGLVideoTexture() { cleanup(); }

void OpenGLVideoTexture::cleanup() {
  if (m_textureId != 0) {
    glDeleteTextures(1, &m_textureId);
    m_textureId = 0;
  }
}

void OpenGLVideoTexture::init(int width, int height) {
  if (m_width == width && m_height == height && m_textureId != 0) {
    return; // Already initialized for this size
  }

  cleanup();

  m_width = width;
  m_height = height;

  glGenTextures(1, &m_textureId);
  glBindTexture(GL_TEXTURE_2D, m_textureId);

  // Nearest or Linear filtering for video
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Allocate immutable storage if possible, or just standard TexImage2D
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLVideoTexture::updatePixels(const uint8_t *pixels) {
  if (m_textureId == 0)
    return;

  glBindTexture(GL_TEXTURE_2D, m_textureId);
  // Replace the entire texture with the new frame's RGBA data
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA,
                  GL_UNSIGNED_BYTE, pixels);
  glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace Graphics
