//// Lebon
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
        void begin(const glm::mat4& vp);

        // Start a batch (call once per frame after begin)
        void beginBatch();

        // Queue a quad into the current batch (no draw calls here)
        void submitQuad(
            const glm::vec2& pos,
            const glm::vec2& size,
            unsigned texId,
            const glm::vec4& color = glm::vec4(1, 1, 1, 1),
            float angleRad = 0.0f,
            const glm::vec4& uvRect = glm::vec4(0, 0, 1, 1),
            Anchor anchor = Anchor::Center
        );

        // Flush current batch (does the actual draw call(s))
        void flush();

        // End the frame (no-op for now)
        void end();

        // (Legacy path) still kept if you need it, but avoid using per-quad:
 /*       void drawQuad(
            float x, float y, float w, float h,
            int texW, int texH, unsigned texId,
            float uvx, float uvy, float uvw, float uvh,
            float r, float g, float b, float a,
            float angleRad, Anchor anchor
        );*/

    private:
        // GPU
        unsigned m_vao = 0, m_vbo = 0, m_ebo = 0;
        // Shader is loaded via ResourceManager<Shader>::Get("texture")
        // and used once per frame in begin()

        // CPU batch buffer
        struct Vertex {
            glm::vec3 pos;    // x,y, z=0
            glm::vec2 uv;     // u,v
            glm::vec4 color;  // r,g,b,a
            float texIndex;   // which bound texture slot
        };

        static constexpr int MAX_SPRITES = 10000; // safe for you
        static constexpr int VERTS_PER_SPR = 4;
        static constexpr int IND_PER_SPR = 6;
        static constexpr int MAX_VERTS = MAX_SPRITES * VERTS_PER_SPR;
        static constexpr int MAX_INDS = MAX_SPRITES * IND_PER_SPR;

        std::vector<Vertex> m_vertices;
        int m_spriteCount = 0;

        // View-projection cache for the frame
        glm::mat4 m_vp{ 1.0f };

        // Texture slots (sampler array)
        static constexpr int MAX_TEXTURE_SLOTS = 8; // GL 3.3 usually has >=16; use 8 for safety
        unsigned m_textures[MAX_TEXTURE_SLOTS] = {};
        int m_numTextures = 0;
        unsigned m_whiteTex = 0;
        int findOrBindTexture(unsigned texId);
    };

} // namespace Graphics

//#pragma once
//#include <glm/mat4x4.hpp>
//
//namespace Graphics {
//
//    enum class Anchor { BottomLeft, Center };
//
//    class Shader;
//
//    class Renderer2D {
//    public:
//        bool init();
//        void shutdown();
//        void begin(const glm::mat4& vp);
//        void end();
//
//        // Full draw with UVs (for sprite sheets)
//        void drawQuad(float x, float y, float w, float h,
//            int texW, int texH, unsigned texId,
//            float uvx, float uvy, float uvw, float uvh,
//            float r, float g, float b, float a, float angleRad = 0.f,
//            Anchor anchor = Anchor::BottomLeft);
//
//    private:
//        // Vertex Array + Buffers for the Bottom-Left (0..1) anchored quad
//        unsigned m_vaoBottomLeft = 0;
//        unsigned m_vboBottomLeft = 0;
//        unsigned m_eboBottomLeft = 0;
//
//        // Vertex Array + Buffers for the Center (-0.5..+0.5) anchored quad
//        unsigned m_vaoCenter = 0;
//        unsigned m_vboCenter = 0;
//        unsigned m_eboCenter = 0;
//        Shader* m_shader = nullptr;
//        glm::mat4 m_vp{ 1.0f };
//    };
//
//} // namespace Graphics
