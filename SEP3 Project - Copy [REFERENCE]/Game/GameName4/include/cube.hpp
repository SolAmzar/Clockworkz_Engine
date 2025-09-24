#pragma once

#include "renderable_object.hpp"
#include "geometry_buffer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <GL/glew.h>

class Cube : public RenderableObject {
public:
    Cube();
    virtual ~Cube() = default;

protected:
    // Implement pure virtual methods from RenderableObject
    void setupGeometry() override;
    
    // Override to use file-based shaders
    std::string getVertexShaderPath() const override;
    std::string getFragmentShaderPath() const override;

private:
    // Geometry data and methods (merged from CubeGeometry)
    std::vector<GLfloat> m_vertexData;
    static constexpr GLsizei STRIDE = 6 * sizeof(GLfloat); // 3 position + 3 color
    
    void generateVertexData();
    const std::vector<GLfloat>& getVertexData() const;
    GLsizei getStride() const;
    GLsizei getVertexCount() const;
};
