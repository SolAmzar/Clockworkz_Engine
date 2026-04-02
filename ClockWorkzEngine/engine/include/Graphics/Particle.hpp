/******************************************************************************/
/*!
\file           Particle.hpp
\project        Lights Out
\author(s)      [Sol-Amzar], 100%
\brief          A header file that defines the structure of a Particle used in a particle system.
				Responsibilities include:
				- Storing position, velocity, acceleration, life span, rotation, size, color, and alive status of a particle.
				- Providing a simple data structure for managing individual particles in a particle system.

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "math/ps_math.hpp"

struct Particle {
	ps::v2    pos{};
	ps::v2    vel{};
	ps::v2    acc{};
	float     life{};
	float     invLife{};
	float     rotation{};
	float     size{};
	ps::Color color{};
	bool      alive{ false };
};
