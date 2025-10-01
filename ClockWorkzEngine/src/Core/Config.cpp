/******************************************************************************/
/*!
\file           Config.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Config, a container for engine settings.
                Responsibilities include:
                - loadFromFile() to parse configuration data (currently stubbed)
                - Providing default values if file parsing is not yet supported

                Designed for integration with JSON configuration files to
                control engine parameters such as window size and graphics.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Core/Config.hpp"
#include <cstdio>
bool Core::Config::loadFromFile(const std::string& path) {
	// TODO: parse JSON later; keep defaults now
	(void)path;
	return true;
}
