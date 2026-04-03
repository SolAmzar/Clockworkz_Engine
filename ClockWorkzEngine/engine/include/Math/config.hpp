/******************************************************************************/
/*!
\file           config.hpp
\project        Lights Out
\author(s)      [Sol-Amzar]
\brief          Places math constants and functions to a separate header.
				Responsibilities include:
				- Defining mathematical constants like PI, Tau, and Epsilon
				- Providing utility functions for angle conversions (degrees <-> radians)
				- Implementing floating-point comparison with tolerance
				- Serving as a lightweight, header-only utility for math operations

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/




#pragma once

#include <cmath>
#include <cfloat>
#include <algorithm>
#include "Types.hpp"

namespace Math {
	constexpr f32 PI = 3.14159265359f;
	constexpr f32 Epsilon = 1e-6f;
	constexpr f32 Tau = 6.28318530717958647692f;

	constexpr f32 to_rad(f32 degrees) {
		return degrees * (PI / 180.0f);
	}
	constexpr f32 to_deg(f32 radians) {
		return radians * (180.0f / PI);
	}

	inline bool float_equal(f32 a, f32 b, f32 epsilon = Epsilon) {
		return std::fabs(a - b) <= epsilon * std::max(1.f, std::max(std::fabs(a), std::fabs(b)));
	}
};