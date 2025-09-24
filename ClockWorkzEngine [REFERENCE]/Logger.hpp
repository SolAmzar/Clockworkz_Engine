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
