#pragma once
#ifndef VEC2
#define VEC2

#include "config.hpp"
#include <cassert>
#include <cmath>

struct Vec2 {
	f32 x, y;

	// Default Constructor for the Vec2
	constexpr Vec2() : x(0), y(0) {};
	// Copy Const
	constexpr Vec2(f32 x, f32 y) : x(x), y(y) {};

	// Unary
	constexpr Vec2 operator+() const {
		return Vec2(+x, +y);
	}
	constexpr Vec2 operator-() const {
		return Vec2(-x, -y);
	}

	// Operators for Vec2
	// Add
	constexpr Vec2 operator+(const Vec2& rhs) const {
		return Vec2(x + rhs.x, y + rhs.y);
	}
	// Minus
	constexpr Vec2 operator-(const Vec2& rhs) const {
		return Vec2(x - rhs.x, y - rhs.y);
	}
	// Multiply
	constexpr Vec2 operator*(const f32& rhs) const {
		return Vec2(x * rhs, y * rhs);
	}
	// Divide
	constexpr Vec2 operator/(const f32& rhs) const {
		return Vec2(x / rhs, y / rhs);
	}
	//+= 
	Vec2& operator+=(Vec2 rhs) {
		x += rhs.x; y += rhs.y; return *this;
	}
	// -=
	Vec2& operator-=(Vec2 rhs) {
		x -= rhs.x; y -= rhs.y; return *this;
	}
	// *=
	Vec2& operator*=(f32 rhs) {
		x *= rhs; y *= rhs; return *this;
	}
	// /=
	Vec2& operator/=(f32 rhs) {
		x /= rhs; y /= rhs; return *this;
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

inline Vec2 operator*(f32 s, const Vec2& v) {
	return {s * v};
}

// returns a scalar value (dot
inline f32 dot(Vec2 a, Vec2 b) {
	return a.x * b.x + a.y * b.y;
}
inline f32 cross(Vec2 a, Vec2 b) {
	return a.x * b.y - a.y * b.x;
}

// Normalise
inline f32 length_sq(Vec2 v) {
	return dot(v, v);
}

inline f32 magnitude(Vec2 v) {
	return std::sqrtf(length_sq(v));
}

inline Vec2 normalize(Vec2 v, f32 eps = Math::EPSILON) {
	f32 l = magnitude(v);
	if (l < eps) {
		return Vec2(0, 0);
	}
	return v * (1.0f/ l);
}

// Helpers for Geometry
inline Vec2 lerp(Vec2 a, Vec2 b, f32 t) {
	return a * (1.f - t) + b * t;
}

// Perpendicular
inline Vec2 perp_ccw(Vec2 v) {
	return { -v.y, v.x }; // 90 degrees
}
inline Vec2 perp_cw(Vec2 v) {
	return { v.y, -v.x }; // -90 degrees
}

// Projection and Reflection
inline Vec2 project(Vec2 a, Vec2 b) {
	f32 d = dot(b, b);
	if (d > Math::EPSILON) {
		return b * (dot(a, b) / d);
	}
	return Vec2(0, 0);
}
inline Vec2 reflect(Vec2 v, Vec2 n) { // n has to be normalised for this to work
	return v - 2.f * project(v, n);
}

// Rotation (radian)
inline Vec2 rotate(Vec2 v, f32 angle) {
	f32 c = std::cos(angle), s = std::sin(angle);
	return { v.x * c - v.y * s, v.x * s + v.y * c };
}

// Angle Between (radian)
inline f32 angle_between(Vec2 a, Vec2 b) {
	f32 d = magnitude(a) * magnitude(b);
	if (d < Math::EPSILON) {
		return 0.f;
	}
	f32 c = dot(a, b) / d;
	c = std::max(-1.f, std::min(1.f, c));
	return std::acos(c);
}

#endif