/******************************************************************************/
/*!
\file           Shader.hpp
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

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

namespace Graphics {

    class Shader {
    public:
        unsigned int ID;
        Shader() = default;          // default constructor (needed by ResourceManager)
        ~Shader() { Unload(); }
        void use() const;
        void setMat4(const std::string& name, const glm::mat4& mat) const;
        void setVec4(const std::string& name, const glm::vec4& value) const;
        void setVec3(const std::string& name, const glm::vec3& value) const;
        bool Load(const std::string& vertexPath, const std::string& fragmentPath);
        void Unload();
    };


} // namespace Graphics
