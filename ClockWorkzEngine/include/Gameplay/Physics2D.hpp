#pragma once
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include "Math/Vec2.hpp"
#include "Math/config.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace Gameplay {
	class GameObject;

	struct Contact { GameObject* a; GameObject* b; };

	class Physics2D {
	public:
        static void step(std::vector<std::unique_ptr<GameObject>>& objects, double dt);
		std::optional<vec2> raycastLight(
			const vec2& origin,
			const vec2& dir,
			float maxDist,
			const std::vector<std::unique_ptr<GameObject>>& objs);
	};

    struct CellKey {
        int x, y;
        bool operator==(CellKey const& o) const { return x == o.x && y == o.y; }
    };
    struct CellKeyHash {
        size_t operator()(CellKey const& k) const noexcept {
            // 64-bit mix (splitmix64-ish)
            uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(k.x));
            uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(k.y));
            uint64_t z = (ux << 32) ^ uy;
            z += 0x9e3779b97f4a7c15ULL;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            z ^= (z >> 31);
            return static_cast<size_t>(z);
        }
    };

    class SpatialHash {
    public:
        explicit SpatialHash(float cell) : m_cell(cell), m_inv(1.0f / cell) {}
        void clear() { m_cells.clear(); }
        void insert(GameObject* obj);
        template <typename F>
        void forNeighbors(GameObject* obj, F&& func) const;

    private:
        inline CellKey key(vec2 p) const {
            return { static_cast<int>(std::floor(p.x * m_inv)),
                     static_cast<int>(std::floor(p.y * m_inv)) };
        }
        float m_cell, m_inv;
        std::unordered_map<CellKey, std::vector<GameObject*>, CellKeyHash> m_cells;
    };


}