/**
 * @file gpu_compute.cpp
 * @brief OpenGL compute shader implementation using Raylib's rlgl.
 * 
 * Raylib includes OpenGL function loading via GLAD internally.
 * We use rlgl headers which expose the GL functions.
 */

#include <isolated/gpu/gpu_compute.hpp>

// Raylib's external/glad.h is included via rlgl.h
// Define RLGL_IMPLEMENTATION only once in the project
#define GRAPHICS_API_OPENGL_43  // Enable OpenGL 4.3+ features
#include "external/glad.h"  // Included in raylib source

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

} // namespace gpu
} // namespace isolated
