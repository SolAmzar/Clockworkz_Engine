/******************************************************************************/
/*!
\file           Surface.hpp
\project        ClockWorkz Engine
\author(s)      [Sol-Amzar] 100%
\brief          Declares surface type data used for gameplay and audio lookups.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <string>

namespace Gameplay {
	enum class SurfaceType {
		Default,
		Tile,
		Wood,
		Concrete
	};

	inline SurfaceType surfaceTypeFromString(const std::string& str) {
		if (str == "Tile") return SurfaceType::Tile;
		if (str == "Wood") return SurfaceType::Wood;
		if (str == "Concrete") return SurfaceType::Concrete;
		return SurfaceType::Default;
	}
}