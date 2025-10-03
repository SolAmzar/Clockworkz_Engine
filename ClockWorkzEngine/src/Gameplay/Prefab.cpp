/******************************************************************************/
/*!
\file           Prefab.hpp
\project        Lights Out
\author(s)      [Amirul Syafiq], 100%
\brief          Defines GameObjectBlueprint and PrefabFactory for creating
                reusable, configurable GameObjects.

GameObjectBlueprint stores type, texture, transform,
                collision, and basic gameplay attributes (HP, damage, etc.).
PrefabFactory saves and retrieves blueprints, and instantiates
              new GameObjects with predefined properties via create() or
              createAt().
Provides setupGameBlueprints() to register standard prefabs.

                Supports a "recipe book" system to easily spawn consistent
                objects in levels.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/Prefab.hpp"

namespace Gameplay {

    PrefabFactory& PrefabFactory::get() {
        static PrefabFactory instance; 
        return instance;
    }

    void PrefabFactory::saveBlueprint(const std::string& name, const GameObjectBlueprint& blueprint) {
        blueprints_[name] = blueprint;
    }

    std::unique_ptr<GameObject> PrefabFactory::create(const std::string& blueprintName) {

        auto it = blueprints_.find(blueprintName);
        if (it == blueprints_.end()) {
            return nullptr;
        }

        const GameObjectBlueprint& blueprint = it->second;

        auto gameObject = std::make_unique<GameObject>(blueprint.type, blueprint.name);

        applyBlueprint(*gameObject, blueprint);

        return gameObject;
    }

    std::unique_ptr<GameObject> PrefabFactory::createAt(const std::string& blueprintName, glm::vec2 position) {

        auto gameObject = create(blueprintName);

        if (gameObject) {
            gameObject->transform().position = position;
        }

        return gameObject;
    }

    void PrefabFactory::applyBlueprint(GameObject& obj, const GameObjectBlueprint& blueprint) {
        // Transform properties
        obj.transform().position = blueprint.position;
        obj.transform().scale = blueprint.scale;
        obj.transform().rotationDeg = blueprint.rotation;
        obj.setSize(blueprint.size);

        // Behavior properties
        obj.setSolid(blueprint.isSolid);
        obj.setHazard(blueprint.isHazard);
        obj.setBlocksLight(blueprint.blocksLight);

        // Rendering and collision
        obj.setTextureId(blueprint.textureId);
        obj.setLayer(blueprint.layer);
        obj.setCollisionMask(blueprint.collisionMask);
        obj.setActive(blueprint.active);

    }

    void setupGameBlueprints() {
        auto& factory = PrefabFactory::get();

        // WALL BLUEPRINT
        {
            GameObjectBlueprint wallBP;
            wallBP.type = GameObjectType::Wall;
            wallBP.name = "Wall";
            wallBP.textureId = "wall_texture";
            wallBP.size = glm::vec2(1.0f, 1.0f);
            wallBP.isSolid = true;
            wallBP.blocksLight = true;
            wallBP.layer = 1;

            factory.saveBlueprint("Wall", wallBP);
        }

        // FLOOR BLUEPRINT  
        {
            GameObjectBlueprint floorBP;
            floorBP.type = GameObjectType::Floor;
            floorBP.name = "Floor";
            floorBP.textureId = "floor_texture";
            floorBP.size = glm::vec2(1.0f, 1.0f);
            floorBP.isSolid = false;
            floorBP.layer = 0;

            factory.saveBlueprint("Floor", floorBP);
        }

        // PLAYER BLUEPRINT
        {
            GameObjectBlueprint playerBP;
            playerBP.type = GameObjectType::Player;
            playerBP.name = "Player";
            playerBP.textureId = "player_texture";
            playerBP.size = glm::vec2(0.8f, 0.8f);
            playerBP.isSolid = true;
            playerBP.layer = 2;
            playerBP.health = 1;
            playerBP.speed = 5.0f;

            factory.saveBlueprint("Player", playerBP);
        }

        // NORMAL ENEMY BLUEPRINT
        {
            GameObjectBlueprint enemyBP;
            enemyBP.type = GameObjectType::Enemy;
            enemyBP.name = "Enemy";
            enemyBP.textureId = "enemy_texture";
            enemyBP.size = glm::vec2(0.9f, 0.9f);
            enemyBP.isSolid = true;
            enemyBP.layer = 2;
            enemyBP.health = 1;
            enemyBP.damage = 10;
            enemyBP.speed = 3.0f;

            factory.saveBlueprint("Enemy", enemyBP);
        }

        // FAST ENEMY
        {
            GameObjectBlueprint fastEnemyBP;
            fastEnemyBP.type = GameObjectType::Enemy;
            fastEnemyBP.name = "FastEnemy";
            fastEnemyBP.textureId = "fast_enemy_texture";
            fastEnemyBP.size = glm::vec2(0.7f, 0.7f);
            fastEnemyBP.isSolid = true;
            fastEnemyBP.layer = 2;
            fastEnemyBP.health = 1;
            fastEnemyBP.damage = 5;
            fastEnemyBP.speed = 8.0f;

            factory.saveBlueprint("FastEnemy", fastEnemyBP);
        }

        // BIG ENEMY
        {
            GameObjectBlueprint bigEnemyBP;
            bigEnemyBP.type = GameObjectType::Enemy;
            bigEnemyBP.name = "BigEnemy";
            bigEnemyBP.textureId = "big_enemy_texture";
            bigEnemyBP.size = glm::vec2(1.5f, 1.5f);
            bigEnemyBP.isSolid = true;
            bigEnemyBP.layer = 2;
            bigEnemyBP.health = 150;
            bigEnemyBP.damage = 25;
            bigEnemyBP.speed = 2.0f;

            factory.saveBlueprint("BigEnemy", bigEnemyBP);
        }
    }

}