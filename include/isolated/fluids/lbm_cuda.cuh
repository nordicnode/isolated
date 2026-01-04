#pragma once

/**
 * @file lbm_cuda.cuh
 * @brief CUDA interface for LBM fluid solver.
 */

#include <cstdint>
#include <vector>

#ifdef __CUDACC__
#include <cuda_runtime.h>
#else
// Minimal definitions for host compiler
struct double3 {
  double x, y, z;
};
typedef struct CUstream_st *cudaStream_t;
#endif

#include <vector>

namespace isolated {
namespace fluids {
namespace cuda {

struct LBMDeviceBuffers {
  double *f = nullptr;
  double *f_tmp = nullptr;
  double *rho = nullptr;
  double *ux = nullptr;
  double *uy = nullptr;
  double *uz = nullptr;
  uint8_t *solid = nullptr;
  size_t n_cells = 0;
  bool initialized = false;

  void allocate(size_t cells);
  void free();
  ~LBMDeviceBuffers() { free(); }
};

// Copy constraints to constant memory
void upload_constants();

// Compute macroscopic fields on GPU
void launch_compute_macroscopic(LBMDeviceBuffers &buf, size_t nx, size_t ny,
                                size_t nz, cudaStream_t stream = 0);

// Run collide + stream step on GPU
void launch_collide_stream(LBMDeviceBuffers &buf, double omega, size_t nx,
                           size_t ny, size_t nz, cudaStream_t stream = 0);

// Data transfer helpers
void copy_to_device(LBMDeviceBuffers &buf, const std::vector<double> &rho_host,
                    const std::vector<double> &ux_host,
                    const std::vector<double> &uy_host,
                    const std::vector<double> &uz_host,
                    const std::vector<uint8_t> &solid_host);

void copy_from_device(LBMDeviceBuffers &buf, std::vector<double> &rho_host,
                      std::vector<double> &ux_host,
                      std::vector<double> &uy_host,
                      std::vector<double> &uz_host);

void device_synchronize();

// ============================================================================
// SPARSE READBACK (Gather at agent positions)
// ============================================================================

struct FluidSample {
  double rho = 0.0;
  double ux = 0.0;
  double uy = 0.0;
  double uz = 0.0;
  double o2_fraction = 0.21;  // Default air
  double temperature = 293.0; // Default room temp
};

struct SparseReadbackBuffers {
  size_t *d_indices = nullptr;      // Device: flattened cell indices
  FluidSample *d_samples = nullptr; // Device: output samples
  size_t capacity = 0;
  bool initialized = false;

  void allocate(size_t max_agents);
  void free();
  ~SparseReadbackBuffers() { free(); }
};

// Gather fluid samples at specific cell indices
void gather_samples(const LBMDeviceBuffers &fluid_buf,
                    SparseReadbackBuffers &sparse_buf,
                    const std::vector<size_t> &cell_indices,
                    std::vector<FluidSample> &out_samples, size_t nx,
                    size_t ny);

} // namespace cuda
} // namespace fluids
} // namespace isolated
