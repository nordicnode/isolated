/**
 * @file gpu_compute.cpp
 * @brief OpenGL compute shader implementation using Raylib's rlgl.
 * 
 * Raylib includes OpenGL function loading via GLAD internally.
 * We use rlgl headers which expose the GL functions.
 */

// Raylib's external/glad.h is included via rlgl.h
// Define RLGL_IMPLEMENTATION only once in the project
#define GRAPHICS_API_OPENGL_43  // Enable OpenGL 4.3+ features
#include "external/glad.h"  // Included in raylib source

#include <isolated/gpu/gpu_compute.hpp>

#include <iostream>
#include <cstring>

namespace isolated {
namespace gpu {

// ============ GPUBuffer ============

void GPUBuffer::create(size_t bytes) {
    glGenBuffers(1, &id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    size = bytes;
}

void GPUBuffer::upload(const void* data, size_t bytes) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GPUBuffer::download(void* data, size_t bytes) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    void* gpu_data = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (gpu_data) {
        std::memcpy(data, gpu_data, bytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GPUBuffer::destroy() {
    if (id) {
        glDeleteBuffers(1, &id);
        id = 0;
    }
}

// ============ ComputeShader ============

ComputeShader::~ComputeShader() {
    if (program_) {
        glDeleteProgram(program_);
    }
}

bool ComputeShader::load(const std::string& source) {
    // Create compute shader
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    // Check compile errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "[GPU] Compute shader compile error: " << log << std::endl;
        glDeleteShader(shader);
        return false;
    }
    
    // Create program
    program_ = glCreateProgram();
    glAttachShader(program_, shader);
    glLinkProgram(program_);
    
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program_, 512, nullptr, log);
        std::cerr << "[GPU] Compute shader link error: " << log << std::endl;
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    
    glDeleteShader(shader);
    std::cout << "[GPU] Compute shader loaded successfully" << std::endl;
    return true;
}

void ComputeShader::dispatch(int groups_x, int groups_y, int groups_z) {
    glUseProgram(program_);
    glDispatchCompute(groups_x, groups_y, groups_z);
}

void ComputeShader::set_uniform(const char* name, float value) {
    glUseProgram(program_);
    GLint loc = glGetUniformLocation(program_, name);
    glUniform1f(loc, value);
}

void ComputeShader::set_uniform(const char* name, int value) {
    glUseProgram(program_);
    GLint loc = glGetUniformLocation(program_, name);
    glUniform1i(loc, value);
}

void ComputeShader::bind_buffer(int binding, GPUBuffer& buffer) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.id);
}

void ComputeShader::barrier() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// ============ ThermalComputeKernel ============

static const char* THERMAL_SHADER_SOURCE = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(std430, binding = 0) buffer TempIn {
    float temp_in[];
};

layout(std430, binding = 1) buffer TempOut {
    float temp_out[];
};

layout(std430, binding = 2) buffer Props {
    float alpha[];  // Pre-computed thermal diffusivity
};

uniform int width;
uniform int height;
uniform float dt;

void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    
    if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) {
        if (x < width && y < height) {
            int idx = x + y * width;
            temp_out[idx] = temp_in[idx];
        }
        return;
    }
    
    int idx = x + y * width;
    float dx = 1.0;
    float dx2 = dx * dx;
    
    // 5-point stencil Laplacian
    float laplacian = (temp_in[idx + 1] + temp_in[idx - 1] +
                       temp_in[idx + width] + temp_in[idx - width] -
                       4.0 * temp_in[idx]) / dx2;
    
    float a = alpha[idx];
    temp_out[idx] = temp_in[idx] + a * laplacian * dt;
}
)";

bool ThermalComputeKernel::init(size_t width, size_t height) {
    width_ = width;
    height_ = height;
    
    if (!shader_.load(THERMAL_SHADER_SOURCE)) {
        std::cerr << "[GPU] Failed to init thermal kernel" << std::endl;
        return false;
    }
    
    size_t n_cells = width * height;
    size_t bytes = n_cells * sizeof(float);
    
    temp_buffer_.create(bytes);
    temp_new_buffer_.create(bytes);
    props_buffer_.create(bytes);
    
    // Default alpha (granite-like)
    std::vector<float> alpha(n_cells, 2.5f / (790.0f * 2700.0f));
    props_buffer_.upload(alpha.data(), bytes);
    
    std::cout << "[GPU] Thermal kernel ready: " << width << "x" << height << std::endl;
    return true;
}

void ThermalComputeKernel::step(double dt) {
    GPUBuffer& in_buf = swap_buffers_ ? temp_new_buffer_ : temp_buffer_;
    GPUBuffer& out_buf = swap_buffers_ ? temp_buffer_ : temp_new_buffer_;
    
    shader_.bind_buffer(0, in_buf);
    shader_.bind_buffer(1, out_buf);
    shader_.bind_buffer(2, props_buffer_);
    
    shader_.set_uniform("width", static_cast<int>(width_));
    shader_.set_uniform("height", static_cast<int>(height_));
    shader_.set_uniform("dt", static_cast<float>(dt));
    
    int groups_x = (width_ + 15) / 16;
    int groups_y = (height_ + 15) / 16;
    shader_.dispatch(groups_x, groups_y, 1);
    
    ComputeShader::barrier();
    swap_buffers_ = !swap_buffers_;
}

void ThermalComputeKernel::upload_temperature(const std::vector<double>& temp) {
    std::vector<float> temp_f(temp.begin(), temp.end());
    GPUBuffer& buf = swap_buffers_ ? temp_new_buffer_ : temp_buffer_;
    buf.upload(temp_f.data(), temp_f.size() * sizeof(float));
}

void ThermalComputeKernel::download_temperature(std::vector<double>& temp) {
    std::vector<float> temp_f(width_ * height_);
    GPUBuffer& buf = swap_buffers_ ? temp_new_buffer_ : temp_buffer_;
    buf.download(temp_f.data(), temp_f.size() * sizeof(float));
    temp.assign(temp_f.begin(), temp_f.end());
}

void ThermalComputeKernel::destroy() {
    temp_buffer_.destroy();
    temp_new_buffer_.destroy();
    props_buffer_.destroy();
}

// ============ LBMComputeKernel ============

// Collide kernel: BGK collision step
static const char* LBM_COLLIDE_SHADER = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

// D2Q9 lattice weights
const float w[9] = float[9](4.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,
                            1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0);
const int ex[9] = int[9](0, 1, 0, -1, 0, 1, -1, -1, 1);
const int ey[9] = int[9](0, 0, 1, 0, -1, 1, 1, -1, -1);

layout(std430, binding = 0) buffer F_In { float f_in[]; };
layout(std430, binding = 1) buffer F_Out { float f_out[]; };
layout(std430, binding = 2) buffer Rho { float rho[]; };
layout(std430, binding = 3) buffer Ux { float ux[]; };
layout(std430, binding = 4) buffer Uy { float uy[]; };
layout(std430, binding = 5) buffer Solid { int solid[]; };

uniform int width;
uniform int height;
uniform float omega;

void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    if (x >= width || y >= height) return;
    
    int idx = x + y * width;
    int base = idx * 9;
    
    // Skip solid cells
    if (solid[idx] != 0) {
        for (int i = 0; i < 9; i++) f_out[base + i] = f_in[base + i];
        return;
    }
    
    // Compute macroscopic quantities
    float r = 0.0, vx = 0.0, vy = 0.0;
    for (int i = 0; i < 9; i++) {
        float fi = f_in[base + i];
        r += fi;
        vx += fi * float(ex[i]);
        vy += fi * float(ey[i]);
    }
    if (r > 0.0) { vx /= r; vy /= r; }
    rho[idx] = r;
    ux[idx] = vx;
    uy[idx] = vy;
    
    // BGK collision
    float usqr = 1.5 * (vx*vx + vy*vy);
    for (int i = 0; i < 9; i++) {
        float eu = float(ex[i])*vx + float(ey[i])*vy;
        float feq = w[i] * r * (1.0 + 3.0*eu + 4.5*eu*eu - usqr);
        f_out[base + i] = f_in[base + i] - omega * (f_in[base + i] - feq);
    }
}
)";

// Stream kernel: propagate distributions
static const char* LBM_STREAM_SHADER = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

const int ex[9] = int[9](0, 1, 0, -1, 0, 1, -1, -1, 1);
const int ey[9] = int[9](0, 0, 1, 0, -1, 1, 1, -1, -1);
const int opp[9] = int[9](0, 3, 4, 1, 2, 7, 8, 5, 6);

layout(std430, binding = 0) buffer F_In { float f_in[]; };
layout(std430, binding = 1) buffer F_Out { float f_out[]; };
layout(std430, binding = 5) buffer Solid { int solid[]; };

uniform int width;
uniform int height;

void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    if (x >= width || y >= height) return;
    
    int idx = x + y * width;
    int base = idx * 9;
    
    for (int i = 0; i < 9; i++) {
        int nx = x - ex[i];
        int ny = y - ey[i];
        
        // Periodic boundaries
        if (nx < 0) nx += width;
        if (nx >= width) nx -= width;
        if (ny < 0) ny += height;
        if (ny >= height) ny -= height;
        
        int nidx = nx + ny * width;
        
        // Bounce-back for solids
        if (solid[nidx] != 0) {
            f_out[base + i] = f_in[base + opp[i]];
        } else {
            f_out[base + i] = f_in[nidx * 9 + i];
        }
    }
}
)";

bool LBMComputeKernel::init(size_t width, size_t height) {
    width_ = width;
    height_ = height;
    
    if (!collide_shader_.load(LBM_COLLIDE_SHADER)) {
        std::cerr << "[GPU] LBM collide shader failed" << std::endl;
        return false;
    }
    if (!stream_shader_.load(LBM_STREAM_SHADER)) {
        std::cerr << "[GPU] LBM stream shader failed" << std::endl;
        return false;
    }
    
    size_t n_cells = width * height;
    
    // 9 floats per cell for distributions
    f_buffer_.create(n_cells * 9 * sizeof(float));
    f_new_buffer_.create(n_cells * 9 * sizeof(float));
    rho_buffer_.create(n_cells * sizeof(float));
    ux_buffer_.create(n_cells * sizeof(float));
    uy_buffer_.create(n_cells * sizeof(float));
    solid_buffer_.create(n_cells * sizeof(int));
    
    // Initialize equilibrium (rho=1, u=0)
    std::vector<float> f_init(n_cells * 9);
    const float w[9] = {4.0f/9, 1.0f/9, 1.0f/9, 1.0f/9, 1.0f/9,
                        1.0f/36, 1.0f/36, 1.0f/36, 1.0f/36};
    for (size_t i = 0; i < n_cells; i++) {
        for (int q = 0; q < 9; q++) {
            f_init[i * 9 + q] = w[q];
        }
    }
    f_buffer_.upload(f_init.data(), f_init.size() * sizeof(float));
    f_new_buffer_.upload(f_init.data(), f_init.size() * sizeof(float));
    
    // Initialize rho=1, u=0
    std::vector<float> ones(n_cells, 1.0f);
    std::vector<float> zeros(n_cells, 0.0f);
    std::vector<int> no_solid(n_cells, 0);
    rho_buffer_.upload(ones.data(), n_cells * sizeof(float));
    ux_buffer_.upload(zeros.data(), n_cells * sizeof(float));
    uy_buffer_.upload(zeros.data(), n_cells * sizeof(float));
    solid_buffer_.upload(no_solid.data(), n_cells * sizeof(int));
    
    std::cout << "[GPU] LBM kernel ready: " << width << "x" << height << std::endl;
    return true;
}

void LBMComputeKernel::step(double dt, double omega) {
    GPUBuffer& f_in = swap_buffers_ ? f_new_buffer_ : f_buffer_;
    GPUBuffer& f_out = swap_buffers_ ? f_buffer_ : f_new_buffer_;
    
    int groups_x = (width_ + 15) / 16;
    int groups_y = (height_ + 15) / 16;
    
    // Collide
    collide_shader_.bind_buffer(0, f_in);
    collide_shader_.bind_buffer(1, f_out);
    collide_shader_.bind_buffer(2, rho_buffer_);
    collide_shader_.bind_buffer(3, ux_buffer_);
    collide_shader_.bind_buffer(4, uy_buffer_);
    collide_shader_.bind_buffer(5, solid_buffer_);
    collide_shader_.set_uniform("width", static_cast<int>(width_));
    collide_shader_.set_uniform("height", static_cast<int>(height_));
    collide_shader_.set_uniform("omega", static_cast<float>(omega));
    collide_shader_.dispatch(groups_x, groups_y, 1);
    ComputeShader::barrier();
    
    // Stream
    stream_shader_.bind_buffer(0, f_out);
    stream_shader_.bind_buffer(1, f_in);
    stream_shader_.bind_buffer(5, solid_buffer_);
    stream_shader_.set_uniform("width", static_cast<int>(width_));
    stream_shader_.set_uniform("height", static_cast<int>(height_));
    stream_shader_.dispatch(groups_x, groups_y, 1);
    ComputeShader::barrier();
    
    swap_buffers_ = !swap_buffers_;
}

void LBMComputeKernel::upload_state(const std::vector<double>& rho,
                                    const std::vector<double>& vx,
                                    const std::vector<double>& vy) {
    std::vector<float> rho_f(rho.begin(), rho.end());
    std::vector<float> ux_f(vx.begin(), vx.end());
    std::vector<float> uy_f(vy.begin(), vy.end());
    
    rho_buffer_.upload(rho_f.data(), rho_f.size() * sizeof(float));
    ux_buffer_.upload(ux_f.data(), ux_f.size() * sizeof(float));
    uy_buffer_.upload(uy_f.data(), uy_f.size() * sizeof(float));
}

void LBMComputeKernel::download_state(std::vector<double>& rho,
                                      std::vector<double>& vx,
                                      std::vector<double>& vy) {
    size_t n = width_ * height_;
    std::vector<float> rho_f(n), ux_f(n), uy_f(n);
    
    rho_buffer_.download(rho_f.data(), n * sizeof(float));
    ux_buffer_.download(ux_f.data(), n * sizeof(float));
    uy_buffer_.download(uy_f.data(), n * sizeof(float));
    
    rho.assign(rho_f.begin(), rho_f.end());
    vx.assign(ux_f.begin(), ux_f.end());
    vy.assign(uy_f.begin(), uy_f.end());
}

void LBMComputeKernel::set_solid(size_t x, size_t y, bool is_solid) {
    // Would need to upload single value - for now, bulk upload only
}

void LBMComputeKernel::destroy() {
    f_buffer_.destroy();
    f_new_buffer_.destroy();
    rho_buffer_.destroy();
    ux_buffer_.destroy();
    uy_buffer_.destroy();
    solid_buffer_.destroy();
}

// ============ TerrainComputeKernel ============

static const char* TERRAIN_GEN_SHADER = R"(
#version 430 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(std430, binding = 0) buffer MaterialOut { uint material[]; };
layout(std430, binding = 1) buffer TempOut { float temperature[]; };
layout(std430, binding = 2) buffer DensityOut { float density[]; };

uniform int chunk_x;
uniform int chunk_y;
uniform int chunk_z;
uniform float seed;

// hash based 3d value noise for basic terrain
// Ported from standard GLSL noise algorithms
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

// FBM
float fbm(vec3 p) {
    float f = 0.0;
    f += 0.5000 * noise(p); p *= 2.02;
    f += 0.2500 * noise(p); p *= 2.03;
    f += 0.1250 * noise(p); p *= 2.01;
    f += 0.0625 * noise(p);
    return f;
}

void main() {
    uvec3 id = gl_GlobalInvocationID;
    if (id.x >= 64 || id.y >= 64 || id.z >= 64) return;
    
    int lx = int(id.x);
    int ly = int(id.y);
    int lz = int(id.z);
    
    // Global world coordinates
    float wx = float(chunk_x + lx);
    float wy = float(chunk_y + ly);
    float wz = float(chunk_z + lz);
    int idx = lx + 64 * (ly + 64 * lz);
    
    // === SURFACE HEIGHT ===
    // 2D terrain height based on x,y only
    float sea_level = 50.0;  // Match CPU config
    float height_noise = fbm(vec3(wx * 0.02, wy * 0.02, seed)) * 30.0;
    height_noise += noise(vec3(wx * 0.08, wy * 0.08, seed * 2.0)) * 8.0;
    int surface_z = int(sea_level + height_noise);
    
    // === DETERMINE MATERIAL ===
    uint mat_id;
    float dens;
    float temp;
    
    if (wz >= float(surface_z)) {
        // ABOVE surface = AIR
        mat_id = 0u;
        dens = 1.225;
        temp = 288.0;
    } else {
        // BELOW surface = SOLID ROCK (default)
        float depth = float(surface_z) - wz;
        
        // Layer selection based on depth
        if (depth < 5.0) {
            // Topsoil
            mat_id = 37u; // SOIL
            dens = 1500.0;
        } else if (depth < 15.0) {
            // Sedimentary rock
            mat_id = 32u; // LIMESTONE
            dens = 2500.0;
        } else if (depth < 30.0) {
            // Mixed rock
            float rock_var = noise(vec3(wx * 0.1, wy * 0.1, wz * 0.1));
            mat_id = (rock_var > 0.5) ? 31u : 32u; // BASALT or LIMESTONE
            dens = 2600.0;
        } else {
            // Deep granite with occasional ores
            float ore_noise = noise(vec3(wx * 0.15, wy * 0.15, wz * 0.15 + seed * 3.0));
            if (ore_noise > 0.88) {
                mat_id = 38u; // IRON_ORE
                dens = 5000.0;
            } else if (ore_noise > 0.84) {
                mat_id = 39u; // COPPER_ORE
                dens = 4500.0;
            } else {
                mat_id = 30u; // GRANITE
                dens = 2700.0;
            }
        }
        
        // Geothermal gradient
        temp = 288.0 + depth * 0.025;
        
        // === CAVE CARVING (rare) ===
        // Only carve caves if we're in the cave zone (5-50 blocks deep)
        if (depth > 5.0 && depth < 50.0) {
            // Simple spherical cave pockets
            float cave_noise = fbm(vec3(wx * 0.05, wy * 0.05, wz * 0.07));
            if (cave_noise > 0.62) {
                // Cave! Replace with air
                mat_id = 0u;
                dens = 1.225;
                temp = 290.0 + depth * 0.01; // Cave temp
            }
        }
    }
    
    material[idx] = mat_id;
    temperature[idx] = temp;
    density[idx] = dens;
}
)";

bool TerrainComputeKernel::init(size_t chunk_size) {
    chunk_size_ = chunk_size;
    if (!shader_.load(TERRAIN_GEN_SHADER)) {
        std::cerr << "[GPU] Terrain gen shader failed" << std::endl;
        return false;
    }
    
    size_t num_voxels = chunk_size * chunk_size * chunk_size;
    material_buffer_.create(num_voxels * sizeof(unsigned int));
    temperature_buffer_.create(num_voxels * sizeof(float));
    density_buffer_.create(num_voxels * sizeof(float));
    
    return true;
}

void TerrainComputeKernel::generate_chunk(int world_x, int world_y, int world_z, double seed) {
    shader_.set_uniform("chunk_x", world_x);
    shader_.set_uniform("chunk_y", world_y);
    shader_.set_uniform("chunk_z", world_z);
    shader_.set_uniform("seed", static_cast<float>(seed));
    
    shader_.bind_buffer(0, material_buffer_);
    shader_.bind_buffer(1, temperature_buffer_);
    shader_.bind_buffer(2, density_buffer_);
    
    // 64x64x64 threads -> 16x16x16 groups of 4x4x4
    int groups = 64 / 4;
    shader_.dispatch(groups, groups, groups);
    ComputeShader::barrier();
}

void TerrainComputeKernel::download_chunk(std::vector<uint8_t>& material,
                                          std::vector<double>& temperature,
                                          std::vector<double>& density) {
    size_t n = chunk_size_ * chunk_size_ * chunk_size_;
    
    // Temp CPU buffers for data conversion (GPU uses floats/ints, CPU uses doubles/uint8)
    // Minimally, we allocate here. Optimization: keep permanent scratch buffers.
    std::vector<unsigned int> mat_gpu(n);
    std::vector<float> temp_gpu(n);
    std::vector<float> dens_gpu(n);
    
    material_buffer_.download(mat_gpu.data(), n * sizeof(unsigned int));
    temperature_buffer_.download(temp_gpu.data(), n * sizeof(float));
    density_buffer_.download(dens_gpu.data(), n * sizeof(float));
    
    // Convert
    for (size_t i = 0; i < n; i++) {
        material[i] = static_cast<uint8_t>(mat_gpu[i]);
        temperature[i] = static_cast<double>(temp_gpu[i]);
        density[i] = static_cast<double>(dens_gpu[i]);
    }
}

void TerrainComputeKernel::destroy() {
    material_buffer_.destroy();
    temperature_buffer_.destroy();
    density_buffer_.destroy();
}

} // namespace gpu
} // namespace isolated
