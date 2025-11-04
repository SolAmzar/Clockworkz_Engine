// Lebon 100% Texture Class that read and load Image using stb_image library
#include "Graphics/Texture2D.hpp"
#include <GL/glew.h>

#define STB_IMAGE_IMPLEMENTATION
#include "Graphics/stb_image.h"

using namespace Graphics;

Texture2D::~Texture2D() { Unload(); }

Texture2D::Texture2D(Texture2D&& o) noexcept { *this = std::move(o); }
Texture2D& Texture2D::operator=(Texture2D&& o) noexcept {
  if (this != &o) {
    Unload();
    m_id = o.m_id; o.m_id = 0;
    m_width = o.m_width; m_height = o.m_height; m_channels = o.m_channels;
  }
  return *this;
}

bool Texture2D::Load(const std::string& path) {
    return LoadFromFile(path);
}
bool Texture2D::LoadFromFile(const std::string& path, bool flipY, bool alphaOnlyIsRed) {
  Unload();
  stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
  unsigned char* data = stbi_load(path.c_str(), &m_width, &m_height, &m_channels, 0);
  if (!data) return false;

  GLenum internalFmt = GL_RGBA8, fmt = GL_RGBA;
  if (m_channels == 3) { internalFmt = GL_RGB8; fmt = GL_RGB; }
  else if (m_channels == 1) {
    internalFmt = alphaOnlyIsRed ? GL_R8 : GL_R8; // keep simple
    fmt = GL_RED;
  }

  glGenTextures(1, &m_id);
  glBindTexture(GL_TEXTURE_2D, m_id);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_width, m_height, 0, fmt, GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(data);
  return true;
}

void Texture2D::Unload() {
  if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
}

void Texture2D::bind(unsigned slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, m_id);
}
