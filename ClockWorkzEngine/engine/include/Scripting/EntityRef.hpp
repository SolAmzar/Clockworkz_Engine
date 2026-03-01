/******************************************************************************/
/*!
\file           EntityRef.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          EntityRef — Lua-safe proxy for ECS entity component access.

                Provides nil-safe getters / setters for Transform, Velocity,
                and Health so that Lua scripts can read and write entity state
                without touching the ECS World directly.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"

namespace Scripting {

/**
 * @brief Lightweight proxy that lets Lua scripts access an entity's components.
 *
 * Created on the stack each frame by ScriptSystem and passed to the script's
 * onUpdate callback. Every getter returns a safe default if the component is
 * missing; every setter silently no-ops if the component is missing.
 */
class EntityRef {
  
public:
  /**
   * @brief Construct an EntityRef.
   * @param entity  The ECS entity handle.
   * @param world   Non-owning pointer to the world (must outlive this object).
   */
  EntityRef(ECS::Entity entity, ECS::World *world)
      : m_entity(entity), m_world(world) {}

  // Identity
  uint32_t getId() const { return m_entity.id; }
  bool isValid() const;

  // Transform accessors

  vec2 getPosition() const;
  void setPosition(const vec2 &pos);
  float getRotation() const;
  void setRotation(float rot);
  vec2 getScale() const;
  void setScale(const vec2 &scale);


  // Velocity accessors

  vec2 getVelocity() const;
  void setVelocity(const vec2 &vel);
  float getMoveSpeed() const;
  void setMoveSpeed(float speed);


  // Health accessors

  float getHp() const;
  void setHp(float hp);
  float getMaxHp() const;
  bool isDead() const;


  // Component-existence checks

  bool hasTransform() const;
  bool hasVelocity() const;
  bool hasHealth() const;

private:
  ECS::Entity m_entity;
  ECS::World *m_world;
};

} // namespace Scripting
