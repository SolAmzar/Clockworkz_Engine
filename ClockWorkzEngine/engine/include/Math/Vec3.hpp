/******************************************************************************/
/*!
\file           Vec3.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Establishes 3D vector structure with common vector operations.
                Responsibilities include:
                - Defining a vec3 struct with x, y, and z components
                - Implementing constructors for initialization
                - Overloading arithmetic operators (+, -, *, /) for vector math
                - Providing utility functions like dot product, cross product, normalization, length, and angle calculations
                - Including helper functions for linear interpolation, projection, rejection, reflection, and angle between vectors
				- Serving as a lightweight, header-only math utility for 3D vector operations
All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/



#pragma once
#ifndef VEC3
#define VEC3


// math/vec3.hpp
#pragma once
#include "Math/config.hpp"   // f32, Epsilon
#include "Math/Vec2.hpp"
#include <cmath>
#include <algorithm>
#include <cassert>


    struct vec3 {
        f32 x, y, z;
        #ifdef CWZ_ENABLE_GLM_BRIDGE
        #include <glm/glm.hpp>
        explicit vec3(glm::vec3 v) : x{ v.x }, y{ v.y }, z{ v.z } {}
        explicit operator glm::vec3() const { return glm::vec3{ x,y,z }; }
        #endif

        // ctors
        constexpr vec3() : x(0), y(0), z(0) {}
        constexpr vec3(f32 X, f32 Y = 0.f, f32 Z = 0.f) : x(X), y(Y), z(Z) {}
        constexpr explicit vec3(vec2 v, f32 Z = 0.f) : x(v.x), y(v.y), z(Z) {}

        // unary
        constexpr vec3 operator+() const { return *this; }
        constexpr vec3 operator-() const { return { -x, -y, -z }; }

        // basic ops
        constexpr vec3 operator+(vec3 r) const { return { x + r.x, y + r.y, z + r.z }; }
        constexpr vec3 operator-(vec3 r) const { return { x - r.x, y - r.y, z - r.z }; }
        constexpr vec3 operator*(f32 s)  const { return { x * s,   y * s,   z * s }; } // scalar
        constexpr vec3 operator/(f32 s)  const { return { x / s,   y / s,   z / s }; }

        vec3& operator+=(vec3 r) { x += r.x; y += r.y; z += r.z; return *this; }
        vec3& operator-=(vec3 r) { x -= r.x; y -= r.y; z -= r.z; return *this; }
        vec3& operator*=(f32 s) { x *= s;   y *= s;   z *= s;   return *this; }
        vec3& operator/=(f32 s) { x /= s;   y /= s;   z /= s;   return *this; }

        // index (0->x, 1->y, 2->z)
        f32& operator[](int i) { assert(i >= 0 && i < 3); return (&x)[i]; }
        f32  operator[](int i) const { assert(i >= 0 && i < 3); return (&x)[i]; }
    };

    inline vec3 operator*(f32 s, vec3 v) { return v * s; }

    // dot / cross
    inline f32 dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline vec3 cross(vec3 a, vec3 b) {
        return { a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x };
    }

    // norms
    inline f32 length_sq(vec3 v) { return dot(v, v); }
    inline f32 length(vec3 v) { return std::sqrt(length_sq(v)); }
    inline vec3 normalize(vec3 v, f32 eps = Math::Epsilon) {
        f32 L = length(v);
        return (L > eps) ? v * (1.0f / L) : vec3{ 0,0,0 };
    }

    // helpers
    inline vec3 lerp(vec3 a, vec3 b, f32 t) { return a * (1.f - t) + b * t; }
    inline vec3 project(vec3 a, vec3 onto) { // proj of a onto onto
        f32 d = dot(onto, onto);
        return (d > Math::Epsilon) ? onto * (dot(a, onto) / d) : vec3{ 0,0,0 };
    }
    inline vec3 reject(vec3 a, vec3 onto) { return a - project(a, onto); }
    inline vec3 reflect(vec3 v, vec3 n) { // n assumed normalized
        return v - 2.f * dot(v, n) * n;
    }
    inline f32 angle_between(vec3 a, vec3 b) {
        f32 denom = std::sqrt(length_sq(a) * length_sq(b));
        if (denom <= Math::Epsilon) return 0.f;
        f32 c = std::clamp(dot(a, b) / denom, -1.f, 1.f);
        return std::acos(c);
    }




#endif