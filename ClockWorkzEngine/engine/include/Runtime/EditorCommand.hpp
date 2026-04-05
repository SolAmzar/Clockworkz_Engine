/******************************************************************************/
/*!
\file           EditorCommand.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 97% [Amzar] 3%
\brief          Declares the engine-level undo/redo command system used by the
                editor, including snapshots for built-in ECS components.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Components/Tag.hpp"
#include "Components/Transform.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/AudioSource.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/Collider.hpp"
#include "Components/Layer.hpp"
#include "Components/Light.hpp"
#include "Components/LuaScript.hpp"
#include "Components/ParticleEmitter.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/UIButton.hpp"
#include "Components/UISlider.hpp"
#include "Components/Velocity.hpp"
#include "Components/Health.hpp"
#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Math/Vec2.hpp"
#include <memory>
#include <string>
#include <vector>

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

struct CompositeEditorCmd final : IEditorCommand {
    std::vector<std::unique_ptr<IEditorCommand>> commands;
    std::string                                  commandLabel;

    explicit CompositeEditorCmd(std::string label)
        : commandLabel(std::move(label)) {}

    void add(std::unique_ptr<IEditorCommand> cmd) {
        if (cmd) commands.push_back(std::move(cmd));
    }

    void execute() override;
    void undo() override;
    const char* label() const override { return commandLabel.c_str(); }
};

// ---------------------------------------------------------------------------
// EntitySnapshot — captures engine-level component state for undo/redo
// ---------------------------------------------------------------------------

struct EntitySnapshot {
    std::string               tagName{"Entity"};
    Components::Transform     transform{};

    bool hasSpriteRenderer{false};  Components::SpriteRenderer spriteRenderer{};
    bool hasAudioSource{false};     Components::AudioSource    audioSource{};
    bool hasCameraComponent{false}; Components::CameraComponent cameraComponent{};
    bool hasCollider{false};        Components::Collider        collider{};
    bool hasLayer{false};           Components::Layer           layer{};
    bool hasLight{false};           Components::Light           light{};
    bool hasLuaScript{false};       Components::LuaScript       luaScript{};
    bool hasParticleEmitter{false}; Components::ParticleEmitter particleEmitter{};
    bool hasSpriteAnimator{false};  Components::SpriteAnimator  spriteAnimator{};
    bool hasStateMachine{false};    Components::StateMachine    stateMachine{};
    bool hasTextRenderer{false};    Components::TextRenderer    textRenderer{};
    bool hasUIButton{false};        Components::UIButton        uiButton{};
    bool hasUISlider{false};        Components::UISlider        uiSlider{};
    bool hasVelocity{false};        Components::Velocity        velocity{};
    bool hasHealth{false};          Components::Health          health{};

    /// Capture all engine-level components from an entity.
    static EntitySnapshot capture(ECS::World& world, ECS::Entity entity);

    /// Recreate an entity from this snapshot in the given world.
    /// Returns the new entity handle (may differ from original ID).
    ECS::Entity restore(ECS::World& world) const;

    /// Apply snapshot data onto an EXISTING entity (in-place update).
    /// Adds/removes optional components to match the snapshot.
    void restoreInPlace(ECS::World& world, ECS::Entity entity) const;
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

// ---------------------------------------------------------------------------
// AddComponentCmd<T> — adds a default-constructed component; undo removes it
// ---------------------------------------------------------------------------

template<typename T>
struct AddComponentCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;

    AddComponentCmd(ECS::World& w, ECS::Entity e) : world(w), entity(e) {}

    void execute() override { world.addComponent(entity, T{}); }
    void undo()    override { world.removeComponent<T>(entity); }
    const char* label() const override { return "Add Component"; }
};

// ---------------------------------------------------------------------------
// RemoveComponentCmd<T> — captures component data, removes it; undo restores
// ---------------------------------------------------------------------------

template<typename T>
struct RemoveComponentCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;
    T           saved{};

    RemoveComponentCmd(ECS::World& w, ECS::Entity e) : world(w), entity(e) {
        if (auto* comp = w.getComponent<T>(e))
            saved = *comp;
    }

    void execute() override { world.removeComponent<T>(entity); }
    void undo()    override { world.addComponent(entity, saved); }
    const char* label() const override { return "Remove Component"; }
};

// ---------------------------------------------------------------------------
// ModifyComponentCmd<T> — records a before/after change to one component
// ---------------------------------------------------------------------------

template<typename T>
struct ModifyComponentCmd final : IEditorCommand {
    ECS::World& world;
    ECS::Entity entity;
    T           before{};
    T           after{};

    ModifyComponentCmd(ECS::World& w, ECS::Entity e, T b, T a)
        : world(w), entity(e), before(std::move(b)), after(std::move(a)) {}

    void execute() override { if (auto* c = world.getComponent<T>(entity)) *c = after; }
    void undo()    override { if (auto* c = world.getComponent<T>(entity)) *c = before; }
    const char* label() const override { return "Modify Component"; }
};

// ---------------------------------------------------------------------------
// ModifyEntityCmd — whole-entity snapshot diff for inspector drags
// ---------------------------------------------------------------------------

struct ModifyEntityCmd final : IEditorCommand {
    ECS::World&    world;
    ECS::Entity    entity;
    EntitySnapshot before;
    EntitySnapshot after;

    ModifyEntityCmd(ECS::World& w, ECS::Entity e, EntitySnapshot b, EntitySnapshot a)
        : world(w), entity(e), before(std::move(b)), after(std::move(a)) {}

    void execute() override;  ///< applies 'after' component state in-place
    void undo()    override;  ///< applies 'before' component state in-place
    const char* label() const override { return "Modify Entity"; }
};

} // namespace Runtime

