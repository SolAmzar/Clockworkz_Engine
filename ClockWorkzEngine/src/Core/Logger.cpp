/******************************************************************************/
/*!
\file           Logger.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Logger, a lightweight system for reporting runtime
				messages. It provides:
				- init() for setup with optional log file
				- info(), warn(), and error() to print categorized log messages
				- shutdown() for cleanup

				Currently outputs to the console, with file logging planned
				for future implementation.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Core/Logger.hpp"
#include <iostream>
bool Core::Logger::init(const std::string& logfile) {
	(void)logfile; // TODO: For Writing into log file with time frame, line number, error/warning.
	info("Logger ready");
	return true;
}
void Core::Logger::info(const std::string& m) { std::cout << "[INFO] " << m << "\n"; }
void Core::Logger::warn(const std::string& m) { std::cout << "[WARN] " << m << "\n"; }
void Core::Logger::error(const std::string& m) { std::cerr << "[ERROR] " << m << "\n"; }
void Core::Logger::shutdown() { info("Logger shutdown"); }
