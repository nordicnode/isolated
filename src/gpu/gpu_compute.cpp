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

} // namespace gpu
} // namespace isolated
