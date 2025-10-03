
/*
Prefab.cpp by Amirul Syafiq (100%)
*/

#pragma once

#include <Gameplay/GameObject.hpp>
#include <string>
#include <memory>
#include <map>
#include "Math/Vec2.hpp"



namespace Gameplay {
	struct GameObjectBlueprint {

		//Initial base object
		GameObjectType type{};
		std::string name{};
		std::string textureId{};

		//Positional 
		glm::vec2 position{ 0.0f,0.0f };
		glm::vec2 size{ 1.0f, 1.0f };
		glm::vec2 scale{ 1.0f,1.0f };
		float rotation = { 0.0f };

		//Conditional
		bool isSolid = false;
		bool isHazard = false;
		bool blocksLight = false;
		uint32_t layer = 0;
		uint32_t collisionMask = 0xFFFFFFu;

		//Basic Attributes
		int health = 0;
		int damage = 0; 
		float speed = 0.0f;
		bool active = true;

	};
	
	class PrefabFactory {
	public:

		//In main, to retrieve the recipebook for each object (access)
		static PrefabFactory& get();

		void saveBlueprint(const std::string& name, const GameObjectBlueprint& blueprint); //Save a new recipe for each object (What to call the recipe, actual recipe data)
		std::unique_ptr<GameObject> create(const std::string& blueprintName); //Create a new Obj with recipe
		std::unique_ptr<GameObject> createAt(const std::string& blueprintName, glm::vec2 position); // Create a new Obj with recipe AND at location. 

	private:
		std::map<std::string, GameObjectBlueprint> blueprints_; //The actual storage for all the recipes, refering GameObjectBlueprint
		void applyBlueprint(GameObject& obj, const GameObjectBlueprint& blueprint); //Takes a blank GameObject and applies a recipe to configure it
	};

	void setupGameBlueprints(); //Sets up the cookbook with the recipe (prep work)

}