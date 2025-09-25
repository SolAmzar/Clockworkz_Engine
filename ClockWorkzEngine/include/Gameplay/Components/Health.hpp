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
