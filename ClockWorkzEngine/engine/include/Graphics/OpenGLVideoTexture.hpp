/******************************************************************************/
/*!
\file           OpenGLVideoTexture.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          Declares a lightweight OpenGL texture helper for uploaded video
                frames.

                Owns a GPU texture sized for decoded RGBA video data and exposes
                update and cleanup helpers used by the video playback systems.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

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
