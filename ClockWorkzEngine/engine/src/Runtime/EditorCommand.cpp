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
    if (auto* sr = world.getComponent<Components::SpriteRenderer>(entity)) {
        snap.hasSpriteRenderer = true;
        snap.spriteRenderer = *sr;
    }
    return snap;
}

ECS::Entity EntitySnapshot::restore(ECS::World& world) const {
    ECS::Entity e = world.createEntity();
    world.addComponent(e, Components::Tag{tagName});
    world.addComponent(e, transform);
    if (hasSpriteRenderer)
        world.addComponent(e, spriteRenderer);
    return e;
}

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

