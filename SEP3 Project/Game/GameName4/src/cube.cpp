#include "cube.hpp"
#include <filesystem>
#include <iostream>

Cube::Cube() {
    generateVertexData();
}

// Helper function to get the executable directory
std::string getExecutableDirectory() {
    try {
        // Get the current working directory
        std::filesystem::path currentPath = std::filesystem::current_path();
        std::cout << "Current working directory: " << currentPath.string() << std::endl;
        
        // Try to find the shaders directory in the current path
        std::filesystem::path shaderDir = currentPath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return currentPath.string();
        }
        
        // Try Debug subdirectory (for build/Debug structure)
        std::filesystem::path debugPath = currentPath / "Debug";
        shaderDir = debugPath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return debugPath.string();
        }
        
        // Try Release subdirectory (for build/Release structure)
        std::filesystem::path releasePath = currentPath / "Release";
        shaderDir = releasePath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return releasePath.string();
        }
        
        // If not found, try going up one directory (in case we're in a subdirectory)
        std::filesystem::path parentPath = currentPath.parent_path();
        shaderDir = parentPath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return parentPath.string();
        }
        
        // Try going up two directories (for build/Debug structure)
        std::filesystem::path grandParentPath = parentPath.parent_path();
        shaderDir = grandParentPath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return grandParentPath.string();
        }
        
        // Try going up three directories (for build/Debug structure)
        std::filesystem::path greatGrandParentPath = grandParentPath.parent_path();
        shaderDir = greatGrandParentPath / "shaders";
        if (std::filesystem::exists(shaderDir)) {
            std::cout << "Found shaders directory at: " << shaderDir.string() << std::endl;
            return greatGrandParentPath.string();
        }
        
        std::cout << "Could not find shaders directory, using current path" << std::endl;
        return currentPath.string();
    }
    catch (const std::exception& e) {
        std::cerr << "Warning: Could not determine executable directory: " << e.what() << std::endl;
        return ".";
    }
}

void Cube::setupGeometry() {
    const auto& vertexData = getVertexData();
    m_geometryBuffer->initialize(vertexData, getStride());
}

std::string Cube::getVertexShaderPath() const {
    std::string exeDir = getExecutableDirectory();
    std::filesystem::path shaderPath = std::filesystem::path(exeDir) / "shaders" / "cube.vert";
    
    std::cout << "Looking for vertex shader at: " << shaderPath.string() << std::endl;
    
    if (std::filesystem::exists(shaderPath)) {
        std::cout << "Found vertex shader at: " << shaderPath.string() << std::endl;
        return shaderPath.string();
    } else {
        std::cout << "Vertex shader not found, using relative path" << std::endl;
        return "shaders/cube.vert";
    }
}

std::string Cube::getFragmentShaderPath() const {
    std::string exeDir = getExecutableDirectory();
    std::filesystem::path shaderPath = std::filesystem::path(exeDir) / "shaders" / "cube.frag";
    
    std::cout << "Looking for fragment shader at: " << shaderPath.string() << std::endl;
    
    if (std::filesystem::exists(shaderPath)) {
        std::cout << "Found fragment shader at: " << shaderPath.string() << std::endl;
        return shaderPath.string();
    } else {
        std::cout << "Fragment shader not found, using relative path" << std::endl;
        return "shaders/cube.frag";
    }
}

// Geometry methods (merged from CubeGeometry)
void Cube::generateVertexData() {
    // Generate a quad (two triangles) with position and color data
    // Format: x, y, z, r, g, b
    m_vertexData = {
        // First triangle
        -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 1.0f,   // Bottom-left, cyan
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 1.0f,   // Bottom-right, magenta
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   // Top-right, yellow

        // Second triangle
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   // Top-right (repeat), red
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   // Top-left, green
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f    // Bottom-left (repeat), blue
    };
}

const std::vector<GLfloat>& Cube::getVertexData() const {
    return m_vertexData;
}

GLsizei Cube::getStride() const {
    return STRIDE;
}

GLsizei Cube::getVertexCount() const {
    return static_cast<GLsizei>(m_vertexData.size()) / 6; // 6 floats per vertex (3 position + 3 color)
}