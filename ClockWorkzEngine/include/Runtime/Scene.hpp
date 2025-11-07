/******************************************************************************/
/*!
\file           Scene.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares Scene, a container for organizing game objects,
				systems, and level data.
				Responsibilities include:
				- Holding and managing collections of GameObjects
				- Providing update and render hooks for all objects in the scene
				- Supporting scene-level setup and teardown

References:
- Scene management patterns (Simillar to GIT): https://gameprogrammingpatterns.com/state.html
- Unity Scenes (conceptual reference): https://docs.unity3d.com/Manual/CreatingScenes.html

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Graphics/LayerManager.hpp"

// forward decalre the class to be used alter in cpp, cpp file will include the required class file.
namespace Gameplay {
	class GameObject;
	class Physics2D;
}

//Syafiq
namespace Graphics {
	class RenderingManager;
	
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
		void render(Graphics::RenderingManager& renderer);

		const std::string& name() const;
		std::vector<std::unique_ptr<Gameplay::GameObject>>& objects();
		bool saveToFile(const std::string& filepath);
		Gameplay::Physics2D* getPhysics();            // returns raw pointer
		const Gameplay::Physics2D* getPhysics() const; // optional const overload
		Gameplay::GameObject* findByName(std::string const& name);
		Gameplay::GameObject* findFirstByLayer(int layer);


		void addObject(std::unique_ptr<Gameplay::GameObject> go);
		void destroyObject(Gameplay::GameObject* go);
		void wireLayerCallback(Gameplay::GameObject* o);
		void registerExistingObjectsToLayers();

	private:
		std::string m_name;
		std::vector<std::unique_ptr<Gameplay::GameObject>> m_objects;
		std::unique_ptr<Gameplay::Physics2D> m_physics;
		Gameplay::LayerManager m_layers;         
	};

} // namespace Runtime
