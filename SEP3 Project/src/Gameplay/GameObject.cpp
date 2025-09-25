#include "Gameplay/GameObject.hpp"
#include <algorithm>

#include "Gameplay/Components/Health.hpp"
#include "Gameplay/Components/DamageDealer.hpp"
namespace Gameplay {

    uint32_t GameObject::s_nextId = 1;

    GameObject::GameObject(GameObjectType type, std::string name)
        : m_id(s_nextId++), m_type(type), m_name(std::move(name)) {
    }

    void GameObject::update(double dt) {
        // basic top-down integration
        m_transform.position += m_velocity * static_cast<float>(dt);
        for (auto& c : m_components) c->onUpdate(*this, dt);
    }

    void GameObject::render() {
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

} // namespace Gameplay
