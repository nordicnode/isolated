/**
 * @file lod_zone_manager.cpp
 * @brief Temporal LOD implementation with viewport priority.
 */

#include <isolated/core/lod_zone_manager.hpp>

namespace isolated {
namespace core {

LODZoneManager::LODZoneManager(const LODConfig& config) : config_(config) {
    // Default viewport covers entire grid
    viewport_.x_min = 0;
    viewport_.x_max = static_cast<int>(config_.nx);
    viewport_.y_min = 0;
    viewport_.y_max = static_cast<int>(config_.ny);
}

void LODZoneManager::set_viewport(const ViewportBounds& bounds) {
    viewport_ = bounds;
}

int LODZoneManager::get_region(size_t x, size_t y) const {
    // Divide grid into quadrants (2x2 = 4 regions)
    // Region 0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right
    int half_x = static_cast<int>(config_.nx / 2);
    int half_y = static_cast<int>(config_.ny / 2);
    
    int region = 0;
    if (x >= half_x) region += 1;
    if (y >= half_y) region += 2;
    
    return region % config_.num_regions;
}

bool LODZoneManager::in_viewport(size_t x, size_t y) const {
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    
    return (ix >= viewport_.x_min && ix <= viewport_.x_max &&
            iy >= viewport_.y_min && iy <= viewport_.y_max);
}

bool LODZoneManager::should_update(size_t x, size_t y, int step_count) const {
    // Viewport cells ALWAYS update (visual consistency)
    if (in_viewport(x, y)) {
        return true;
    }
    
    // Non-viewport cells update on their region's turn
    int cell_region = get_region(x, y);
    int active_region = step_count % config_.num_regions;
    
    return (cell_region == active_region);
}

} // namespace core
} // namespace isolated
