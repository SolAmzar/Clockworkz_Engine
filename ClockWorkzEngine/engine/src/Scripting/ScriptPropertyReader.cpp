/******************************************************************************/
/*!
\file           ScriptPropertyReader.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          ScriptPropertyReader implementation.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Scripting/ScriptPropertyReader.hpp"

#pragma warning(push, 0)
#include <sol/sol.hpp>
#pragma warning(pop)

#include <iostream>

namespace Scripting {

std::unordered_map<std::string, ScriptPropertyReader::CacheEntry> ScriptPropertyReader::s_cache;

const std::vector<ScriptProperty>& ScriptPropertyReader::read(const std::string& scriptPath) {
    static const std::vector<ScriptProperty> s_empty;

    std::string fullPath = "assets/scripts/" + scriptPath;

    namespace fs = std::filesystem;
    if (!fs::exists(fullPath)) {
        return s_empty;
    }

    fs::file_time_type mtime = fs::last_write_time(fullPath);

    auto it = s_cache.find(scriptPath);
    if (it != s_cache.end() && it->second.timestamp == mtime) {
        return it->second.props;
    }

    // Create minimal Lua state — no game bindings, just base libs
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // Execute the script; catch errors
    auto result = lua.safe_script_file(fullPath, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "[ScriptPropertyReader] Error loading '" << fullPath
                  << "': " << err.what() << "\n";
        // Cache empty entry so we don't retry every frame
        s_cache[scriptPath] = {s_empty, mtime};
        return s_cache[scriptPath].props;
    }

    // Look for a top-level Properties table
    sol::optional<sol::table> propTableOpt = lua["Properties"];
    if (!propTableOpt || !propTableOpt->valid()) {
        s_cache[scriptPath] = {s_empty, mtime};
        return s_cache[scriptPath].props;
    }

    sol::table propTable = *propTableOpt;
    std::vector<ScriptProperty> props;

    // Iterate array portion: each entry is a table { name, type, default, label }
    for (size_t i = 1; i <= propTable.size(); ++i) {
        sol::optional<sol::table> entry = propTable[i];
        if (!entry) continue;

        auto nameOpt  = entry->get<sol::optional<std::string>>("name");
        auto typeOpt  = entry->get<sol::optional<std::string>>("type");
        auto labelOpt = entry->get<sol::optional<std::string>>("label");
        std::string name  = nameOpt  ? *nameOpt  : std::string{};
        std::string type  = typeOpt  ? *typeOpt  : std::string{"float"};
        std::string label = labelOpt ? *labelOpt : name;
        if (name.empty()) continue;

        ScriptPropValue defVal;
        if (type == "float") {
            auto opt = entry->get<sol::optional<float>>("default");
            defVal = opt ? *opt : 0.0f;
        } else if (type == "int") {
            auto opt = entry->get<sol::optional<int>>("default");
            defVal = opt ? *opt : 0;
        } else if (type == "bool") {
            auto opt = entry->get<sol::optional<bool>>("default");
            defVal = opt ? *opt : false;
        } else if (type == "string") {
            auto opt = entry->get<sol::optional<std::string>>("default");
            defVal = opt ? *opt : std::string{};
        } else {
            // Unknown type — treat as float
            auto opt = entry->get<sol::optional<float>>("default");
            defVal = opt ? *opt : 0.0f;
        }

        props.push_back({name, type, label, defVal});
    }

    s_cache[scriptPath] = {std::move(props), mtime};
    return s_cache[scriptPath].props;
}

void ScriptPropertyReader::invalidate(const std::string& scriptPath) {
    s_cache.erase(scriptPath);
}

void ScriptPropertyReader::clearAll() {
    s_cache.clear();
}

} // namespace Scripting
