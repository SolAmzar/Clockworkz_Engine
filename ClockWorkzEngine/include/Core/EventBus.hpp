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