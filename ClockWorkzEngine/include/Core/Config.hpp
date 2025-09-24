#pragma once
#include <string>
namespace Core {
	struct Config {
		int   width = 1920;
		int   height = 1080;
		bool  vsync = true;
		std::string title = "ClockWorkzEngine";
		bool loadFromFile(const std::string& path); // returns true even if file missing (uses defaults)
	};
}
