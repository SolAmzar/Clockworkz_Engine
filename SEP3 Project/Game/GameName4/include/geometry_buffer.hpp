#pragma once

#include <GL/glew.h>
#include <vector>

class GeometryBuffer {
public:
    GeometryBuffer();
    ~GeometryBuffer();

    // Initialize the buffer with vertex data
    void initialize(const std::vector<GLfloat>& vertexData, GLsizei stride);
    
    // Bind the buffer for rendering
    void bind();
    
    // Unbind the buffer
    void unbind();
    
    // Get the number of vertices
    GLsizei getVertexCount() const;
    
    // Get the stride (bytes per vertex)
    GLsizei getStride() const;

private:
    GLuint m_vertexArrayId;
    GLuint m_vertexBuffer;
    GLsizei m_vertexCount;
    GLsizei m_stride;
    bool m_initialized;
    
    void setupVertexAttributes(GLsizei stride);
};
