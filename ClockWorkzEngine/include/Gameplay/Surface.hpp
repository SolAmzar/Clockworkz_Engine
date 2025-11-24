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