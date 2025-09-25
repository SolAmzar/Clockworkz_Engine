#include "geometry_buffer.hpp"

GeometryBuffer::GeometryBuffer() 
    : m_vertexArrayId(0), m_vertexBuffer(0), m_vertexCount(0), m_stride(0), m_initialized(false) {
}

GeometryBuffer::~GeometryBuffer() {
    if (m_initialized) {
        glDeleteBuffers(1, &m_vertexBuffer);
        glDeleteVertexArrays(1, &m_vertexArrayId);
    }
}

void GeometryBuffer::initialize(const std::vector<GLfloat>& vertexData, GLsizei stride) {
    if (m_initialized) {
        // Clean up existing resources
        glDeleteBuffers(1, &m_vertexBuffer);
        glDeleteVertexArrays(1, &m_vertexArrayId);
    }
    
    m_stride = stride;
    m_vertexCount = static_cast<GLsizei>(vertexData.size()) / (stride / sizeof(GLfloat));
    
    // Generate and bind vertex array object
    glGenVertexArrays(1, &m_vertexArrayId);
    glBindVertexArray(m_vertexArrayId);
    
    // Generate and bind vertex buffer
    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(GLfloat), vertexData.data(), GL_STATIC_DRAW);
    
    // Setup vertex attributes
    setupVertexAttributes(stride);
    
    m_initialized = true;
}

void GeometryBuffer::bind() {
    if (m_initialized) {
        glBindVertexArray(m_vertexArrayId);
    }
}

void GeometryBuffer::unbind() {
    glBindVertexArray(0);
}

GLsizei GeometryBuffer::getVertexCount() const {
    return m_vertexCount;
}

GLsizei GeometryBuffer::getStride() const {
    return m_stride;
}

void GeometryBuffer::setupVertexAttributes(GLsizei stride) {
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    
    // Color attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(GLfloat)));
}
