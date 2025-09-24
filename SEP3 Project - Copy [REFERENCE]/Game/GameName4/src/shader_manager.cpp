#include "shader_manager.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

ShaderManager::ShaderManager() {
}

ShaderManager::~ShaderManager() {
}

GLuint ShaderManager::createProgram(const std::string& vertexShader, const std::string& fragmentShader) {
    GLuint vertexShaderId = compileShader(vertexShader, GL_VERTEX_SHADER);
    GLuint fragmentShaderId = compileShader(fragmentShader, GL_FRAGMENT_SHADER);
    
    GLuint programId = linkProgram(vertexShaderId, fragmentShaderId);
    
    // Clean up individual shaders
    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);
    
    return programId;
}

GLuint ShaderManager::createProgramFromFiles(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
    std::string vertexShaderSource = readShaderFile(vertexShaderPath);
    std::string fragmentShaderSource = readShaderFile(fragmentShaderPath);
    
    return createProgram(vertexShaderSource, fragmentShaderSource);
}

void ShaderManager::useProgram(GLuint programId) {
    glUseProgram(programId);
}

GLint ShaderManager::getUniformLocation(GLuint programId, const std::string& uniformName) {
    return glGetUniformLocation(programId, uniformName.c_str());
}

void ShaderManager::setUniformMatrix4fv(GLuint programId, const std::string& uniformName, const float* matrix) {
    GLint location = getUniformLocation(programId, uniformName);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
}

void ShaderManager::deleteProgram(GLuint programId) {
    glDeleteProgram(programId);
}

GLuint ShaderManager::compileShader(const std::string& source, GLenum shaderType) {
    GLuint shaderId = glCreateShader(shaderType);
    const char* sourcePtr = source.c_str();
    glShaderSource(shaderId, 1, &sourcePtr, nullptr);
    glCompileShader(shaderId);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shaderId, 1024, nullptr, infoLog);
        std::string shaderTypeStr = (shaderType == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        throw std::runtime_error("ERROR::SHADER::" + shaderTypeStr + "::COMPILATION_FAILED\n" + std::string(infoLog));
    }
    
    return shaderId;
}

GLuint ShaderManager::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShader);
    glAttachShader(programId, fragmentShader);
    glLinkProgram(programId);
    
    // Check linking status
    GLint success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(programId, 1024, nullptr, infoLog);
        throw std::runtime_error("ERROR::SHADER::PROGRAM::LINKING_FAILED\n" + std::string(infoLog));
    }
    
    return programId;
}

std::string ShaderManager::readShaderFile(const std::string& filePath) {
    std::ifstream shaderFile;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    // Debug: Print the path we're trying to read
    std::cout << "Attempting to read shader file: " << filePath << std::endl;
    
    try {
        shaderFile.open(filePath);
        std::stringstream shaderStream;
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
        std::cout << "Successfully read shader file: " << filePath << std::endl;
        return shaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cout << "Failed to read shader file: " << filePath << std::endl;
        throw std::runtime_error("ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " + filePath + " - " + std::string(e.what()));
    }
}
