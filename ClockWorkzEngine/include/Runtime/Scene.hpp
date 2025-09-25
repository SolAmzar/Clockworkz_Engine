#pragma once
#include <memory>
#include <string>
#include <vector>

// forward decalre the class to be used alter in cpp, cpp file will include the required class file.
namespace Gameplay {
	class GameObject;
	class Physics2D;
}


namespace Runtime {


	class Scene {
	public:
		explicit Scene(const std::string& name);
		~Scene();

		// Lifecycle
		bool load();     // create objects, load assets
		void unload();   // destroy objects, free assets

		// Frame
		void update(double dt);
		void render();

		const std::string& name() const;
		std::vector<std::unique_ptr<Gameplay::GameObject>>& objects();
	private:
		std::string m_name;
		std::vector<std::unique_ptr<Gameplay::GameObject>> m_objects;
		std::unique_ptr<Gameplay::Physics2D> m_physics;
	};

} // namespace Runtime
