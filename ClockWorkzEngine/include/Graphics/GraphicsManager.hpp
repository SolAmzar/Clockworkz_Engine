/******************************************************************************/
/*!
\file           GraphicsManager.hpp
\project        Lights Out
\author(s)      [Lebon], 95%, [Syafiq], 5%
\brief          Declares GraphicsManager, the subsystem responsible for
                initializing and configuring global graphics state.
                Responsibilities include:
                - Initializing GLEW and OpenGL context
                - Setting viewport and clear color
                - Enabling blending and other global graphics options
                - Providing utility functions for graphics configuration
                - Used by Engine to prepare rendering environment

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include <glm/vec4.hpp>

namespace Platform { class WindowManager; }
namespace Graphics {
    class GraphicsManager {
    public:
        bool init(Platform::WindowManager& window);
        void setViewport(int w, int h);
        void setClearColor(float r, float g, float b, float a);
        
        // Helper methods for debug rendering state
        void enableLineSmooth();
        void setLineWidth(float width);
        void setPointSize(float size);
    };
}
