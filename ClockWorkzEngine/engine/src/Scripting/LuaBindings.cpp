/******************************************************************************/
/*!
\file           LuaBindings.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Implements Sol2 usertype registrations for Lua scripting.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Scripting/LuaBindings.hpp"
#include "Audio/AudioManager.hpp"
#include "Core/Engine.hpp"
#include "Gameplay/Components/DamageDealer.hpp"
#include "Gameplay/Components/Health.hpp"
#include "Gameplay/Components/SpriteRenderer.hpp"
#include "Gameplay/GameObject.hpp"
#include "Graphics/Camera2D.hpp"
#include "Math/Vec2.hpp"
#include "Math/config.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include "Scripting/EntityRef.hpp"


#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>


namespace Scripting {

//-------------------------------------------------------------------------
// vec2 Binding
//-------------------------------------------------------------------------
void RegisterVec2(sol::state &lua) {
  lua.new_usertype<vec2>(
      "vec2",
      // Constructors
      sol::constructors<vec2(), vec2(f32), vec2(f32, f32)>(),

      // Properties
      "x", &vec2::x, "y", &vec2::y,

      // Operators
      sol::meta_function::addition,
      sol::overload([](const vec2 &a, const vec2 &b) { return a + b; }),
      sol::meta_function::subtraction,
      sol::overload([](const vec2 &a, const vec2 &b) { return a - b; }),
      sol::meta_function::multiplication,
      sol::overload([](const vec2 &a, f32 s) { return a * s; },
                    [](f32 s, const vec2 &a) { return a * s; },
                    [](const vec2 &a, const vec2 &b) { return a * b; }),
      sol::meta_function::division,
      sol::overload([](const vec2 &a, f32 s) { return a / s; },
                    [](const vec2 &a, const vec2 &b) { return a / b; }),
      sol::meta_function::unary_minus, [](const vec2 &v) { return -v; },
      sol::meta_function::equal_to,
      [](const vec2 &a, const vec2 &b) { return a.x == b.x && a.y == b.y; },
      sol::meta_function::to_string,
      [](const vec2 &v) {
        return "vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
      },

      // Methods
      "length", [](const vec2 &v) { return magnitude(v); }, "lengthSquared",
      [](const vec2 &v) { return length_sq(v); }, "normalized",
      [](const vec2 &v) { return normalize(v); }, "dot",
      [](const vec2 &a, const vec2 &b) { return dot(a, b); }, "cross",
      [](const vec2 &a, const vec2 &b) { return cross(a, b); }, "lerp",
      [](const vec2 &a, const vec2 &b, f32 t) { return lerp(a, b, t); },
      "rotate", [](const vec2 &v, f32 angle) { return rotate(v, angle); },
      "perpCW", [](const vec2 &v) { return perp_cw(v); }, "perpCCW",
      [](const vec2 &v) { return perp_ccw(v); },

      // Set both components at once
      "set",
      [](vec2 &v, f32 x, f32 y) {
        v.x = x;
        v.y = y;
      });

  // Global constructor function (alternative syntax)
  lua["Vec2"] = [](f32 x, f32 y) { return vec2(x, y); };

  // Static helper functions
  lua["vec2_zero"] = []() { return vec2(0.f, 0.f); };
  lua["vec2_one"] = []() { return vec2(1.f, 1.f); };
  lua["vec2_up"] = []() { return vec2(0.f, 1.f); };
  lua["vec2_down"] = []() { return vec2(0.f, -1.f); };
  lua["vec2_left"] = []() { return vec2(-1.f, 0.f); };
  lua["vec2_right"] = []() { return vec2(1.f, 0.f); };
}

//-------------------------------------------------------------------------
// Transform2D Binding
//-------------------------------------------------------------------------
void RegisterTransform2D(sol::state &lua) {
  lua.new_usertype<Gameplay::Transform2D>(
      "Transform2D", sol::constructors<Gameplay::Transform2D()>(),

      // Properties
      "position", &Gameplay::Transform2D::position, "rotationRad",
      &Gameplay::Transform2D::rotationRad, "scale",
      &Gameplay::Transform2D::scale,

      // Convenience methods
      "setPosition",
      [](Gameplay::Transform2D &t, f32 x, f32 y) { t.position = vec2(x, y); },
      "getPosition", [](const Gameplay::Transform2D &t) { return t.position; },

      // Rotation in degrees (convenience)
      "rotationDeg",
      sol::property(
          [](const Gameplay::Transform2D &t) {
            return t.rotationRad * 180.0f / 3.14159265f;
          },
          [](Gameplay::Transform2D &t, float deg) {
            t.rotationRad = deg * 3.14159265f / 180.0f;
          }),

      "setScale",
      [](Gameplay::Transform2D &t, f32 x, f32 y) { t.scale = vec2(x, y); },
      "setUniformScale",
      [](Gameplay::Transform2D &t, f32 s) { t.scale = vec2(s, s); });
}

//-------------------------------------------------------------------------
// GameObjectType Enum Binding
//-------------------------------------------------------------------------
void RegisterGameObjectType(sol::state &lua) {
  lua.new_enum(
      "GameObjectType", "Floor", Gameplay::GameObjectType::Floor, "Wall",
      Gameplay::GameObjectType::Wall, "Player",
      Gameplay::GameObjectType::Player, "Enemy",
      Gameplay::GameObjectType::Enemy, "Trap", Gameplay::GameObjectType::Trap,
      "Unknown", Gameplay::GameObjectType::Unknown, "Ball",
      Gameplay::GameObjectType::Ball, "Box", Gameplay::GameObjectType::Box);
}

//-------------------------------------------------------------------------
// GameObject Binding
//-------------------------------------------------------------------------
void RegisterGameObject(sol::state &lua) {
  lua.new_usertype<Gameplay::GameObject>(
      "GameObject",
      // No direct construction - GameObjects are created via factories/scenes
      sol::no_constructor,

      // Identity
      "id", sol::property(&Gameplay::GameObject::id), "type",
      sol::property(&Gameplay::GameObject::type), "name",
      sol::property(&Gameplay::GameObject::name,
                    &Gameplay::GameObject::setName),
      "active",
      sol::property(&Gameplay::GameObject::active,
                    &Gameplay::GameObject::setActive),

      // Transform access (returns reference for modification)
      "transform",
      sol::property([](Gameplay::GameObject &obj) -> Gameplay::Transform2D & {
        return obj.transform();
      }),

      // Position shortcut (common operation)
      "position",
      sol::property(
          [](Gameplay::GameObject &obj) { return obj.transform().position; },
          [](Gameplay::GameObject &obj, vec2 pos) { obj.setPosition(pos); }),

      // Rotation shortcut
      "rotation",
      sol::property(
          [](Gameplay::GameObject &obj) { return obj.transform().rotationRad; },
          [](Gameplay::GameObject &obj, float r) {
            obj.transform().rotationRad = r;
          }),

      // Scale shortcut
      "scale",
      sol::property(
          [](Gameplay::GameObject &obj) { return obj.transform().scale; },
          [](Gameplay::GameObject &obj, vec2 s) { obj.transform().scale = s; }),

      // Physics properties
      "velocity",
      sol::property(
          [](Gameplay::GameObject &obj) { return obj.velocity(); },
          [](Gameplay::GameObject &obj, vec2 v) { obj.setVelocity(v); }),
      "size",
      sol::property(&Gameplay::GameObject::size,
                    &Gameplay::GameObject::setSize),
      "moveSpeed",
      sol::property([](Gameplay::GameObject &obj) { return obj.moveSpeed(); },
                    &Gameplay::GameObject::setMoveSpeed),

      // Flags
      "solid",
      sol::property(&Gameplay::GameObject::isSolid,
                    &Gameplay::GameObject::setSolid),
      "hazard",
      sol::property(&Gameplay::GameObject::isHazard,
                    &Gameplay::GameObject::setHazard),
      "blocksLight",
      sol::property(&Gameplay::GameObject::blocksLight,
                    &Gameplay::GameObject::setBlocksLight),
      "moveable",
      sol::property(&Gameplay::GameObject::ismoveable,
                    &Gameplay::GameObject::setMoveable),

      // Rendering
      "textureId",
      sol::property(&Gameplay::GameObject::textureId,
                    &Gameplay::GameObject::setTextureId),
      "layer",
      sol::property(&Gameplay::GameObject::layer,
                    &Gameplay::GameObject::setLayer),

      // Collision
      "collisionMask",
      sol::property(&Gameplay::GameObject::collisionMask,
                    &Gameplay::GameObject::setCollisionMask),
      "collisionCategory",
      sol::property(&Gameplay::GameObject::collisionCategory,
                    &Gameplay::GameObject::setCollisionCategory),

      // Battery (for player)
      "battery",
      sol::property(&Gameplay::GameObject::checkBatt,
                    &Gameplay::GameObject::setBatt),

      // Light exposure
      "lightExposureTime",
      sol::property(&Gameplay::GameObject::lightExposureTime),
      "addLightExposure", &Gameplay::GameObject::addLightExposure,
      "resetLightExposure", &Gameplay::GameObject::resetLightExposure,

      // Component accessors - typed getters
      "getHealth",
      [](Gameplay::GameObject &obj) -> Gameplay::HealthComponent * {
        return obj.getComponent<Gameplay::HealthComponent>();
      },
      "getDamageDealer",
      [](Gameplay::GameObject &obj) -> Gameplay::DamageDealerComponent * {
        return obj.getComponent<Gameplay::DamageDealerComponent>();
      },
      "getSpriteRenderer",
      [](Gameplay::GameObject &obj) -> Gameplay::SpriteRendererComponent * {
        return obj.getComponent<Gameplay::SpriteRendererComponent>();
      },

      // Utility methods
      "markDirty", &Gameplay::GameObject::markDirty);
}

//-------------------------------------------------------------------------
// Component Bindings
//-------------------------------------------------------------------------
void RegisterGameObjectComponents(sol::state &lua) {
  // HealthComponent
  lua.new_usertype<Gameplay::HealthComponent>(
      "HealthComponent", sol::no_constructor, "hp",
      sol::property(&Gameplay::HealthComponent::hp), "damage",
      [](Gameplay::HealthComponent &h, Gameplay::GameObject &owner, int d) {
        h.damage(owner, d);
      },
      "heal", &Gameplay::HealthComponent::heal);

  // DamageDealerComponent
  lua.new_usertype<Gameplay::DamageDealerComponent>(
      "DamageDealerComponent", sol::no_constructor, "damage",
      sol::property(&Gameplay::DamageDealerComponent::damage,
                    &Gameplay::DamageDealerComponent::setDamage));

  // SpriteRendererComponent
  lua.new_usertype<Gameplay::SpriteRendererComponent>(
      "SpriteRendererComponent", sol::no_constructor, "textureAlias",
      sol::property(&Gameplay::SpriteRendererComponent::textureAlias,
                    &Gameplay::SpriteRendererComponent::setTexture),
      "setTint",
      [](Gameplay::SpriteRendererComponent &sr, f32 r, f32 g, f32 b, f32 a) {
        sr.setTint(glm::vec4(r, g, b, a));
      },
      "getTint",
      [](Gameplay::SpriteRendererComponent &sr) {
        const auto &t = sr.tint();
        return std::make_tuple(t.r, t.g, t.b, t.a);
      },
      "isUI",
      sol::property(&Gameplay::SpriteRendererComponent::IsUI,
                    &Gameplay::SpriteRendererComponent::setIsUI));
}

//-------------------------------------------------------------------------
// Input Binding
//-------------------------------------------------------------------------
void RegisterInput(sol::state &lua) {
  lua.new_usertype<Platform::InputManager>(
      "InputManager", sol::no_constructor,

      // Keyboard
      "isKeyDown", &Platform::InputManager::isKeyDown, "wasKeyPressed",
      &Platform::InputManager::wasKeyPressed, "wasKeyReleased",
      &Platform::InputManager::wasKeyReleased,

      // Mouse buttons
      "isMouseDown", &Platform::InputManager::isMouseDown, "wasMousePressed",
      &Platform::InputManager::wasMousePressed, "wasMouseReleased",
      &Platform::InputManager::wasMouseReleased, "wasMouseClicked",
      &Platform::InputManager::wasMouseClicked,

      // Mouse position
      "mousePosition", &Platform::InputManager::mousePosition);

  // Key constants table
  auto keys = lua.create_table();
  keys["A"] = GLFW_KEY_A;
  keys["B"] = GLFW_KEY_B;
  keys["C"] = GLFW_KEY_C;
  keys["D"] = GLFW_KEY_D;
  keys["E"] = GLFW_KEY_E;
  keys["F"] = GLFW_KEY_F;
  keys["G"] = GLFW_KEY_G;
  keys["H"] = GLFW_KEY_H;
  keys["I"] = GLFW_KEY_I;
  keys["J"] = GLFW_KEY_J;
  keys["K"] = GLFW_KEY_K;
  keys["L"] = GLFW_KEY_L;
  keys["M"] = GLFW_KEY_M;
  keys["N"] = GLFW_KEY_N;
  keys["O"] = GLFW_KEY_O;
  keys["P"] = GLFW_KEY_P;
  keys["Q"] = GLFW_KEY_Q;
  keys["R"] = GLFW_KEY_R;
  keys["S"] = GLFW_KEY_S;
  keys["T"] = GLFW_KEY_T;
  keys["U"] = GLFW_KEY_U;
  keys["V"] = GLFW_KEY_V;
  keys["W"] = GLFW_KEY_W;
  keys["X"] = GLFW_KEY_X;
  keys["Y"] = GLFW_KEY_Y;
  keys["Z"] = GLFW_KEY_Z;

  keys["NUM_0"] = GLFW_KEY_0;
  keys["NUM_1"] = GLFW_KEY_1;
  keys["NUM_2"] = GLFW_KEY_2;
  keys["NUM_3"] = GLFW_KEY_3;
  keys["NUM_4"] = GLFW_KEY_4;
  keys["NUM_5"] = GLFW_KEY_5;
  keys["NUM_6"] = GLFW_KEY_6;
  keys["NUM_7"] = GLFW_KEY_7;
  keys["NUM_8"] = GLFW_KEY_8;
  keys["NUM_9"] = GLFW_KEY_9;

  keys["SPACE"] = GLFW_KEY_SPACE;
  keys["ENTER"] = GLFW_KEY_ENTER;
  keys["ESCAPE"] = GLFW_KEY_ESCAPE;
  keys["TAB"] = GLFW_KEY_TAB;
  keys["BACKSPACE"] = GLFW_KEY_BACKSPACE;

  keys["LEFT_SHIFT"] = GLFW_KEY_LEFT_SHIFT;
  keys["RIGHT_SHIFT"] = GLFW_KEY_RIGHT_SHIFT;
  keys["LEFT_CTRL"] = GLFW_KEY_LEFT_CONTROL;
  keys["RIGHT_CTRL"] = GLFW_KEY_RIGHT_CONTROL;
  keys["LEFT_ALT"] = GLFW_KEY_LEFT_ALT;
  keys["RIGHT_ALT"] = GLFW_KEY_RIGHT_ALT;

  keys["UP"] = GLFW_KEY_UP;
  keys["DOWN"] = GLFW_KEY_DOWN;
  keys["LEFT"] = GLFW_KEY_LEFT;
  keys["RIGHT"] = GLFW_KEY_RIGHT;

  keys["F1"] = GLFW_KEY_F1;
  keys["F2"] = GLFW_KEY_F2;
  keys["F3"] = GLFW_KEY_F3;
  keys["F4"] = GLFW_KEY_F4;
  keys["F5"] = GLFW_KEY_F5;
  keys["F6"] = GLFW_KEY_F6;
  keys["F7"] = GLFW_KEY_F7;
  keys["F8"] = GLFW_KEY_F8;
  keys["F9"] = GLFW_KEY_F9;
  keys["F10"] = GLFW_KEY_F10;
  keys["F11"] = GLFW_KEY_F11;
  keys["F12"] = GLFW_KEY_F12;

  lua["Keys"] = keys;

  // Mouse button constants table
  auto mouse = lua.create_table();
  mouse["LEFT"] = GLFW_MOUSE_BUTTON_LEFT;
  mouse["RIGHT"] = GLFW_MOUSE_BUTTON_RIGHT;
  mouse["MIDDLE"] = GLFW_MOUSE_BUTTON_MIDDLE;
  mouse["BUTTON_4"] = GLFW_MOUSE_BUTTON_4;
  mouse["BUTTON_5"] = GLFW_MOUSE_BUTTON_5;
  lua["Mouse"] = mouse;
}

//-------------------------------------------------------------------------
// EntityRef Binding — static helpers (no heap allocation, no closures)
//-------------------------------------------------------------------------

// playAnimation with optional force parameter
static void playAnimation1(Scripting::EntityRef &ref, const std::string &name) {
  ref.playAnimation(name, false);
}
static void playAnimation2(Scripting::EntityRef &ref, const std::string &name,
                           bool force) {
  ref.playAnimation(name, force);
}

// getScriptProp / setScriptProp (sol::this_state injection works in free fns
// too)
static sol::object getScriptPropFn(Scripting::EntityRef &ref,
                                   const std::string &scriptPath,
                                   const std::string &key, sol::this_state ts) {
  auto sv = sol::state_view{ts};
  auto *entry = ref.findScriptEntry(scriptPath);
  if (!entry)
    return sol::make_object(sv, sol::nil);
  auto it = entry->properties.find(key);
  if (it == entry->properties.end())
    return sol::make_object(sv, sol::nil);
#ifndef __INTELLISENSE__
  return std::visit(
      [&sv](auto &&v) -> sol::object { return sol::make_object(sv, v); },
      it->second);
#else
  return sol::make_object(sv, sol::nil);
#endif
}

static void setScriptPropFn(Scripting::EntityRef &ref,
                            const std::string &scriptPath,
                            const std::string &key, sol::object val) {
  auto *entry = ref.findScriptEntry(scriptPath);
  if (!entry)
    return;
  auto it = entry->properties.find(key);
  if (it == entry->properties.end())
    return;
#ifndef __INTELLISENSE__
  std::visit(
      [&](auto &&existing) {
        using T = std::decay_t<decltype(existing)>;
        auto opt = val.as<sol::optional<T>>();
        if (opt)
          it->second = *opt;
      },
      it->second);
#endif
}

//-------------------------------------------------------------------------
// EntityRef Binding
//-------------------------------------------------------------------------
void RegisterEntityRef(sol::state &lua) {
  lua.new_usertype<Scripting::EntityRef>(
      "EntityRef", sol::no_constructor,

      // Identity
      "id", sol::property(&Scripting::EntityRef::getId), "isValid",
      &Scripting::EntityRef::isValid,

      // Transform
      "position",
      sol::property(&Scripting::EntityRef::getPosition,
                    &Scripting::EntityRef::setPosition),
      "rotation",
      sol::property(&Scripting::EntityRef::getRotation,
                    &Scripting::EntityRef::setRotation),
      "scale",
      sol::property(&Scripting::EntityRef::getScale,
                    &Scripting::EntityRef::setScale),

      // Velocity
      "velocity",
      sol::property(&Scripting::EntityRef::getVelocity,
                    &Scripting::EntityRef::setVelocity),
      "moveSpeed",
      sol::property(&Scripting::EntityRef::getMoveSpeed,
                    &Scripting::EntityRef::setMoveSpeed),

      // Health
      "hp",
      sol::property(&Scripting::EntityRef::getHp, &Scripting::EntityRef::setHp),
      "maxHp", sol::property(&Scripting::EntityRef::getMaxHp), "isDead",
      sol::property(&Scripting::EntityRef::isDead),

      // Component checks
      "hasTransform", &Scripting::EntityRef::hasTransform, "hasVelocity",
      &Scripting::EntityRef::hasVelocity, "hasHealth",
      &Scripting::EntityRef::hasHealth,

      // Light
      "hasLight", &Scripting::EntityRef::hasLight, "isLightActive",
      &Scripting::EntityRef::isLightActive, "setLightActive",
      &Scripting::EntityRef::setLightActive, "getLightRadius",
      &Scripting::EntityRef::getLightRadius, "getLightAngle",
      &Scripting::EntityRef::getLightAngle, "getLightOffsetX",
      &Scripting::EntityRef::getLightOffsetX, "getLightOffsetY",
      &Scripting::EntityRef::getLightOffsetY, "setLightOffsetX",
      &Scripting::EntityRef::setLightOffsetX, "setLightOffsetY",
      &Scripting::EntityRef::setLightOffsetY,

      // TextRenderer
      "hasTextRenderer", &Scripting::EntityRef::hasTextRenderer, "getText",
      &Scripting::EntityRef::getText, "setText", &Scripting::EntityRef::setText,

      // SpriteRenderer
      "hasSpriteRenderer", &Scripting::EntityRef::hasSpriteRenderer,
      "getSpriteTextureId", &Scripting::EntityRef::getSpriteTextureId,
      "setSpriteTextureId", &Scripting::EntityRef::setSpriteTextureId,
      "getSpriteAlpha", &Scripting::EntityRef::getSpriteAlpha, "setSpriteAlpha",
      &Scripting::EntityRef::setSpriteAlpha, "isSpriteUnlit",
      &Scripting::EntityRef::isSpriteUnlit, "setSpriteUnlit",
      &Scripting::EntityRef::setSpriteUnlit,

      // Collider
      "hasCollider", &Scripting::EntityRef::hasCollider, "isColliderSolid",
      &Scripting::EntityRef::isColliderSolid, "setColliderSolid",
      &Scripting::EntityRef::setColliderSolid, "isColliderMoveable",
      &Scripting::EntityRef::isColliderMoveable, "setColliderMoveable",
      &Scripting::EntityRef::setColliderMoveable,

      // Tag
      "hasTag", &Scripting::EntityRef::hasTag, "getTagValue",
      &Scripting::EntityRef::getTagValue,

            // SpriteAnimator
            "hasSpriteAnimator", &Scripting::EntityRef::hasSpriteAnimator,
            "playAnimation", sol::overload(&playAnimation1, &playAnimation2),
            "loadAnimationFile",       &Scripting::EntityRef::loadAnimationFile,
            "getAnimationFile",        &Scripting::EntityRef::getAnimationFile,
            "getCurrentClip",    &Scripting::EntityRef::getCurrentAnimationClip,
            "isAnimationPlaying",      &Scripting::EntityRef::isAnimationPlaying,
            "getAnimationPlaybackSpeed",&Scripting::EntityRef::getAnimationPlaybackSpeed,
            "setAnimationPlaybackSpeed",&Scripting::EntityRef::setAnimationPlaybackSpeed,

      // Generic script property access (Unity-like: game components as pure
      // Lua)
      "hasScript", &Scripting::EntityRef::hasScript, "getScriptProp",
      &getScriptPropFn, "setScriptProp", &setScriptPropFn);
}

//-------------------------------------------------------------------------
// Input Global Binding — Helpers
//-------------------------------------------------------------------------
static const std::unordered_map<std::string, int> s_keyMap = {
    {"A", GLFW_KEY_A},
    {"B", GLFW_KEY_B},
    {"C", GLFW_KEY_C},
    {"D", GLFW_KEY_D},
    {"E", GLFW_KEY_E},
    {"F", GLFW_KEY_F},
    {"G", GLFW_KEY_G},
    {"H", GLFW_KEY_H},
    {"I", GLFW_KEY_I},
    {"J", GLFW_KEY_J},
    {"K", GLFW_KEY_K},
    {"L", GLFW_KEY_L},
    {"M", GLFW_KEY_M},
    {"N", GLFW_KEY_N},
    {"O", GLFW_KEY_O},
    {"P", GLFW_KEY_P},
    {"Q", GLFW_KEY_Q},
    {"R", GLFW_KEY_R},
    {"S", GLFW_KEY_S},
    {"T", GLFW_KEY_T},
    {"U", GLFW_KEY_U},
    {"V", GLFW_KEY_V},
    {"W", GLFW_KEY_W},
    {"X", GLFW_KEY_X},
    {"Y", GLFW_KEY_Y},
    {"Z", GLFW_KEY_Z},
    {"0", GLFW_KEY_0},
    {"1", GLFW_KEY_1},
    {"2", GLFW_KEY_2},
    {"3", GLFW_KEY_3},
    {"4", GLFW_KEY_4},
    {"5", GLFW_KEY_5},
    {"6", GLFW_KEY_6},
    {"7", GLFW_KEY_7},
    {"8", GLFW_KEY_8},
    {"9", GLFW_KEY_9},
    {"SPACE", GLFW_KEY_SPACE},
    {"ENTER", GLFW_KEY_ENTER},
    {"ESCAPE", GLFW_KEY_ESCAPE},
    {"TAB", GLFW_KEY_TAB},
    {"BACKSPACE", GLFW_KEY_BACKSPACE},
    {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT},
    {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
    {"SHIFT", GLFW_KEY_LEFT_SHIFT},
    {"LEFT_CTRL", GLFW_KEY_LEFT_CONTROL},
    {"RIGHT_CTRL", GLFW_KEY_RIGHT_CONTROL},
    {"CTRL", GLFW_KEY_LEFT_CONTROL},
    {"LEFT_ALT", GLFW_KEY_LEFT_ALT},
    {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT},
    {"UP", GLFW_KEY_UP},
    {"DOWN", GLFW_KEY_DOWN},
    {"LEFT", GLFW_KEY_LEFT},
    {"RIGHT", GLFW_KEY_RIGHT},
    {"F1", GLFW_KEY_F1},
    {"F2", GLFW_KEY_F2},
    {"F3", GLFW_KEY_F3},
    {"F4", GLFW_KEY_F4},
    {"F5", GLFW_KEY_F5},
    {"F6", GLFW_KEY_F6},
    {"F7", GLFW_KEY_F7},
    {"F8", GLFW_KEY_F8},
    {"F9", GLFW_KEY_F9},
    {"F10", GLFW_KEY_F10},
    {"F11", GLFW_KEY_F11},
    {"F12", GLFW_KEY_F12},
    {"APOSTROPHE", GLFW_KEY_APOSTROPHE},
    {"COMMA", GLFW_KEY_COMMA},
    {"MINUS", GLFW_KEY_MINUS},
    {"PERIOD", GLFW_KEY_PERIOD},
    {"SLASH", GLFW_KEY_SLASH},
    {"SEMICOLON", GLFW_KEY_SEMICOLON},
    {"EQUAL", GLFW_KEY_EQUAL},
    {"LEFT_BRACKET", GLFW_KEY_LEFT_BRACKET},
    {"BACKSLASH", GLFW_KEY_BACKSLASH},
    {"RIGHT_BRACKET", GLFW_KEY_RIGHT_BRACKET},
    {"GRAVE_ACCENT", GLFW_KEY_GRAVE_ACCENT},
};

static const std::unordered_map<std::string, int> s_mouseMap = {
    {"LEFT", GLFW_MOUSE_BUTTON_LEFT},
    {"RIGHT", GLFW_MOUSE_BUTTON_RIGHT},
    {"MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE},
};

static int ensureKey(const std::string &name) {
  auto it = s_keyMap.find(name);
  if (it == s_keyMap.end())
    return -1;
  auto &inp = Core::Engine::Get().input();
  if (!inp.isKeyTracked(it->second))
    inp.trackKey(it->second);
  return it->second;
}

static int ensureMouse(const std::string &name) {
  auto it = s_mouseMap.find(name);
  if (it == s_mouseMap.end())
    return -1;
  auto &inp = Core::Engine::Get().input();
  if (!inp.isMouseButtonTracked(it->second))
    inp.trackMouseButton(it->second);
  return it->second;
}

static bool LuaIsKeyDown(const std::string &name) {
  int k = ensureKey(name);
  return k != -1 && Core::Engine::Get().input().isKeyDown(k);
}
static bool LuaWasKeyPressed(const std::string &name) {
  int k = ensureKey(name);
  return k != -1 && Core::Engine::Get().input().wasKeyPressed(k);
}
static bool LuaWasKeyReleased(const std::string &name) {
  int k = ensureKey(name);
  return k != -1 && Core::Engine::Get().input().wasKeyReleased(k);
}
static bool LuaIsMouseDown(const std::string &name) {
  int b = ensureMouse(name);
  return b != -1 && Core::Engine::Get().input().isMouseDown(b);
}
static bool LuaWasMousePressed(const std::string &name) {
  int b = ensureMouse(name);
  return b != -1 && Core::Engine::Get().input().wasMousePressed(b);
}
static vec2 LuaGetMousePos() {
  return Core::Engine::Get().input().mousePosition();
}
static vec2 LuaGetMouseWorldPos() {
  auto &eng = Core::Engine::Get();
  return eng.input().getMouseWorldPos(
      eng.camera(), static_cast<float>(eng.window().width()),
      static_cast<float>(eng.window().height()));
}

void RegisterInputGlobal(sol::state &lua) {
  auto t = lua.create_table();
  t.set_function("isKeyDown", &LuaIsKeyDown);
  t.set_function("wasKeyPressed", &LuaWasKeyPressed);
  t.set_function("wasKeyReleased", &LuaWasKeyReleased);
  t.set_function("isMouseDown", &LuaIsMouseDown);
  t.set_function("wasMousePressed", &LuaWasMousePressed);
  t.set_function("getMousePos", &LuaGetMousePos);
  t.set_function("getMouseWorldPos", &LuaGetMouseWorldPos);
  lua["Input"] = t;
}

//-------------------------------------------------------------------------
// Audio Global Binding — Static helpers (avoid closures)
//-------------------------------------------------------------------------
static void LuaAudioPlay(const std::string &name) {
  Core::Engine::Get().audio().play(name);
}
static void LuaAudioPlayOneShot(const std::string &name) {
  Core::Engine::Get().audio().playOneShot(name);
}
static void LuaAudioStop(const std::string &name) {
  Core::Engine::Get().audio().stop(name);
}
static bool LuaAudioIsPlaying(const std::string &name) {
  return Core::Engine::Get().audio().isPlaying(name);
}

void RegisterAudioGlobal(sol::state &lua) {
  auto t = lua.create_table();
  t.set_function("play", &LuaAudioPlay);
  t.set_function("playOneShot", &LuaAudioPlayOneShot);
  t.set_function("stop", &LuaAudioStop);
  t.set_function("isPlaying", &LuaAudioIsPlaying);
  lua["Audio"] = t;
}

//-------------------------------------------------------------------------
// Master Registration — Static helpers (avoid closures)
//-------------------------------------------------------------------------
static Platform::InputManager &LuaGetInput() {
  return Core::Engine::Get().input();
}
static vec2 LuaGetMouseWorldPosGlobal() {
  auto &eng = Core::Engine::Get();
  auto &input = eng.input();
  auto &cam = eng.camera();
  auto &window = eng.window();
  return input.getMouseWorldPos(cam, static_cast<float>(window.width()),
                                static_cast<float>(window.height()));
}

void RegisterAllBindings(sol::state &lua) {
  std::cout << "[LuaBindings] Registering engine types..." << std::endl;

  // Order matters for dependencies
  RegisterVec2(lua);
  std::cout << "[LuaBindings] Registered vec2" << std::endl;

  RegisterTransform2D(lua);
  std::cout << "[LuaBindings] Registered Transform2D" << std::endl;

  RegisterGameObjectType(lua);
  std::cout << "[LuaBindings] Registered GameObjectType enum" << std::endl;

  RegisterGameObjectComponents(lua);
  std::cout << "[LuaBindings] Registered Components" << std::endl;

  RegisterGameObject(lua);
  std::cout << "[LuaBindings] Registered GameObject" << std::endl;

  RegisterInput(lua);
  std::cout << "[LuaBindings] Registered Input and key constants" << std::endl;

  RegisterEntityRef(lua);
  std::cout << "[LuaBindings] Registered EntityRef" << std::endl;

  RegisterInputGlobal(lua);
  std::cout << "[LuaBindings] Registered Input global" << std::endl;

  RegisterAudioGlobal(lua);
  std::cout << "[LuaBindings] Registered Audio global" << std::endl;

  // Create Engine global table for accessing engine services
  auto engine = lua.create_table();
  engine.set_function("input", &LuaGetInput);
  engine.set_function("mouseWorldPos", &LuaGetMouseWorldPosGlobal);
  engine.set_function("triggerGameOver",
                      []() { Core::Engine::Get().setGameOverPending(); });
  lua["Engine"] = engine;
  std::cout << "[LuaBindings] Registered Engine global table" << std::endl;

  std::cout << "[LuaBindings] All engine types registered successfully"
            << std::endl;
}

} // namespace Scripting
