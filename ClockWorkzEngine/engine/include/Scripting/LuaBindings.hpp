/******************************************************************************/
/*!
\file           LuaBindings.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Declares registration functions for all Lua type bindings.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <sol/sol.hpp>

// Forward declarations
namespace Gameplay {
    class GameObject;
    class Component;
    class HealthComponent;
    class DamageDealerComponent;
    class SpriteRendererComponent;
    struct Transform2D;
}

namespace Platform {
    class InputManager;
}

namespace Scripting {
    class EntityRef;
}

namespace Scripting {

    //-------------------------------------------------------------------------
    // Math Bindings
    //-------------------------------------------------------------------------

    // Register vec2 type with Lua
    void RegisterVec2(sol::state& lua);

    //-------------------------------------------------------------------------
    // GameObject System Bindings
    //-------------------------------------------------------------------------

    // Register Transform2D struct with Lua
    // Exposes: position, rotationRad, scale properties
    void RegisterTransform2D(sol::state& lua);

    // Register GameObjectType enum with Lua
    // Exposes: Floor, Wall, Player, Enemy, Trap, Unknown, Ball, Box, Fragment
    void RegisterGameObjectType(sol::state& lua);

    // @brief Register GameObject class with Lua
    // Exposes: id, type, name, active, transform, position, velocity, size,
    //          moveSpeed, solid, hazard, blocksLight, moveable, textureId, layer,
    //          and component accessor methods
    void RegisterGameObject(sol::state& lua);

    //-------------------------------------------------------------------------
    // Component Bindings
    //-------------------------------------------------------------------------

     // Register all gameplay components with Lua
     // HealthComponent, DamageDealerComponent, SpriteRendererComponent
    void RegisterGameObjectComponents(sol::state& lua);

    //-------------------------------------------------------------------------
    // Engine Service Bindings
    //-------------------------------------------------------------------------

    // Register InputManager with Lua
    // Exposes: isKeyDown, wasKeyPressed, wasKeyReleased,
    //          isMouseDown, wasMousePressed, mousePosition
    //          Also creates Keys and Mouse constant tables
    void RegisterInput(sol::state& lua);

    //-------------------------------------------------------------------------
    // EntityRef Binding (ScriptSystem proxy)
    //-------------------------------------------------------------------------

    // Register EntityRef usertype with Lua
    // Exposes: id, position, rotation, scale, velocity, moveSpeed, hp, maxHp,
    //          isDead, isValid, hasTransform, hasVelocity, hasHealth
    void RegisterEntityRef(sol::state& lua);

    //-------------------------------------------------------------------------
    // Global Service Bindings (Lua-script API)
    //-------------------------------------------------------------------------

    // Register Input global table — string-key API for Lua scripts:
    //   Input.isKeyDown("W"), Input.wasKeyPressed("SPACE"),
    //   Input.isMouseDown("LEFT"), Input.getMousePos(), Input.getMouseWorldPos()
    void RegisterInputGlobal(sol::state& lua);

    // Register Audio global table — sound playback for Lua scripts:
    //   Audio.play("name"), Audio.playOneShot("name"),
    //   Audio.stop("name"), Audio.isPlaying("name")
    void RegisterAudioGlobal(sol::state& lua);

    //-------------------------------------------------------------------------
    // Master Registration
    //-------------------------------------------------------------------------

    // Register all engine types and functions with Lua (called once)
    // Registration order respects dependencies:
    // 1. vec2 (no dependencies)
    // 2. Transform2D (depends on vec2)
    // 3. GameObjectType enum
    // 4. Components
    // 5. GameObject (depends on all above)
    // 6. Input
    void RegisterAllBindings(sol::state& lua);

} // namespace Scripting
