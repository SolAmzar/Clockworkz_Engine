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
#include "Components/SpriteRenderer.hpp"
#include "Components/Collider.hpp"
#include "Components/Tag.hpp"
// Game-specific components (top-level include/ is on the engine path)
#include "Components/Battery.hpp"
#include "Components/Torch.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/LightExposure.hpp"

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

// --- Battery ---
bool EntityRef::hasBattery() const { return m_world && m_world->hasComponent<Components::Battery>(m_entity); }
float EntityRef::getBatteryCharge() const { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; return b ? b->charge : 0.f; }
void  EntityRef::setBatteryCharge(float c) { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; if (b) b->charge = c; }
float EntityRef::getBatteryMaxCharge() const { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; return b ? b->maxCharge : 0.f; }
float EntityRef::getBatteryDrainRate() const { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; return b ? b->drainRate : 0.f; }
float EntityRef::getBatteryChargeRate() const { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; return b ? b->chargeRate : 0.f; }
bool  EntityRef::isBatteryCharging() const { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; return b ? b->isCharging : false; }
void  EntityRef::setBatteryCharging(bool v) { auto* b = m_world ? m_world->getComponent<Components::Battery>(m_entity) : nullptr; if (b) b->isCharging = v; }

// --- Torch ---
bool  EntityRef::hasTorch() const { return m_world && m_world->hasComponent<Components::TorchComponent>(m_entity); }
bool  EntityRef::isTorchActive() const { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; return t ? t->active : false; }
void  EntityRef::setTorchActive(bool v) { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; if (t) t->active = v; }
float EntityRef::getTorchRadius() const { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; return t ? t->radius : 0.f; }
float EntityRef::getTorchAngle() const { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; return t ? t->angle : 0.f; }
float EntityRef::getTorchOffsetX() const { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; return t ? t->offset.x : 0.f; }
float EntityRef::getTorchOffsetY() const { auto* t = m_world ? m_world->getComponent<Components::TorchComponent>(m_entity) : nullptr; return t ? t->offset.y : 0.f; }

// --- Fragment ---
bool  EntityRef::hasFragment() const { return m_world && m_world->hasComponent<Components::Fragment>(m_entity); }
bool  EntityRef::isFragmentCollected() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->collected : false; }
void  EntityRef::setFragmentCollected(bool v) { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; if (f) f->collected = v; }
float EntityRef::getFragmentBobSpeed() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->bobSpeed : 0.f; }
float EntityRef::getFragmentBobAmount() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->bobAmount : 0.f; }
float EntityRef::getFragmentAnimTimer() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->animationTimer : 0.f; }
void  EntityRef::setFragmentAnimTimer(float t) { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; if (f) f->animationTimer = t; }
float EntityRef::getFragmentAlpha() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->currentAlpha : 0.f; }
void  EntityRef::setFragmentAlpha(float a) { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; if (f) f->currentAlpha = a; }
bool  EntityRef::getFragmentRequiresLight() const { auto* f = m_world ? m_world->getComponent<Components::Fragment>(m_entity) : nullptr; return f ? f->requiresLight : true; }

// --- FragmentCollector ---
bool  EntityRef::hasFragmentCollector() const { return m_world && m_world->hasComponent<Components::FragmentCollector>(m_entity); }
int   EntityRef::getCollectedCount() const { auto* c = m_world ? m_world->getComponent<Components::FragmentCollector>(m_entity) : nullptr; return c ? c->collectedCount : 0; }
void  EntityRef::setCollectedCount(int n) { auto* c = m_world ? m_world->getComponent<Components::FragmentCollector>(m_entity) : nullptr; if (c) c->collectedCount = n; }
int   EntityRef::getRequiredFragments() const { auto* c = m_world ? m_world->getComponent<Components::FragmentCollector>(m_entity) : nullptr; return c ? c->requiredFragments : 0; }
float EntityRef::getPickupRadius() const { auto* c = m_world ? m_world->getComponent<Components::FragmentCollector>(m_entity) : nullptr; return c ? c->pickupRadius : 50.f; }

// --- Door ---
bool        EntityRef::hasDoor() const { return m_world && m_world->hasComponent<Components::Door>(m_entity); }
bool        EntityRef::isDoorOpen() const { auto* d = m_world ? m_world->getComponent<Components::Door>(m_entity) : nullptr; return d ? d->isOpen : false; }
void        EntityRef::setDoorOpen(bool v) { auto* d = m_world ? m_world->getComponent<Components::Door>(m_entity) : nullptr; if (d) d->isOpen = v; }
int         EntityRef::getDoorRequiredFragments() const { auto* d = m_world ? m_world->getComponent<Components::Door>(m_entity) : nullptr; return d ? d->requiredFragments : 1; }
std::string EntityRef::getDoorOpenTexture() const { auto* d = m_world ? m_world->getComponent<Components::Door>(m_entity) : nullptr; return d ? d->openTexture : ""; }
std::string EntityRef::getDoorClosedTexture() const { auto* d = m_world ? m_world->getComponent<Components::Door>(m_entity) : nullptr; return d ? d->closedTexture : ""; }

// --- DoorSwing ---
bool  EntityRef::hasDoorSwing() const { return m_world && m_world->hasComponent<Components::DoorSwing>(m_entity); }
bool  EntityRef::isDoorSwingOpen() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->isOpen : false; }
void  EntityRef::setDoorSwingOpen(bool v) { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; if (d) d->isOpen = v; }
bool  EntityRef::isDoorSwingInitialized() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->initialized : false; }
void  EntityRef::setDoorSwingInitialized(bool v) { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; if (d) d->initialized = v; }
float EntityRef::getDoorSwingInitialRotation() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->initialRotation : 0.f; }
void  EntityRef::setDoorSwingInitialRotation(float r) { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; if (d) d->initialRotation = r; }
float EntityRef::getDoorSwingAngle() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->swingAngle : 90.f; }
bool  EntityRef::isDoorSwingClockwise() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->clockwise : true; }
float EntityRef::getDoorSwingSpeed() const { auto* d = m_world ? m_world->getComponent<Components::DoorSwing>(m_entity) : nullptr; return d ? d->speed : 90.f; }

// --- LightExposure ---
bool  EntityRef::hasLightExposure() const { return m_world && m_world->hasComponent<Components::LightExposure>(m_entity); }
float EntityRef::getLightExposureTime() const { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; return l ? l->exposureTime : 0.f; }
void  EntityRef::setLightExposureTime(float t) { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; if (l) l->exposureTime = t; }
float EntityRef::getMaxLightExposureTime() const { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; return l ? l->maxExposureTime : 10.f; }
bool  EntityRef::isInLight() const { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; return l ? l->inLight : false; }
void  EntityRef::setInLight(bool v) { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; if (l) l->inLight = v; }
bool  EntityRef::isExposureDangerous() const { auto* l = m_world ? m_world->getComponent<Components::LightExposure>(m_entity) : nullptr; return l ? l->isDangerous : false; }

// --- SpriteRenderer ---
bool        EntityRef::hasSpriteRenderer() const { return m_world && m_world->hasComponent<Components::SpriteRenderer>(m_entity); }
std::string EntityRef::getSpriteTextureId() const { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; return s ? s->textureId : ""; }
void        EntityRef::setSpriteTextureId(const std::string& id) { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; if (s) s->textureId = id; }
float       EntityRef::getSpriteAlpha() const { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; return s ? s->color.a : 1.f; }
void        EntityRef::setSpriteAlpha(float a) { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; if (s) s->color.a = a; }
bool        EntityRef::isSpriteUnlit() const { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; return s ? s->unlit : false; }
void        EntityRef::setSpriteUnlit(bool v) { auto* s = m_world ? m_world->getComponent<Components::SpriteRenderer>(m_entity) : nullptr; if (s) s->unlit = v; }

// --- Collider ---
bool EntityRef::hasCollider() const { return m_world && m_world->hasComponent<Components::Collider>(m_entity); }
bool EntityRef::isColliderSolid() const { auto* c = m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr; return c ? c->isSolid : false; }
void EntityRef::setColliderSolid(bool v) { auto* c = m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr; if (c) c->isSolid = v; }
bool EntityRef::isColliderMoveable() const { auto* c = m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr; return c ? c->moveable : false; }
void EntityRef::setColliderMoveable(bool v) { auto* c = m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr; if (c) c->moveable = v; }

// --- Tag ---
bool        EntityRef::hasTag() const { return m_world && m_world->hasComponent<Components::Tag>(m_entity); }
std::string EntityRef::getTagValue() const { auto* t = m_world ? m_world->getComponent<Components::Tag>(m_entity) : nullptr; return t ? t->tag : ""; }

} // namespace Scripting
