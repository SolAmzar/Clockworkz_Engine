#include "Core/Logger.hpp"
#include <iostream>
bool Core::Logger::init(const std::string& logfile) {
	(void)logfile; // TODO: file sink later
	info("Logger ready");
	return true;
}
void Core::Logger::info(const std::string& m) { std::cout << "[INFO] " << m << "\n"; }
void Core::Logger::warn(const std::string& m) { std::cout << "[WARN] " << m << "\n"; }
void Core::Logger::error(const std::string& m) { std::cerr << "[ERROR] " << m << "\n"; }
void Core::Logger::shutdown() { info("Logger shutdown"); }
