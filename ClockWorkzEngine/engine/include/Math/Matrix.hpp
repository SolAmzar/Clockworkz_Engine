/******************************************************************************/
/*!
\file           Matrix.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Helps with common matrix operations for 2D/3D transformations.
                Responsibilities include:
                - Defining Mat2, Mat3, and Mat4 structures for 2x2, 3x3, and 4x4 matrices
                - Providing constructors and identity matrix functions
                - Implementing common transformation builders (translation, rotation, scale)
                - Overloading operators for matrix multiplication and vector transformation
                - Including utility functions for determinant and inverse of Mat4
				- Serving as a lightweight, header-only math utility for graphics applications

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once

#include <cmath>
#include <cstdint>
#include <array>
#include <initializer_list>

#include "Math/vec2.hpp"
#include "Math/vec3.hpp"
#include "Math/vec4.hpp"

namespace cwz {

    struct Mat2 {
        // Column-major storage: m[col][row]
        //   | m00 m01 |
        //   | m10 m11 |
        float m00, m10; 
        float m01, m11; 

        // Constructors
        constexpr Mat2() : m00(1), m10(0), m01(0), m11(1) {}
        constexpr Mat2(float _m00, float _m01, float _m10, float _m11)
            : m00(_m00), m10(_m10), m01(_m01), m11(_m11) {
        }

        static constexpr Mat2 identity() { return Mat2{}; }

        // Access
        constexpr float operator()(int row, int col) const {
            return (col == 0 ? (row == 0 ? m00 : m10) : (row == 0 ? m01 : m11));
        }
        float& at(int row, int col) {
            return (col == 0 ? (row == 0 ? m00 : m10) : (row == 0 ? m01 : m11));
        }

        // Basic ops
        static Mat2 rotation(float radians) {
            float c = std::cos(radians), s = std::sin(radians);
            // column-major
            return Mat2(c, -s, s, c);
        }
        static constexpr Mat2 scale(float sx, float sy) { return Mat2(sx, 0, 0, sy); }

        friend constexpr Mat2 operator+(Mat2 const& a, Mat2 const& b) {
            return Mat2(a.m00 + b.m00, a.m01 + b.m01, a.m10 + b.m10, a.m11 + b.m11);
        }
        friend constexpr Mat2 operator-(Mat2 const& a, Mat2 const& b) {
            return Mat2(a.m00 - b.m00, a.m01 - b.m01, a.m10 - b.m10, a.m11 - b.m11);
        }
        friend constexpr Mat2 operator*(Mat2 const& a, Mat2 const& b) {
            // column-major: result columns = a * b.columns
            return Mat2(
                a.m00 * b.m00 + a.m01 * b.m10, // m00
                a.m00 * b.m01 + a.m01 * b.m11, // m01
                a.m10 * b.m00 + a.m11 * b.m10, // m10
                a.m10 * b.m01 + a.m11 * b.m11  // m11
            );
        }
        friend constexpr vec2 operator*(Mat2 const& m, vec2 const& v) {
            return { m.m00 * v.x + m.m01 * v.y, m.m10 * v.x + m.m11 * v.y };
        }
    };

    struct Mat3 {
        // Column-major storage (affine 2D in top-left 2x2, translation in col2 xy)
        // [ m00 m01 m02 ]
        // [ m10 m11 m12 ]
        // [ m20 m21 m22 ]
        float m00, m10, m20; 
        float m01, m11, m21; 
        float m02, m12, m22; 

        constexpr Mat3()
            : m00(1), m10(0), m20(0), m01(0), m11(1), m21(0), m02(0), m12(0), m22(1) {
        }

        constexpr Mat3(float _m00, float _m01, float _m02, float _m10, float _m11,
            float _m12, float _m20, float _m21, float _m22)
            : m00(_m00), m10(_m10), m20(_m20), m01(_m01), m11(_m11), m21(_m21),
            m02(_m02), m12(_m12), m22(_m22) {
        }

        static constexpr Mat3 identity() { return Mat3{}; }

        // Builders (2D affine)
        static Mat3 translation(float tx, float ty) {
            Mat3 r = Mat3::identity();
            r.m02 = tx; r.m12 = ty;
            return r;
        }
        static constexpr Mat3 scale(float sx, float sy) { return Mat3(sx, 0, 0, 0, sy, 0, 0, 0, 1); }
        static Mat3 rotationZ(float radians) {
            float c = std::cos(radians), s = std::sin(radians);
            return Mat3(c, -s, 0, s, c, 0, 0, 0, 1);
        }

        // Multiplication
        friend constexpr Mat3 operator*(Mat3 const& a, Mat3 const& b) {
            return Mat3(
                a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20,
                a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21,
                a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22,
                a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20,
                a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21,
                a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22,
                a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20,
                a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21,
                a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22);
        }
        friend constexpr vec3 operator*(Mat3 const& m, vec3 const& v) {
            return { m.m00 * v.x + m.m01 * v.y + m.m02 * v.z,
                    m.m10 * v.x + m.m11 * v.y + m.m12 * v.z,
                    m.m20 * v.x + m.m21 * v.y + m.m22 * v.z };
        }

        // 2D point transform (homogeneous with z=1)
        static vec2 transformPoint(Mat3 const& T, vec2 const& p) {
            float x = T.m00 * p.x + T.m01 * p.y + T.m02;
            float y = T.m10 * p.x + T.m11 * p.y + T.m12;
            return { x, y };
        }
    };

    struct Mat4 {
        // Column-major storage, OpenGL-style
        // [ m00 m01 m02 m03 ]
        // [ m10 m11 m12 m13 ]
        // [ m20 m21 m22 m23 ]
        // [ m30 m31 m32 m33 ]
        float m00, m10, m20, m30; 
        float m01, m11, m21, m31; 
        float m02, m12, m22, m32; 
        float m03, m13, m23, m33; 

        constexpr Mat4()
            : m00(1), m10(0), m20(0), m30(0),
            m01(0), m11(1), m21(0), m31(0),
            m02(0), m12(0), m22(1), m32(0),
            m03(0), m13(0), m23(0), m33(1) {
        }

        constexpr Mat4(float _m00, float _m01, float _m02, float _m03,
            float _m10, float _m11, float _m12, float _m13,
            float _m20, float _m21, float _m22, float _m23,
            float _m30, float _m31, float _m32, float _m33)
            : m00(_m00), m10(_m10), m20(_m20), m30(_m30),
            m01(_m01), m11(_m11), m21(_m21), m31(_m31),
            m02(_m02), m12(_m12), m22(_m22), m32(_m32),
            m03(_m03), m13(_m13), m23(_m23), m33(_m33) {
        }

        static constexpr Mat4 identity() { return Mat4{}; }

        // Builders (3D affine)
        static constexpr Mat4 translation(float tx, float ty, float tz) {
            Mat4 r = Mat4::identity(); r.m03 = tx; r.m13 = ty; r.m23 = tz; return r;
        }
        static constexpr Mat4 scale(float sx, float sy, float sz) {
            return Mat4(sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1);
        }
        static Mat4 rotationX(float r) {
            float c = std::cos(r), s = std::sin(r);
            return Mat4(1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0, 0, 0, 0, 1);
        }
        static Mat4 rotationY(float r) {
            float c = std::cos(r), s = std::sin(r);
            return Mat4(c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1);
        }
        static Mat4 rotationZ(float r) {
            float c = std::cos(r), s = std::sin(r);
            return Mat4(c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        }

        // View / Projection (right-handed lookAt, OpenGL-depth NDC [-1,1])
        static Mat4 lookAtRH(vec3 const& eye, vec3 const& target, vec3 const& up) {
            vec3 f = normalize(eye - target);      // forward (camera -Z)
            vec3 r = normalize(cross(up, f));      // right
            vec3 u = cross(f, r);                  // up (re-orthogonalized)

            Mat4 M = Mat4::identity();
            // rotation part
            M.m00 = r.x; M.m01 = u.x; M.m02 = f.x;
            M.m10 = r.y; M.m11 = u.y; M.m12 = f.y;
            M.m20 = r.z; M.m21 = u.z; M.m22 = f.z;
            // translation
            M.m03 = -(r.x * eye.x + r.y * eye.y + r.z * eye.z);
            M.m13 = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
            M.m23 = -(f.x * eye.x + f.y * eye.y + f.z * eye.z);
            return M;
        }

        static Mat4 perspectiveRH(float fovyRadians, float aspect, float zNear, float zFar) {
            float f = 1.0f / std::tan(fovyRadians * 0.5f);
            Mat4 r{}; // identity
            r.m00 = f / aspect;
            r.m11 = f;
            r.m22 = (zFar + zNear) / (zNear - zFar);
            r.m32 = -1.0f;
            r.m23 = (2.0f * zFar * zNear) / (zNear - zFar);
            r.m33 = 0.0f;
            r.m03 = r.m13 = 0.0f; 
            return r;
        }

        static Mat4 orthoRH(float left, float right, float bottom, float top, float zNear, float zFar) {
            Mat4 r{};
            r.m00 = 2.0f / (right - left);
            r.m11 = 2.0f / (top - bottom);
            r.m22 = -2.0f / (zFar - zNear);
            r.m03 = -(right + left) / (right - left);
            r.m13 = -(top + bottom) / (top - bottom);
            r.m23 = -(zFar + zNear) / (zFar - zNear);
            return r;
        }

        // Basic ops
        friend constexpr Mat4 operator*(Mat4 const& a, Mat4 const& b) {
            Mat4 r{}; // identity then overwrite all
            r.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30;
            r.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31;
            r.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32;
            r.m03 = a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33;

            r.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30;
            r.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31;
            r.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32;
            r.m13 = a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33;

            r.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30;
            r.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31;
            r.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32;
            r.m23 = a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33;

            r.m30 = a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30;
            r.m31 = a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31;
            r.m32 = a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32;
            r.m33 = a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33;
            return r;
        }

        friend constexpr vec4 operator*(Mat4 const& m, vec4 const& v) {
            return { m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03 * v.w,
                    m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13 * v.w,
                    m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23 * v.w,
                    m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33 * v.w };
        }

        // Transform a 3D point (implicit w=1) and a direction (w=0)
        static vec3 transformPoint(Mat4 const& T, vec3 const& p) {
            float x = T.m00 * p.x + T.m01 * p.y + T.m02 * p.z + T.m03;
            float y = T.m10 * p.x + T.m11 * p.y + T.m12 * p.z + T.m13;
            float z = T.m20 * p.x + T.m21 * p.y + T.m22 * p.z + T.m23;
            float w = T.m30 * p.x + T.m31 * p.y + T.m32 * p.z + T.m33;
            if (w != 0.0f) { float invw = 1.0f / w; x *= invw; y *= invw; z *= invw; }
            return { x, y, z };
        }
        static vec3 transformDir(Mat4 const& T, vec3 const& v) {
            return { T.m00 * v.x + T.m01 * v.y + T.m02 * v.z,
                    T.m10 * v.x + T.m11 * v.y + T.m12 * v.z,
                    T.m20 * v.x + T.m21 * v.y + T.m22 * v.z };
        }

        // Determinant & inverse (general 4x4, robust enough for engine use)
        static float det(Mat4 const& A) {
            // Compute using cofactor expansion with precomputed minors.
            float a00 = A.m00, a01 = A.m01, a02 = A.m02, a03 = A.m03;
            float a10 = A.m10, a11 = A.m11, a12 = A.m12, a13 = A.m13;
            float a20 = A.m20, a21 = A.m21, a22 = A.m22, a23 = A.m23;
            float a30 = A.m30, a31 = A.m31, a32 = A.m32, a33 = A.m33;

            float b00 = a00 * a11 - a01 * a10;
            float b01 = a00 * a12 - a02 * a10;
            float b02 = a00 * a13 - a03 * a10;
            float b03 = a01 * a12 - a02 * a11;
            float b04 = a01 * a13 - a03 * a11;
            float b05 = a02 * a13 - a03 * a12;
            float b06 = a20 * a31 - a21 * a30;
            float b07 = a20 * a32 - a22 * a30;
            float b08 = a20 * a33 - a23 * a30;
            float b09 = a21 * a32 - a22 * a31;
            float b10 = a21 * a33 - a23 * a31;
            float b11 = a22 * a33 - a23 * a32;

            return b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
        }

        static Mat4 inverse(Mat4 const& A) {
            float a00 = A.m00, a01 = A.m01, a02 = A.m02, a03 = A.m03;
            float a10 = A.m10, a11 = A.m11, a12 = A.m12, a13 = A.m13;
            float a20 = A.m20, a21 = A.m21, a22 = A.m22, a23 = A.m23;
            float a30 = A.m30, a31 = A.m31, a32 = A.m32, a33 = A.m33;

            float b00 = a00 * a11 - a01 * a10;
            float b01 = a00 * a12 - a02 * a10;
            float b02 = a00 * a13 - a03 * a10;
            float b03 = a01 * a12 - a02 * a11;
            float b04 = a01 * a13 - a03 * a11;
            float b05 = a02 * a13 - a03 * a12;
            float b06 = a20 * a31 - a21 * a30;
            float b07 = a20 * a32 - a22 * a30;
            float b08 = a20 * a33 - a23 * a30;
            float b09 = a21 * a32 - a22 * a31;
            float b10 = a21 * a33 - a23 * a31;
            float b11 = a22 * a33 - a23 * a32;

            float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
            float invDet = 1.0f / det;

            Mat4 r{};
            r.m00 = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
            r.m01 = (-a01 * b11 + a02 * b10 - a03 * b09) * invDet;
            r.m02 = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
            r.m03 = (-a21 * b05 + a22 * b04 - a23 * b03) * invDet;

            r.m10 = (-a10 * b11 + a12 * b08 - a13 * b07) * invDet;
            r.m11 = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
            r.m12 = (-a30 * b05 + a32 * b02 - a33 * b01) * invDet;
            r.m13 = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;

            r.m20 = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
            r.m21 = (-a00 * b10 + a01 * b08 - a03 * b06) * invDet;
            r.m22 = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
            r.m23 = (-a20 * b04 + a21 * b02 - a23 * b00) * invDet;

            r.m30 = (-a10 * b09 + a11 * b07 - a12 * b06) * invDet;
            r.m31 = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
            r.m32 = (-a30 * b03 + a31 * b01 - a32 * b00) * invDet;
            r.m33 = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;

            return r;
        }

        // Raw pointer access for GPU uploads
        const float* data() const { return &m00; }
        float* data() { return &m00; }
    };

} // namespace cwz
