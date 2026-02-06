/******************************************************************************/
/*!
\file           EntityRef.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          EntityRef implementation — nil-safe ECS component access.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Scripting/EntityRef.hpp"
#include "Components/Health.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"

namespace Scripting {

// Identity 

bool EntityRef::isValid() const {
  return m_world && m_world->isValid(m_entity);
}

// Transform 

vec2 EntityRef::getPosition() const {
  if (!m_world) return {0.f, 0.f};
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->position : vec2{0.f, 0.f};
}

void EntityRef::setPosition(const vec2 &pos) {
  if (!m_world) return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t) t->position = pos;
}

float EntityRef::getRotation() const {
  if (!m_world) return 0.f;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->rotation : 0.f;
}

void EntityRef::setRotation(float rot) {
  if (!m_world) return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t) t->rotation = rot;
}

vec2 EntityRef::getScale() const {
  if (!m_world) return {1.f, 1.f};
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->scale : vec2{1.f, 1.f};
}

void EntityRef::setScale(const vec2 &scale) {
  if (!m_world) return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t) t->scale = scale;
}

// Velocity 

vec2 EntityRef::getVelocity() const {
  if (!m_world) return {0.f, 0.f};
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  return v ? v->velocity : vec2{0.f, 0.f};
}

void EntityRef::setVelocity(const vec2 &vel) {
  if (!m_world) return;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  if (v) v->velocity = vel;
}

float EntityRef::getMoveSpeed() const {
  if (!m_world) return 0.f;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  return v ? v->moveSpeed : 0.f;
}

void EntityRef::setMoveSpeed(float speed) {
  if (!m_world) return;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  if (v) v->moveSpeed = speed;
}

// Health 

float EntityRef::getHp() const {
  if (!m_world) return 0.f;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  return h ? h->hp : 0.f;
}

void EntityRef::setHp(float hp) {
  if (!m_world) return;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  if (h) h->hp = hp;
}

float EntityRef::getMaxHp() const {
  if (!m_world) return 0.f;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  return h ? h->maxHp : 0.f;
}

bool EntityRef::isDead() const {
  if (!m_world) return false;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  return h ? h->isDead : false;
}

// Component checks 
bool EntityRef::hasTransform() const {
  return m_world && m_world->hasComponent<Components::Transform>(m_entity);
}

bool EntityRef::hasVelocity() const {
  return m_world && m_world->hasComponent<Components::Velocity>(m_entity);
}

bool EntityRef::hasHealth() const {
  return m_world && m_world->hasComponent<Components::Health>(m_entity);
}

} // namespace Scripting
