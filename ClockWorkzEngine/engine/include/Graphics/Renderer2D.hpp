/******************************************************************************/
/*!
\file           Renderer2D.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the Renderer2D class responsible for batched 2D sprite
                rendering using OpenGL.

                Responsibilities:
                - Initializes and manages OpenGL vertex/index buffers for
                  efficient quad batching.
                - Supports textured and colored quads with rotation, anchor,
                  and UV sub-rectangle control.
                - Handles texture slot management to reduce draw calls.
                - Provides functions to begin, submit, flush, and end batches.
                - Integrates with Shader and ResourceManager systems for
                  GPU rendering setup.

                Constants:
                - MAX_SPRITES: Defines the upper limit for quads in a single
batch.
                - MAX_TEXTURE_SLOTS: Determines how many unique textures can be
                  bound per draw call.

                Designed as the core 2D renderer used by gameplay systems such
as SpriteRenderer, UI components, and visual effects.

All content c 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace Graphics {
enum class Anchor { BottomLeft, Center };
class Renderer2D {
public:
  bool init();
  void shutdown();

  // Called once per frame with camera VP
  void begin(const glm::mat4 &vp);

  // Start a batch (call once per frame after begin)
  void beginBatch();

  // Queue a quad into the current batch (no draw calls here)
  void submitQuad(const glm::vec2 &pos, const glm::vec2 &size, unsigned texId,
                  const glm::vec4 &color = glm::vec4(1, 1, 1, 1),
                  float angleRad = 0.0f,
                  const glm::vec4 &uvRect = glm::vec4(0, 0, 1, 1),
                  Anchor anchor = Anchor::Center,
                  const glm::vec2 &pivot = glm::vec2(0.f, 0.f));

  // Flush current batch (does the actual draw call(s))
  void flush();

  // End the frame (no-op for now)
  void end();

private:
  // GPU
  unsigned m_vao = 0, m_vbo = 0, m_ebo = 0;
  // Shader is loaded via ResourceManager<Shader>::Get("texture")
  // and used once per frame in begin()

  // CPU batch buffer
  struct Vertex {
    glm::vec3 pos;   // x,y, z=0
    glm::vec2 uv;    // u,v
    glm::vec4 color; // r,g,b,a
    float texIndex;  // which bound texture slot
  };

  static constexpr int MAX_SPRITES = 10000; // safe for you
  static constexpr int VERTS_PER_SPR = 4;
  static constexpr int IND_PER_SPR = 6;
  static constexpr int MAX_VERTS = MAX_SPRITES * VERTS_PER_SPR;
  static constexpr int MAX_INDS = MAX_SPRITES * IND_PER_SPR;

  std::vector<Vertex> m_vertices;
  int m_spriteCount = 0;

  // View-projection cache for the frame
  glm::mat4 m_vp{1.0f};

  // Texture slots (sampler array)
  static constexpr int MAX_TEXTURE_SLOTS =
      8; // GL 3.3 usually has >=16; use 8 for safety
  unsigned m_textures[MAX_TEXTURE_SLOTS] = {};
  int m_numTextures = 0;
  unsigned m_whiteTex = 0;
};

} // namespace Graphics
