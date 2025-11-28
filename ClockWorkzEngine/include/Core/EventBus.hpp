/******************************************************************************/
/*!
\file           EventBus.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Implements the EventBus, a lightweight publish–subscribe system
                used for decoupled communication across the engine. Provides:
                - subscribe<T>(): register a callback for a specific event type
                - publish<T>(): broadcast an event to all subscribed listeners
                - unsubscribeOwner(): remove all callbacks tied to an owner

                Each event type is identified by a std::type_index key,
                allowing type-safe dispatch without manual IDs.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Event.hpp"
#include <typeindex>
#include <functional>
#include <unordered_map>
#include <vector>
namespace Core
{
    class EventBus
    {
    public:
        using Callback = std::function<void(const BaseEvent &)>;

        template <typename EventT>
        void subscribe(void *owner, std::function<void(const EventT &)> func)
        {
            EventID id = std::type_index(typeid(EventT));

            subscribers[id].push_back({owner,
                                       [func](const BaseEvent &e)
                                       {
                                           func(static_cast<const EventT &>(e));
                                       }});
        }

        template <typename EventT>
        void publish(const EventT &event)
        {
            EventID id = std::type_index(typeid(EventT));

            auto it = subscribers.find(id);
            if (it == subscribers.end())
                return;

            for (auto &entry : it->second)
            {
                entry.callback(event);
            }
        }

        void unsubscribeOwner(void *owner)
        {
            for (auto &[id, list] : subscribers)
            {
                list.erase(
                    std::remove_if(list.begin(), list.end(),
                                   [owner](const Subscriber &s)
                                   { return s.owner == owner; }),
                    list.end());
            }
        }

    private:
        struct Subscriber
        {
            void *owner;
            Callback callback;
        };

        std::unordered_map<EventID, std::vector<Subscriber>> subscribers;
    };
}