#pragma once

/**
 * @file thermal_cuda.cuh
 * @brief CUDA interface for Thermal Engine (Heat Diffusion).
 */

#include <cstdint>
#include <vector>

#ifdef __CUDACC__
#include <cuda_runtime.h>
#else
// Minimal definitions for host compiler
typedef struct CUstream_st *cudaStream_t;
#endif

namespace isolated {
namespace thermal {
namespace cuda {

/**
 * @brief Device-side buffers for thermal simulation.
 */
struct ThermalDeviceBuffers {
  double *temperature = nullptr;     // Current temperature field
  double *temperature_tmp = nullptr; // Ping-pong buffer
  double *k = nullptr;               // Thermal conductivity
  double *cp = nullptr;              // Specific heat
  double *rho = nullptr;             // Density
  double *heat_sources = nullptr;    // External heat sources
  size_t n_cells = 0;
  size_t nx = 0, ny = 0, nz = 1;
  double dx = 1.0;
  bool initialized = false;

  void allocate(size_t cells, size_t grid_nx, size_t grid_ny, size_t grid_nz, double grid_dx);
  void free();
  ~ThermalDeviceBuffers() { free(); }
};

/**
 * @brief Launch heat conduction kernel (finite difference).
 * Uses 5-point stencil for 2D, 7-point for 3D.
 */
void launch_conduction_step(ThermalDeviceBuffers& buf, double dt, cudaStream_t stream = 0);

/**
 * @brief Launch heat sources kernel (add Q to temperature).
 */
void launch_sources_step(ThermalDeviceBuffers& buf, double dt, cudaStream_t stream = 0);

/**
 * @brief Copy data from host to device.
 */
void copy_to_device(ThermalDeviceBuffers& buf,
                    const std::vector<double>& temperature_host,
                    const std::vector<double>& k_host,
                    const std::vector<double>& cp_host,
                    const std::vector<double>& rho_host,
                    const std::vector<double>& heat_sources_host);

/**
 * @brief Copy temperature from device to host.
 */
void copy_from_device(ThermalDeviceBuffers& buf,
                      std::vector<double>& temperature_host);

/**
 * @brief Inject heat at specific location (for entity heat).
 * @param joules Amount of energy to add.
 */
void inject_heat_gpu(ThermalDeviceBuffers& buf, size_t x, size_t y, size_t z, double joules,
                     cudaStream_t stream = 0);

/**
 * @brief Synchronize GPU.
 */
void device_synchronize();

} // namespace cuda
} // namespace thermal
} // namespace isolated
