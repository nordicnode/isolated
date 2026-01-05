#pragma once

/**
 * @file lod_zone_manager.hpp
 * @brief Temporal LOD: Time-slice physics across grid regions.
 * 
 * Instead of skipping distant cells, we rotate which regions update each step.
 * Viewport cells ALWAYS update for visual consistency.
 */

#include <cstdint>
#include <vector>

namespace isolated {
namespace core {

/**
 * @brief Configuration for temporal LOD.
 */
struct LODConfig {
    size_t nx = 200;
    size_t ny = 200;
    int num_regions = 4;  // Grid divided into 4 quadrants
    int viewport_padding = 50; // Cells around camera always update
};

/**
 * @brief Viewport bounds for camera-priority updates.
 */
struct ViewportBounds {
    int x_min = 0, x_max = 200;
    int y_min = 0, y_max = 200;
};

/**
 * @brief Temporal LOD manager: Time-slices physics updates.
 */
class LODZoneManager {
public:
    explicit LODZoneManager(const LODConfig& config);
    
    /**
     * @brief Set current viewport bounds (camera view area).
     */
    void set_viewport(const ViewportBounds& bounds);
    
    /**
     * @brief Check if a cell should update this step.
     * @param x Cell X coordinate
     * @param y Cell Y coordinate
     * @param step_count Current simulation step
     * @return true if cell should be updated this step
     */
    bool should_update(size_t x, size_t y, int step_count) const;
    
    /**
     * @brief Get region index for a cell (0 to num_regions-1).
     */
    int get_region(size_t x, size_t y) const;

private:
    LODConfig config_;
    ViewportBounds viewport_;
    
    bool in_viewport(size_t x, size_t y) const;
};

} // namespace core
} // namespace isolated
