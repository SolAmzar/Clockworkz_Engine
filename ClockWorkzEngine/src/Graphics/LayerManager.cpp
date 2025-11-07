#include "Graphics/LayerManager.hpp"
#include "Gameplay/GameObject.hpp"   // safe to include here
#include <algorithm>

namespace Gameplay {

    int bit_to_index(uint32_t bit) {
        for (size_t i = 0; i < kOrderedBits.size(); ++i)
            if (kOrderedBits[i] == bit) return static_cast<int>(i);
        return -1;
    }

    void LayerManager::clear() {
        for (auto& b : buckets_) b.clear();
        all_.clear();
    }

    void LayerManager::add(ObjPtr o, uint32_t layerBits) {
        all_.push_back(o);
        reindex_object(o, layerBits);
    }

    void LayerManager::remove(ObjPtr o) {
        all_.erase(std::remove(all_.begin(), all_.end(), o), all_.end());
        for (auto& b : buckets_) {
            b.erase(std::remove(b.begin(), b.end(), o), b.end());
        }
    }

    void LayerManager::onLayerChanged(ObjPtr o, uint32_t newLayerBits) {
        reindex_object(o, newLayerBits);
    }

    void LayerManager::reindex_object(ObjPtr o, uint32_t layerBits) {
        // Remove from all buckets first
        for (auto& b : buckets_) {
            b.erase(std::remove(b.begin(), b.end(), o), b.end());
        }
        // Add to buckets matching layer bits (usually one)
        for (uint32_t bit : kOrderedBits) {
            if (layerBits & bit) buckets_[bit_to_index(bit)].push_back(o);
        }
    }

} // namespace Gameplay
