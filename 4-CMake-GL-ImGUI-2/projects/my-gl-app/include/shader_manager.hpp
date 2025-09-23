#pragma once

#include <GL/glew.h>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Compile and link shaders
    GLuint createProgram(const std::string& vertexShader, const std::string& fragmentShader);
    
    // Load shaders from files and create program
    GLuint createProgramFromFiles(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
    
    // Use the shader program
    void useProgram(GLuint programId);
    
    // Get uniform location
    GLint getUniformLocation(GLuint programId, const std::string& uniformName);
    
    // Set uniform values
    void setUniformMatrix4fv(GLuint programId, const std::string& uniformName, const float* matrix);
    
    // Cleanup
    void deleteProgram(GLuint programId);

private:
    GLuint compileShader(const std::string& source, GLenum shaderType);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::string readShaderFile(const std::string& filePath);
};
