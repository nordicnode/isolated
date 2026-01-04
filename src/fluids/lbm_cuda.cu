/**
 * @file lbm_cuda.cu
 * @brief CUDA kernel implementations for LBM.
 */

#include <cstdio>
#include <isolated/fluids/lbm_cuda.cuh>

namespace isolated {
namespace fluids {
namespace cuda {

// ============================================================================
// CONSTANT MEMORY
// ============================================================================

__constant__ int d_cx[19];
__constant__ int d_cy[19];
__constant__ int d_cz[19];
__constant__ double d_w[19];
__constant__ int d_opp[19];

void upload_constants() {
  const int h_cx[19] = {0,  1, -1, 0, 0,  0, 0, 1, -1, 1,
                        -1, 1, -1, 1, -1, 0, 0, 0, 0};
  const int h_cy[19] = {0,  0, 0, 1, -1, 0, 0,  1, 1, -1,
                        -1, 0, 0, 0, 0,  1, -1, 1, -1};
  const int h_cz[19] = {0, 0, 0, 0,  0,  1, -1, 0,  0, 0,
                        0, 1, 1, -1, -1, 1, 1,  -1, -1};
  const double h_w[19] = {1.0 / 3.0,  1.0 / 18.0, 1.0 / 18.0, 1.0 / 18.0,
                          1.0 / 18.0, 1.0 / 18.0, 1.0 / 18.0, 1.0 / 36.0,
                          1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0,
                          1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0,
                          1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0};
  const int h_opp[19] = {0, 2,  1,  4,  3,  6,  5,  10, 9, 8,
                         7, 14, 13, 12, 11, 18, 17, 16, 15};

  cudaMemcpyToSymbol(d_cx, h_cx, 19 * sizeof(int));
  cudaMemcpyToSymbol(d_cy, h_cy, 19 * sizeof(int));
  cudaMemcpyToSymbol(d_cz, h_cz, 19 * sizeof(int));
  cudaMemcpyToSymbol(d_w, h_w, 19 * sizeof(double));
  cudaMemcpyToSymbol(d_opp, h_opp, 19 * sizeof(int));
}

// ============================================================================
// KERNELS
// ============================================================================

__global__ void kernel_init_equilibrium(double *f, const double *rho,
                                        const double *ux, const double *uy,
                                        const double *uz, size_t n_cells) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n_cells)
    return;

  double r = rho[i];
  double vx = ux[i], vy = uy[i], vz = uz[i];
  double u2 = vx * vx + vy * vy + vz * vz;
  double u2_term = 1.0 - 1.5 * u2;

#pragma unroll
  for (int q = 0; q < 19; ++q) {
    double cu = d_cx[q] * vx + d_cy[q] * vy + d_cz[q] * vz;
    f[q * n_cells + i] = d_w[q] * r * (u2_term + 3.0 * cu + 4.5 * cu * cu);
  }
}

__global__ void
kernel_compute_macroscopic(const double *__restrict__ f,
                           double *__restrict__ rho, double *__restrict__ ux,
                           double *__restrict__ uy, double *__restrict__ uz,
                           const uint8_t *__restrict__ solid, size_t n_cells) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n_cells || solid[i])
    return;

  double r = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;

#pragma unroll
  for (int q = 0; q < 19; ++q) {
    double fq = f[q * n_cells + i];
    r += fq;
    vx += d_cx[q] * fq;
    vy += d_cy[q] * fq;
    vz += d_cz[q] * fq;
  }

  rho[i] = r;
  if (r > 1e-10) {
    double inv_r = 1.0 / r;
    ux[i] = vx * inv_r;
    uy[i] = vy * inv_r;
    uz[i] = vz * inv_r;
  } else {
    ux[i] = uy[i] = uz[i] = 0.0;
  }
}

__global__ void kernel_collide_stream(
    const double *__restrict__ f_in, double *__restrict__ f_out,
    double *__restrict__ rho, double *__restrict__ ux, double *__restrict__ uy,
    double *__restrict__ uz, const uint8_t *__restrict__ solid, double omega,
    size_t n_cells, size_t nx, size_t ny, size_t nz) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n_cells)
    return;

  // Stream logic needs coordinates
  size_t x = i % nx;
  size_t y = (i / nx) % ny;
  size_t z = i / (nx * ny);

  if (solid[i]) {
// Bounce-back
#pragma unroll
    for (int q = 1; q < 19; ++q) {
      // Stream towards neighbor, but it hits wall?
      // Standard full-way bounce-back:
      // f_out[q] at 'i' comes from opposite direction of 'i'
      // effectively swapping directions in place
      // BUT for streaming step f_out[j] = f_next
      // It's simpler to just swap directions in the output buffer SAME location
      // No, for streaming we push to neighbors.
      // If we are solid, we don't stream OUT.
      // But we need to handle boundary conditions.
      // Let's implement simple bounce-back:
      // Assume solid cells contain the reflected population
      f_out[q * n_cells + i] = f_in[d_opp[q] * n_cells + i];
    }
    f_out[i] = f_in[i]; // q=0
    return;
  }

  // 1. Compute Macroscopic (on the fly - DO NOT write to global memory here)
  // The separate kernel_compute_macroscopic will write the t+1 state after
  // streaming
  double r = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;
#pragma unroll
  for (int q = 0; q < 19; ++q) {
    double fq = f_in[q * n_cells + i];
    r += fq;
    vx += d_cx[q] * fq;
    vy += d_cy[q] * fq;
    vz += d_cz[q] * fq;
  }

  // Normalize velocity (keep in registers only)
  if (r > 1e-10) {
    double inv = 1.0 / r;
    vx *= inv;
    vy *= inv;
    vz *= inv;
  }

  double u2 = vx * vx + vy * vy + vz * vz;
  double u2_term = 1.0 - 1.5 * u2;

// 2. Collide & Stream
#pragma unroll
  for (int q = 0; q < 19; ++q) {
    double cu = d_cx[q] * vx + d_cy[q] * vy + d_cz[q] * vz;
    double f_eq = d_w[q] * r * (u2_term + 3.0 * cu + 4.5 * cu * cu);

    // BGK Collision
    double f_new =
        f_in[q * n_cells + i] + omega * (f_eq - f_in[q * n_cells + i]);

    // Streaming: push f_new to neighbor j
    size_t dx = (x + d_cx[q] + nx) % nx;
    size_t dy = (y + d_cy[q] + ny) % ny;
    size_t dz = (z + d_cz[q] + nz) % nz;
    size_t j = dx + nx * (dy + ny * dz);

    f_out[q * n_cells + j] = f_new;
  }
}

// ============================================================================
// HOST IMPLEMENTATIONS
// ============================================================================

void LBMDeviceBuffers::allocate(size_t cells) {
  if (initialized && cells == n_cells)
    return;
  free();

  n_cells = cells;
  size_t f_size = 19 * n_cells * sizeof(double);
  size_t field_size = n_cells * sizeof(double);
  size_t solid_size = n_cells * sizeof(uint8_t);

  cudaMalloc(&f, f_size);
  cudaMalloc(&f_tmp, f_size);
  cudaMalloc(&rho, field_size);
  cudaMalloc(&ux, field_size);
  cudaMalloc(&uy, field_size);
  cudaMalloc(&uz, field_size);
  cudaMalloc(&solid, solid_size);

  initialized = true;
}

void LBMDeviceBuffers::free() {
  if (!initialized)
    return;
  cudaFree(f);
  cudaFree(f_tmp);
  cudaFree(rho);
  cudaFree(ux);
  cudaFree(uy);
  cudaFree(uz);
  cudaFree(solid);
  initialized = false;
}

void launch_compute_macroscopic(LBMDeviceBuffers &buf, size_t nx, size_t ny,
                                size_t nz, cudaStream_t stream) {
  int threads = 128;
  int blocks = (buf.n_cells + threads - 1) / threads;
  kernel_compute_macroscopic<<<blocks, threads, 0, stream>>>(
      buf.f, buf.rho, buf.ux, buf.uy, buf.uz, buf.solid, buf.n_cells);
}

void launch_collide_stream(LBMDeviceBuffers &buf, double omega, size_t nx,
                           size_t ny, size_t nz, cudaStream_t stream) {
  int threads = 128;
  int blocks = (buf.n_cells + threads - 1) / threads;
  kernel_collide_stream<<<blocks, threads, 0, stream>>>(
      buf.f, buf.f_tmp, buf.rho, buf.ux, buf.uy, buf.uz, buf.solid, omega,
      buf.n_cells, nx, ny, nz);
  // Swap pointers on device struct? No, that's host side struct.
  std::swap(buf.f, buf.f_tmp);
}

void copy_to_device(LBMDeviceBuffers &buf, const std::vector<double> &rho_host,
                    const std::vector<double> &ux_host,
                    const std::vector<double> &uy_host,
                    const std::vector<double> &uz_host,
                    const std::vector<uint8_t> &solid_host) {
  if (!buf.initialized)
    return;
  cudaMemcpy(buf.rho, rho_host.data(), buf.n_cells * sizeof(double),
             cudaMemcpyHostToDevice);
  cudaMemcpy(buf.ux, ux_host.data(), buf.n_cells * sizeof(double),
             cudaMemcpyHostToDevice);
  cudaMemcpy(buf.uy, uy_host.data(), buf.n_cells * sizeof(double),
             cudaMemcpyHostToDevice);
  cudaMemcpy(buf.uz, uz_host.data(), buf.n_cells * sizeof(double),
             cudaMemcpyHostToDevice);
  cudaMemcpy(buf.solid, solid_host.data(), buf.n_cells * sizeof(uint8_t),
             cudaMemcpyHostToDevice);

  // Initial equilibrium distribution
  int threads = 128;
  int blocks = (buf.n_cells + threads - 1) / threads;
  kernel_init_equilibrium<<<blocks, threads>>>(buf.f, buf.rho, buf.ux, buf.uy,
                                               buf.uz, buf.n_cells);
  cudaDeviceSynchronize();
}

void copy_from_device(LBMDeviceBuffers &buf, std::vector<double> &rho_host,
                      std::vector<double> &ux_host,
                      std::vector<double> &uy_host,
                      std::vector<double> &uz_host) {
  if (!buf.initialized)
    return;
  cudaMemcpy(rho_host.data(), buf.rho, buf.n_cells * sizeof(double),
             cudaMemcpyDeviceToHost);
  cudaMemcpy(ux_host.data(), buf.ux, buf.n_cells * sizeof(double),
             cudaMemcpyDeviceToHost);
  cudaMemcpy(uy_host.data(), buf.uy, buf.n_cells * sizeof(double),
             cudaMemcpyDeviceToHost);
  cudaMemcpy(uz_host.data(), buf.uz, buf.n_cells * sizeof(double),
             cudaMemcpyDeviceToHost);
}

void device_synchronize() { cudaDeviceSynchronize(); }

// ============================================================================
// SPARSE READBACK IMPLEMENTATION
// ============================================================================

__global__ void kernel_gather_samples(const double *__restrict__ rho,
                                      const double *__restrict__ ux,
                                      const double *__restrict__ uy,
                                      const double *__restrict__ uz,
                                      const size_t *__restrict__ indices,
                                      FluidSample *__restrict__ samples,
                                      size_t n_samples) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n_samples)
    return;

  size_t idx = indices[i];
  samples[i].rho = rho[idx];
  samples[i].ux = ux[idx];
  samples[i].uy = uy[idx];
  samples[i].uz = uz[idx];
  // o2_fraction and temperature would come from coupled systems
  // For now, leave at defaults
}

void SparseReadbackBuffers::allocate(size_t max_agents) {
  if (initialized && max_agents <= capacity)
    return;
  free();

  capacity = max_agents;
  cudaMalloc(&d_indices, capacity * sizeof(size_t));
  cudaMalloc(&d_samples, capacity * sizeof(FluidSample));
  initialized = true;
}

void SparseReadbackBuffers::free() {
  if (!initialized)
    return;
  cudaFree(d_indices);
  cudaFree(d_samples);
  d_indices = nullptr;
  d_samples = nullptr;
  initialized = false;
  capacity = 0;
}

void gather_samples(const LBMDeviceBuffers &fluid_buf,
                    SparseReadbackBuffers &sparse_buf,
                    const std::vector<size_t> &cell_indices,
                    std::vector<FluidSample> &out_samples, size_t nx,
                    size_t ny) {
  size_t n = cell_indices.size();
  if (n == 0)
    return;

  // Ensure buffer is large enough
  sparse_buf.allocate(n);

  // Upload indices to GPU
  cudaMemcpy(sparse_buf.d_indices, cell_indices.data(), n * sizeof(size_t),
             cudaMemcpyHostToDevice);

  // Launch gather kernel
  int threads = 128;
  int blocks = (n + threads - 1) / threads;
  kernel_gather_samples<<<blocks, threads>>>(
      fluid_buf.rho, fluid_buf.ux, fluid_buf.uy, fluid_buf.uz,
      sparse_buf.d_indices, sparse_buf.d_samples, n);

  // Download results
  out_samples.resize(n);
  cudaMemcpy(out_samples.data(), sparse_buf.d_samples, n * sizeof(FluidSample),
             cudaMemcpyDeviceToHost);
}

} // namespace cuda
} // namespace fluids
} // namespace isolated
