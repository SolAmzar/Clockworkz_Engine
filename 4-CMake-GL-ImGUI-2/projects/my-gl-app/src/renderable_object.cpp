#include "renderable_object.hpp"
#include "shader_manager.hpp"
#include "geometry_buffer.hpp"

RenderableObject::RenderableObject() 
    : m_programId(0) {
    m_shaderManager = std::make_unique<ShaderManager>();
    m_geometryBuffer = std::make_unique<GeometryBuffer>();
}

RenderableObject::~RenderableObject() {
    if (m_programId != 0) {
        m_shaderManager->deleteProgram(m_programId);
    }
}

void RenderableObject::initialize() {
    // Create shader program - check for file paths first
    std::string vertexPath = getVertexShaderPath();
    std::string fragmentPath = getFragmentShaderPath();
    
    if (!vertexPath.empty() && !fragmentPath.empty()) {
        // Load shaders from files
        m_programId = m_shaderManager->createProgramFromFiles(vertexPath, fragmentPath);
    }
    
    // Setup geometry
    setupGeometry();
}

void RenderableObject::render(const glm::mat4& modelMatrix) {
    if (m_programId == 0) {
        return; // Not initialized
    }
    
    // Use the shader program
    m_shaderManager->useProgram(m_programId);
    
    // Set the model matrix uniform
    m_shaderManager->setUniformMatrix4fv(m_programId, "vertexTransform", &modelMatrix[0][0]);
    
    // Bind and draw the geometry
    m_geometryBuffer->bind();
    glDrawArrays(GL_TRIANGLES, 0, m_geometryBuffer->getVertexCount());
    m_geometryBuffer->unbind();
}

GLuint RenderableObject::getProgramId() const {
    return m_programId;
}
