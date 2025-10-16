#pragma once
#include <glm/mat4x4.hpp>
namespace Graphics {
  class Shader;
  class Renderer2D {
  public:
    bool init();
    void shutdown();
    void begin(const glm::mat4& vp); // view-projection
    void drawQuad(float x, float y, float w, float h,
                  int texW, int texH, // for UV normalization
                  unsigned texId,
                  float uvx, float uvy, float uvw, float uvh,
                  float r, float g, float b, float a);
    void end();
  private:
    unsigned m_vao = 0, m_vbo = 0, m_ebo = 0;
    Shader*  m_shader = nullptr; // obtain from your ShaderManager if you have one
    glm::mat4 m_vp{1.0f};
  };
}
