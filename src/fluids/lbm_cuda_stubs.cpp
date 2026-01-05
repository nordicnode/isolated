/**
 * @file lbm_cuda_stubs.cpp
 * @brief Stub implementations for CUDA functions when building without CUDA.
 * 
 * These are no-op implementations that allow the project to link
 * when CUDA is not available.
 */

#include <isolated/fluids/lbm_cuda.cuh>

namespace isolated {
namespace fluids {
namespace cuda {

// LBMDeviceBuffers stubs
void LBMDeviceBuffers::allocate(size_t cells) {
  n_cells = cells;
  initialized = false; // Never initialized without CUDA
}

void LBMDeviceBuffers::free() {
  // No-op without CUDA
  initialized = false;
}

// SparseReadbackBuffers stubs
void SparseReadbackBuffers::allocate(size_t max_agents) {
  capacity = max_agents;
  initialized = false;
}

void SparseReadbackBuffers::free() {
  // No-op without CUDA
  initialized = false;
}

// Function stubs
void upload_constants() {
  // No-op without CUDA
}

void launch_compute_macroscopic(LBMDeviceBuffers &buf, size_t nx, size_t ny,
                                size_t nz, cudaStream_t stream) {
  // No-op without CUDA
}

void launch_collide_stream(LBMDeviceBuffers &buf, double omega, size_t nx,
                           size_t ny, size_t nz, cudaStream_t stream) {
  // No-op without CUDA
}

void copy_to_device(LBMDeviceBuffers &buf, const std::vector<double> &rho_host,
                    const std::vector<double> &ux_host,
                    const std::vector<double> &uy_host,
                    const std::vector<double> &uz_host,
                    const std::vector<uint8_t> &solid_host) {
  // No-op without CUDA
}

void copy_from_device(LBMDeviceBuffers &buf, std::vector<double> &rho_host,
                      std::vector<double> &ux_host,
                      std::vector<double> &uy_host,
                      std::vector<double> &uz_host) {
  // No-op without CUDA
}

void device_synchronize() {
  // No-op without CUDA
}

void gather_samples(const LBMDeviceBuffers &fluid_buf,
                    SparseReadbackBuffers &sparse_buf,
                    const std::vector<size_t> &cell_indices,
                    std::vector<FluidSample> &out_samples, size_t nx,
                    size_t ny) {
  // No-op without CUDA - the CPU path in sample_at_positions handles this
}

} // namespace cuda
} // namespace fluids
} // namespace isolated
