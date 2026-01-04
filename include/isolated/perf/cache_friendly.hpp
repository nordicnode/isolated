#pragma once

/**
 * @file cache_friendly.hpp
 * @brief Cache-friendly data layouts and access patterns.
 *
 * Features:
 * - Structure of Arrays (SoA) containers
 * - Blocked iteration patterns
 * - Prefetching hints
 * - Alignment utilities
 */

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace isolated {
namespace perf {

// ============================================================================
// ALIGNED ALLOCATION
// ============================================================================

constexpr size_t CACHE_LINE_SIZE = 64;
constexpr size_t SIMD_WIDTH = 32; // AVX2 = 256 bits = 32 bytes

/**
 * @brief Aligned allocator for cache-line alignment.
 */
template <typename T, size_t Alignment = CACHE_LINE_SIZE>
class AlignedAllocator {
public:
  using value_type = T;

  AlignedAllocator() noexcept = default;

  template <typename U>
  AlignedAllocator(const AlignedAllocator<U, Alignment> &) noexcept {}

  T *allocate(size_t n) {
    void *ptr = std::aligned_alloc(Alignment, n * sizeof(T));
    if (!ptr)
      throw std::bad_alloc();
    return static_cast<T *>(ptr);
  }

  void deallocate(T *ptr, size_t) noexcept { std::free(ptr); }

  template <typename U> struct rebind {
    using other = AlignedAllocator<U, Alignment>;
  };
};

template <typename T, typename U, size_t A>
bool operator==(const AlignedAllocator<T, A> &,
                const AlignedAllocator<U, A> &) {
  return true;
}

template <typename T, typename U, size_t A>
bool operator!=(const AlignedAllocator<T, A> &,
                const AlignedAllocator<U, A> &) {
  return false;
}

/// Aligned vector type
template <typename T> using AlignedVector = std::vector<T, AlignedAllocator<T>>;

// ============================================================================
// STRUCTURE OF ARRAYS (SoA)
// ============================================================================

/**
 * @brief SoA container for 3D vector data.
 *
 * Stores x, y, z components in separate contiguous arrays
 * for better cache utilization during vectorized operations.
 */
template <typename T = float> class SoAVec3 {
public:
  AlignedVector<T> x, y, z;

  void resize(size_t n) {
    x.resize(n);
    y.resize(n);
    z.resize(n);
  }

  void reserve(size_t n) {
    x.reserve(n);
    y.reserve(n);
    z.reserve(n);
  }

  size_t size() const { return x.size(); }

  void clear() {
    x.clear();
    y.clear();
    z.clear();
  }

  void push_back(T vx, T vy, T vz) {
    x.push_back(vx);
    y.push_back(vy);
    z.push_back(vz);
  }
};

/**
 * @brief SoA container for particle data.
 */
template <typename T = float> struct SoAParticles {
  AlignedVector<T> x, y, z;    // Position
  AlignedVector<T> vx, vy, vz; // Velocity
  AlignedVector<T> mass;
  AlignedVector<T> radius;
  AlignedVector<uint32_t> type;

  void resize(size_t n) {
    x.resize(n);
    y.resize(n);
    z.resize(n);
    vx.resize(n);
    vy.resize(n);
    vz.resize(n);
    mass.resize(n);
    radius.resize(n);
    type.resize(n);
  }

  size_t size() const { return x.size(); }
};

// ============================================================================
// BLOCKED ITERATION
// ============================================================================

/**
 * @brief Cache-blocked 2D iteration.
 *
 * Iterates over a 2D domain in cache-friendly blocks.
 */
template <typename Func>
void iterate_blocked_2d(size_t nx, size_t ny, size_t block_size, Func &&func) {
  for (size_t by = 0; by < ny; by += block_size) {
    for (size_t bx = 0; bx < nx; bx += block_size) {
      size_t end_y = std::min(by + block_size, ny);
      size_t end_x = std::min(bx + block_size, nx);

      for (size_t y = by; y < end_y; ++y) {
        for (size_t x = bx; x < end_x; ++x) {
          func(x, y);
        }
      }
    }
  }
}

/**
 * @brief Cache-blocked 3D iteration.
 */
template <typename Func>
void iterate_blocked_3d(size_t nx, size_t ny, size_t nz, size_t block_size,
                        Func &&func) {
  for (size_t bz = 0; bz < nz; bz += block_size) {
    for (size_t by = 0; by < ny; by += block_size) {
      for (size_t bx = 0; bx < nx; bx += block_size) {
        size_t end_z = std::min(bz + block_size, nz);
        size_t end_y = std::min(by + block_size, ny);
        size_t end_x = std::min(bx + block_size, nx);

        for (size_t z = bz; z < end_z; ++z) {
          for (size_t y = by; y < end_y; ++y) {
            for (size_t x = bx; x < end_x; ++x) {
              func(x, y, z);
            }
          }
        }
      }
    }
  }
}

// ============================================================================
// PREFETCHING
// ============================================================================

/**
 * @brief Software prefetch hints.
 */
inline void prefetch_read(const void *ptr) {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 0, 3); // Read, high temporal locality
#endif
}

inline void prefetch_write(void *ptr) {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 1, 3); // Write, high temporal locality
#endif
}

inline void prefetch_read_nt(const void *ptr) {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 0, 0); // Read, no temporal locality (streaming)
#endif
}

// ============================================================================
// MEMORY ACCESS PATTERNS
// ============================================================================

/**
 * @brief Calculate optimal block size for cache.
 * @param element_size Size of each element in bytes
 * @param l1_size L1 cache size (default 32KB)
 * @return Block size that fits in L1
 */
constexpr size_t optimal_block_size(size_t element_size,
                                    size_t l1_size = 32768) {
  // Target using ~1/3 of L1 for one block
  size_t target = l1_size / 3;
  size_t elements = target / element_size;

  // Round down to power of 2
  size_t block = 1;
  while (block * 2 <= elements && block * 2 <= 64) {
    block *= 2;
  }
  return block;
}

/**
 * @brief Calculate stride for avoiding cache line conflicts.
 */
constexpr size_t anti_conflict_stride(size_t base_stride, size_t element_size) {
  // Add small offset to avoid power-of-2 conflicts
  const size_t conflict_stride = 4096 / element_size; // Pages
  if (base_stride % conflict_stride == 0) {
    return base_stride + (CACHE_LINE_SIZE / element_size);
  }
  return base_stride;
}

// ============================================================================
// TILE LAYOUT
// ============================================================================

/**
 * @brief Morton (Z-order) encoding for 2D coordinates.
 * Improves cache locality for 2D spatial data.
 */
inline uint32_t morton_encode_2d(uint16_t x, uint16_t y) {
  auto expand = [](uint32_t v) {
    v = (v | (v << 8)) & 0x00FF00FF;
    v = (v | (v << 4)) & 0x0F0F0F0F;
    v = (v | (v << 2)) & 0x33333333;
    v = (v | (v << 1)) & 0x55555555;
    return v;
  };
  return expand(x) | (expand(y) << 1);
}

/**
 * @brief Morton decode.
 */
inline void morton_decode_2d(uint32_t code, uint16_t &x, uint16_t &y) {
  auto compact = [](uint32_t v) {
    v &= 0x55555555;
    v = (v | (v >> 1)) & 0x33333333;
    v = (v | (v >> 2)) & 0x0F0F0F0F;
    v = (v | (v >> 4)) & 0x00FF00FF;
    v = (v | (v >> 8)) & 0x0000FFFF;
    return static_cast<uint16_t>(v);
  };
  x = compact(code);
  y = compact(code >> 1);
}

} // namespace perf
} // namespace isolated
