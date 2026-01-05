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
 * Must stay in sync with thermal::MATERIALS map.
 */
enum class Material : uint8_t {
    // Gases (individual species for LBM simulation)
    AIR = 0,           // Mixed atmosphere
    OXYGEN = 1,        // O2 - respiration product
    NITROGEN = 2,      // N2 - inert
    CO2 = 3,           // Carbon dioxide - respiration/combustion waste
    METHANE = 4,       // CH4 - decomposition, swamp gas
    WATER_VAPOR = 5,   // H2O gas - evaporation
    HYDROGEN = 6,      // H2 - volcanic, industrial
    AMMONIA = 7,       // NH3 - decomposition, alien atmospheres
    SULFUR_DIOXIDE = 8,// SO2 - volcanic
    // Liquids
    WATER = 10,
    MAGMA = 11,
    
    // Solids - Natural
    ICE = 20,
    CO2_ICE = 21,
    GRANITE = 30,
    BASALT = 31,
    LIMESTONE = 32,
    SANDSTONE = 33,
    SHALE = 34,
    MARBLE = 35,
    REGOLITH = 36,
    SOIL = 37,
    
    // Solids - Manufactured
    STEEL = 50,
    
    // Biological
    FLESH = 60,
    
    // Ores
    IRON_ORE = 100,
    COPPER_ORE = 101,
    GOLD_ORE = 102,
    COAL = 103,
    URANIUM_ORE = 104,
    
    MAX_MATERIALS = 255
};

/**
 * @brief Convert Material enum to string key for thermal::MATERIALS lookup.
 */
inline const char* material_to_string(Material mat) {
    switch (mat) {
        // Gases
        case Material::AIR: return "air";
        case Material::OXYGEN: return "oxygen";
        case Material::NITROGEN: return "nitrogen";
        case Material::CO2: return "co2";
        case Material::METHANE: return "methane";
        case Material::WATER_VAPOR: return "water_vapor";
        case Material::HYDROGEN: return "hydrogen";
        case Material::AMMONIA: return "ammonia";
        case Material::SULFUR_DIOXIDE: return "sulfur_dioxide";
        // Liquids
        case Material::WATER: return "water";
        case Material::MAGMA: return "magma";
        // Solids
        case Material::ICE: return "ice";
        case Material::CO2_ICE: return "co2_ice";
        case Material::GRANITE: return "granite";
        case Material::BASALT: return "basalt";
        case Material::LIMESTONE: return "limestone";
        case Material::SANDSTONE: return "sandstone";
        case Material::SHALE: return "shale";
        case Material::MARBLE: return "marble";
        case Material::REGOLITH: return "regolith";
        case Material::SOIL: return "soil";
        case Material::STEEL: return "steel";
        case Material::FLESH: return "flesh";
        // Ores
        case Material::IRON_ORE: return "iron_ore";
        case Material::COPPER_ORE: return "copper_ore";
        case Material::GOLD_ORE: return "gold_ore";
        case Material::COAL: return "coal";
        case Material::URANIUM_ORE: return "uranium_ore";
        default: return "air";
    }
}
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
