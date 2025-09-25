#pragma once
#include "Gameplay/GameObject.hpp"

namespace Gameplay {

    // Tag component that says "I deal N damage on contact"
    class DamageDealerComponent final : public Component {
    public:
        explicit DamageDealerComponent(int dmg = 1) : m_damage(dmg) {}

        int  damage() const { return m_damage; }
        void setDamage(int d) { m_damage = d; }

    private:
        int m_damage{ 1 };
    };

} // namespace Gameplay
