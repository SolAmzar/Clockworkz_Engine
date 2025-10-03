/******************************************************************************/
/*!
\file           Config.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Config, a container for engine settings.
				Responsibilities include:
				- loadFromFile() to parse configuration data json format
				- Providing default values if file parsing is not yet supported

				Designed for integration with JSON configuration files to
				control engine parameters such as window size and graphics.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

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
