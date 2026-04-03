/******************************************************************************/
/*!
\file			VecCompatGLM.hpp
\project        Lights Out
\author(s)      [Sol-Amzar], 100%
\brief          Declares conversion functions between your custom vector/matrix types
                and GLM types for interoperability.
                Responsibilities include:
                - to_vec2, to_vec3, to_vec4: Convert glm::vec* to your vec*
                - to_glm: Convert your vec* to glm::vec*
				- to_glm and to_mat4: Convert between your Mat4 and glm::mat4
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <glm/glm.hpp>

#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Math/Matrix.hpp"   

namespace compat {

    // -------- vec2 <-> glm::vec2 --------
    inline vec2 to_vec2(glm::vec2 v) noexcept { return { v.x, v.y }; }
    inline glm::vec2 to_glm(vec2 v) noexcept { return { v.x, v.y }; }

    // -------- vec3 <-> glm::vec3 --------
    inline vec3 to_vec3(glm::vec3 v) noexcept { return { v.x, v.y, v.z }; }
    inline glm::vec3 to_glm(vec3 v) noexcept { return { v.x, v.y, v.z }; }

    // -------- vec4 <-> glm::vec4 --------
    inline vec4 to_vec4(glm::vec4 v) noexcept { return { v.x, v.y, v.z, v.w }; }
    inline glm::vec4 to_glm(vec4 v) noexcept { return { v.x, v.y, v.z, v.w }; }

    // -------- cwz::Mat4 <-> glm::mat4 --------
    // Your Mat4 fields are column-major named m00,m10,m20,m30, m01,m11,... etc.
    // glm::mat4(ct0, ct1, ct2, ct3) takes columns as glm::vec4.

    inline glm::mat4 to_glm(cwz::Mat4 const& m) noexcept {
        return glm::mat4{
          glm::vec4(m.m00, m.m10, m.m20, m.m30),  // col 0
          glm::vec4(m.m01, m.m11, m.m21, m.m31),  // col 1
          glm::vec4(m.m02, m.m12, m.m22, m.m32),  // col 2
          glm::vec4(m.m03, m.m13, m.m23, m.m33)   // col 3
        };
    }

    inline cwz::Mat4 to_mat4(glm::mat4 const& g) noexcept {
        cwz::Mat4 m;
        // g[col][row]
        m.m00 = g[0][0]; m.m10 = g[0][1]; m.m20 = g[0][2]; m.m30 = g[0][3];
        m.m01 = g[1][0]; m.m11 = g[1][1]; m.m21 = g[1][2]; m.m31 = g[1][3];
        m.m02 = g[2][0]; m.m12 = g[2][1]; m.m22 = g[2][2]; m.m32 = g[2][3];
        m.m03 = g[3][0]; m.m13 = g[3][1]; m.m23 = g[3][2]; m.m33 = g[3][3];
        return m;
    }

} // namespace compat
