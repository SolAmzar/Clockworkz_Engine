/******************************************************************************/
/*!
\file           GameObject.cpp
\project        Lights Out
\author(s)      [Lebon], 10% - Will be shared later once other features combine in.
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
#include "Physics/Collision.hpp"
#include "Gameplay/Collider.hpp" 
#include "Math/Vec3.hpp"
#include <Gameplay/Components/SpriteRenderer.hpp>
#include <Gameplay/Components/SpriteAnimator.hpp>
#include <fstream>

namespace Gameplay {

    uint32_t GameObject::s_nextId = 1;

    GameObject::GameObject(GameObjectType type, std::string name, GameObjectShape shape)
        : m_id(s_nextId++), m_type(type), m_name(std::move(name)), m_shapeid(shape)
    {
    }

    void GameObject::update(double dt)
    {

        for (auto& c : m_components) c->onBeforePhysics(*this, dt);

        if(m_chase){
            for (auto& c : m_components) c->onUpdate(*this, dt);
            m_transform.position += m_velocity * static_cast<float>(dt);
            markDirty();
        }
        else {
            // basic top-down integration
            m_transform.position += m_velocity * static_cast<float>(dt);
            markDirty();
            for (auto& c : m_components) c->onUpdate(*this, dt);
        }

        
    }

    std::unique_ptr<GameObject> GameObject::clone() const {
        auto copy = std::make_unique<GameObject>(m_type, m_name, m_shapeid);

        // Copy all properties (existing code - lines 68-82)
        copy->m_transform = m_transform;
        copy->m_size = m_size;
        copy->m_velocity = m_velocity;
        copy->m_moveSpeed = m_moveSpeed;
        copy->m_lastHitNormal = m_lastHitNormal;
        copy->m_textureId = m_textureId;
        copy->m_layer = m_layer;
        copy->m_active = m_active;
        copy->m_collisionMask = m_collisionMask;
        copy->m_collisionCategory = m_collisionCategory;
        copy->m_isSolid = m_isSolid;
        copy->m_isHazard = m_isHazard;
        copy->m_blocksLight = m_blocksLight;
        copy->m_moveable = m_moveable;
        copy->m_chase = m_chase;

        // ✓ ADD THIS: Copy colliders
        for (const auto& collider : m_colliders) {
            if (auto* aabb = dynamic_cast<AABBCollider*>(collider.get())) {
                auto* newAabb = copy->addCollider<AABBCollider>();
                newAabb->type = aabb->type;
                newAabb->halfExtents = aabb->halfExtents;
                newAabb->offset = aabb->offset;
                newAabb->blocksLight = aabb->blocksLight;
                newAabb->solid = aabb->solid;
            }
            else if (auto* circle = dynamic_cast<CircleCollider*>(collider.get())) {
                auto* newCircle = copy->addCollider<CircleCollider>();
                newCircle->type = circle->type;
                newCircle->r = circle->r;
                newCircle->offset = circle->offset;
                newCircle->blocksLight = circle->blocksLight;
                newCircle->solid = circle->solid;
            }
        }

        // ✓ ADD THIS: Copy components
        for (const auto& component : m_components) {
            // SpriteRenderer
            if (auto* srcRenderer = dynamic_cast<SpriteRendererComponent*>(component.get())) {
                auto* destRenderer = copy->addComponent<SpriteRendererComponent>();
                destRenderer->setTint(srcRenderer->tint());
                destRenderer->setUV(srcRenderer->uv());
                if (!srcRenderer->textureAlias().empty()) {
                    destRenderer->setTexture(srcRenderer->textureAlias());
                }
            }
            // SpriteAnimator
            else if (auto* srcAnimator = dynamic_cast<SpriteAnimatorComponent*>(component.get())) {
                std::string filename = srcAnimator->getFilename();
                if (!filename.empty()) {
                    auto* destAnimator = copy->addComponent<SpriteAnimatorComponent>();
                    std::ifstream file(filename);
                    if (file.is_open()) {
                        nlohmann::json j;
                        file >> j;
                        destAnimator->loadFromJson(j);
                        // Link to SpriteRenderer if exists
                        if (auto* renderer = copy->getComponent<SpriteRendererComponent>()) {
                            destAnimator->setSpriteRenderer(renderer);
                        }
                    }
                }
            }
            // Add other component types as needed
        }

        return copy;
    }

    void GameObject::debugRender(Graphics::RenderingManager& renderer) {
        // Need to segment out this rendering of debug draw somewhere else, cannot do it here. too expensive.
        glm::vec3 color(1.0f, 1.0f, 1.0f);

        switch (m_type)
        {
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
        case GameObjectType::Box:
            color = glm::vec3(1.0f, 0.5f, 0.0f);
			break;
        case GameObjectType::Unknown:
            color = glm::vec3(0.6f, 0.6f, 0.6f);
            break;
        }

        switch (m_shapeid)
        {
        case GameObjectShape::Circle:
            renderer.drawCircle(
                m_transform.position.x,
                m_transform.position.y,
                (m_size.x * 0.5f) * m_transform.scale.x,
                color.r, color.g, color.b);
            break;
        case GameObjectShape::Rectangle:
            // Collision draw
            renderer.drawRect(
                m_transform.position.x,
                m_transform.position.y,
                m_size.x * m_transform.scale.x,
                m_size.y * m_transform.scale.y,
                color.r, color.g, color.b);
            break;
        default:
            // Collision draw
            renderer.drawRect(
                m_transform.position.x,
                m_transform.position.y,
                m_size.x * m_transform.scale.x,
                m_size.y * m_transform.scale.y,
                color.r, color.g, color.b);
            break;
        }

        for (auto& c : m_components) c->onRender(*this);
    }

    void GameObject::render(Graphics::RenderingManager& renderer) {
        // Placeholder: actual rendering handled by Graphics system using textureId
        (void)renderer;
        for (auto &c : m_components)
			c->onRender(*this);
	}

    //might want to change to this
    //void GameObject::render(Graphics::RenderingManager& renderer) {
    //    // 1. Lookup the texture by name _every frame_
    //    auto texPtr = Assets::TextureManager::Get(m_textureId);
    //    unsigned texId = texPtr ? texPtr->id() : 0;
    //    renderer.drawSprite(
    //        texId,
    //        glm::vec2(m_transform.position.x, m_transform.position.y),
    //        glm::vec2(m_size.x, m_size.y),
    //        m_transform.rotationRad
    //    );
    //    // 3. Then render overlays/components if needed
    //    for (auto& c : m_components) c->onRender(*this);
    //}





    Gameplay::AABB GameObject::worldAABB() const {
        if (!m_boundsDirty) return m_cachedAABB;

        const vec2 center = m_transform.position;
        AABB out;

        // Default to box from size*scale
        const vec2 half = 0.5f * (m_size * m_transform.scale);
        out.min = center - half;
        out.max = center + half;

        // If any Circle collider exists, override with circle bounds
        for (auto const& c : m_colliders) {
            if (c->type == ColliderType::Circle) {
                float r = static_cast<CircleCollider const*>(c.get())->radius();
                out.min = center + vec2(-r, -r);
                out.max = center + vec2(r, r);
                break;
            }
        }

        m_cachedAABB = out;
        m_boundsDirty = false;
        return m_cachedAABB;
    }

    void GameObject::setName(const std::string &newName)
    {
        m_name = newName;
    }

    void GameObject::autoUpdateCollisionBaseOnScale() {
        vec2 objScale = m_transform.scale;
        vec2 objSize = m_size; // assuming you have a size field

        for (auto &col : m_colliders)
        {
            if (col->type == ColliderType::AABB)
            {
                auto *aabb = static_cast<AABBCollider *>(col.get());
                aabb->halfExtents = (objSize * 0.5f) * objScale; // update half size
            }
            else if (col->type == ColliderType::Circle)
            {
                auto *circ = static_cast<CircleCollider *>(col.get());
                float avgScale = (objScale.x + objScale.y) * 0.5f;
                circ->r = std::max(objSize.x, objSize.y) * 0.5f * avgScale;
            }
        }
    }

    void GameObject::respawnAt(vec2 spawn, bool resetVelocity, bool resetHealth) {
        // Only meaningful for the Player type
        if (m_type != GameObjectType::Player)
            return;

        // Place at spawn and reactivate
        m_transform.position = spawn;
        if (resetVelocity)
            m_velocity = {0.f, 0.f};
        setActive(true);

        // Restore health if the component exists
        if (resetHealth)
        {
            if (auto *hp = getComponent<HealthComponent>())
            {
                // Prefer a 'reset' or 'restore-to-max' API if your HealthComponent exposes it:
                // hp->reset();   // (common pattern)
                // Otherwise, set to max explicitly (uncomment if you have these accessors):
                // hp->setCurrent(hp->max());
            }
        }

        // Keep colliders in sync (in case size/scale changed earlier)
        autoUpdateCollisionBaseOnScale();

        // Optional: clear transient state here (e.g., stun, knockback, i-frames, animation)
        // m_flags.grounded = false;
        // m_animation.play("idle");
    }

    Gameplay::AABB GameObject::aabbWorld() const {
        vec2 half = 0.5f * m_size * m_transform.scale;
        return { m_transform.position - half, m_transform.position + half };
    }

    // ----------------- Prefabs ----------------- (Temp)

    //std::unique_ptr<GameObject> GameObject::MakeFloor(vec2 pos, vec2 size, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Floor, "Floor", GameObjectShape::Rectangle);
    //    go->transform().position = pos; go->setSize(size);
    //    go->setSolid(false); go->setHazard(false); go->setBlocksLight(false);
    //    go->setLayer(Layer::Wall);
    //    go->setCollisionMask(0); // floors are visual only; no collision
    //    go->setTextureId(std::move(tex));
    //    // No collider on purpose (not collidable)
    //    return go;
    //}

    //std::unique_ptr<GameObject> GameObject::MakeWall(vec2 pos, vec2 size, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Wall, "Wall", GameObjectShape::Rectangle);
    //    go->transform().position = pos; go->setSize(size);
    //    go->setSolid(true); go->setBlocksLight(true); go->setHazard(false);
    //    go->setLayer(Layer::Wall);
    //    // walls collide with players, enemies, and balls
    //    go->setCollisionMask(Mask(Layer::Player, Layer::Enemy, Layer::Ball));
    //    go->setTextureId(std::move(tex));
    //    // AABB collider the size of the object
    //    auto* aabb = go->addCollider<AABBCollider>();
    //    aabb->type = ColliderType::AABB;
    //    aabb->halfExtents = 0.5f * size * go->transform().scale;
    //    aabb->offset = { 0.f, 0.f };
    //    aabb->blocksLight = true;
    //    return go;
    //}

    //std::unique_ptr<GameObject> GameObject::MakePlayer(vec2 pos, vec2 size, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Player, "Player", GameObjectShape::Rectangle);
    //    go->transform().position = pos; go->setSize(size);
    //    go->setSolid(true); go->setBlocksLight(false); go->setHazard(false);
    //    go->setLayer(Layer::Player);
    //    // player hits walls, enemies, traps, balls
    //    go->setCollisionMask(Mask(Layer::Wall, Layer::Enemy, Layer::Trap, Layer::Ball));
    //    go->setTextureId(std::move(tex));
    //    // Collider
    //    auto* aabb = go->addCollider<AABBCollider>();
    //    aabb->type = ColliderType::AABB;
    //    aabb->halfExtents = 0.5f * size * go->transform().scale;
    //    aabb->offset = { 0.f, 0.f };
    //    aabb->blocksLight = false;
    //    // Components
    //    go->addComponent<HealthComponent>(3);
    //    return go;
    //}

    //std::unique_ptr<GameObject> GameObject::MakeEnemy(vec2 pos, vec2 size, int hp, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Enemy, "Enemy", GameObjectShape::Rectangle);
    //    go->transform().position = pos; go->setSize(size);
    //    go->setSolid(true); go->setBlocksLight(false); go->setHazard(true);
    //    go->setLayer(Layer::Enemy);
    //    // enemy hits walls, player, balls (but not other enemies if you prefer)
    //    go->setCollisionMask(Mask(Layer::Wall, Layer::Player, Layer::Ball));
    //    go->setTextureId(std::move(tex));
    //    // Collider
    //    auto* aabb = go->addCollider<AABBCollider>();
    //    aabb->type = ColliderType::AABB;
    //    aabb->halfExtents = 0.5f * size * go->transform().scale;
    //    aabb->offset = { 0.f, 0.f };
    //    aabb->blocksLight = false;
    //    // Components
    //    go->addComponent<HealthComponent>(hp);
    //    go->addComponent<DamageDealerComponent>(1); // contact damage
    //    return go;
    //}

    //std::unique_ptr<GameObject> GameObject::MakeTrap(vec2 pos, vec2 size, int damage, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Trap, "Trap", GameObjectShape::Rectangle);
    //    go->transform().position = pos; go->setSize(size);
    //    go->setSolid(false); go->setBlocksLight(false); go->setHazard(true);
    //    go->setLayer(Layer::Trap);
    //    // traps only affect player (and optionally enemy)
    //    go->setCollisionMask(Mask(Layer::Player));
    //    go->setTextureId(std::move(tex));
    //    // Collider (if you want the trap to have a volume)
    //    auto* aabb = go->addCollider<AABBCollider>();
    //    aabb->type = ColliderType::AABB;
    //    aabb->halfExtents = 0.5f * size * go->transform().scale;
    //    aabb->offset = { 0.f, 0.f };
    //    aabb->blocksLight = false;
    //    // Components
    //    go->addComponent<DamageDealerComponent>(damage);
    //    return go;
    //}

    //std::unique_ptr<GameObject> GameObject::MakeBall(vec2 pos, float radius, vec2 size, std::string tex) {
    //    auto go = std::make_unique<GameObject>(GameObjectType::Ball, "Ball", GameObjectShape::Circle);
    //    go->transform().position = pos; go->setSize({ radius * 2.f, radius * 2.f });
    //    go->setSolid(true); go->setBlocksLight(false); go->setHazard(false);
    //    go->setLayer(Layer::Ball);
    //    // ball collides with walls, player, enemies
    //    go->setCollisionMask(Mask(Layer::Wall, Layer::Player, Layer::Enemy));
    //    go->setTextureId(std::move(tex));
    //    // Circle collider
    //    auto* circ = go->addCollider<CircleCollider>();
    //    circ->type = ColliderType::Circle;
    //    circ->r = radius * 0.5f * (go->transform().scale.x + go->transform().scale.y); // average scale
    //    circ->offset = { 0.f, 0.f };
    //    circ->blocksLight = false;
    //    return go;
    //}

} // namespace Gameplay



//void GameObject::onCollision(GameObject& other) {
//    // This is not with reference to the intended gameplay
//    // have to modify along the way.
//    // Generic contact damage: if THIS has DamageDealer and OTHER has Health, apply
//    // Current object damage other object with health on collide
//
//    auto resolvePlayerVsWall = [](GameObject& player, GameObject& wall) {
//        // Compute world-space AABBs 
//        AABB pa = player.aabbWorld();
//        AABB wa = wall.aabbWorld();
//
//        // centers and half-sizes
//        vec2 pc = 0.5f * (pa.min + pa.max);
//        vec2 wc = 0.5f * (wa.min + wa.max);
//        vec2 ph = 0.5f * (pa.max - pa.min);
//        vec2 wh = 0.5f * (wa.max - wa.min);
//
//        // overlap along each axis
//        vec2 d = pc - wc;
//        vec2 o = (ph + wh) - abs_vec2(d);
//
//        // if both overlaps are positive, we are intersecting
//        if (o.x > 0.f && o.y > 0.f) {
//            if (o.x < o.y) {
//                // resolve on X
//                float push = (d.x < 0.f) ? -o.x : o.x;
//                player.transform().position.x += push;
//                // kill X velocity so we don't slide back in
//                player.velocity().x = 0.f;
//            }
//            else {
//                // resolve on Y
//                float push = (d.y < 0.f) ? -o.y : o.y;
//                player.transform().position.y += push;
//                // kill Y velocity
//                player.velocity().y = 0.f;
//            }
//        }
//        };
//
//    // Run resolution if the pair is (Player, Wall) in either order
//    if ((m_type == GameObjectType::Player && other.m_type == GameObjectType::Wall)) {
//        resolvePlayerVsWall(*this, other);
//    }
//    else if ((m_type == GameObjectType::Wall && other.m_type == GameObjectType::Player)) {
//        resolvePlayerVsWall(other, *this);
//    }
//
//    if (auto* dealer = this->getComponent<DamageDealerComponent>()) {
//        if (auto* hp = other.getComponent<HealthComponent>()) {
//            hp->damage(other, dealer->damage());
//        }
//    }
//    // If OTHER is a dealer and THIS has health, you could also apply:
//    // other object damage currend object with health on collide
//    if (auto* dealer2 = other.getComponent<DamageDealerComponent>()) {
//        if (auto* hp2 = this->getComponent<HealthComponent>()) {
//            hp2->damage(*this, dealer2->damage());
//        }
//    }
//
//    other.getComponent<HealthComponent>();
//}