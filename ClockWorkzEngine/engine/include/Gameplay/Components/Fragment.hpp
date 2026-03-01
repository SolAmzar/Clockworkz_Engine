/******************************************************************************/
/*!
\file           Fragment.hpp
\project        Lights Out
\author(s)      [Syafiq], 100%
\brief          Defines FragmentComponent for optional fragment metadata.

                IMPORTANT: Fragments are GameObjects with type GameObjectType::Fragment

                FragmentComponent is OPTIONAL and only adds extra metadata like:
                - Fragment ID for tracking specific fragments
                - Value/points for weighted collection
                - Collection state

                The player tracks collection with FragmentCollectorComponent.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include "Gameplay/GameObject.hpp"
#include <string>

namespace Gameplay
{
    class FragmentComponent : public Component
    {
    public:
        FragmentComponent(const std::string& fragmentId = "fragment", int value = 1)
            : m_fragmentId(fragmentId)
            , m_value(value)
            , m_collected(false)
        {
        }

        const std::string& fragmentId() const { return m_fragmentId; }
        int value() const { return m_value; }
        bool isCollected() const { return m_collected; }

        void setFragmentId(const std::string& id) { m_fragmentId = id; }
        void setValue(int value) { m_value = value; }
        void markCollected() { m_collected = true; }

    private:
        friend class GameObject;
        GameObject* m_owner = nullptr;

        std::string m_fragmentId;
        int m_value;
        bool m_collected;
    };

    class FragmentCollectorComponent : public Component
    {
    public:
        FragmentCollectorComponent(int requiredFragments = 3)
            : m_collectedCount(0)
            , m_requiredFragments(requiredFragments)
            , m_totalValue(0)
            , m_pickupRadius(80.0f)  // Configurable!
        {
        }

        // Collection
        void collectFragment(int value = 1)
        {
            m_collectedCount++;
            m_totalValue += value;
        }

        void resetCollection()
        {
            m_collectedCount = 0;
            m_totalValue = 0;
        }

        // Query
        int collectedCount() const { return m_collectedCount; }
        int requiredFragments() const { return m_requiredFragments; }
        int totalValue() const { return m_totalValue; }
        float pickupRadius() const { return m_pickupRadius; }
        bool canEscape() const { return m_collectedCount >= m_requiredFragments; }
        float completionPercentage() const
        {
            if (m_requiredFragments == 0) return 1.0f;
            return static_cast<float>(m_collectedCount) / static_cast<float>(m_requiredFragments);
        }

        // Setters (for JSON deserialization)
        void setRequiredFragments(int required) { m_requiredFragments = required; }
        void setCollectedCount(int count) { m_collectedCount = count; }
        void setPickupRadius(float radius) { m_pickupRadius = radius; }

    private:
        friend class GameObject;
        GameObject* m_owner = nullptr;

        int m_collectedCount;
        int m_requiredFragments;
        int m_totalValue;
        float m_pickupRadius;  // JSON-configurable pickup distance
    };

} // namespace Gameplay