/******************************************************************************/
/*!
\file           ps_math.hpp
\project        Lights Out
\author(s)      [Amzar], 100%
\brief          A header file that defines basic math types and utilities for the particle system.
				Responsibilities include:
				- Defining a 2D vector type (v2) for particle positions and velocities
				- Providing common vector operations: dot product, length, normalization, rotation
				- Defining a Color struct for RGBA color representation
				- Including a utility function to convert degrees to radians
All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/



#pragma once
#include "Vec2.hpp"   // your file

// Use your vector type directly
namespace ps {
	using v2 = vec2;

	// Use your helpers (already defined in Vec2.hpp)
	inline float dot(v2 a, v2 b) { return ::dot(a, b); }
	inline float length(v2 v) { return ::magnitude(v); }
	inline v2    normalize(v2 v) { return ::normalize(v); }
	inline v2    rotate(v2 v, float r) { return ::rotate(v, r); }

	struct Color { float r{ 1 }, g{ 1 }, b{ 1 }, a{ 1 }; };

	// small utility
	inline float radians(float deg) { return deg * 0.017453292519943295f; }
}