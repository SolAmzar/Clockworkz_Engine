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
#include "Components/LuaScript.hpp"
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"
#include <string>

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

  // --- Battery ---
  bool hasBattery() const;
  float getBatteryCharge() const;
  void  setBatteryCharge(float c);
  float getBatteryMaxCharge() const;
  float getBatteryDrainRate() const;
  float getBatteryChargeRate() const;
  bool  isBatteryCharging() const;
  void  setBatteryCharging(bool v);

  // --- Light ---
  bool  hasLight() const;
  bool  isLightActive() const;
  void  setLightActive(bool v);
  float getLightRadius() const;
  float getLightAngle() const;
  float getLightOffsetX() const;
  float getLightOffsetY() const;

  // --- Fragment ---
  bool  hasFragment() const;
  bool  isFragmentCollected() const;
  void  setFragmentCollected(bool v);
  float getFragmentBobSpeed() const;
  float getFragmentBobAmount() const;
  float getFragmentAnimTimer() const;
  void  setFragmentAnimTimer(float t);
  float getFragmentAlpha() const;
  void  setFragmentAlpha(float a);
  bool  getFragmentRequiresLight() const;

  // --- FragmentCollector ---
  bool  hasFragmentCollector() const;
  int   getCollectedCount() const;
  void  setCollectedCount(int n);
  int   getRequiredFragments() const;
  float getPickupRadius() const;

  // --- Door ---
  bool        hasDoor() const;
  bool        isDoorOpen() const;
  void        setDoorOpen(bool v);
  int         getDoorRequiredFragments() const;
  std::string getDoorOpenTexture() const;
  std::string getDoorClosedTexture() const;

  // --- DoorSwing ---
  bool  hasDoorSwing() const;
  bool  isDoorSwingOpen() const;
  void  setDoorSwingOpen(bool v);
  bool  isDoorSwingInitialized() const;
  void  setDoorSwingInitialized(bool v);
  float getDoorSwingInitialRotation() const;
  void  setDoorSwingInitialRotation(float r);
  float getDoorSwingAngle() const;
  bool  isDoorSwingClockwise() const;
  float getDoorSwingSpeed() const;

  // --- LightExposure ---
  bool  hasLightExposure() const;
  float getLightExposureTime() const;
  void  setLightExposureTime(float t);
  float getMaxLightExposureTime() const;
  bool  isInLight() const;
  void  setInLight(bool v);
  bool  isExposureDangerous() const;

  // --- SpriteRenderer ---
  bool        hasSpriteRenderer() const;
  std::string getSpriteTextureId() const;
  void        setSpriteTextureId(const std::string &id);
  float       getSpriteAlpha() const;
  void        setSpriteAlpha(float a);
  bool        isSpriteUnlit() const;
  void        setSpriteUnlit(bool v);

  // --- Collider ---
  bool hasCollider() const;
  bool isColliderSolid() const;
  void setColliderSolid(bool v);
  bool isColliderMoveable() const;
  void setColliderMoveable(bool v);

  // --- Tag ---
  bool        hasTag() const;
  std::string getTagValue() const;

  // --- Generic script property access (Unity-like: game components as pure Lua) ---
  bool hasScript(const std::string& scriptPath) const;
  Components::LuaScriptEntry*       findScriptEntry(const std::string& scriptPath);
  const Components::LuaScriptEntry* findScriptEntry(const std::string& scriptPath) const;

private:
  ECS::Entity m_entity;
  ECS::World *m_world;
};

} // namespace Scripting
