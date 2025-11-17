/******************************************************************************/
/*!
\file           UndoRedoSystem.hpp
\project        Lights Out - Level Editor
\brief          Command pattern implementation for undo/redo functionality
                Supports up to 20 undo operations

                Features:
                - Undo/Redo with Ctrl+Z / Ctrl+Y
                - Tracks up to 20 changes
                - Supports multiple operation types (move, rotate, scale, delete)
                - Memory efficient

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Math/Vec2.hpp"
#include "Gameplay/GameObject.hpp"
#include "Runtime/Scene.hpp"

namespace Gameplay {
    class GameObject;
}

namespace Platform {

    /******************************************************************************/
    /*!
    \class  EditorCommand
    \brief  Base class for all undoable commands (Command Pattern)
    */
    /******************************************************************************/
    class EditorCommand {
    public:
        virtual ~EditorCommand() = default;

        virtual void execute() = 0;      // Apply the change
        virtual void undo() = 0;         // Reverse the change
        virtual std::string getName() const = 0;  // For UI display
    };

    /******************************************************************************/
    /*!
    \class  MoveCommand
    \brief  Command for moving objects
    */
    /******************************************************************************/
    class MoveCommand : public EditorCommand {
    public:
        MoveCommand(Gameplay::GameObject* obj, vec2 oldPos, vec2 newPos)
            : m_object(obj)
            , m_oldPosition(oldPos)
            , m_newPosition(newPos)
        {
        }

        void execute() override {
            if (m_object) {
                m_object->transform().position = m_newPosition;
            }
        }

        void undo() override {
            if (m_object) {
                m_object->transform().position = m_oldPosition;
            }
        }

        std::string getName() const override {
            return "Move Object";
        }

    private:
        Gameplay::GameObject* m_object;
        vec2 m_oldPosition;
        vec2 m_newPosition;
    };

    /******************************************************************************/
    /*!
    \class  RotateCommand
    \brief  Command for rotating objects
    */
    /******************************************************************************/
    class RotateCommand : public EditorCommand {
    public:
        RotateCommand(Gameplay::GameObject* obj, float oldRot, float newRot)
            : m_object(obj)
            , m_oldRotation(oldRot)
            , m_newRotation(newRot)
        {
        }

        void execute() override {
            if (m_object) {
                m_object->transform().rotationRad = m_newRotation;
            }
        }

        void undo() override {
            if (m_object) {
                m_object->transform().rotationRad = m_oldRotation;
            }
        }

        std::string getName() const override {
            return "Rotate Object";
        }

    private:
        Gameplay::GameObject* m_object;
        float m_oldRotation;
        float m_newRotation;
    };

    /******************************************************************************/
    /*!
    \class  ScaleCommand
    \brief  Command for scaling objects
    */
    /******************************************************************************/
    class ScaleCommand : public EditorCommand {
    public:
        ScaleCommand(Gameplay::GameObject* obj, vec2 oldScale, vec2 newScale)
            : m_object(obj)
            , m_oldScale(oldScale)
            , m_newScale(newScale)
        {
        }

        void execute() override {
            if (m_object) {
                m_object->transform().scale = m_newScale;
            }
        }

        void undo() override {
            if (m_object) {
                m_object->transform().scale = m_oldScale;
            }
        }

        std::string getName() const override {
            return "Scale Object";
        }

    private:
        Gameplay::GameObject* m_object;
        vec2 m_oldScale;
        vec2 m_newScale;
    };

    /******************************************************************************/
    /*!
    \class  DeleteCommand
    \brief  Command for deleting objects
    */
    /******************************************************************************/
    class DeleteCommand : public EditorCommand {
    public:
        DeleteCommand(Gameplay::GameObject* obj, Runtime::Scene* scene)
            : m_object(obj)
            , m_scene(scene)
            , m_wasDeleted(false)
        {
        }

        void execute() override {
            if (m_object && !m_wasDeleted) {
                m_object->setActive(false);  // Soft delete
                m_wasDeleted = true;
            }
        }

        void undo() override {
            if (m_object && m_wasDeleted) {
                m_object->setActive(true);  // Restore
                m_wasDeleted = false;
            }
        }

        std::string getName() const override {
            return "Delete Object";
        }

    private:
        Gameplay::GameObject* m_object;
        Runtime::Scene* m_scene;
        bool m_wasDeleted;
    };

    /******************************************************************************/
    /*!
    \class  CreateCommand
    \brief  Command for creating new objects
    */
    /******************************************************************************/
    class CreateCommand : public EditorCommand {
    public:
        CreateCommand(Gameplay::GameObject* obj, Runtime::Scene* scene)
            : m_object(obj)
            , m_scene(scene)
            , m_wasCreated(false)
        {
        }

        void execute() override {
            if (m_object && !m_wasCreated) {
                m_object->setActive(true);
                m_wasCreated = true;
            }
        }

        void undo() override {
            if (m_object && m_wasCreated) {
                m_object->setActive(false);
                m_wasCreated = false;
            }
        }

        std::string getName() const override {
            return "Create Object";
        }

    private:
        Gameplay::GameObject* m_object;
        Runtime::Scene* m_scene;
        bool m_wasCreated;
    };

    /******************************************************************************/
    /*!
    \class  PropertyChangeCommand
    \brief  Command for changing object properties (name, texture, etc.)
    */
    /******************************************************************************/
    class PropertyChangeCommand : public EditorCommand {
    public:
        PropertyChangeCommand(Gameplay::GameObject* obj,
            const std::string& oldValue,
            const std::string& newValue,
            const std::string& propertyName)
            : m_object(obj)
            , m_oldValue(oldValue)
            , m_newValue(newValue)
            , m_propertyName(propertyName)
        {
        }

        void execute() override {
            if (m_object) {
                if (m_propertyName == "name") {
                    m_object->setName(m_newValue);
                }
                else if (m_propertyName == "texture") {
                    m_object->setTextureId(m_newValue);
                }
            }
        }

        void undo() override {
            if (m_object) {
                if (m_propertyName == "name") {
                    m_object->setName(m_oldValue);
                }
                else if (m_propertyName == "texture") {
                    m_object->setTextureId(m_oldValue);
                }
            }
        }

        std::string getName() const override {
            return "Change " + m_propertyName;
        }

    private:
        Gameplay::GameObject* m_object;
        std::string m_oldValue;
        std::string m_newValue;
        std::string m_propertyName;
    };

} // namespace Platform