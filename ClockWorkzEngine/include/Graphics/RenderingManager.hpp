/******************************************************************************/
/*!
\file           RenderingManager.hpp
\project        Lights Out
\author(s)      [Wei An] 50% [Lebon]50%
\brief          Declares RenderingManager, the graphics subsystem responsible
                for all debug and primitive rendering in the engine.
                Responsibilities include:
                - Initializing and managing OpenGL resources for rendering
                - Providing methods to draw points, lines, rectangles, circles,
                  and other debug primitives
                - Supporting frame lifecycle (beginFrame, endFrame) and camera
                - Offering convenience methods for game object visualization
                - Used by Engine and game states to visualize gameplay and debug
                - Coordinates with GraphicsManager for rendering state

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Math/Vec2.hpp"
#include "Math/config.hpp"
#include "Graphics/Renderer2D.hpp"
namespace Gameplay {
    class Physics2D;
    class GameObject;
}
namespace Graphics { 
    class Camera2D; 

    class RenderingManager {
    public:
        bool init();
        void beginFrame();
        void endFrame();
        void beginDebugPass(const glm::mat4& vp);
        void endDebugPass();
        void beginScenePass(const glm::mat4& vp);
        void endScenePass();
        void beginFontPass(const glm::mat4& vp);
        void endFontPass();

        void beginLightPass(const glm::mat4& vp, const glm::vec2& lightPos, const glm::vec3& color, const float& radius, const float& playerRotationAngle, const float& lightAngle);
        void endLightPass();
        // Debug drawing methods
        void drawPoint(float x, float y, float r, float g, float b, float size = 5.0f);
        void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b);
        void drawRect(float x, float y, float w, float h, float r, float g, float b);
        void drawCircle(float x, float y, float radius, float r, float g, float b, int segments = 32);
        
        // Convenience methods for game objects
        void drawAABB(const glm::vec2& min, const glm::vec2& max, const glm::vec3& color);
        void drawVelocity(const glm::vec2& pos, const glm::vec2& vel, const glm::vec3& color);
        // new high-level sprite draw
        void drawSprite(unsigned texId, const glm::vec2& pos, const glm::vec2& size, float angleRad = 0.f, 
            const glm::vec4& uv = glm::vec4(0.f,0.f,1.f,1.0f),
            const glm::vec4& color = glm::vec4(1.0f));
        std::vector<vec2> computeVisibilityPolygon(
            const vec2& origin,
            float angleCenter,
            float fov,
            float radius,
            Gameplay::Physics2D& physics,
            const std::vector<std::unique_ptr<Gameplay::GameObject>>& objs);

        void drawLightCone(
            const glm::vec2& origin,
            const std::vector<glm::vec2>& polygon,
            const glm::vec3& color) const;
    private:
        static constexpr int MAX_TEXTURE_SLOTS = 8;
        Renderer2D m_renderer2D; // textured renderer
        unsigned int m_vao{0}, m_vbo{0}; // Debug draw
        //unsigned int m_shader{0};
    };
}
