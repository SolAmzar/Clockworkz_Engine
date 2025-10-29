/******************************************************************************/
/*!
\file           GameObject.cpp
\project        Lights Out
\author(s)      [Lebon], 100% - Will be shared later once other features combine in.
\brief          Declares GameObject, the fundamental gameplay entity.
                Responsibilities include:
                - Holding transform, size, velocity, texture, and component data
                - Updating and rendering components each frame
                - Handling collisions by applying damage or health effects
                - Providing prefab factory methods (MakeFloor, MakeWall,
                  MakePlayer, MakeEnemy, MakeTrap) for quickly spawning
                  configured objects
                  (Temporary, to replace once scene loader and prefab factory solidifies)
                - Computing world-space bounding boxes for collision checks
                - To hold Sprite (Texture/Animated frames + Mesh later)

                Serves as the core building block for interactive entities in
                the game world.

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/GameObject.hpp"
#include <algorithm>

#include "Gameplay/Components/Health.hpp"
#include "Gameplay/Components/DamageDealer.hpp"
#include "Graphics/RenderingManager.hpp"
namespace Gameplay {

    uint32_t GameObject::s_nextId = 1;

    GameObject::GameObject(GameObjectType type, std::string name, GameObjectShape shape)
        : m_id(s_nextId++), m_type(type), m_name(std::move(name)), m_shapeid(shape) {
    }

    void GameObject::update(double dt) {
        // basic top-down integration
        m_transform.position += m_velocity * static_cast<float>(dt);
        for (auto& c : m_components) c->onUpdate(*this, dt);
    }

    //Syafiq - Create game object using rect
    void GameObject::render(Graphics::RenderingManager& renderer) {
        glm::vec3 color(1.0f,1.0f,1.0f);

        switch (m_type) {
        case GameObjectType::Player:
            color = glm::vec3(0.0f, 1.0f, 0.0f);
            break;
        case GameObjectType::Enemy:
            color = glm::vec3(1.0f, 0.0f, 0.0f);
            break;
        case GameObjectType::Floor:
            color = glm::vec3(0.0f, 0.0f, 1.0f);
            break;
        case GameObjectType::Trap:
            color = glm::vec3(0.0f, 1.0f, 1.0f);
            break;
        case GameObjectType::Wall:
            color = glm::vec3(1.0f, 1.0f, 0.0f);
            break;
        case GameObjectType::Ball:
            color = glm::vec3(1.0f, 0.0f, 1.0f);
			break;
        case GameObjectType::Unknown:
            color = glm::vec3(0.6f, 0.6f, 0.6f);
            break;
        }
        
        switch (m_shapeid) {
        case GameObjectShape::Circle:
            renderer.drawCircle(
                m_transform.position.x + (m_size.x * 0.5f * m_transform.scale.x),
                m_transform.position.y + (m_size.y * 0.5f * m_transform.scale.y),
                (m_size.x * 0.5f) * m_transform.scale.x,
                color.r, color.g, color.b
            );
            break;
        case GameObjectShape::Rectangle:
            // Collision draw
            renderer.drawRect(
                m_transform.position.x,
                m_transform.position.y,
                m_size.x * m_transform.scale.x,
                m_size.y * m_transform.scale.y,
                color.r, color.g, color.b
            );
            break;
        default:
            // Collision draw
            renderer.drawRect(
                m_transform.position.x,
                m_transform.position.y,
                m_size.x * m_transform.scale.x,
                m_size.y * m_transform.scale.y,
                color.r, color.g, color.b
            );
            break;
        }

        // later: call RenderingManager with textureId/transform/size
        for (auto& c : m_components) c->onRender(*this);
    }

    void GameObject::onCollision(GameObject& other) {
        // This is not with reference to the intended gameplay
        // have to modify along the way.
        // Generic contact damage: if THIS has DamageDealer and OTHER has Health, apply
        // Current object damage other object with health on collide
        if (auto* dealer = this->getComponent<DamageDealerComponent>()) {
            if (auto* hp = other.getComponent<HealthComponent>()) {
                hp->damage(other, dealer->damage());
            }
        }
        // If OTHER is a dealer and THIS has health, you could also apply:
        // other object damage currend object with health on collide
        if (auto* dealer2 = other.getComponent<DamageDealerComponent>()) {
            if (auto* hp2 = this->getComponent<HealthComponent>()) {
                hp2->damage(*this, dealer2->damage());
            }
        }
    }

    void GameObject::autoUpdateCollisionBaseOnScale() {
        glm::vec2 objScale = m_transform.scale;
        glm::vec2 objSize = m_size; // assuming you have a size field

        for (auto& col : m_colliders) {
            if (col->type == ColliderType::AABB) {
                auto* aabb = static_cast<AABBCollider*>(col.get());
                aabb->halfExtents = (objSize * 0.5f) * objScale; // update half size
            }
            else if (col->type == ColliderType::Circle) {
                auto* circ = static_cast<CircleCollider*>(col.get());
                float avgScale = (objScale.x + objScale.y) * 0.5f;
                circ->r = std::max(objSize.x, objSize.y) * 0.5f * avgScale;
            }
        }
    }

    AABB GameObject::aabbWorld() const {
        glm::vec2 half = 0.5f * m_size * m_transform.scale;
        return { m_transform.position - half, m_transform.position + half };
    }

    // ----------------- Prefabs -----------------

    std::unique_ptr<GameObject> GameObject::MakeFloor(glm::vec2 pos, glm::vec2 size, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Floor, "Floor");
        go->transform().position = pos; go->setSize(size);
        go->setSolid(false); go->setHazard(false); go->setBlocksLight(false);
        go->setCollisionMask(0); go->setLayer(0);
        go->setTextureId(std::move(tex));
        return go;
    }

    std::unique_ptr<GameObject> GameObject::MakeWall(glm::vec2 pos, glm::vec2 size, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Wall, "Wall");
        go->transform().position = pos; go->setSize(size);
        go->setSolid(true); go->setBlocksLight(true); go->setHazard(false);
        go->setCollisionMask(0xFFFFFFFFu); go->setLayer(1);
        go->setTextureId(std::move(tex));
        return go;
    }

    std::unique_ptr<GameObject> GameObject::MakePlayer(glm::vec2 pos, glm::vec2 size, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Player, "Player");
        go->transform().position = pos; go->setSize(size);
        go->setSolid(true); go->setBlocksLight(false); go->setHazard(false);
        go->setCollisionMask(0xFFFFFFFFu); go->setLayer(2);
        go->setTextureId(std::move(tex));
        // Components
        go->addComponent<HealthComponent>(3); // e.g., 3 HP
        return go;
    }

    std::unique_ptr<GameObject> GameObject::MakeEnemy(glm::vec2 pos, glm::vec2 size, int hp, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Enemy, "Enemy");
        go->transform().position = pos; go->setSize(size);
        go->setSolid(true); go->setBlocksLight(false); go->setHazard(true);
        go->setCollisionMask(0xFFFFFFFFu); go->setLayer(2);
        go->setTextureId(std::move(tex));
        // Components
        go->addComponent<HealthComponent>(hp);
        go->addComponent<DamageDealerComponent>(1); // contact damage
        return go;
    }

    std::unique_ptr<GameObject> GameObject::MakeTrap(glm::vec2 pos, glm::vec2 size, int damage, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Trap, "Trap");
        go->transform().position = pos; go->setSize(size);
        go->setSolid(false); go->setBlocksLight(false); go->setHazard(true);
        go->setCollisionMask(0xFFFFFFFFu); go->setLayer(1);
        go->setTextureId(std::move(tex));
        // Components
        go->addComponent<DamageDealerComponent>(damage);
        return go;
    }

    std::unique_ptr<GameObject> GameObject::MakeBall(glm::vec2 pos, float radius, glm::vec2 size, std::string tex) {
        auto go = std::make_unique<GameObject>(GameObjectType::Ball, "Ball", GameObjectShape::Circle);
        go->transform().position = pos; go->setSize(glm::vec2(radius * 2.f, radius * 2.f));
        go->setSolid(true); go->setBlocksLight(false); go->setHazard(false);
        go->setCollisionMask(0xFFFFFFFFu); go->setLayer(2);
        go->setTextureId(std::move(tex));
        return go;
	}

} // namespace Gameplay
