// Lebon
#pragma once
#include <glm/mat4x4.hpp>

namespace Graphics {

    enum class Anchor { BottomLeft, Center };

    class Shader;

    class Renderer2D {
    public:
        bool init();
        void shutdown();
        void begin(const glm::mat4& vp);
        void end();

        // Full draw with UVs (for sprite sheets)
        void drawQuad(float x, float y, float w, float h,
            int texW, int texH, unsigned texId,
            float uvx, float uvy, float uvw, float uvh,
            float r, float g, float b, float a, float angleRad = 0.f,
            Anchor anchor = Anchor::BottomLeft);

    private:
        // Vertex Array + Buffers for the Bottom-Left (0..1) anchored quad
        unsigned m_vaoBottomLeft = 0;
        unsigned m_vboBottomLeft = 0;
        unsigned m_eboBottomLeft = 0;

        // Vertex Array + Buffers for the Center (-0.5..+0.5) anchored quad
        unsigned m_vaoCenter = 0;
        unsigned m_vboCenter = 0;
        unsigned m_eboCenter = 0;
        Shader* m_shader = nullptr;
        glm::mat4 m_vp{ 1.0f };
    };

} // namespace Graphics
