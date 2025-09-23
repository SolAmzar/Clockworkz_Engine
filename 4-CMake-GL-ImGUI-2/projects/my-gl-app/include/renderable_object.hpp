#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

class ShaderManager;
class GeometryBuffer;

class RenderableObject {
public:
    RenderableObject();
    virtual ~RenderableObject();

    // Initialize the renderable object
    virtual void initialize();
    
    // Render the object with the given model matrix
    virtual void render(const glm::mat4& modelMatrix);
    
    // Get the shader program ID
    GLuint getProgramId() const;

protected:
    GLuint m_programId;
    std::unique_ptr<GeometryBuffer> m_geometryBuffer;
    std::unique_ptr<ShaderManager> m_shaderManager;
    
    // Pure virtual methods to be implemented by derived classes
    virtual void setupGeometry() = 0;
    
    // Optional: Override to load shaders from files instead of strings
    virtual std::string getVertexShaderPath() const { return ""; }
    virtual std::string getFragmentShaderPath() const { return ""; }
};
