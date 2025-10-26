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
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

bool Core::Logger::init(const std::string& logfile) {
    m_logFilePath = logfile;
    m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);

    std::cout << "========================================\n";
    std::cout << "[LOGGER] Log files location:\n";
    std::cout << "[LOGGER] " << std::filesystem::absolute(m_logFilePath) << "\n";
    std::cout << "[LOGGER] Working directory: " << std::filesystem::current_path() << "\n";
    std::cout << "========================================\n";

    if (!m_logFile.is_open()) {
        std::cerr << "[LOGGER] Failed to open log file\n";
    }

    info("Logger ready");
    return true;
}
void Core::Logger::info(const std::string& m) {
    std::string fullMsg = "[INFO] " + m;
    std::cout << fullMsg << "\n";
    writeToFile(fullMsg);
}
void Core::Logger::warn(const std::string& m) {
    std::string fullMsg = "[WARN] " + m;
    std::cout << fullMsg << "\n";
    writeToFile(fullMsg);
}
void Core::Logger::error(const std::string& m) {
    std::string fullMsg = "[ERROR] " + m;
    std::cerr << fullMsg << "\n";
    writeToFile(fullMsg);
}



void Core::Logger::fatal(const std::string& msg)
{
    std::string timestamp = getCurrentTimestamp();
    std::string logMsg = "[FATAL] [" + timestamp + "] " + msg + "\n";

    std::cerr << logMsg;  // Print to console

    if (m_logFile.is_open()) {
        m_logFile << logMsg;
        m_logFile.flush();  // Ensure it's written immediately
    }

    // Optionally write to a crash dump file
    std::ofstream crashFile("crash_report.txt", std::ios::app);
    if (crashFile.is_open()) {
        crashFile << logMsg;
        crashFile << "Stack trace: (implement if needed)\n";
        crashFile.close();
    }
}
void Core::Logger::shutdown() { info("Logger shutdown"); }

std::string Core::Logger::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Core::Logger::writeToFile(const std::string& msg)
{
    if (m_logFile.is_open()) {
        std::string timestamp = getCurrentTimestamp();
        m_logFile << "[" << timestamp << "] " << msg << "\n";
        m_logFile.flush();  // Ensure it's written immediately
    }
}
