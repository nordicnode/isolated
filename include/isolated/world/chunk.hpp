#pragma once

/**
 * @file chunk.hpp
 * @brief Chunk data structure for massive world streaming.
 * 
 * Each chunk is 64³ cells containing voxel data for physics and terrain.
 */

#include <cstdint>
#include <vector>
#include <array>

namespace isolated {
namespace world {

// Chunk dimensions
constexpr size_t CHUNK_SIZE = 64;
constexpr size_t CHUNK_CELLS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 262,144

/**
 * @brief Material types for terrain.
 */
enum class Material : uint8_t {
    AIR = 0,
    GRANITE = 1,
    BASALT = 2,
    SANDSTONE = 3,
    LIMESTONE = 4,
    SHALE = 5,
    MARBLE = 6,
    SOIL = 7,
    WATER = 8,
    MAGMA = 9,
    ICE = 10,
    // Ores
    IRON_ORE = 50,
    COPPER_ORE = 51,
    GOLD_ORE = 52,
    COAL = 53,
    // Add more as needed
    MAX_MATERIALS = 255
};

/**
 * @brief Chunk coordinate (world-space chunk index).
 */
struct ChunkCoord {
    int x, y, z;
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief Hash function for ChunkCoord.
 */
struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        // Simple spatial hash
        return static_cast<size_t>(c.x) ^
               (static_cast<size_t>(c.y) << 16) ^
               (static_cast<size_t>(c.z) << 32);
    }
};

/**
 * @brief A single chunk of the world.
 * 
 * Contains all voxel data for a 64³ region.
 */
struct Chunk {
    ChunkCoord coords;
    
    // Terrain data (static after generation)
    std::vector<Material> material;     // 64³ = 262K bytes
    std::vector<uint16_t> strata_age;   // Geological layer age (millions of years)
    
    // Physics data (dynamic, updated each step)
    std::vector<double> temperature;    // Kelvin
    std::vector<double> density;        // kg/m³ (for fluids)
    std::vector<double> pressure;       // Pa
    
    // Gas composition (for LBM)
    std::vector<double> o2_fraction;
    std::vector<double> co2_fraction;
    
    // State flags
    bool generated = false;
    bool dirty = false;     // Needs save to disk
    bool physics_active = true;
    
    // Ghost cells (borders from neighbors, 6 faces)
    // Each face is CHUNK_SIZE² cells
    std::array<std::vector<double>, 6> ghost_temp;  // +X, -X, +Y, -Y, +Z, -Z
    
    Chunk() {
        allocate();
    }
    
    explicit Chunk(ChunkCoord c) : coords(c) {
        allocate();
    }
    
    void allocate() {
        material.resize(CHUNK_CELLS, Material::AIR);
        strata_age.resize(CHUNK_CELLS, 0);
        temperature.resize(CHUNK_CELLS, 293.0); // Room temp
        density.resize(CHUNK_CELLS, 1.225);     // Air
        pressure.resize(CHUNK_CELLS, 101325.0); // 1 atm
        o2_fraction.resize(CHUNK_CELLS, 0.21);
        co2_fraction.resize(CHUNK_CELLS, 0.0004);
        
        // Ghost cells for each face
        for (auto& face : ghost_temp) {
            face.resize(CHUNK_SIZE * CHUNK_SIZE, 293.0);
        }
    }
    
    // Convert local (x,y,z) to flat index
    static size_t idx(size_t x, size_t y, size_t z) {
        return x + CHUNK_SIZE * (y + CHUNK_SIZE * z);
    }
    
    // World position of chunk's (0,0,0) corner
    std::array<int, 3> world_origin() const {
        return {
            coords.x * static_cast<int>(CHUNK_SIZE),
            coords.y * static_cast<int>(CHUNK_SIZE),
            coords.z * static_cast<int>(CHUNK_SIZE)
        };
    }
};

} // namespace world
} // namespace isolated
