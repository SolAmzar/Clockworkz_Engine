/******************************************************************************/
/*!
\file           UndoableImGui.hpp
\brief          ImGui wrappers that automatically create undo commands
                Use these instead of raw ImGui calls to enable undo/redo
*/
/******************************************************************************/

#pragma once

#include "IMGUI/imgui.h"
#include "SandboxCommands.hpp"
#include <functional>
#include <cstring>

namespace Sandbox {

    // ============================================================================
    // UndoableImGui - Wrapper class for ImGui that creates undo commands
    // ============================================================================
    class UndoableImGui {
    public:
        using CommandCallback = std::function<void(std::unique_ptr<SandboxCommand>)>;

        UndoableImGui(ECS::World& world, ECS::Entity entity,
            CommandCallback executeCommand)
            : m_world(world), m_entity(entity), m_executeCommand(executeCommand) {
        }

        // ========================================================================
        // FLOAT INPUTS
        // ========================================================================

        template <typename ComponentType>
        bool InputFloat(const char* label, float ComponentType::* property,
            const char* commandName, float step = 0.0f,
            float step_fast = 0.0f, const char* format = "%.3f",
            ImGuiInputTextFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            float oldValue = comp->*property;
            float value = oldValue;

            if (ImGui::InputFloat(label, &value, step, step_fast, format, flags)) {
                // User is still typing, update immediately but don't create command yet
                comp->*property = value;
                return true;
            }

            // Check if value changed when user finished editing (lost focus or pressed
            // Enter)
            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                // Create undo command with the old and new values
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        template <typename ComponentType>
        bool InputFloat2(const char* label, vec2 ComponentType::* property,
            const char* commandName, const char* format = "%.3f",
            ImGuiInputTextFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            vec2 oldValue = comp->*property;
            vec2 value = oldValue;

            if (ImGui::InputFloat2(label, &value.x, format, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() &&
                (oldValue.x != value.x || oldValue.y != value.y)) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // DRAG FLOAT
        // ========================================================================

        template <typename ComponentType>
        bool DragFloat(const char* label, float ComponentType::* property,
            const char* commandName, float v_speed = 1.0f,
            float v_min = 0.0f, float v_max = 0.0f,
            const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            float oldValue = comp->*property;
            float value = oldValue;

            if (ImGui::DragFloat(label, &value, v_speed, v_min, v_max, format, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        template <typename ComponentType>
        bool DragFloat2(const char* label, vec2 ComponentType::* property,
            const char* commandName, float v_speed = 1.0f,
            float v_min = 0.0f, float v_max = 0.0f,
            const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            vec2 oldValue = comp->*property;
            vec2 value = oldValue;

            if (ImGui::DragFloat2(label, &value.x, v_speed, v_min, v_max, format,
                flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() &&
                (oldValue.x != value.x || oldValue.y != value.y)) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // SLIDER FLOAT
        // ========================================================================

        template <typename ComponentType>
        bool SliderFloat(const char* label, float ComponentType::* property,
            const char* commandName, float v_min, float v_max,
            const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            float oldValue = comp->*property;
            float value = oldValue;

            if (ImGui::SliderFloat(label, &value, v_min, v_max, format, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // CHECKBOX
        // ========================================================================

        template <typename ComponentType>
        bool Checkbox(const char* label, bool ComponentType::* property,
            const char* commandName) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            bool oldValue = comp->*property;
            bool value = oldValue;

            if (ImGui::Checkbox(label, &value)) {
                comp->*property = value;
                // Create command immediately for checkboxes
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // INPUT INT
        // ========================================================================

        template <typename ComponentType>
        bool InputInt(const char* label, int ComponentType::* property,
            const char* commandName, int step = 1, int step_fast = 100,
            ImGuiInputTextFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            int oldValue = comp->*property;
            int value = oldValue;

            if (ImGui::InputInt(label, &value, step, step_fast, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // COLOR EDIT
        // ========================================================================

        template <typename ComponentType>
        bool ColorEdit4(const char* label, glm::vec4 ComponentType::* property,
            const char* commandName, ImGuiColorEditFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            glm::vec4 oldValue = comp->*property;
            glm::vec4 value = oldValue;

            if (ImGui::ColorEdit4(label, &value.r, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        template <typename ComponentType>
        bool ColorEdit3(const char* label, glm::vec3 ComponentType::* property,
            const char* commandName, ImGuiColorEditFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            glm::vec3 oldValue = comp->*property;
            glm::vec3 value = oldValue;

            if (ImGui::ColorEdit3(label, &value.r, flags)) {
                comp->*property = value;
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() && oldValue != value) {
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    value, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // COMBO (for enums or selections)
        // ========================================================================

        template <typename ComponentType, typename EnumType>
        bool Combo(const char* label, EnumType ComponentType::* property,
            const char* commandName, const char* items[], int items_count) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            EnumType oldValue = comp->*property;
            int currentItem = static_cast<int>(oldValue);

            if (ImGui::Combo(label, &currentItem, items, items_count)) {
                EnumType newValue = static_cast<EnumType>(currentItem);
                comp->*property = newValue;
                // Create command immediately
                auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                    newValue, commandName);
                m_executeCommand(std::move(cmd));
                return true;
            }

            return false;
        }

        // ========================================================================
        // INPUT TEXT (for strings)
        // ========================================================================

        template <typename ComponentType>
        bool InputText(const char* label, std::string ComponentType::* property,
            const char* commandName, size_t buffer_size = 256,
            ImGuiInputTextFlags flags = 0) {
            auto* comp = m_world.getComponent<ComponentType>(m_entity);
            if (!comp)
                return false;

            std::string oldValue = comp->*property;
            static char buffer[256];

            // Portable string copy
#ifdef _MSC_VER
            strncpy_s(buffer, sizeof(buffer), (comp->*property).c_str(), sizeof(buffer) - 1);
#else
            strncpy(buffer, (comp->*property).c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
#endif

            if (ImGui::InputText(label, buffer, buffer_size, flags)) {
                comp->*property = std::string(buffer);
                return true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string newValue = std::string(buffer);
                if (oldValue != newValue) {
                    auto cmd = makePropertyCommand(m_world, m_entity, property, oldValue,
                        newValue, commandName);
                    m_executeCommand(std::move(cmd));
                    return true;
                }
            }

            return false;
        }

    private:
        ECS::World& m_world;
        ECS::Entity m_entity;
        CommandCallback m_executeCommand;
    };

} // namespace Sandbox