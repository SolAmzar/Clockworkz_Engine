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


}