/******************************************************************************/
/*!
\file           Event.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the event types and base event class used by the
                game's event system. Provides:
                - BaseEvent: common interface for all events
                - Strongly-typed event structs (e.g., WindowResizeEvent,
                  Focus events, QuitGameEvent, PlayerDiedEvent)
                - EventID alias for runtime type identification via std::type_index

                These events are dispatched through the engine�s event bus to
                allow decoupled communication between systems such as Window,
                Input, Renderer, and Game States.


All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

namespace Core
{

    struct BaseEvent
    {
        virtual ~BaseEvent() = default;
    };

    using EventID = std::type_index;

    struct WindowResizeEvent : BaseEvent
    {
        int width, height;
        WindowResizeEvent(int w, int h) : width(w), height(h) {}
    };

    struct QuitGameEvent : BaseEvent {};

    struct WindowFocusLostEvent : public BaseEvent {};
    struct WindowFocusGainedEvent : public BaseEvent {};


    struct PlayerDiedEvent : BaseEvent
    {
        int id;
        PlayerDiedEvent(int id) : id(id) {}
    };

    // Fired by GamepadManager when a controller is plugged in / unplugged.
    // index is the slot (0-3) that was assigned or freed.
    struct GamepadConnectedEvent : BaseEvent
    {
        int index;
        GamepadConnectedEvent(int i) : index(i) {}
    };
    struct GamepadDisconnectedEvent : BaseEvent
    {
        int index;
        GamepadDisconnectedEvent(int i) : index(i) {}
    };

}