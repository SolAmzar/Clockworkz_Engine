/******************************************************************************/
/*!
\file           GameLuaBindings.hpp
\project        Lights Out
\author(s)      [Lebon] 86% [Syafiq] 14%
\brief          Game-specific Lua bindings extension.
                Registers LightsOut component accessors on EntityRef without
                touching engine code.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once

namespace Scripting { class LuaManager; }

namespace Scripting {

/// Called once right after the engine's RegisterAllBindings().
/// Extends the EntityRef Lua usertype with LightsOut-specific component
/// accessors backed by Lua script properties (battery.lua, fragment.lua, etc.)
/// instead of C++ component structs. Engine remains clean.
void RegisterGameBindings(LuaManager& lua);

} // namespace Scripting
