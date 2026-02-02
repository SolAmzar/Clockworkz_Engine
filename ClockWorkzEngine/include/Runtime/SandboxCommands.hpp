/******************************************************************************/
/*!
\file           SandboxCommands.hpp
\brief          Command pattern implementation for undo/redo in SandboxState
                Captures ALL property modifications for complete undo/redo support

                Uses SandboxCommand (not EditorCommand) to avoid conflicts with LevelEditor
*/
/******************************************************************************/

#pragma once

// Include guards to prevent GL header conflicts
#ifndef SANDBOXCOMMANDS_INCLUDED
#define SANDBOXCOMMANDS_INCLUDED

#include "Components/AudioSource.hpp"
#include "Components/Battery.hpp"
#include "Components/Collider.hpp"
#include "Components/EnemyAI.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Health.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Torch.hpp"
#include "Components/Transform.hpp"
#include "ECS/World.hpp"
#include "Runtime/Scene.hpp"
#include "Math/Vec2.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Sandbox {  // Changed from Platform to avoid conflicts

    // ============================================================================
    // Base Command Interface - RENAMED to avoid LevelEditor conflicts
    // ============================================================================
    class SandboxCommand {
    public:
        virtual ~SandboxCommand() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
        virtual std::string getName() const = 0;
    };

    // ============================================================================
    // Entity Snapshot - Captures full entity state for restoration
    // ============================================================================
    struct EntitySnapshot {
        // Transform (always present)
        Components::Transform transform;

        // Optional components
        bool hasVelocity = false;
        Components::Velocity velocity;

        bool hasPlayerController = false;
        Components::PlayerController playerController;

        bool hasSprite = false;
        Components::SpriteRenderer sprite;

        bool hasCollider = false;
        Components::Collider collider;

        bool hasHealth = false;
        Components::Health health;

        bool hasBattery = false;
        Components::Battery battery;

        bool hasLightExposure = false;
        Components::LightExposure lightExposure;

        bool hasFragment = false;
        Components::Fragment fragment;

        bool hasFragmentCollector = false;
        Components::FragmentCollector fragmentCollector;

        bool hasEnemyAI = false;
        Components::EnemyAI enemyAI;

        bool hasAudioSource = false;
        Components::AudioSource audioSource;

        bool hasSpriteAnimator = false;
        Components::SpriteAnimator spriteAnimator;

        bool hasTorch = false;
        Components::TorchComponent torch;

        void captureFrom(ECS::World& world, ECS::Entity entity) {
            // Capture Transform
            if (auto* t = world.getComponent<Components::Transform>(entity)) {
                transform = *t;
            }

            // Capture all optional components
            if (auto* v = world.getComponent<Components::Velocity>(entity)) {
                hasVelocity = true;
                velocity = *v;
            }
            if (auto* pc = world.getComponent<Components::PlayerController>(entity)) {
                hasPlayerController = true;
                playerController = *pc;
            }
            if (auto* s = world.getComponent<Components::SpriteRenderer>(entity)) {
                hasSprite = true;
                sprite = *s;
            }
            if (auto* c = world.getComponent<Components::Collider>(entity)) {
                hasCollider = true;
                collider = *c;
            }
            if (auto* h = world.getComponent<Components::Health>(entity)) {
                hasHealth = true;
                health = *h;
            }
            if (auto* b = world.getComponent<Components::Battery>(entity)) {
                hasBattery = true;
                battery = *b;
            }
            if (auto* l = world.getComponent<Components::LightExposure>(entity)) {
                hasLightExposure = true;
                lightExposure = *l;
            }
            if (auto* f = world.getComponent<Components::Fragment>(entity)) {
                hasFragment = true;
                fragment = *f;
            }
            if (auto* fc = world.getComponent<Components::FragmentCollector>(entity)) {
                hasFragmentCollector = true;
                fragmentCollector = *fc;
            }
            if (auto* ai = world.getComponent<Components::EnemyAI>(entity)) {
                hasEnemyAI = true;
                enemyAI = *ai;
            }
            if (auto* a = world.getComponent<Components::AudioSource>(entity)) {
                hasAudioSource = true;
                audioSource = *a;
            }
            if (auto* anim = world.getComponent<Components::SpriteAnimator>(entity)) {
                hasSpriteAnimator = true;
                spriteAnimator = *anim;
            }
            if (auto* torch = world.getComponent<Components::TorchComponent>(entity)) {
                hasTorch = true;
                this->torch = *torch; // Dereference the pointer
            }
        }

        void restoreTo(ECS::World& world, ECS::Entity entity) {
            // Restore Transform
            if (auto* t = world.getComponent<Components::Transform>(entity)) {
                *t = transform;
            } else {
                world.addComponent(entity, transform);
            }

            // Restore optional components
            if (hasVelocity) {
                if (auto* v = world.getComponent<Components::Velocity>(entity)) {
                    *v = velocity;
                }
                else {
                    world.addComponent(entity, velocity);
                }
            }
            if (hasPlayerController) {
                if (auto* pc = world.getComponent<Components::PlayerController>(entity)) {
                    *pc = playerController;
                }
                else {
                    world.addComponent(entity, playerController);
                }
            }
            if (hasSprite) {
                if (auto* s = world.getComponent<Components::SpriteRenderer>(entity)) {
                    *s = sprite;
                }
                else {
                    world.addComponent(entity, sprite);
                }
            }
            if (hasCollider) {
                if (auto* c = world.getComponent<Components::Collider>(entity)) {
                    *c = collider;
                }
                else {
                    world.addComponent(entity, collider);
                }
            }
            if (hasHealth) {
                if (auto* h = world.getComponent<Components::Health>(entity)) {
                    *h = health;
                }
                else {
                    world.addComponent(entity, health);
                }
            }
            if (hasBattery) {
                if (auto* b = world.getComponent<Components::Battery>(entity)) {
                    *b = battery;
                }
                else {
                    world.addComponent(entity, battery);
                }
            }
            if (hasLightExposure) {
                if (auto* l = world.getComponent<Components::LightExposure>(entity)) {
                    *l = lightExposure;
                }
                else {
                    world.addComponent(entity, lightExposure);
                }
            }
            if (hasFragment) {
                if (auto* f = world.getComponent<Components::Fragment>(entity)) {
                    *f = fragment;
                }
                else {
                    world.addComponent(entity, fragment);
                }
            }
            if (hasFragmentCollector) {
                if (auto* fc =
                    world.getComponent<Components::FragmentCollector>(entity)) {
                    *fc = fragmentCollector;
                }
                else {
                    world.addComponent(entity, fragmentCollector);
                }
            }
            if (hasEnemyAI) {
                if (auto* ai = world.getComponent<Components::EnemyAI>(entity)) {
                    *ai = enemyAI;
                }
                else {
                    world.addComponent(entity, enemyAI);
                }
            }
            if (hasAudioSource) {
                if (auto* a = world.getComponent<Components::AudioSource>(entity)) {
                    *a = audioSource;
                }
                else {
                    world.addComponent(entity, audioSource);
                }
            }
            if (hasSpriteAnimator) {
                if (auto* anim = world.getComponent<Components::SpriteAnimator>(entity)) {
                    *anim = spriteAnimator;
                }
                else {
                    world.addComponent(entity, spriteAnimator);
                }
            }
            if (hasTorch) {
                if (auto* torch =
                    world.getComponent<Components::TorchComponent>(entity)) {
                    *torch = this->torch;
                }
                else {
                    world.addComponent(entity, this->torch);
                }
            }
        }
    };

    // ============================================================================
    // ENTITY COMMANDS
    // ============================================================================

    // Create Entity Command
    class CreateEntityCommand : public SandboxCommand {
    private:
        Runtime::Scene& m_scene;
        std::vector<ECS::Entity>& m_entityList;
        ECS::Entity m_entity;
        EntitySnapshot m_snapshot;
        bool m_wasExecuted = false;

    public:
        CreateEntityCommand(Runtime::Scene& scene,
            std::vector<ECS::Entity>& entityList, ECS::Entity entity)
            : m_scene(scene), m_entityList(entityList), m_entity(entity) {
            // Capture initial state
            m_snapshot.captureFrom(m_scene.GetWorld(), entity);
        }

        void execute() override {
            if (!m_wasExecuted) {
                // First time: entity was already created, just add to tracked list
                m_entityList.push_back(m_entity);
                m_wasExecuted = true;
            } else {
                // Redo: recreate entity from snapshot
                m_entity = m_scene.GetWorld().createEntity();
                m_snapshot.restoreTo(m_scene.GetWorld(), m_entity);
                m_entityList.push_back(m_entity);
            }
        }

        void undo() override {
            // Re-capture snapshot so redo restores latest state
            m_snapshot.captureFrom(m_scene.GetWorld(), m_entity);
            // Remove from list
            auto it = std::find(m_entityList.begin(), m_entityList.end(), m_entity);
            if (it != m_entityList.end()) {
                m_entityList.erase(it);
            }
            // Destroy entity
            m_scene.GetWorld().destroyEntity(m_entity);
        }

        std::string getName() const override { return "Create Entity"; }
    };

    // Delete Entity Command
    class DeleteEntityCommand : public SandboxCommand {
    private:
        Runtime::Scene& m_scene;
        std::vector<ECS::Entity>& m_entityList;
        ECS::Entity m_entity;
        size_t m_originalIndex;
        EntitySnapshot m_snapshot;
        bool m_wasExecuted = false;

    public:
        DeleteEntityCommand(Runtime::Scene& scene,
            std::vector<ECS::Entity>& entityList, ECS::Entity entity,
            size_t index)
            : m_scene(scene), m_entityList(entityList), m_entity(entity),
            m_originalIndex(index) {
            // Capture state before deletion
            m_snapshot.captureFrom(m_scene.GetWorld(), entity);
        }

        void execute() override {
            if (!m_wasExecuted) {
                // Remove from list
                m_entityList.erase(m_entityList.begin() + m_originalIndex);
                // Destroy entity
                m_scene.GetWorld().destroyEntity(m_entity);
                m_wasExecuted = true;
            }
        }

        void undo() override {
            // Recreate entity
            m_entity = m_scene.GetWorld().createEntity();
            // Restore all components
            m_snapshot.restoreTo(m_scene.GetWorld(), m_entity);
            // Reinsert at original position
            if (m_originalIndex >= m_entityList.size()) {
                m_entityList.push_back(m_entity);
            }
            else {
                m_entityList.insert(m_entityList.begin() + m_originalIndex, m_entity);
            }
            m_wasExecuted = false;
        }

        std::string getName() const override { return "Delete Entity"; }
    };

    // Clone Entity Command
    class CloneEntityCommand : public SandboxCommand {
    private:
        Runtime::Scene& m_scene;
        std::vector<ECS::Entity>& m_entityList;
        ECS::Entity m_clonedEntity;
        EntitySnapshot m_snapshot;
        bool m_wasExecuted = false;

    public:
        CloneEntityCommand(Runtime::Scene& scene,
            std::vector<ECS::Entity>& entityList,
            ECS::Entity sourceEntity)
            : m_scene(scene), m_entityList(entityList) {
            // Capture source entity state
            m_snapshot.captureFrom(m_scene.GetWorld(), sourceEntity);
            // Offset position slightly
            m_snapshot.transform.position.x += 50.0f;
            m_snapshot.transform.position.y += 50.0f;
        }

        void execute() override {
            // Create (or recreate on redo) the cloned entity from snapshot
            m_clonedEntity = m_scene.GetWorld().createEntity();
            m_snapshot.restoreTo(m_scene.GetWorld(), m_clonedEntity);
            m_entityList.push_back(m_clonedEntity);
        }

        void undo() override {
            // Remove from list
            auto it =
                std::find(m_entityList.begin(), m_entityList.end(), m_clonedEntity);
            if (it != m_entityList.end()) {
                m_entityList.erase(it);
            }
            // Destroy cloned entity
            m_scene.GetWorld().destroyEntity(m_clonedEntity);
        }

        std::string getName() const override { return "Clone Entity"; }
    };

    // ============================================================================
    // COMPONENT PROPERTY CHANGE COMMANDS
    // ============================================================================

    // Generic Property Change Command Template
    template <typename ComponentType, typename PropertyType>
    class PropertyChangeCommand : public SandboxCommand {
    private:
        ECS::World& m_world;
        ECS::Entity m_entity;
        PropertyType ComponentType::* m_property;
        PropertyType m_oldValue;
        PropertyType m_newValue;
        std::string m_name;

    public:
        PropertyChangeCommand(ECS::World& world, ECS::Entity entity,
            PropertyType ComponentType::* property,
            const PropertyType& oldValue,
            const PropertyType& newValue, const std::string& name)
            : m_world(world), m_entity(entity), m_property(property),
            m_oldValue(oldValue), m_newValue(newValue), m_name(name) {
        }

        void execute() override {
            if (auto* comp = m_world.getComponent<ComponentType>(m_entity)) {
                comp->*m_property = m_newValue;
            }
        }

        void undo() override {
            if (auto* comp = m_world.getComponent<ComponentType>(m_entity)) {
                comp->*m_property = m_oldValue;
            }
        }

        std::string getName() const override { return m_name; }
    };

    // ============================================================================
    // ADD/REMOVE COMPONENT COMMANDS
    // ============================================================================

    // Add Component Command
    template <typename ComponentType>
    class AddComponentCommand : public SandboxCommand {
    private:
        ECS::World& m_world;
        ECS::Entity m_entity;
        ComponentType m_component;
        std::string m_name;

    public:
        AddComponentCommand(ECS::World& world, ECS::Entity entity,
            const ComponentType& component, const std::string& name)
            : m_world(world), m_entity(entity), m_component(component), m_name(name) {
        }

        void execute() override { m_world.addComponent(m_entity, m_component); }

        void undo() override { m_world.removeComponent<ComponentType>(m_entity); }

        std::string getName() const override { return "Add " + m_name; }
    };

    // Remove Component Command
    template <typename ComponentType>
    class RemoveComponentCommand : public SandboxCommand {
    private:
        ECS::World& m_world;
        ECS::Entity m_entity;
        ComponentType m_savedComponent;
        std::string m_name;

    public:
        RemoveComponentCommand(ECS::World& world, ECS::Entity entity,
            const std::string& name)
            : m_world(world), m_entity(entity), m_name(name) {
            // Capture component before removal
            if (auto* comp = m_world.getComponent<ComponentType>(entity)) {
                m_savedComponent = *comp;
            }
        }

        void execute() override { m_world.removeComponent<ComponentType>(m_entity); }

        void undo() override { m_world.addComponent(m_entity, m_savedComponent); }

        std::string getName() const override { return "Remove " + m_name; }
    };

    // ============================================================================
    // HELPER FUNCTIONS FOR CREATING COMMANDS
    // ============================================================================

    // Helper to create property change commands easily
    template <typename ComponentType, typename PropertyType>
    std::unique_ptr<SandboxCommand>
        makePropertyCommand(ECS::World& world, ECS::Entity entity,
            PropertyType ComponentType::* property,
            const PropertyType& oldValue, const PropertyType& newValue,
            const std::string& name) {
        return std::make_unique<PropertyChangeCommand<ComponentType, PropertyType>>(
            world, entity, property, oldValue, newValue, name);
    }

} // namespace Sandbox
#endif // SANDBOXCOMMANDS_INCLUDED