/******************************************************************************/
/*!
\file           EditorCommand.cpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Implementation of engine-level editor undo/redo commands.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Runtime/EditorCommand.hpp"

namespace Runtime {

// ---------------------------------------------------------------------------
// EntitySnapshot
// ---------------------------------------------------------------------------

EntitySnapshot EntitySnapshot::capture(ECS::World& world, ECS::Entity entity) {
    EntitySnapshot snap;
    if (auto* tag = world.getComponent<Components::Tag>(entity))
        snap.tagName = tag->tag;
    if (auto* t = world.getComponent<Components::Transform>(entity))
        snap.transform = *t;
    if (auto* c = world.getComponent<Components::SpriteRenderer>(entity))
        { snap.hasSpriteRenderer = true; snap.spriteRenderer = *c; }
    if (auto* c = world.getComponent<Components::AudioSource>(entity))
        { snap.hasAudioSource = true; snap.audioSource = *c; }
    if (auto* c = world.getComponent<Components::CameraComponent>(entity))
        { snap.hasCameraComponent = true; snap.cameraComponent = *c; }
    if (auto* c = world.getComponent<Components::Collider>(entity))
        { snap.hasCollider = true; snap.collider = *c; }
    if (auto* c = world.getComponent<Components::Layer>(entity))
        { snap.hasLayer = true; snap.layer = *c; }
    if (auto* c = world.getComponent<Components::Light>(entity))
        { snap.hasLight = true; snap.light = *c; }
    if (auto* c = world.getComponent<Components::LuaScript>(entity))
        { snap.hasLuaScript = true; snap.luaScript = *c; }
    if (auto* c = world.getComponent<Components::SpriteAnimator>(entity))
        { snap.hasSpriteAnimator = true; snap.spriteAnimator = *c; }
    if (auto* c = world.getComponent<Components::StateMachine>(entity))
        { snap.hasStateMachine = true; snap.stateMachine = *c; }
    if (auto* c = world.getComponent<Components::TextRenderer>(entity))
        { snap.hasTextRenderer = true; snap.textRenderer = *c; }
    if (auto* c = world.getComponent<Components::UIButton>(entity))
        { snap.hasUIButton = true; snap.uiButton = *c; }
    if (auto* c = world.getComponent<Components::UISlider>(entity))
        { snap.hasUISlider = true; snap.uiSlider = *c; }
    if (auto* c = world.getComponent<Components::Velocity>(entity))
        { snap.hasVelocity = true; snap.velocity = *c; }
    if (auto* c = world.getComponent<Components::Health>(entity))
        { snap.hasHealth = true; snap.health = *c; }
    return snap;
}

ECS::Entity EntitySnapshot::restore(ECS::World& world) const {
    ECS::Entity e = world.createEntity();
    world.addComponent(e, Components::Tag{tagName});
    world.addComponent(e, transform);
    if (hasSpriteRenderer)   world.addComponent(e, spriteRenderer);
    if (hasAudioSource)      world.addComponent(e, audioSource);
    if (hasCameraComponent)  world.addComponent(e, cameraComponent);
    if (hasCollider)         world.addComponent(e, collider);
    if (hasLayer)            world.addComponent(e, layer);
    if (hasLight)            world.addComponent(e, light);
    if (hasLuaScript)        world.addComponent(e, luaScript);
    if (hasSpriteAnimator)   world.addComponent(e, spriteAnimator);
    if (hasStateMachine)     world.addComponent(e, stateMachine);
    if (hasTextRenderer)     world.addComponent(e, textRenderer);
    if (hasUIButton)         world.addComponent(e, uiButton);
    if (hasUISlider)         world.addComponent(e, uiSlider);
    if (hasVelocity)         world.addComponent(e, velocity);
    if (hasHealth)           world.addComponent(e, health);
    return e;
}

void EntitySnapshot::restoreInPlace(ECS::World& world, ECS::Entity entity) const {
    if (!world.isValid(entity)) return;

    // Always-present components
    if (auto* tag = world.getComponent<Components::Tag>(entity))       tag->tag = tagName;
    if (auto* t   = world.getComponent<Components::Transform>(entity)) *t = transform;

    // Helper: update existing, add if missing, remove if snapshot doesn't have it
    auto apply = [&](auto has, const auto& data) {
        using T = std::decay_t<decltype(data)>;
        if (has) {
            if (auto* c = world.getComponent<T>(entity)) *c = data;
            else                                          world.addComponent(entity, data);
        } else {
            if (world.hasComponent<T>(entity)) world.removeComponent<T>(entity);
        }
    };

    apply(hasSpriteRenderer,   spriteRenderer);
    apply(hasAudioSource,      audioSource);
    apply(hasCameraComponent,  cameraComponent);
    apply(hasCollider,         collider);
    apply(hasLayer,            layer);
    apply(hasLight,            light);
    apply(hasLuaScript,        luaScript);
    apply(hasSpriteAnimator,   spriteAnimator);
    apply(hasStateMachine,     stateMachine);
    apply(hasTextRenderer,     textRenderer);
    apply(hasUIButton,         uiButton);
    apply(hasUISlider,         uiSlider);
    apply(hasVelocity,         velocity);
    apply(hasHealth,           health);
}

// ---------------------------------------------------------------------------
// ModifyEntityCmd
// ---------------------------------------------------------------------------

void ModifyEntityCmd::execute() { after.restoreInPlace(world, entity); }
void ModifyEntityCmd::undo()    { before.restoreInPlace(world, entity); }

// ---------------------------------------------------------------------------
// MoveEntityCmd
// ---------------------------------------------------------------------------

void MoveEntityCmd::execute() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->position = newPos;
}

void MoveEntityCmd::undo() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->position = oldPos;
}

// ---------------------------------------------------------------------------
// RotateEntityCmd
// ---------------------------------------------------------------------------

void RotateEntityCmd::execute() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->rotation = newRot;
}

void RotateEntityCmd::undo() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->rotation = oldRot;
}

// ---------------------------------------------------------------------------
// ScaleEntityCmd
// ---------------------------------------------------------------------------

void ScaleEntityCmd::execute() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->scale = newScale;
}

void ScaleEntityCmd::undo() {
    if (auto* t = world.getComponent<Components::Transform>(entity))
        t->scale = oldScale;
}

// ---------------------------------------------------------------------------
// CreateEntityCmd
// ---------------------------------------------------------------------------

void CreateEntityCmd::execute() {
    m_current = snapshot.restore(world);
}

void CreateEntityCmd::undo() {
    world.destroyEntity(m_current);
    m_current = {0, 0};
}

// ---------------------------------------------------------------------------
// DeleteEntityCmd
// ---------------------------------------------------------------------------

void DeleteEntityCmd::execute() {
    world.destroyEntity(m_current);
    m_current = {0, 0};
}

void DeleteEntityCmd::undo() {
    m_current = snapshot.restore(world);
}

} // namespace Runtime
