/******************************************************************************/
/*!
\file           ScriptPropertyReader.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          Reads the Properties table from a Lua script file and caches
                the result per file timestamp.  Used by the editor inspector
                and scene serializer to discover per-instance fields without
                running the full game bindings.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace Scripting {

using ScriptPropValue = std::variant<float, int, bool, std::string>;

struct ScriptProperty {
    std::string     name;
    std::string     type;         // "float" | "int" | "bool" | "string"
    std::string     label;        // display label in inspector
    ScriptPropValue defaultValue;
};

class ScriptPropertyReader {
public:
    // Returns declared properties for a script (path relative to assets/scripts/).
    // Results are file-timestamp cached; returns empty vector on parse error.
    static const std::vector<ScriptProperty>& read(const std::string& scriptPath);

    // Call after hot-reload so stale cache is discarded.
    static void invalidate(const std::string& scriptPath);

private:
    struct CacheEntry {
        std::vector<ScriptProperty>      props;
        std::filesystem::file_time_type  timestamp;
    };
    static std::unordered_map<std::string, CacheEntry> s_cache;
};

} // namespace Scripting
