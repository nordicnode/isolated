#pragma once

#include <vector>
#include "entt/entt.hpp"

namespace isolated {
namespace entities {

/**
 * @brief Spatial index for fast entity lookups by position.
 * 
 * Maps grid coordinates (x, y) to a list of entities residing in that cell.
 * Currently supports a single Z-level (2D grid).
 */
class SpatialIndex {
public:
    void init(int width, int height) {
        width_ = width;
        height_ = height;
        // Resize grid to width * height
        grid_.resize(width * height);
    }

    void clear() {
        for (auto& cell : grid_) {
            cell.clear();
        }
    }

    void insert(entt::entity entity, int x, int y) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        
        size_t index = y * width_ + x;
        grid_[index].push_back(entity);
    }

    const std::vector<entt::entity>& get_entities_at(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            static const std::vector<entt::entity> empty;
            return empty;
        }
        
        size_t index = y * width_ + x;
        return grid_[index];
    }
    
    // Allow querying a range of cells (useful for radius checks)
    void query_range(int min_x, int min_y, int max_x, int max_y, std::vector<entt::entity>& out_result) const {
        // Clamp bounds
        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x >= width_) max_x = width_ - 1;
        if (max_y >= height_) max_y = height_ - 1;
        
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const auto& cell = grid_[y * width_ + x];
                out_result.insert(out_result.end(), cell.begin(), cell.end());
            }
        }
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::vector<entt::entity>> grid_;
};

} // namespace entities
} // namespace isolated
