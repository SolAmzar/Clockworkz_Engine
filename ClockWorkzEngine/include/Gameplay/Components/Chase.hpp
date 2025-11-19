/******************************************************************************/
/*!
\file           Chase.hpp
\project        Lights Out
\author(s)      [Amzar], 10%
\brief          Declares Chase component that makes a GameObject chase the player.
				Responsibilities include:
				- Moving the owner GameObject towards the player at a specified speed
				- Optionally only engaging when within a certain radius
				- Stopping when close enough to avoid jittering on top of the player
				Used to create enemy AI that pursues the player character.
				(Template for more complex AI behaviors)

All content ? 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/





// Gameplay/Components/Chase.hpp
#pragma once
#include "Gameplay/GameObject.hpp"
#include "Math/Vec2.hpp"

namespace Gameplay {

    class Chase : public Component {
    public:
        // speed: units/sec; engageRadius (optional): only chase when player is close (<= 0 to always chase)
        explicit Chase(GameObject& enemy, GameObject* player, float speed, float engageRadius = 0.0f);

        void onUpdate(GameObject& enemy, double dt) override;

        // Optional tunables you can expose:
        void setSpeed(float s) { m_speed = s; }
        void setEngageRadius(float r) { m_engageRadius = r; }
        void setStopDistance(float r) { m_stopDist = r; }   // stop when this close to player

    private:
        float m_speed{};
        float m_engageRadius{};  // 0 = always chase
        float m_stopDist{ 25.0f }; // don�t jitter right on top of player
        GameObject& m_enemy;
        GameObject* m_player = nullptr;
    };

} // namespace Gameplay
