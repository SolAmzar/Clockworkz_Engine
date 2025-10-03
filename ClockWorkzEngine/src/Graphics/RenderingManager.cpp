#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>
#include <vector>
#include <cmath>

namespace Graphics {

namespace {
    const char* debugVertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec3 aColor;
        uniform mat4 uProjection;
        out vec3 Color;
        void main() {
            gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
            Color = aColor;
        }
    )";

    const char* debugFragmentShader = R"(
        #version 330 core
        in vec3 Color;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(Color, 1.0);
        }
    )";
}

bool RenderingManager::init() {
    // Create and compile shaders
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &debugVertexShader, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &debugFragmentShader, nullptr);
    glCompileShader(fs);

    m_shader = glCreateProgram();
    glAttachShader(m_shader, vs);
    glAttachShader(m_shader, fs);
    glLinkProgram(m_shader);

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Create buffers
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    return true;
}

void RenderingManager::drawPoint(float x, float y, float r, float g, float b, float size) {
    struct Vertex { float x, y; float r, g, b; };
    Vertex v = {x, y, r, g, b};
    
    glPointSize(size);
    glUseProgram(m_shader);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), &v, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    
    glDrawArrays(GL_POINTS, 0, 1);
}

void RenderingManager::drawLine(float x1, float y1, float x2, float y2, float r, float g, float b) {
    struct Vertex { float x, y; float r, g, b; };
    Vertex vertices[2] = {
        {x1, y1, r, g, b},
        {x2, y2, r, g, b}
    };
    
    glUseProgram(m_shader);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    
    glDrawArrays(GL_LINES, 0, 2);
}

void RenderingManager::drawRect(float x, float y, float w, float h, float r, float g, float b) {
    drawLine(x, y, x + w, y, r, g, b);
    drawLine(x + w, y, x + w, y + h, r, g, b);
    drawLine(x + w, y + h, x, y + h, r, g, b);
    drawLine(x, y + h, x, y, r, g, b);
}

void RenderingManager::drawCircle(float x, float y, float radius, float r, float g, float b, int segments) {
    const float step = 2.0f * 3.14159f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float angle1 = step * i;
        float angle2 = step * (i + 1);
        
        float x1 = x + radius * cosf(angle1);
        float y1 = y + radius * sinf(angle1);
        float x2 = x + radius * cosf(angle2);
        float y2 = y + radius * sinf(angle2);
        
        drawLine(x1, y1, x2, y2, r, g, b);
    }
}

void RenderingManager::drawAABB(const glm::vec2& min, const glm::vec2& max, const glm::vec3& color) {
    drawRect(min.x, min.y, max.x - min.x, max.y - min.y, color.r, color.g, color.b);
}

void RenderingManager::drawVelocity(const glm::vec2& pos, const glm::vec2& vel, const glm::vec3& color) {
    drawLine(pos.x, pos.y, pos.x + vel.x, pos.y + vel.y, color.r, color.g, color.b);
}

void RenderingManager::beginFrame(const Camera2D& cam) {
    // Clear buffers at the start of a new frame
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_shader);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "uProjection"), 
                       1, GL_FALSE, &cam.viewProj()[0][0]);
}

void RenderingManager::endFrame() {
    glBindVertexArray(0);
    glUseProgram(0);
}

} // namespace Graphics


