/**
 * @file thermal_cuda_stubs.cpp
 * @brief CPU stubs for systems without CUDA.
 */

#include <isolated/thermal/thermal_cuda.cuh>

namespace isolated {
namespace thermal {
namespace cuda {

void ThermalDeviceBuffers::allocate(size_t, size_t, size_t, size_t, double) {}
void ThermalDeviceBuffers::free() {}

void launch_conduction_step(ThermalDeviceBuffers&, double, cudaStream_t) {}
void launch_sources_step(ThermalDeviceBuffers&, double, cudaStream_t) {}
void inject_heat_gpu(ThermalDeviceBuffers&, size_t, size_t, size_t, double, cudaStream_t) {}

void copy_to_device(ThermalDeviceBuffers&,
                    const std::vector<double>&,
                    const std::vector<double>&,
                    const std::vector<double>&,
                    const std::vector<double>&,
                    const std::vector<double>&) {}

void copy_from_device(ThermalDeviceBuffers&, std::vector<double>&) {}

void device_synchronize() {}

} // namespace cuda
} // namespace thermal
} // namespace isolated
