/**
 * @file terrain_generator.cpp
 * @brief Perlin noise-based terrain generation.
 */

#include <isolated/world/terrain_generator.hpp>
#include <algorithm>
#include <numeric>
#include <random>

namespace isolated {
namespace world {

TerrainGenerator::TerrainGenerator(const TerrainConfig& config) : config_(config) {
    init_permutation();
}

void TerrainGenerator::init_permutation() {
    std::array<uint8_t, 256> p;
    std::iota(p.begin(), p.end(), 0);
    
    std::mt19937 gen(config_.seed);
    std::shuffle(p.begin(), p.end(), gen);
    
    for (int i = 0; i < 256; ++i) {
        perm_[i] = perm_[i + 256] = p[i];
    }
}

double TerrainGenerator::grad(int hash, double x, double y, double z) const {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

double TerrainGenerator::noise3d(double x, double y, double z) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;
    
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);
    
    double u = fade(x);
    double v = fade(y);
    double w = fade(z);
    
    int A = perm_[X] + Y;
    int AA = perm_[A] + Z;
    int AB = perm_[A + 1] + Z;
    int B = perm_[X + 1] + Y;
    int BA = perm_[B] + Z;
    int BB = perm_[B + 1] + Z;
    
    return lerp(w,
        lerp(v,
            lerp(u, grad(perm_[AA], x, y, z), grad(perm_[BA], x - 1, y, z)),
            lerp(u, grad(perm_[AB], x, y - 1, z), grad(perm_[BB], x - 1, y - 1, z))
        ),
        lerp(v,
            lerp(u, grad(perm_[AA + 1], x, y, z - 1), grad(perm_[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad(perm_[AB + 1], x, y - 1, z - 1), grad(perm_[BB + 1], x - 1, y - 1, z - 1))
        )
    );
}

double TerrainGenerator::noise2d(double x, double y) const {
    return noise3d(x, y, 0.0);
}

double TerrainGenerator::fbm2d(double x, double y, int octaves, double persistence) const {
    double total = 0.0;
    double frequency = 1.0;
    double amplitude = 1.0;
    double max_value = 0.0;
    
    for (int i = 0; i < octaves; ++i) {
        total += noise2d(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    
    return total / max_value;
}

void TerrainGenerator::generate(Chunk& chunk) {
    auto [ox, oy, oz] = chunk.world_origin();
    
    for (size_t y = 0; y < CHUNK_SIZE; ++y) {
        for (size_t x = 0; x < CHUNK_SIZE; ++x) {
            double world_x = ox + static_cast<double>(x);
            double world_y = oy + static_cast<double>(y);
            
            // Generate height using FBM noise
            double height_noise = fbm2d(
                world_x * config_.terrain_scale,
                world_y * config_.terrain_scale,
                6, 0.5
            );
            
            int surface_z = static_cast<int>(config_.sea_level + height_noise * config_.height_amplitude);
            
            for (size_t z = 0; z < CHUNK_SIZE; ++z) {
                int world_z = oz + static_cast<int>(z);
                size_t idx = Chunk::idx(x, y, z);
                
                if (world_z < surface_z - 20) {
                    // Deep underground: granite
                    chunk.material[idx] = Material::GRANITE;
                    chunk.density[idx] = 2700.0;
                    chunk.strata_age[idx] = 4000; // 4 billion years
                } else if (world_z < surface_z - 5) {
                    // Underground: basalt or limestone
                    double rock_noise = noise3d(world_x * 0.1, world_y * 0.1, world_z * 0.1);
                    chunk.material[idx] = (rock_noise > 0) ? Material::BASALT : Material::LIMESTONE;
                    chunk.density[idx] = 2500.0;
                    chunk.strata_age[idx] = 500; // 500 million years
                } else if (world_z < surface_z) {
                    // Near surface: soil
                    chunk.material[idx] = Material::SOIL;
                    chunk.density[idx] = 1500.0;
                    chunk.strata_age[idx] = 10; // 10 million years
                } else if (world_z < config_.sea_level) {
                    // Below sea level, above surface: water
                    chunk.material[idx] = Material::WATER;
                    chunk.density[idx] = 1000.0;
                } else {
                    // Above surface: air
                    chunk.material[idx] = Material::AIR;
                    chunk.density[idx] = 1.225;
                }
                
                // Temperature gradient with depth
                if (world_z < surface_z) {
                    // Geothermal gradient: ~25°C per km
                    double depth = surface_z - world_z;
                    chunk.temperature[idx] = 288.0 + depth * 0.025;
                } else {
                    chunk.temperature[idx] = 288.0; // 15°C surface
                }
            }
        }
    }
    
    chunk.generated = true;
}

} // namespace world
} // namespace isolated
