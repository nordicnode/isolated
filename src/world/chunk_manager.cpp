/**
 * @file chunk_manager.cpp
 * @brief ChunkManager implementation for world streaming.
 */

#include <isolated/world/chunk_manager.hpp>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>

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

void ChunkManager::sync_to_physics(std::vector<double>& temp_buffer,
                                   std::vector<double>& density_buffer,
                                   int physics_width, int physics_height, int z_level) {
    // Resize buffers if needed
    size_t size = physics_width * physics_height;
    if (temp_buffer.size() != size) temp_buffer.resize(size, 293.0);
    if (density_buffer.size() != size) density_buffer.resize(size, 1.0);
    
    // Calculate world origin for physics grid (centered on camera)
    int origin_x = (camera_chunk_.x * static_cast<int>(CHUNK_SIZE)) - physics_width / 2;
    int origin_y = (camera_chunk_.y * static_cast<int>(CHUNK_SIZE)) - physics_height / 2;
    
    // Copy chunk data to physics buffers (ONLY from loaded chunks - no loading!)
    for (int py = 0; py < physics_height; ++py) {
        for (int px = 0; px < physics_width; ++px) {
            int world_x = origin_x + px;
            int world_y = origin_y + py;
            size_t idx = py * physics_width + px;
            
            // Get chunk WITHOUT triggering load
            ChunkCoord cc = world_to_chunk(world_x, world_y, z_level);
            auto it = loaded_chunks_.find(cc);
            if (it != loaded_chunks_.end()) {
                int lx = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = ((z_level % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                size_t cidx = Chunk::idx(lx, ly, lz);
                temp_buffer[idx] = it->second->temperature[cidx];
                density_buffer[idx] = it->second->density[cidx];
            }
            // else: keep default values (no load triggered)
        }
    }
}

void ChunkManager::sync_from_physics(const std::vector<double>& temp_buffer,
                                     const std::vector<double>& density_buffer,
                                     int physics_width, int physics_height, int z_level) {
    if (temp_buffer.size() != static_cast<size_t>(physics_width * physics_height)) return;
    
    // Calculate world origin (same as sync_to_physics)
    int origin_x = (camera_chunk_.x * static_cast<int>(CHUNK_SIZE)) - physics_width / 2;
    int origin_y = (camera_chunk_.y * static_cast<int>(CHUNK_SIZE)) - physics_height / 2;
    
    // Copy physics results back to chunks (ONLY loaded chunks - no loading!)
    for (int py = 0; py < physics_height; ++py) {
        for (int px = 0; px < physics_width; ++px) {
            int world_x = origin_x + px;
            int world_y = origin_y + py;
            size_t idx = py * physics_width + px;
            
            // Get chunk WITHOUT triggering load
            ChunkCoord cc = world_to_chunk(world_x, world_y, z_level);
            auto it = loaded_chunks_.find(cc);
            if (it != loaded_chunks_.end()) {
                int lx = ((world_x % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = ((world_y % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = ((z_level % static_cast<int>(CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
                size_t cidx = Chunk::idx(lx, ly, lz);
                it->second->temperature[cidx] = temp_buffer[idx];
                it->second->dirty = true;
            }
        }
    }
}

void ChunkManager::load_chunk(ChunkCoord coords) {
    // Evict oldest chunk if at capacity
    while (loaded_chunks_.size() >= config_.max_loaded) {
        evict_lru();
    }
    
    auto chunk = std::make_unique<Chunk>(coords);
    
    // Try disk first
    if (!try_load_from_disk(*chunk)) {
        // Generate new terrain
        generate_chunk(*chunk);
    }
    
    loaded_chunks_[coords] = std::move(chunk);
    
    // Add to LRU (newest at back)
    lru_order_.push_back(coords);
    lru_map_[coords] = std::prev(lru_order_.end());
}

void ChunkManager::unload_chunk(ChunkCoord coords) {
    auto it = loaded_chunks_.find(coords);
    if (it != loaded_chunks_.end()) {
        if (it->second->dirty) {
            save_to_disk(*it->second);
        }
        loaded_chunks_.erase(it);
        
        // Remove from LRU tracking
        auto lru_it = lru_map_.find(coords);
        if (lru_it != lru_map_.end()) {
            lru_order_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
    }
}

void ChunkManager::generate_chunk(Chunk& chunk) {
    if (terrain_gen_) {
        terrain_gen_(chunk);
    }
}

bool ChunkManager::try_load_from_disk(Chunk& chunk) {
    std::string path = get_chunk_path(chunk.coords);
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    // Read header
    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "ICHK") return false;
    
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (version != 1) return false;
    
    // Read material array (as uint8_t)
    constexpr size_t VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    std::vector<uint8_t> mat_bytes(VOXELS);
    file.read(reinterpret_cast<char*>(mat_bytes.data()), VOXELS);
    for (size_t i = 0; i < VOXELS; ++i) {
        chunk.material[i] = static_cast<Material>(mat_bytes[i]);
    }
    
    // Read temperature
    file.read(reinterpret_cast<char*>(chunk.temperature.data()), VOXELS * sizeof(double));
    
    // Read density
    file.read(reinterpret_cast<char*>(chunk.density.data()), VOXELS * sizeof(double));
    
    // Read o2_fraction
    file.read(reinterpret_cast<char*>(chunk.o2_fraction.data()), VOXELS * sizeof(double));
    
    chunk.dirty = false;
    return true;
}

void ChunkManager::save_to_disk(const Chunk& chunk) {
    std::string path = get_chunk_path(chunk.coords);
    
    // Ensure directory exists
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    std::filesystem::create_directories(dir);
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save chunk: " << path << std::endl;
        return;
    }
    
    // Write header
    file.write("ICHK", 4);  // Magic
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), 4);
    
    // Write material array (as uint8_t)
    constexpr size_t VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    std::vector<uint8_t> mat_bytes(VOXELS);
    for (size_t i = 0; i < VOXELS; ++i) {
        mat_bytes[i] = static_cast<uint8_t>(chunk.material[i]);
    }
    file.write(reinterpret_cast<const char*>(mat_bytes.data()), VOXELS);
    
    // Write temperature
    file.write(reinterpret_cast<const char*>(chunk.temperature.data()), VOXELS * sizeof(double));
    
    // Write density
    file.write(reinterpret_cast<const char*>(chunk.density.data()), VOXELS * sizeof(double));
    
    // Write o2_fraction
    file.write(reinterpret_cast<const char*>(chunk.o2_fraction.data()), VOXELS * sizeof(double));
}

void ChunkManager::touch_lru(ChunkCoord coords) {
    auto it = lru_map_.find(coords);
    if (it != lru_map_.end()) {
        // Move to back (most recently used)
        lru_order_.erase(it->second);
        lru_order_.push_back(coords);
        it->second = std::prev(lru_order_.end());
    }
}

void ChunkManager::evict_lru() {
    if (lru_order_.empty()) return;
    
    // Get oldest chunk (front of list)
    ChunkCoord oldest = lru_order_.front();
    unload_chunk(oldest);
}

std::string ChunkManager::get_chunk_path(ChunkCoord coords) const {
    return config_.save_path + "chunk_" + 
           std::to_string(coords.x) + "_" + 
           std::to_string(coords.y) + "_" + 
           std::to_string(coords.z) + ".bin";
}

} // namespace world
} // namespace isolated
