/******************************************************************************/
/*!
\file           EntityRef.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 87% [Amzar] 4% [Lebon] 9%; Wei An — navVersion
                counter and collider-setter bumps (setColliderSolid/Moveable)
\brief          EntityRef implementation — nil-safe ECS component access.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Scripting/EntityRef.hpp"
#include "Scripting/NavVersion.hpp"
#include "Components/Collider.hpp"
#include "Components/Health.hpp"
#include "Components/Light.hpp"
#include "Components/LuaScript.hpp"
#include "Components/ParticleEmitter.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Tag.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"


namespace Scripting {

// --- Navigation version -----------------------------------------------------
// Translation-unit counter behind NavVersion.hpp. Bumped by the collider
// setters below when solidity/movability actually changes, so pathfinding
// scripts can detect when a door opens (or any obstacle appears/clears) and
// rebuild their grid instead of relying on the one built at onStart.
namespace {
    uint32_t g_navVersion = 0;
}
uint32_t navVersion() { return g_navVersion; }
void bumpNavVersion() { ++g_navVersion; }

// Identity

bool EntityRef::isValid() const {
  return m_world && m_world->isValid(m_entity);
}

// Transform

vec2 EntityRef::getPosition() const {
  if (!m_world)
    return {0.f, 0.f};
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->position : vec2{0.f, 0.f};
}

void EntityRef::setPosition(const vec2 &pos) {
  if (!m_world)
    return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t)
    t->position = pos;
}

float EntityRef::getRotation() const {
  if (!m_world)
    return 0.f;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->rotation : 0.f;
}

void EntityRef::setRotation(float rot) {
  if (!m_world)
    return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t)
    t->rotation = rot;
}

vec2 EntityRef::getScale() const {
  if (!m_world)
    return {1.f, 1.f};
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  return t ? t->scale : vec2{1.f, 1.f};
}

void EntityRef::setScale(const vec2 &scale) {
  if (!m_world)
    return;
  auto *t = m_world->getComponent<Components::Transform>(m_entity);
  if (t)
    t->scale = scale;
}

// Velocity

vec2 EntityRef::getVelocity() const {
  if (!m_world)
    return {0.f, 0.f};
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  return v ? v->velocity : vec2{0.f, 0.f};
}

void EntityRef::setVelocity(const vec2 &vel) {
  if (!m_world)
    return;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  if (v)
    v->velocity = vel;
}

float EntityRef::getMoveSpeed() const {
  if (!m_world)
    return 0.f;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  return v ? v->moveSpeed : 0.f;
}

void EntityRef::setMoveSpeed(float speed) {
  if (!m_world)
    return;
  auto *v = m_world->getComponent<Components::Velocity>(m_entity);
  if (v)
    v->moveSpeed = speed;
}

// Health

float EntityRef::getHp() const {
  if (!m_world)
    return 0.f;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  return h ? h->hp : 0.f;
}

void EntityRef::setHp(float hp) {
  if (!m_world)
    return;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  if (h)
    h->hp = hp;
}

float EntityRef::getMaxHp() const {
  if (!m_world)
    return 0.f;
  auto *h = m_world->getComponent<Components::Health>(m_entity);
  return h ? h->maxHp : 0.f;
}

bool EntityRef::isDead() const {
  if (!m_world)
    return false;
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

// --- Light ---
bool EntityRef::hasLight() const {
  return m_world && m_world->hasComponent<Components::Light>(m_entity);
}
bool EntityRef::isLightActive() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->active : false;
}
void EntityRef::setLightActive(bool v) {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  if (t)
    t->active = v;
}
float EntityRef::getLightRadius() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->radius : 0.f;
}
float EntityRef::getLightAngle() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->angle : 0.f;
}
float EntityRef::getLightOffsetX() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->offset.x : 0.f;
}
float EntityRef::getLightOffsetY() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->offset.y : 0.f;
}
float EntityRef::getLightIntensity() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  return t ? t->intensity : 1.0f;
}
void EntityRef::setLightIntensity(float i) {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  if (t)
    t->intensity = i;
}
void EntityRef::setLightOffsetX(float x) {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  if (t)
    t->offset.x = x;
}
void EntityRef::setLightOffsetY(float y) {
  auto *t =
      m_world ? m_world->getComponent<Components::Light>(m_entity) : nullptr;
  if (t)
    t->offset.y = y;
}

// --- TextRenderer ---
bool EntityRef::hasTextRenderer() const {
  return m_world && m_world->hasComponent<Components::TextRenderer>(m_entity);
}
std::string EntityRef::getText() const {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  return r ? r->text : "";
}
void EntityRef::setText(const std::string &t) {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  if (r)
    r->text = t;
}
bool EntityRef::isTextVisible() const {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  return r ? r->visible : false;
}
void EntityRef::setTextVisible(bool visible) {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  if (r)
    r->visible = visible;
}
float EntityRef::getTextFontSize() const {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  return r ? r->fontSize : 0.0f;
}
void EntityRef::setTextFontSize(float size) {
  auto *r = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                    : nullptr;
  if (r)
    r->fontSize = size;
}

// --- ParticleEmitter ---
bool EntityRef::hasParticleEmitter() const {
  return m_world &&
         m_world->hasComponent<Components::ParticleEmitter>(m_entity);
}
bool EntityRef::isParticleEmitterActive() const {
  auto *emitter =
      m_world ? m_world->getComponent<Components::ParticleEmitter>(m_entity)
              : nullptr;
  return emitter ? emitter->active() : false;
}
void EntityRef::setParticleEmitterActive(bool active) {
  auto *emitter =
      m_world ? m_world->getComponent<Components::ParticleEmitter>(m_entity)
              : nullptr;
  if (emitter)
    emitter->setActive(active);
}

void EntityRef::setTextColor(float r, float g, float b) {
  auto *tr = m_world ? m_world->getComponent<Components::TextRenderer>(m_entity)
                     : nullptr;
  if (tr)
    tr->color = glm::vec3(r, g, b);
}

// --- SpriteRenderer ---
bool EntityRef::hasSpriteRenderer() const {
  return m_world && m_world->hasComponent<Components::SpriteRenderer>(m_entity);
}
std::string EntityRef::getSpriteTextureId() const {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  return s ? s->textureId : "";
}
void EntityRef::setSpriteTextureId(const std::string &id) {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  if (s)
    s->textureId = id;
}
float EntityRef::getSpriteAlpha() const {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  return s ? s->color.a : 1.f;
}
void EntityRef::setSpriteAlpha(float a) {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  if (s)
    s->color.a = a;
}
bool EntityRef::isSpriteUnlit() const {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  return s ? s->unlit : false;
}
void EntityRef::setSpriteUnlit(bool v) {
  auto *s = m_world
                ? m_world->getComponent<Components::SpriteRenderer>(m_entity)
                : nullptr;
  if (s)
    s->unlit = v;
}

// --- Collider ---
bool EntityRef::hasCollider() const {
  return m_world && m_world->hasComponent<Components::Collider>(m_entity);
}
bool EntityRef::isColliderSolid() const {
  auto *c =
      m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr;
  return c ? c->isSolid : false;
}
void EntityRef::setColliderSolid(bool v) {
  auto *c =
      m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr;
  if (c && c->isSolid != v) {
    c->isSolid = v;
    // A wall/door became passable (or a new one appeared): invalidate nav grids.
    bumpNavVersion();
  }
}
bool EntityRef::isColliderMoveable() const {
  auto *c =
      m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr;
  return c ? c->moveable : false;
}
void EntityRef::setColliderMoveable(bool v) {
  auto *c =
      m_world ? m_world->getComponent<Components::Collider>(m_entity) : nullptr;
  if (c && c->moveable != v) {
    c->moveable = v;
    // Movability gates whether a collider counts as a wall (see eachWall):
    // a door made moveable on open must clear its grid cells. Only bump on an
    // actual change so door_system.
    bumpNavVersion();
  }
}

// --- Tag ---
bool EntityRef::hasTag() const {
  return m_world && m_world->hasComponent<Components::Tag>(m_entity);
}
std::string EntityRef::getTagValue() const {
  auto *t =
      m_world ? m_world->getComponent<Components::Tag>(m_entity) : nullptr;
  return t ? t->tag : "";
}

// --- SpriteAnimator ---
bool EntityRef::hasSpriteAnimator() const { return m_world && m_world->hasComponent<Components::SpriteAnimator>(m_entity); }
void EntityRef::playAnimation(const std::string& name, bool force) { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; if (a) a->play(name, force); }
void EntityRef::loadAnimationFile(const std::string& filename) { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; if (a && !filename.empty()) { a->loadFromPath("assets/animations/" + filename); a->filename = filename; } }
std::string EntityRef::getAnimationFile() const { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; return a ? a->filename : std::string{}; }
std::string EntityRef::getCurrentAnimationClip() const { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; return a ? a->currentClip : std::string{}; }
bool EntityRef::isAnimationPlaying() const { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; return a && a->playing; }
float EntityRef::getAnimationPlaybackSpeed() const { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; return a ? a->playbackSpeed : 1.f; }
void  EntityRef::setAnimationPlaybackSpeed(float s) { auto* a = m_world ? m_world->getComponent<Components::SpriteAnimator>(m_entity) : nullptr; if (a) a->playbackSpeed = s; }

// --- Generic script property access ---

bool EntityRef::hasScript(const std::string &scriptPath) const {
  auto *ls = m_world ? m_world->getComponent<Components::LuaScript>(m_entity)
                     : nullptr;
  if (!ls)
    return false;
  for (const auto &entry : ls->scripts)
    if (entry.scriptPath == scriptPath && entry.enabled)
      return true;
  return false;
}

Components::LuaScriptEntry *
EntityRef::findScriptEntry(const std::string &scriptPath) {
  auto *ls = m_world ? m_world->getComponent<Components::LuaScript>(m_entity)
                     : nullptr;
  if (!ls)
    return nullptr;
  for (auto &entry : ls->scripts)
    if (entry.scriptPath == scriptPath)
      return &entry;
  return nullptr;
}

const Components::LuaScriptEntry *
EntityRef::findScriptEntry(const std::string &scriptPath) const {
  auto *ls = m_world ? m_world->getComponent<Components::LuaScript>(m_entity)
                     : nullptr;
  if (!ls)
    return nullptr;
  for (const auto &entry : ls->scripts)
    if (entry.scriptPath == scriptPath)
      return &entry;
  return nullptr;
}

} // namespace Scripting
