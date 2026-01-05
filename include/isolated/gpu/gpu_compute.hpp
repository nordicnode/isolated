#pragma once

/**
 * @file gpu_compute.hpp
 * @brief OpenGL compute shader utilities for GPU physics.
 * 
 * Uses Raylib's rlgl to run compute shaders on the GPU.
 * No CUDA required - works with any OpenGL 4.3+ GPU.
 */

#include "raylib.h"
#include "rlgl.h"
#include <string>
#include <vector>

namespace isolated {
namespace gpu {

/**
 * @brief GPU buffer handle for compute shaders.
 */
struct GPUBuffer {
    unsigned int id = 0;
    size_t size = 0;
    
    void create(size_t bytes);
    void upload(const void* data, size_t bytes);
    void download(void* data, size_t bytes);
    void destroy();
};

/**
 * @brief Compute shader wrapper.
 */
class ComputeShader {
public:
    ComputeShader() = default;
    ~ComputeShader();
    
    /**
     * @brief Load compute shader from source string.
     */
    bool load(const std::string& source);
    
    /**
     * @brief Dispatch compute shader.
     * @param groups_x Number of work groups in X
     * @param groups_y Number of work groups in Y
     * @param groups_z Number of work groups in Z
     */
    void dispatch(int groups_x, int groups_y, int groups_z);
    
    /**
     * @brief Set uniform value.
     */
    void set_uniform(const char* name, float value);
    void set_uniform(const char* name, int value);
    
    /**
     * @brief Bind buffer to binding point.
     */
    void bind_buffer(int binding, GPUBuffer& buffer);
    
    /**
     * @brief Wait for GPU to finish.
     */
    static void barrier();
    
    bool is_loaded() const { return program_ != 0; }

private:
    unsigned int program_ = 0;
};

/**
 * @brief Thermal conduction compute kernel.
 */
class ThermalComputeKernel {
public:
    bool init(size_t width, size_t height);
    void step(double dt);
    void upload_temperature(const std::vector<double>& temp);
    void download_temperature(std::vector<double>& temp);
    void destroy();

private:
    ComputeShader shader_;
    GPUBuffer temp_buffer_;     // Current temperature
    GPUBuffer temp_new_buffer_; // New temperature (double-buffered)
    GPUBuffer props_buffer_;    // Material properties (k, cp, rho)
    size_t width_ = 0, height_ = 0;
    bool swap_buffers_ = false;
};

/**
 * @brief LBM D2Q9 fluid simulation compute kernel.
 */
class LBMComputeKernel {
public:
    bool init(size_t width, size_t height);
    void step(double dt, double omega);
    void upload_state(const std::vector<double>& rho, 
                      const std::vector<double>& ux,
                      const std::vector<double>& uy);
    void download_state(std::vector<double>& rho,
                        std::vector<double>& ux,
                        std::vector<double>& uy);
    void set_solid(size_t x, size_t y, bool is_solid);
    void destroy();

private:
    ComputeShader collide_shader_;
    ComputeShader stream_shader_;
    ComputeShader macro_shader_;
    
    GPUBuffer f_buffer_;       // Distribution functions (9 * width * height floats)
    GPUBuffer f_new_buffer_;   // New distributions (double-buffered)
    GPUBuffer rho_buffer_;     // Density
    GPUBuffer ux_buffer_;      // Velocity X
    GPUBuffer uy_buffer_;      // Velocity Y
    GPUBuffer solid_buffer_;   // Solid obstacles
    
    size_t width_ = 0, height_ = 0;
    bool swap_buffers_ = false;
};



/**
 * @brief GPU-based Terrain Generator using Compute Shaders.
 */
class TerrainComputeKernel {
public:
    bool init(size_t chunk_size = 64);
    void generate_chunk(int world_x, int world_y, int world_z, double seed);
    void download_chunk(std::vector<uint8_t>& material, 
                        std::vector<double>& temperature,
                        std::vector<double>& density);
    void destroy();

private:
    ComputeShader shader_;
    
    // Output buffers
    GPUBuffer material_buffer_;     // uint8_t (but aligned to uint on GPU side)
    GPUBuffer temperature_buffer_;  // float
    GPUBuffer density_buffer_;      // float
    
    size_t chunk_size_ = 64;
};

} // namespace gpu
} // namespace isolated
