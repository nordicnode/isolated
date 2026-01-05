#pragma once

/**
 * @file chunk_manager.hpp
 * @brief Manages chunk loading, unloading, and streaming.
 */

#include <isolated/world/chunk.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>

namespace isolated {
namespace world {

/**
 * @brief Configuration for ChunkManager.
 */
struct ChunkManagerConfig {
    int load_radius = 8;      // Chunks to load around camera
    int unload_radius = 12;   // Chunks to unload beyond this
    size_t max_loaded = 500;  // Maximum chunks in memory
    std::string save_path = "./world_data/";
};

/**
 * @brief Manages chunk lifecycle and streaming.
 */
class ChunkManager {
public:
    explicit ChunkManager(const ChunkManagerConfig& config);
    ~ChunkManager() = default;
    
    /**
     * @brief Update chunk loading based on camera position.
     * @param world_x Camera world X position
     * @param world_y Camera world Y position
     * @param world_z Camera world Z position
     */
    void update(float world_x, float world_y, float world_z);
    
    /**
     * @brief Get chunk at world coordinates (loads/generates if needed).
     * @return Pointer to chunk, or nullptr if out of world bounds.
     */
    Chunk* get_chunk(int world_x, int world_y, int world_z);
    
    /**
     * @brief Get chunk by chunk coordinates.
     */
    Chunk* get_chunk_at(ChunkCoord coords);
    
    /**
     * @brief Get voxel at world coordinates.
     */
    Material get_material(int world_x, int world_y, int world_z);
    double get_temperature(int world_x, int world_y, int world_z);
    
    /**
     * @brief Set voxel at world coordinates.
     */
    void set_material(int world_x, int world_y, int world_z, Material mat);
    void set_temperature(int world_x, int world_y, int world_z, double temp);
    
    /**
     * @brief Get all loaded chunks for physics processing.
     */
    std::vector<Chunk*> get_loaded_chunks();
    
    /**
     * @brief Exchange ghost cells between adjacent chunks.
     */
    void exchange_ghost_cells();
    
    /**
     * @brief Save all dirty chunks to disk.
     */
    void save_all();
    
    /**
     * @brief Set terrain generator callback.
     */
    using TerrainGenerator = std::function<void(Chunk&)>;
    void set_terrain_generator(TerrainGenerator gen) { terrain_gen_ = gen; }
    
    // Statistics
    size_t loaded_count() const { return loaded_chunks_.size(); }

private:
    ChunkManagerConfig config_;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> loaded_chunks_;
    
    // Current camera chunk
    ChunkCoord camera_chunk_{0, 0, 0};
    
    // Terrain generator
    TerrainGenerator terrain_gen_;
    
    // Internal helpers
    ChunkCoord world_to_chunk(int world_x, int world_y, int world_z) const;
    void load_chunk(ChunkCoord coords);
    void unload_chunk(ChunkCoord coords);
    void generate_chunk(Chunk& chunk);
    bool try_load_from_disk(Chunk& chunk);
    void save_to_disk(const Chunk& chunk);
};

// Inline helpers
inline ChunkCoord ChunkManager::world_to_chunk(int world_x, int world_y, int world_z) const {
    // Handle negative coordinates correctly with floor division
    auto floor_div = [](int a, int b) {
        return a >= 0 ? a / b : (a - b + 1) / b;
    };
    return {
        floor_div(world_x, static_cast<int>(CHUNK_SIZE)),
        floor_div(world_y, static_cast<int>(CHUNK_SIZE)),
        floor_div(world_z, static_cast<int>(CHUNK_SIZE))
    };
}

} // namespace world
} // namespace isolated
