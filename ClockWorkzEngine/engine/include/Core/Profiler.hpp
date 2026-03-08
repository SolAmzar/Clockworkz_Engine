/******************************************************************************/
/*!
\file           Profiler.hpp
\project        Lights Out
\author(s)      [Yusuf], 100%
\brief          Declares a profiler

                Currently outputs to the console

All content  2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>

namespace Core {

    struct ProfileData {
        double totalTime = 0.0;
        int callCount = 0;
        std::string parentScope;

        void addSample(double time) {
            totalTime += time;
            callCount++;
        }

        double avgTime() const {
            return callCount > 0 ? totalTime / callCount : 0.0;
        }
    };

    class Profiler {
    public:
        static Profiler& Get() {
            if (!s_instance) s_instance = new Profiler();
            return *s_instance;
        }

        // Call before _CrtDumpMemoryLeaks() to free internal map allocations.
        static void Destroy() {
            delete s_instance;
            s_instance = nullptr;
        }

        void begin(const std::string& name) {
            if (!m_enabled) return;
            m_startTimes[name] = std::chrono::high_resolution_clock::now();

            if (!m_scopeStack.empty()) {
                m_data[name].parentScope = m_scopeStack.back();
            }
            m_scopeStack.push_back(name);
        }

        void end(const std::string& name) {
            if (!m_enabled) return;
            auto endTime = std::chrono::high_resolution_clock::now();
            auto it = m_startTimes.find(name);
            if (it != m_startTimes.end()) {
                auto duration = std::chrono::duration<double, std::milli>(endTime - it->second).count();
                m_data[name].addSample(duration);
            }

            if (!m_scopeStack.empty() && m_scopeStack.back() == name) {
                m_scopeStack.pop_back();
            }
        }

        void print() {
            if (!m_enabled) return;
            std::cout << "\n";
            std::cout << "========================================\n";
            std::cout << "PERFORMANCE PROFILE\n";
            std::cout << "========================================\n";

            // Find game loop scope
            std::string gameLoopName = findGameLoopScope();

            if (gameLoopName.empty()) {
                std::cout << "Warning: No game loop found. Add PROFILE_SCOPE(\"Frame\") to your main loop.\n";
                std::cout << "========================================\n\n";
                return;
            }

            double totalFrameTime = m_data[gameLoopName].avgTime();

            if (totalFrameTime < 0.001) {
                std::cout << "Warning: Not enough data. Run for at least 60 frames.\n";
                std::cout << "========================================\n\n";
                return;
            }

            double fps = 1000.0 / totalFrameTime;

            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Total Frame Time: " << totalFrameTime << " ms\n";
            std::cout << "FPS: " << std::setprecision(1) << fps << "\n\n";

            // Collect child systems
            std::vector<std::pair<std::string, const ProfileData*>> systems;
            for (const auto& [name, data] : m_data) {
                if (data.parentScope == gameLoopName) {
                    systems.push_back({ name, &data });
                }
            }

            // Sort by time
            std::sort(systems.begin(), systems.end(),
                [](const auto& a, const auto& b) {
                    return a.second->avgTime() > b.second->avgTime();
                });

            // Print system percentages
            std::cout << "System Breakdown:\n";
            std::cout << "----------------------------------------\n";

            for (const auto& [name, data] : systems) {
                double avgTime = data->avgTime();
                double percentage = (avgTime / totalFrameTime) * 100.0;

                std::cout << std::left << std::setw(20) << name
                    << std::right << std::setw(8) << std::fixed << std::setprecision(2) << avgTime << " ms"
                    << std::setw(10) << std::setprecision(1) << percentage << "%\n";
            }

            std::cout << "========================================\n\n";
        }

        const std::unordered_map<std::string, ProfileData>& data() const { return m_data; }

        void reset() {
            m_enabled = false;
            m_data = {};
            m_startTimes = {};
            m_scopeStack = {};
        }

        void setEnabled(bool enabled) { m_enabled = enabled; }

    private:
        Profiler() = default;

        inline static Profiler* s_instance = nullptr;

        bool m_enabled = true;
        std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> m_startTimes;
        std::unordered_map<std::string, ProfileData> m_data;
        std::vector<std::string> m_scopeStack;

        std::string findGameLoopScope() const {
            const std::vector<std::string> candidates = { "Frame", "GameLoop", "Update", "MainLoop" };

            for (const auto& name : candidates) {
                if (m_data.find(name) != m_data.end()) {
                    return name;
                }
            }

            std::string best;
            int maxChildren = 0;

            for (const auto& [name, data] : m_data) {
                if (data.parentScope.empty()) {
                    int childCount = 0;
                    for (const auto& [childName, childData] : m_data) {
                        if (childData.parentScope == name) {
                            childCount++;
                        }
                    }
                    if (childCount > maxChildren) {
                        maxChildren = childCount;
                        best = name;
                    }
                }
            }

            return best;
        }
    };

    class ScopedProfile {
    public:
        ScopedProfile(const std::string& name) : m_name(name) {
            Profiler::Get().begin(m_name);
        }
        ~ScopedProfile() {
            Profiler::Get().end(m_name);
        }
    private:
        std::string m_name;
    };

#define PROFILE_FUNCTION() Core::ScopedProfile _profile_##__LINE__(__FUNCTION__)
//#define PROFILE_SCOPE(name) Core::ScopedProfile _profile_##__LINE__(name)
#define PROFILE_SCOPE(name) Core::ScopedProfile _profile_##__COUNTER__(name)
} // namespace Core