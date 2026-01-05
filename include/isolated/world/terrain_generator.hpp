#pragma once

/**
 * @file terrain_generator.hpp
 * @brief Procedural terrain generation for chunks.
 */

#include <isolated/world/chunk.hpp>
#include <cstdint>
#include <cmath>

namespace isolated {
namespace world {

/**
 * @brief Configuration for terrain generation.
 */
struct TerrainConfig {
    int seed = 12345;
    double sea_level = 0.0;          // World Z for sea level
    double terrain_scale = 0.02;     // Noise scale for terrain
    double height_amplitude = 50.0;  // Max terrain height variation
};

/**
 * @brief Simple noise-based terrain generator.
 */
class TerrainGenerator {
public:
    explicit TerrainGenerator(const TerrainConfig& config);
    
    /**
     * @brief Generate terrain for a chunk.
     */
    void generate(Chunk& chunk);
    
private:
    TerrainConfig config_;
    
    // Simple permutation table for noise
    uint8_t perm_[512];
    
    // Noise functions
    double noise2d(double x, double y) const;
    double noise3d(double x, double y, double z) const;
    double fbm2d(double x, double y, int octaves, double persistence) const;
    
    // Internal helpers
    double fade(double t) const { return t * t * t * (t * (t * 6 - 15) + 10); }
    double lerp(double t, double a, double b) const { return a + t * (b - a); }
    double grad(int hash, double x, double y, double z) const;
    
    void init_permutation();
};

} // namespace world
} // namespace isolated
