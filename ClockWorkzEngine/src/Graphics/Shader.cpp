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
