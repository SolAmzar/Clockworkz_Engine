/******************************************************************************/
/*!
\file           EditorCommand.hpp
\project        ClockWorkz Engine
\author         [Syafiq]
\brief          Engine-level undo/redo command system for EditorState.
                Only references engine-level components (Tag, Transform,
                SpriteRenderer). Game-specific components are excluded.

All content (C) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/SpriteRenderer.hpp"
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"
#include <string>

namespace Runtime {

// ---------------------------------------------------------------------------
// Base command interface
// ---------------------------------------------------------------------------

struct IEditorCommand {
    virtual ~IEditorCommand() = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;
    virtual const char* label() const = 0;
};

// ---------------------------------------------------------------------------
// EntitySnapshot — captures engine-level component state for undo/redo
// ---------------------------------------------------------------------------

struct EntitySnapshot {
    std::string               tagName{"Entity"};
    Components::Transform     transform{};
    bool                      hasSpriteRenderer{false};
    Components::SpriteRenderer spriteRenderer{};

    /// Capture all engine-level components from an entity.
    static EntitySnapshot capture(ECS::World& world, ECS::Entity entity);

    /// Recreate an entity from this snapshot in the given world.
    /// Returns the new entity handle (may differ from original ID).
    ECS::Entity restore(ECS::World& world) const;
};

// ---------------------------------------------------------------------------
// MoveEntityCmd — records a position change (from gizmo or inspector)
// ---------------------------------------------------------------------------

struct MoveEntityCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;
    vec2        oldPos;
    vec2        newPos;

    MoveEntityCmd(ECS::World& w, ECS::Entity e, vec2 old_, vec2 new_)
        : world(w), entity(e), oldPos(old_), newPos(new_) {}

    void execute() override;
    void undo()    override;
    const char* label() const override { return "Move Entity"; }
};

// ---------------------------------------------------------------------------
// RotateEntityCmd — records a rotation change
// ---------------------------------------------------------------------------

struct RotateEntityCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;
    float       oldRot;
    float       newRot;

    RotateEntityCmd(ECS::World& w, ECS::Entity e, float old_, float new_)
        : world(w), entity(e), oldRot(old_), newRot(new_) {}

    void execute() override;
    void undo()    override;
    const char* label() const override { return "Rotate Entity"; }
};

// ---------------------------------------------------------------------------
// ScaleEntityCmd — records a scale change (from gizmo or inspector)
// ---------------------------------------------------------------------------

struct ScaleEntityCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;
    vec2        oldScale;
    vec2        newScale;

    ScaleEntityCmd(ECS::World& w, ECS::Entity e, vec2 old_, vec2 new_)
        : world(w), entity(e), oldScale(old_), newScale(new_) {}

    void execute() override;
    void undo()    override;
    const char* label() const override { return "Scale Entity"; }
};

// ---------------------------------------------------------------------------
// CreateEntityCmd — creates an entity from a snapshot; undo destroys it
// ---------------------------------------------------------------------------

struct CreateEntityCmd final : IEditorCommand {
    ECS::World&    world;
    EntitySnapshot snapshot;
    ECS::Entity    m_current{0, 0}; ///< tracks most recently created entity

    CreateEntityCmd(ECS::World& w, EntitySnapshot snap)
        : world(w), snapshot(std::move(snap)) {}

    void execute() override;  ///< creates entity from snapshot
    void undo()    override;  ///< destroys the created entity
    const char* label() const override { return "Create Entity"; }

    /// Returns the entity created by the most recent execute().
    ECS::Entity createdEntity() const { return m_current; }
};

// ---------------------------------------------------------------------------
// DeleteEntityCmd — destroys an entity; undo restores it from snapshot
// ---------------------------------------------------------------------------

struct DeleteEntityCmd final : IEditorCommand {
    ECS::World&    world;
    EntitySnapshot snapshot;
    ECS::Entity    m_current; ///< Changes on each execute/undo cycle

    /// Snapshot is captured at construction time.
    DeleteEntityCmd(ECS::World& w, ECS::Entity entity)
        : world(w), m_current(entity) {
        snapshot = EntitySnapshot::capture(w, entity);
    }

    void execute() override;  ///< destroys m_current
    void undo()    override;  ///< restores from snapshot → new m_current
    const char* label() const override { return "Delete Entity"; }
};

} // namespace Runtime
