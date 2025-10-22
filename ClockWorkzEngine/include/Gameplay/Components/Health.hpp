/******************************************************************************/
/*!
\file           Health.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares HealthComponent, a component that tracks hit points
                for a GameObject. It provides methods to:
                - Apply damage and deactivate the owner when health reaches 0
                - Heal the object by a specified amount
                - Query current HP

                Used to give durability and survival mechanics to entities.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <algorithm>
#include "Gameplay/GameObject.hpp"

namespace Gameplay {
    class HealthComponent final : public Component {
    public:
        explicit HealthComponent(int h = 1) : m_hp(h) {}
        int hp() const { return m_hp; }
        void damage(GameObject& owner, int d) {
            m_hp = std::max(0, m_hp - d);
            if (m_hp == 0) owner.setActive(false);
        }
        void heal(int h) { if (h > 0) m_hp += h; }
    private:
        int m_hp{ 1 };
    };
}
