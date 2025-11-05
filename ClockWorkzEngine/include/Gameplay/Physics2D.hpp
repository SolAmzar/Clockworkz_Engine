/******************************************************************************/
/*!
\file           Physics2D.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares the 2D physics system responsible for collision
                detection and light raycasting.

                Responsibilities:
                - step(): Performs broad and narrow-phase collision checks
                  between active GameObjects using AABB and Circle colliders.
                  Invokes collision callbacks when overlaps are detected.
                - raycastLight(): Casts a ray through the scene to find the
                  nearest intersection point against colliders that block light,
                  supporting dynamic lighting and shadow behavior.
                - Contact struct: Represents a collision pair between two
                  GameObjects for potential event handling.

                Designed to be extended with future features such as spatial
                partitioning (grids or quadtrees) and physics responses.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <vector>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace Gameplay {
	class GameObject;

	struct Contact { GameObject* a; GameObject* b; };

	class Physics2D {
	public:
		void step(std::vector<std::unique_ptr<GameObject>>& objs, double dt);
		std::optional<glm::vec2> raycastLight(
			const glm::vec2& origin,
			const glm::vec2& dir,
			float maxDist,
			const std::vector<std::unique_ptr<GameObject>>& objs);
	};
}