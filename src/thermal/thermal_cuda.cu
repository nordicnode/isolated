/**
 * @file thermal_cuda.cu
 * @brief CUDA kernels for Thermal Engine heat diffusion.
 */

#include <isolated/thermal/thermal_cuda.cuh>
#include <algorithm>
#include <cstdio>

namespace isolated {
namespace thermal {
namespace cuda {

// =============================================================================
// Device Buffer Management
// =============================================================================

void ThermalDeviceBuffers::allocate(size_t cells, size_t grid_nx, size_t grid_ny, 
                                    size_t grid_nz, double grid_dx) {
    if (initialized && n_cells == cells) return;
    free();
    
    n_cells = cells;
    nx = grid_nx;
    ny = grid_ny;
    nz = grid_nz;
    dx = grid_dx;
    
    size_t bytes = cells * sizeof(double);
    
    cudaMalloc(&temperature, bytes);
    cudaMalloc(&temperature_tmp, bytes);
    cudaMalloc(&k, bytes);
    cudaMalloc(&cp, bytes);
    cudaMalloc(&rho, bytes);
    cudaMalloc(&heat_sources, bytes);
    
    // Initialize to zero
    cudaMemset(temperature, 0, bytes);
    cudaMemset(temperature_tmp, 0, bytes);
    cudaMemset(heat_sources, 0, bytes);
    
    initialized = true;
}

void ThermalDeviceBuffers::free() {
    if (!initialized) return;
    
    cudaFree(temperature);
    cudaFree(temperature_tmp);
    cudaFree(k);
    cudaFree(cp);
    cudaFree(rho);
    cudaFree(heat_sources);
    
    temperature = temperature_tmp = k = cp = rho = heat_sources = nullptr;
    initialized = false;
}

// =============================================================================
// CUDA Kernels
// =============================================================================

/**
 * @brief Heat conduction kernel using 5-point stencil (2D).
 * dT/dt = alpha * laplacian(T)
 * alpha = k / (rho * cp)
 */
__global__ void kernel_conduction_2d(double* __restrict__ T_new,
                                     const double* __restrict__ T_old,
                                     const double* __restrict__ k,
                                     const double* __restrict__ cp,
                                     const double* __restrict__ rho,
                                     size_t nx, size_t ny, double dx, double dt) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x <= 0 || x >= nx - 1 || y <= 0 || y >= ny - 1) return;
    
    size_t idx = x + y * nx;
    
    double alpha = k[idx] / (rho[idx] * cp[idx] + 1e-10);
    double dx2 = dx * dx;
    
    // 5-point stencil Laplacian
    double laplacian = (T_old[idx + 1] + T_old[idx - 1] + 
                        T_old[idx + nx] + T_old[idx - nx] - 
                        4.0 * T_old[idx]) / dx2;
    
    T_new[idx] = T_old[idx] + alpha * laplacian * dt;
}

/**
 * @brief Apply heat sources kernel.
 * dT = Q / (rho * cp * V)
 */
__global__ void kernel_apply_sources(double* __restrict__ T,
                                     const double* __restrict__ sources,
                                     const double* __restrict__ cp,
                                     const double* __restrict__ rho,
                                     size_t n_cells, double dx, double dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_cells) return;
    
    double volume = dx * dx * dx;
    double heat_capacity = rho[idx] * cp[idx] * volume;
    
    if (heat_capacity > 1e-6) {
        double Q = sources[idx] * dt; // Watts * dt = Joules
        T[idx] += Q / heat_capacity;
    }
}

// =============================================================================
// Kernel Launchers
// =============================================================================

void launch_conduction_step(ThermalDeviceBuffers& buf, double dt, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((buf.nx + block.x - 1) / block.x, 
              (buf.ny + block.y - 1) / block.y);
    
    kernel_conduction_2d<<<grid, block, 0, stream>>>(
        buf.temperature_tmp, buf.temperature,
        buf.k, buf.cp, buf.rho,
        buf.nx, buf.ny, buf.dx, dt
    );
    
    // Swap buffers
    double* tmp = buf.temperature;
    buf.temperature = buf.temperature_tmp;
    buf.temperature_tmp = tmp;
}

void launch_sources_step(ThermalDeviceBuffers& buf, double dt, cudaStream_t stream) {
    int threads = 256;
    int blocks = (buf.n_cells + threads - 1) / threads;
    
    kernel_apply_sources<<<blocks, threads, 0, stream>>>(
        buf.temperature, buf.heat_sources,
        buf.cp, buf.rho, buf.n_cells, buf.dx, dt
    );
}

void inject_heat_gpu(ThermalDeviceBuffers& buf, size_t x, size_t y, size_t z, 
                     double joules, cudaStream_t stream) {
    // For simplicity, add to heat_sources buffer (will be applied next step)
    // In production, use atomic add directly to temperature
    size_t idx = x + y * buf.nx + z * buf.nx * buf.ny;
    
    // Copy joules to device and add atomically
    // Simplified: use cudaMemcpy for single value (not optimal but works)
    double current;
    cudaMemcpy(&current, &buf.heat_sources[idx], sizeof(double), cudaMemcpyDeviceToHost);
    current += joules;
    cudaMemcpy(&buf.heat_sources[idx], &current, sizeof(double), cudaMemcpyHostToDevice);
}

// =============================================================================
// Data Transfer
// =============================================================================

void copy_to_device(ThermalDeviceBuffers& buf,
                    const std::vector<double>& temperature_host,
                    const std::vector<double>& k_host,
                    const std::vector<double>& cp_host,
                    const std::vector<double>& rho_host,
                    const std::vector<double>& heat_sources_host) {
    size_t bytes = buf.n_cells * sizeof(double);
    
    cudaMemcpy(buf.temperature, temperature_host.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(buf.k, k_host.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(buf.cp, cp_host.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(buf.rho, rho_host.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(buf.heat_sources, heat_sources_host.data(), bytes, cudaMemcpyHostToDevice);
}

void copy_from_device(ThermalDeviceBuffers& buf, std::vector<double>& temperature_host) {
    size_t bytes = buf.n_cells * sizeof(double);
    cudaMemcpy(temperature_host.data(), buf.temperature, bytes, cudaMemcpyDeviceToHost);
}

void device_synchronize() {
    cudaDeviceSynchronize();
}

} // namespace cuda
} // namespace thermal
} // namespace isolated
