/******************************************************************************/
/*!
\file           GameObject.hpp
\project        Lights Out
\author(s)      [Lebon], 100% - Will be shared later once other features combine in.
Serializer guy come in split the 100%
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
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "Graphics/LayerManager.hpp"
#include "Math/Vec2.hpp"
#include "Math/config.hpp"
#include "Gameplay/Collider.hpp"

//Syafiq - forward decalre the class to be used alter in cpp, cpp file will include the required class file.
namespace Graphics {
    class RenderingManager;
}


namespace Gameplay {
	class LayerManager;

    // Keep it small; components can add behavior later.
    enum class GameObjectType : uint8_t { Floor, Wall, Player, Enemy, Trap, Unknown, Ball, Box};

	// To disguish between different collision shapes
    enum class CollisionShapeType : uint8_t { AABB, Circle, Polygon, Unknown}; 

    enum CollisionCategory : uint32_t {
        CC_None = 0,
        CC_Wall = 1u << 0,
        CC_Player = 1u << 1,
        CC_Enemy = 1u << 2,
        CC_Trap = 1u << 3,
        CC_Ball = 1u << 4,
        CC_Box = 1u << 5,
        CC_All = 0xFFFFFFFFu
    };

    constexpr uint32_t Mask(std::initializer_list<uint32_t> cats) {
        uint32_t m = 0; for (auto c : cats) m |= c; return m;
    }

	// To disguish between different game object shapes
    enum class GameObjectShape : uint8_t { Square ,Rectangle, Circle, Polygon, Unknown};

    // ToDo -> Update the glm::vec2 or glm structures into own custom math libraries
    struct Transform2D {
        vec2           position{ 0.f, 0.f };
        float          rotationRad{ 0.f };
        vec2           scale{ 1.f, 1.f };
    };

    struct AABB {
        vec2 min;
        vec2 max;
        bool intersects(AABB const& o) const {
            return !(max.x < o.min.x || min.x > o.max.x || max.y < o.min.y || min.y > o.max.y);
        }
    };

    class Component; // fwd

    class GameObject {
    public:
		GameObject(GameObjectType type, std::string name, GameObjectShape shape = GameObjectShape::Square); // default shape is square
        virtual ~GameObject() = default;

        GameObject(GameObject const&) = delete;
        GameObject& operator=(GameObject const&) = delete;

        // Lifecycle
        virtual void update(double dt);
        virtual void debugRender(Graphics::RenderingManager& renderer);
        virtual void render(Graphics::RenderingManager& renderer); // hook into Graphics later

        //deep copy
        // Clone this GameObject (deep copy)
        std::unique_ptr<GameObject> clone() const;

        // Interactions
       // virtual void onCollision(GameObject& other);

        // Data
        uint32_t              id() const { return m_id; }
        GameObjectType        type() const { return m_type; }
        std::string const&    name() const { return m_name; }
        GameObjectShape       shapeId() const { return m_shapeid; }
        bool                  active() const { return m_active; }
        void                  setActive(bool a) { m_active = a; }

        Transform2D&          transform() {  m_boundsDirty = true; return m_transform;}
        Transform2D const&    transform() const { return m_transform; }
        void                  setPosition(vec2 pos) { m_transform.position.x = pos.x;  m_transform.position.y = pos.y; m_boundsDirty = true;}


        vec2                  size() const { return m_size; }
        void                  setSize(vec2 s) { m_size = s; m_boundsDirty = true; }

        vec2&                 velocity() { return m_velocity; }
        vec2 const&           velocity() const { return m_velocity; }
        void                  setVelocity(const vec2& v) { m_velocity = v; }

        bool                  hasVelocity() const { return true; }

        float const&          moveSpeed() { return m_moveSpeed; }
        void                  setMoveSpeed(float ms) { m_moveSpeed = ms; }

        void                  setLastHitNormal(vec2 n) { m_lastHitNormal = n; }
        vec2                  lastHitNormal() const { return m_lastHitNormal; }



        // Collision + rendering layers
        uint32_t              layer() const { return m_layer; }
        void                  setLayer(uint32_t l) {m_layer = l; if (m_onLayerChanged) m_onLayerChanged(this);}
        uint32_t              collisionMask() const { return m_collisionMask; }
        void                  setCollisionMask(uint32_t m) { m_collisionMask = m; }
        uint32_t              collisionCategory() const { return m_collisionCategory; }
        void                  setCollisionCategory(uint32_t c) { m_collisionCategory = c; }

        // Flags to support horror mechanics (light, blocking, hazards)
        bool isSolid() const { return m_isSolid; }
		bool ismoveable() const { return m_moveable; }
        bool isHazard() const { return m_isHazard; }
        bool blocksLight() const { return m_blocksLight; }
        void setSolid(bool v) { m_isSolid = v; }
        void setHazard(bool v) { m_isHazard = v; }
        void setBlocksLight(bool v) { m_blocksLight = v; }
        void setName(const std::string& newName); 
        void setBatt(f32 v) { m_battery = v; }
        float checkBatt() const { return m_battery; }
        float checkDrain() const { return drainRate; }
        float checkCharge() const { return chargeRate; }
        float lightExposureTime() const { return m_lightExposureTime; }
        void addLightExposure(float dt) { m_lightExposureTime += dt; }
        void resetLightExposure() { m_lightExposureTime = 0.0f; }

        // Rendering handle for now (swap to Sprite component later)
        std::string const& textureId() const { return m_textureId; }
        void setTextureId(std::string id) { m_textureId = std::move(id); }

        void autoUpdateCollisionBaseOnScale();
        AABB aabbWorld() const;
        void respawnAt(vec2 spawn, bool resetVelocity = true, bool resetHealth = true);

        AABB worldAABB() const;

        void markDirty() { m_boundsDirty = true; }

        using LayerChangedFn = std::function<void(GameObject*)>;

        // Setter accepts a std::function (can be a capturing lambda)
        void setLayerChangedCallback(LayerChangedFn cb) { m_onLayerChanged = std::move(cb); }

       
        const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }


        // Very light component hook (safe to ignore at start)
        template<typename T, typename... Args>
        T* addComponent(Args&&... args);

        template<typename T>
        T* getComponent() const;

        // Prefabs for your types
  //      static std::unique_ptr<GameObject> MakeFloor(vec2 pos, vec2 size, std::string tex = "");
  //      static std::unique_ptr<GameObject> MakeWall(vec2 pos, vec2 size, std::string tex = "");
  //      static std::unique_ptr<GameObject> MakePlayer(vec2 pos, vec2 size, std::string tex = "");
  //      static std::unique_ptr<GameObject> MakeEnemy(vec2 pos, vec2 size, int hp = 2, std::string tex = "");
  //      static std::unique_ptr<GameObject> MakeTrap(vec2 pos, vec2 size, int damage = 1, std::string tex = "");
		//static std::unique_ptr<GameObject> MakeBall(vec2 pos, float radius, vec2 size, std::string tex = "");

        template <typename T, typename... Args>
        T* addCollider(Args&&... args);

        const std::vector<std::unique_ptr<Collider>>& colliders() const { return m_colliders; }
        std::vector<std::unique_ptr<Collider>>& colliders() { return m_colliders; }

        // helpers
        // GameObjectType::Player already exists in your code.
        inline Gameplay::GameObject* FindPlayer(std::vector<std::unique_ptr<Gameplay::GameObject>> const& objects)
        {
            for (auto const& obj : objects)
                if (obj && obj->active() && obj->type() == Gameplay::GameObjectType::Player)
                    return obj.get();
            return nullptr;
        }

        // non-const variant if you want to mutate the player:
        inline Gameplay::GameObject* FindPlayer(std::vector<std::unique_ptr<Gameplay::GameObject>>& objects)
        {
            for (auto& obj : objects)
                if (obj && obj->active() && obj->type() == Gameplay::GameObjectType::Player)
                    return obj.get();
            return nullptr;
        }

    private:
        static uint32_t s_nextId;

        uint32_t        m_id{ 0 };
        GameObjectType  m_type{ GameObjectType::Floor };
        std::string     m_name;
		GameObjectShape m_shapeid{ GameObjectShape::Square };
        bool            m_active{ true };

        Transform2D     m_transform{};
        vec2            m_size{ 1.f, 1.f };
        vec2            m_velocity{ 0.f, 0.f };
        vec2            m_lastHitNormal{ 0.f, 0.f };
        float           m_moveSpeed{ 100.0f };

        uint32_t        m_collisionMask{ 0xFFFFFFFFu };
        uint32_t        m_collisionCategory{ 0 };
        bool            m_isSolid{ false };
        bool            m_isHazard{ false };
        bool            m_blocksLight{ true };
		bool 		    m_moveable{ false };
        // Battery field
        float           m_battery = 10.0f;
        const float     drainRate = 2.0f;   // battery units per second
        const float     chargeRate = 1.0f;  // battery units per second

        float           m_lightExposureTime{ 0.0f };  // Time exposed to light

        mutable bool    m_boundsDirty{ true };
        mutable AABB    m_cachedAABB{};

        std::string     m_textureId{};

        std::vector<std::unique_ptr<Collider>> m_colliders;
        std::vector<std::unique_ptr<Component>> m_components;

        LayerChangedFn m_onLayerChanged{ nullptr };
        uint32_t m_layer{ DrawLayer::World }; // default
    };

   

    // --- Component base kept tiny to avoid ECS bloat now ---
    class Component {
    public:
        virtual ~Component() = default;
        virtual void onBeforePhysics(GameObject& owner, double dt) {}  
        virtual void onUpdate(GameObject& owner, double dt) { (void)owner; (void)dt; }
        // Render need to take the resource like textures/sprite information from resourcemanager.
        virtual void onRender(GameObject& owner) { (void)owner; }
        GameObject* owner() const { return m_owner; }
    private:
        friend class GameObject;   // lets GameObject assign this
        GameObject* m_owner = nullptr;
    };

    template<typename T, typename... Args>
    T* GameObject::addComponent(Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        raw->m_owner = this;
        m_components.emplace_back(std::move(ptr));      
        return raw;
    }

    template<typename T>
    T* GameObject::getComponent() const {
        for (auto& c : m_components)
            if (auto p = dynamic_cast<T*>(c.get())) return p;
        return nullptr;
    }

    template <typename T, typename... Args>
    T* GameObject::addCollider(Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        m_colliders.emplace_back(std::move(ptr));
        return raw;
    }

} // namespace Gameplay
