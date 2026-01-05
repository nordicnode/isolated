/**
 * @file chunk_manager.cpp
 * @brief ChunkManager implementation for world streaming.
 */

#include <isolated/world/chunk_manager.hpp>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace isolated {
namespace world {

ChunkManager::ChunkManager(const ChunkManagerConfig& config) : config_(config) {
    // Default terrain generator (flat world)
    terrain_gen_ = [](Chunk& chunk) {
        auto [ox, oy, oz] = chunk.world_origin();
        
        for (size_t z = 0; z < CHUNK_SIZE; ++z) {
            for (size_t y = 0; y < CHUNK_SIZE; ++y) {
                for (size_t x = 0; x < CHUNK_SIZE; ++x) {
                    int world_z = oz + static_cast<int>(z);
                    size_t idx = Chunk::idx(x, y, z);
                    
                    // Simple terrain: below z=0 is stone, above is air
                    if (world_z < -10) {
                        chunk.material[idx] = Material::GRANITE;
                    } else if (world_z < 0) {
                        chunk.material[idx] = Material::SOIL;
                    } else {
                        chunk.material[idx] = Material::AIR;
                        chunk.density[idx] = 1.225; // Air density
                    }
                }
            }
        }
        chunk.generated = true;
    };
}

void ChunkManager::update(float world_x, float world_y, float world_z) {
    ChunkCoord new_cam = world_to_chunk(
        static_cast<int>(world_x),
        static_cast<int>(world_y),
        static_cast<int>(world_z)
    );
    
    // Load chunks around camera
    for (int dz = -config_.load_radius; dz <= config_.load_radius; ++dz) {
        for (int dy = -config_.load_radius; dy <= config_.load_radius; ++dy) {
            for (int dx = -config_.load_radius; dx <= config_.load_radius; ++dx) {
                ChunkCoord target{new_cam.x + dx, new_cam.y + dy, new_cam.z + dz};
                
                if (loaded_chunks_.find(target) == loaded_chunks_.end()) {
                    load_chunk(target);
                }
            }
        }
    }
    
    // Unload distant chunks
    std::vector<ChunkCoord> to_unload;
    for (auto& [coord, chunk] : loaded_chunks_) {
        int dist = std::max({
            std::abs(coord.x - new_cam.x),
            std::abs(coord.y - new_cam.y),
            std::abs(coord.z - new_cam.z)
        });
        
        if (dist > config_.unload_radius) {
            to_unload.push_back(coord);
        }
    }
    
    for (const auto& coord : to_unload) {
        unload_chunk(coord);
    }
    
    camera_chunk_ = new_cam;
}

Chunk* ChunkManager::get_chunk(int world_x, int world_y, int world_z) {
    ChunkCoord coords = world_to_chunk(world_x, world_y, world_z);
    return get_chunk_at(coords);
}

Chunk* ChunkManager::get_chunk_at(ChunkCoord coords) {
    auto it = loaded_chunks_.find(coords);
    if (it != loaded_chunks_.end()) {
        return it->second.get();
    }
    
    // Load on demand
    load_chunk(coords);
    it = loaded_chunks_.find(coords);
    return it != loaded_chunks_.end() ? it->second.get() : nullptr;
}

Material ChunkManager::get_material(int world_x, int world_y, int world_z) {
    Chunk* chunk = get_chunk(world_x, world_y, world_z);
    if (!chunk) return Material::AIR;
    
    // Local coordinates within chunk
    auto local_x = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_y = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_z = ((world_z % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    
    return chunk->material[Chunk::idx(local_x, local_y, local_z)];
}

double ChunkManager::get_temperature(int world_x, int world_y, int world_z) {
    Chunk* chunk = get_chunk(world_x, world_y, world_z);
    if (!chunk) return 293.0;
    
    auto local_x = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_y = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_z = ((world_z % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    
    return chunk->temperature[Chunk::idx(local_x, local_y, local_z)];
}

void ChunkManager::set_material(int world_x, int world_y, int world_z, Material mat) {
    Chunk* chunk = get_chunk(world_x, world_y, world_z);
    if (!chunk) return;
    
    auto local_x = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_y = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_z = ((world_z % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    
    chunk->material[Chunk::idx(local_x, local_y, local_z)] = mat;
    chunk->dirty = true;
}

void ChunkManager::set_temperature(int world_x, int world_y, int world_z, double temp) {
    Chunk* chunk = get_chunk(world_x, world_y, world_z);
    if (!chunk) return;
    
    auto local_x = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_y = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_z = ((world_z % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    
    chunk->temperature[Chunk::idx(local_x, local_y, local_z)] = temp;
    chunk->dirty = true;
}

double ChunkManager::get_density(int world_x, int world_y, int world_z) {
    Chunk* chunk = get_chunk(world_x, world_y, world_z);
    if (!chunk) return 1.225; // Default air density
    
    auto local_x = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_y = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    auto local_z = ((world_z % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    
    return chunk->density[Chunk::idx(local_x, local_y, local_z)];
}

std::vector<Chunk*> ChunkManager::get_loaded_chunks() {
    std::vector<Chunk*> result;
    result.reserve(loaded_chunks_.size());
    for (auto& [coord, chunk] : loaded_chunks_) {
        result.push_back(chunk.get());
    }
    return result;
}

void ChunkManager::exchange_ghost_cells() {
    // For each loaded chunk, copy border data from neighbors
    for (auto& [coord, chunk] : loaded_chunks_) {
        // +X neighbor
        auto it = loaded_chunks_.find({coord.x + 1, coord.y, coord.z});
        if (it != loaded_chunks_.end()) {
            // Copy first YZ slice of neighbor to ghost[0]
            for (size_t z = 0; z < CHUNK_SIZE; ++z) {
                for (size_t y = 0; y < CHUNK_SIZE; ++y) {
                    chunk->ghost_temp[0][y + z * CHUNK_SIZE] = 
                        it->second->temperature[Chunk::idx(0, y, z)];
                }
            }
        }
        // -X neighbor
        it = loaded_chunks_.find({coord.x - 1, coord.y, coord.z});
        if (it != loaded_chunks_.end()) {
            for (size_t z = 0; z < CHUNK_SIZE; ++z) {
                for (size_t y = 0; y < CHUNK_SIZE; ++y) {
                    chunk->ghost_temp[1][y + z * CHUNK_SIZE] = 
                        it->second->temperature[Chunk::idx(CHUNK_SIZE - 1, y, z)];
                }
            }
        }
        // +Y, -Y, +Z, -Z similar...
    }
}

void ChunkManager::save_all() {
    for (auto& [coord, chunk] : loaded_chunks_) {
        if (chunk->dirty) {
            save_to_disk(*chunk);
            chunk->dirty = false;
        }
    }
}

void ChunkManager::load_chunk(ChunkCoord coords) {
    if (loaded_chunks_.size() >= config_.max_loaded) {
        // TODO: Evict LRU chunk
        return;
    }
    
    auto chunk = std::make_unique<Chunk>(coords);
    
    // Try disk first
    if (!try_load_from_disk(*chunk)) {
        // Generate new terrain
        generate_chunk(*chunk);
    }
    
    loaded_chunks_[coords] = std::move(chunk);
}

void ChunkManager::unload_chunk(ChunkCoord coords) {
    auto it = loaded_chunks_.find(coords);
    if (it != loaded_chunks_.end()) {
        if (it->second->dirty) {
            save_to_disk(*it->second);
        }
        loaded_chunks_.erase(it);
    }
}

void ChunkManager::generate_chunk(Chunk& chunk) {
    if (terrain_gen_) {
        terrain_gen_(chunk);
    }
}

bool ChunkManager::try_load_from_disk(Chunk& chunk) {
    // TODO: Implement binary chunk file loading
    // Format: <save_path>/chunk_x_y_z.bin
    return false; // Not found
}

void ChunkManager::save_to_disk(const Chunk& chunk) {
    // TODO: Implement binary chunk file saving
    // For now, just mark as saved
}

} // namespace world
} // namespace isolated
