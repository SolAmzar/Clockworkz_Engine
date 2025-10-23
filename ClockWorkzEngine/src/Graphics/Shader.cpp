/******************************************************************************/
/*!
\file           Shader.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Shader, a wrapper around OpenGL shader programs.
                Responsibilities include:
                - Load() to compile and link vertex and fragment shaders from
                  file paths
                - use() to activate the shader program for rendering
                - setMat4() and setVec3() to upload uniforms to the GPU
                - Unload() to delete the OpenGL program when no longer needed

                Simplifies shader management for the rendering pipeline.
                ResourceManger will load in the shader and manages this.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Graphics/Shader.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace Graphics;

static std::string loadFile(const std::string &path)
{
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void Shader::use() const { glUseProgram(ID); }

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

bool Shader::Load(const std::string &vertexPath, const std::string &fragmentPath)
{
    std::string vCode = loadFile(vertexPath);
    std::string fCode = loadFile(fragmentPath);
    if (vCode.empty() || fCode.empty())
    {
        std::cerr << "Failed to read shader source files.\n";
        return false;
    }

    const char *vSrc = vCode.c_str();
    const char *fSrc = fCode.c_str();

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vSrc, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fSrc, nullptr);
    glCompileShader(fs);

    ID = glCreateProgram();
    glAttachShader(ID, vs);
    glAttachShader(ID, fs);
    glLinkProgram(ID);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return true;
}

void Shader::Unload()
{
    if (ID)
    {
        glDeleteProgram(ID);
        ID = 0;
    }
}
