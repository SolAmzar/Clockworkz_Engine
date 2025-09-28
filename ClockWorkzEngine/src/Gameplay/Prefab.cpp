
/*
Prefab.cpp by Amirul Syafiq (100%)
*/

#include <Gameplay/Prefab.hpp>

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
            playerBP.health = 100;
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
            enemyBP.health = 50;
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
            fastEnemyBP.health = 25;          // Less health
            fastEnemyBP.damage = 5;           // Less damage  
            fastEnemyBP.speed = 8.0f;         // Much faster!

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
            bigEnemyBP.health = 150;          // More health
            bigEnemyBP.damage = 25;           // More damage
            bigEnemyBP.speed = 2.0f;          // Slower

            factory.saveBlueprint("BigEnemy", bigEnemyBP);
        }

        // TRAP
        {
            GameObjectBlueprint trapBP;
            trapBP.type = GameObjectType::Trap;
            trapBP.name = "Trap";
            trapBP.textureId = "trap_texture";
            trapBP.size = glm::vec2(1.0f, 1.0f);
            trapBP.isHazard = true;
            trapBP.layer = 0;                 // Ground level
            trapBP.damage = 20;

            factory.saveBlueprint("Trap", trapBP);
        }

        // SPIKE TRAP
        {
            GameObjectBlueprint spikeTrapBP;
            spikeTrapBP.type = GameObjectType::Trap;
            spikeTrapBP.name = "SpikeTrap";
            spikeTrapBP.textureId = "spike_trap_texture";
            spikeTrapBP.size = glm::vec2(1.0f, 1.0f);
            spikeTrapBP.isHazard = true;
            spikeTrapBP.layer = 0;
            spikeTrapBP.damage = 40;          // More dangerous!

            factory.saveBlueprint("SpikeTrap", spikeTrapBP);
        }
    }

} // namespace Gameplay