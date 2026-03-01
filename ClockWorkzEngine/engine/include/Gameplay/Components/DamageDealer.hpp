/******************************************************************************/
/*!
\file           DamageDealer.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares DamageDealerComponent, a tag component that marks a 
                GameObject as capable of inflicting damage. It stores an 
                integer damage value and provides methods to get or set it. 
                Typically attached to hazardous objects or attacks to 
                determine how much health to subtract from targets on contact.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include "Gameplay/GameObject.hpp"

namespace Gameplay {

    // Tag component that says "I deal X damage on contact"
    class DamageDealerComponent final : public Component {
    public:
        explicit DamageDealerComponent(int dmg = 1) : m_damage(dmg) {}

        int  damage() const { return m_damage; }
        void setDamage(int d) { m_damage = d; }

    private:
        int m_damage{ 1 };
    };

} // namespace Gameplay
