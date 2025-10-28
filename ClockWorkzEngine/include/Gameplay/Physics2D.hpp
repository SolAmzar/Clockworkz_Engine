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