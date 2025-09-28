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
        void setVec3(const std::string& name, const glm::vec3& value) const;
        bool Load(const std::string& vertexPath, const std::string& fragmentPath);
        void Unload();
    };


} // namespace Graphics
