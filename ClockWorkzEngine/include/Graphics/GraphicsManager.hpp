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
