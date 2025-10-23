/******************************************************************************/
/*!
\file           Logger.hpp
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
#pragma once
#include <string>
namespace Core {
	class Logger {
	public:
		bool init(const std::string& logfile = "engine.log");
		void info(const std::string& msg);
		void warn(const std::string& msg);
		void error(const std::string& msg);
		void shutdown();
	};
}
