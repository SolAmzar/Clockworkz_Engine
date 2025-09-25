#pragma once
#include <vector>
#include <memory>

namespace Gameplay {
	class GameObject;

	struct Contact { GameObject* a; GameObject* b; };

	class Physics2D {
	public:
		void step(std::vector<std::unique_ptr<GameObject>>& objs, double dt);
	};
}