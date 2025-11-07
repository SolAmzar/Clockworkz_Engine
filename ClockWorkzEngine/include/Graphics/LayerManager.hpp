#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

namespace Gameplay {

    // Forward declaration; do NOT include GameObject.hpp here.
    class GameObject;

    // Canonical draw/update layers
    namespace DrawLayer {
        enum : uint32_t {
            Background = 1u << 0,
            World = 1u << 1,
            Actors = 1u << 2,
            FX = 1u << 3,
            UI = 1u << 4,
        };
    }

    // Fixed render/update order
    inline constexpr std::array<uint32_t, 5> kOrderedBits = {
      DrawLayer::Background, DrawLayer::World, DrawLayer::Actors, DrawLayer::FX, DrawLayer::UI
    };

    int bit_to_index(uint32_t bit);

    class LayerManager {
    public:
        using ObjPtr = GameObject*;

        // Lifecycle
        void clear();

        // Register/remove (caller supplies the object's current draw-layer bits)
        void add(ObjPtr o, uint32_t layerBits);
        void remove(ObjPtr o);

        // Call when an object's layer changes (pass new bits)
        void onLayerChanged(ObjPtr o, uint32_t newLayerBits);

        // Iteration helpers (templates must stay in the header)
        template <typename F>
        void for_each_layered_update(F&& fn) {
            for (uint32_t bit : kOrderedBits) {
                auto& bucket = buckets_[bit_to_index(bit)];
                for (auto* o : bucket) fn(o, bit); // caller can check active() etc.
            }
        }

        template <typename F>
        void for_each_layered_render(F&& fn) const {
            for (uint32_t bit : kOrderedBits) {
                auto const& bucket = buckets_[bit_to_index(bit)];
                for (auto* o : bucket) fn(o, bit);
            }
        }

        // Accessors
        std::vector<ObjPtr> const& objects_in(uint32_t bit) const {
            return buckets_[bit_to_index(bit)];
        }
        std::vector<ObjPtr> const& all_objects() const { return all_; }

    private:
        void reindex_object(ObjPtr o, uint32_t layerBits);

        std::vector<ObjPtr> all_;
        std::array<std::vector<ObjPtr>, kOrderedBits.size()> buckets_{};
    };

} // namespace Gameplay
