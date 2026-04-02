/******************************************************************************/
/*!
\file           Vec4.hpp
\project        Lights Out
\author(s)      [Sol-Amzar]
\brief          Establishes 4D vector structure with common vector operations.
				Responsibilities include:
				- Defining a vec4 struct with x, y, z, and w components
				- Implementing constructors for initialization
				- Overloading arithmetic operators (+, -, *, /) for vector math
				- Providing utility functions like dot product, normalization, length
				- Including helper functions for linear interpolation and conversion to vec3
				- Serving as a lightweight, header-only math utility for 4D vector operations
All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/

#pragma once
#include "config.hpp"  // f32, Epsilon
#include "vec3.hpp"
#include <cmath>
#include <algorithm>
#include <cassert>


	struct vec4 {
		f32 x, y, z, w;

		// Vec4.hpp
		#ifdef CWZ_ENABLE_GLM_BRIDGE
		#include <glm/glm.hpp>
		// build from glm::vec4 (explicit = no accidental implicit conversions)
		explicit vec4(glm::vec4 v) : x{ v.x }, y{ v.y }, z{ v.z }, w{ v.w } {}

		// convert to glm::vec4 (also explicit)
		explicit operator glm::vec4() const { return glm::vec4{ x, y, z, w }; }
		#endif

		// ctors
		constexpr vec4() : x(0), y(0), z(0), w(0) {}
		constexpr vec4(f32 X, f32 Y = 0.f, f32 Z = 0.f, f32 W = 0.f) : x(X), y(Y), z(Z), w(W) {}
		constexpr explicit vec4(vec3 v, f32 W = 0.f) : x(v.x), y(v.y), z(v.z), w(W) {}

		// unary
		constexpr vec4 operator+() const { return *this; }
		constexpr vec4 operator-() const { return { -x, -y, -z, -w }; }

		// basic ops
		constexpr vec4 operator+(vec4 r) const { return { x + r.x, y + r.y, z + r.z, w + r.w }; }
		constexpr vec4 operator-(vec4 r) const { return { x - r.x, y - r.y, z - r.z, w - r.w }; }
		constexpr vec4 operator*(f32 s)  const { return { x * s,   y * s,   z * s,   w * s }; }
		constexpr vec4 operator/(f32 s)  const { return { x / s,   y / s,   z / s,   w / s }; }

		vec4& operator+=(vec4 r) { x += r.x; y += r.y; z += r.z; w += r.w; return *this; }
		vec4& operator-=(vec4 r) { x -= r.x; y -= r.y; z -= r.z; w -= r.w; return *this; }
		vec4& operator*=(f32 s) { x *= s;   y *= s;   z *= s;   w *= s;   return *this; }
		vec4& operator/=(f32 s) { x /= s;   y /= s;   z /= s;   w /= s;   return *this; }

		// index (0->x,1->y,2->z,3->w)
		f32& operator[](int i) { assert(i >= 0 && i < 4); return (&x)[i]; }
		f32  operator[](int i) const { assert(i >= 0 && i < 4); return (&x)[i]; }
	};

	inline vec4 operator*(f32 s, vec4 v) { return v * s; }

	// dot / norms
	inline f32 dot(vec4 a, vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
	inline f32 length_sq(vec4 v) { return dot(v, v); }
	inline f32 length(vec4 v) { return std::sqrt(length_sq(v)); }
	inline vec4 normalize(vec4 v, f32 eps = Math::Epsilon) {
		f32 L = length(v);
		return (L > eps) ? v * (1.0f / L) : vec4{ 0,0,0,0 };
	}

	// helpers
	inline vec4 lerp(vec4 a, vec4 b, f32 t) { return a * (1.f - t) + b * t; }
	inline vec3 xyz(vec4 v) { return { v.x, v.y, v.z }; }

	// Homogeneous helpers (optional)
	inline vec4 as_point(vec3 p) { return { p.x, p.y, p.z, 1.f }; }   // w = 1
	inline vec4 as_dir(vec3 v) { return { v.x, v.y, v.z, 0.f }; }   // w = 0

