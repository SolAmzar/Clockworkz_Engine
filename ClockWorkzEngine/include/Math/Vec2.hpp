/******************************************************************************/
/*!
\file           Vec2.hpp
\project        Lights Out
\author(s)      [Amzar]
\brief          Establishes a 2D vector structure with common vector operations.
				Responsibilities include:
				- Defining a vec2 struct with x and y components
				- Implementing constructors for initialization
				- Overloading arithmetic operators (+, -, *, /) for vector math
				- Providing utility functions like dot product, cross product, normalization, length, and angle calculations
				- Including helper functions for linear interpolation, perpendicular vectors, projection, reflection, and rotation
				- Serving as a lightweight, header-only math utility for 2D vector operations

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/


#pragma once
#ifndef VEC2
#define VEC2

#include "config.hpp"
#include "Types.hpp"
#include <cassert>
#include <cmath>

struct vec2 {
	f32 x, y;

	// In your Vec2.hpp (guarded by a macro)
	#ifdef CWZ_ENABLE_GLM_BRIDGE
	#include <glm/glm.hpp>
	explicit vec2(glm::vec2 v) : x{ v.x }, y{ v.y } {}
	explicit operator glm::vec2() const { return glm::vec2{ x,y }; }
	#endif


	// Default Constructor for the vec2
	constexpr vec2() : x(0), y(0) {};
	// Copy Const
	constexpr vec2(f32 x, f32 y = 0.f) : x(x), y(y) {};

	// Unary
	constexpr vec2 operator+() const {
		return vec2(+x, +y);
	}
	constexpr vec2 operator-() const {
		return vec2(-x, -y);
	}

	// Operators for vec2
	// Add
	constexpr vec2 operator+(const vec2& rhs) const {
		return vec2(x + rhs.x, y + rhs.y);
	}
	// Minus
	constexpr vec2 operator-(const vec2& rhs) const {
		return vec2(x - rhs.x, y - rhs.y);
	}
	// Multiply
	constexpr vec2 operator*(const f32& rhs) const {
		return vec2(x * rhs, y * rhs);
	}
	constexpr vec2 operator*(const vec2& rhs) const {
		return vec2{ x * rhs.x, y * rhs.y };
	}
	// Divide
	constexpr vec2 operator/(const f32& rhs) const {
		return vec2(x / rhs, y / rhs);
	}
	// Divide
	constexpr vec2 operator/(const vec2& rhs) const {
		return vec2(x / rhs.x, y / rhs.y);
	}
	//+= 
	vec2& operator+=(f32 rhs) {
		x += rhs; y += rhs; return *this;
	}

	vec2& operator+=(vec2 rhs) {
		x += rhs.x; y += rhs.y; return *this;
	}
	// -=
	vec2& operator-=(f32 rhs) {
		x -= rhs; y -= rhs; return *this;
	}

	vec2& operator-=(vec2 rhs) {
		x -= rhs.x; y -= rhs.y; return *this;
	}
	// *=
	vec2& operator*=(f32 rhs) {
		x *= rhs; y *= rhs; return *this;
	}

	vec2& operator*=(vec2 rhs) {
		x *= rhs.x; y *= rhs.y; return *this;
	}
	// /=
	vec2& operator/=(f32 rhs) {
		x /= rhs; y /= rhs; return *this;
	}
	vec2& operator/=(vec2 rhs) {
		x /= rhs.x; y /= rhs.y; return *this;
	}

	//index usage
	f32& operator[](int i) {
		assert(i == 0 || i == 1);
		return (&x)[i];
	}
	f32 operator[](int i) const {
		assert(i == 0 || i == 1);
		return (&x)[i];
	}
};

inline vec2 abs_vec2(vec2 v) { return { std::fabs(v.x), std::fabs(v.y) }; }

inline vec2 operator*(f32 s, const vec2& v) {
	return {v * s};
}

// returns a scalar value (dot
inline f32 dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}
inline f32 cross(vec2 a, vec2 b) {
	return a.x * b.y - a.y * b.x;
}

// Normalise
inline f32 length_sq(vec2 v) {
	return dot(v, v);
}

inline f32 magnitude(vec2 v) {
	return std::sqrtf(length_sq(v));
}

inline vec2 normalize(vec2 v, f32 eps = Math::Epsilon) {
	f32 l = magnitude(v);
	if (l < eps) {
		return vec2(0, 0);
	}
	return v * (1.0f/ l);
}

// Helpers for Geometry
inline vec2 lerp(vec2 a, vec2 b, f32 t) {
	return a * (1.f - t) + b * t;
}

// Perpendicular
inline vec2 perp_ccw(vec2 v) {
	return { -v.y, v.x }; // 90 degrees
}
inline vec2 perp_cw(vec2 v) {
	return { v.y, -v.x }; // -90 degrees
}

// Projection and Reflection
inline vec2 project(vec2 a, vec2 b) {
	f32 d = dot(b, b);
	if (d > Math::Epsilon) {
		return b * (dot(a, b) / d);
	}
	return vec2(0, 0);
}
inline vec2 reflect(vec2 v, vec2 n) { // n has to be normalised for this to work
	return v - 2.f * project(v, n);
}

// Rotation (radian)
inline vec2 rotate(vec2 v, f32 angle) {
	f32 c = std::cos(angle), s = std::sin(angle);
	return { v.x * c - v.y * s, v.x * s + v.y * c };
}

// Angle Between (radian)
inline f32 angle_between(vec2 a, vec2 b) {
	f32 d = magnitude(a) * magnitude(b);
	if (d < Math::Epsilon) {
		return 0.f;
	}
	f32 c = dot(a, b) / d;
	c = std::max(-1.f, std::min(1.f, c));
	return std::acos(c);
}

#endif