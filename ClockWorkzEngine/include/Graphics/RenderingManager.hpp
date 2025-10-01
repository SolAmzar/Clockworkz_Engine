#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Graphics { 
    class Camera2D; 

    class RenderingManager {
    public:
        bool init();
        void beginFrame(const Camera2D& cam);
        void endFrame();

        // Debug drawing methods
        void drawPoint(float x, float y, float r, float g, float b, float size = 5.0f);
        void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b);
        void drawRect(float x, float y, float w, float h, float r, float g, float b);
        void drawCircle(float x, float y, float radius, float r, float g, float b, int segments = 32);
        
        // Convenience methods for game objects
        void drawAABB(const glm::vec2& min, const glm::vec2& max, const glm::vec3& color);
        void drawVelocity(const glm::vec2& pos, const glm::vec2& vel, const glm::vec3& color);

    private:
        unsigned int m_vao{0}, m_vbo{0};
        unsigned int m_shader{0};
    };
}
