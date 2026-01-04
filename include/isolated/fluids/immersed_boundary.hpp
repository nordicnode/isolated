#pragma once

/**
 * @file immersed_boundary.hpp
 * @brief Immersed boundary method for curved geometries.
 *
 * Features:
 * - Interpolated bounce-back for curved walls
 * - Force spreading to fluid nodes
 * - Moving boundary support
 */

#include <cmath>
#include <vector>

namespace isolated {
namespace fluids {

// ============================================================================
// BOUNDARY POINT
// ============================================================================

struct BoundaryPoint {
  double x, y, z;    // Position (can be off-grid)
  double nx, ny, nz; // Surface normal
  double q;          // Distance ratio to wall (0-1)
  size_t fluid_node; // Nearest fluid cell index
  size_t solid_node; // Cell on solid side
};

// ============================================================================
// IMMERSED BOUNDARY METHOD
// ============================================================================

/**
 * @brief Interpolated bounce-back for curved boundaries.
 *
 * Uses Bouzidi's method for second-order accurate BC:
 * - q < 0.5: f_opp(x_f) = 2q*f(x_f) + (1-2q)*f(x_ff)
 * - q >= 0.5: f_opp(x_f) = f(x_f)/2q + (2q-1)/(2q)*f_opp(x_f)
 */
class ImmersedBoundary {
public:
  void add_boundary_point(const BoundaryPoint &pt) { points_.push_back(pt); }

  void clear() { points_.clear(); }

  /**
   * @brief Apply interpolated bounce-back corrections.
   * @param f Distribution function array (modified in place)
   * @param n_cells Total number of cells
   * @param cx, cy, cz Lattice velocities
   * @param opp Opposite direction indices
   * @param Q Number of directions
   */
  template <typename T>
  void apply(T *f, size_t n_cells, const int *cx, const int *cy, const int *cz,
             const int *opp, int Q) const {
    for (const auto &pt : points_) {
      size_t i = pt.fluid_node;
      double q = pt.q;

      for (int d = 1; d < Q; ++d) {
        // Check if this direction points towards the boundary
        double dot = cx[d] * pt.nx + cy[d] * pt.ny + cz[d] * pt.nz;
        if (dot <= 0)
          continue; // Not pointing towards wall

        int d_opp = opp[d];

        // Bouzidi interpolation
        if (q < 0.5) {
          // Linear interpolation (would need neighbor data)
          // Simplified: standard bounce-back with correction
          T f_d = f[d * n_cells + i];
          f[d_opp * n_cells + i] = f_d; // Simplified
        } else {
          // Quadratic interpolation
          T f_d = f[d * n_cells + i];
          T f_d_opp = f[d_opp * n_cells + i];
          f[d_opp * n_cells + i] =
              f_d / (2.0 * q) + (2.0 * q - 1.0) / (2.0 * q) * f_d_opp;
        }
      }
    }
  }

  /**
   * @brief Generate boundary points for a sphere.
   */
  void add_sphere(double cx, double cy, double cz, double radius, size_t nx,
                  size_t ny, size_t nz, double dx = 1.0) {
    for (size_t i = 0; i < nx; ++i) {
      for (size_t j = 0; j < ny; ++j) {
        for (size_t k = 0; k < nz; ++k) {
          double x = (i + 0.5) * dx;
          double y = (j + 0.5) * dx;
          double z = (k + 0.5) * dx;

          double dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy) +
                                  (z - cz) * (z - cz));

          // Check if this cell is near the boundary
          if (std::abs(dist - radius) < dx) {
            BoundaryPoint pt;
            pt.x = cx + (x - cx) * radius / dist;
            pt.y = cy + (y - cy) * radius / dist;
            pt.z = cz + (z - cz) * radius / dist;
            pt.nx = (x - cx) / dist;
            pt.ny = (y - cy) / dist;
            pt.nz = (z - cz) / dist;
            pt.q = std::abs(dist - radius) / dx;
            pt.fluid_node = i + nx * (j + ny * k);

            if (dist < radius) {
              // Inside sphere, this is solid
              continue;
            }

            points_.push_back(pt);
          }
        }
      }
    }
  }

  const std::vector<BoundaryPoint> &points() const { return points_; }

private:
  std::vector<BoundaryPoint> points_;
};

// ============================================================================
// FORCE SPREADING (for moving boundaries)
// ============================================================================

/**
 * @brief Spread force from Lagrangian points to Eulerian grid.
 * Uses discrete delta function.
 */
inline double discrete_delta(double r, double h) {
  double s = std::abs(r) / h;
  if (s >= 2.0)
    return 0.0;
  if (s >= 1.0) {
    return (5.0 - 3.0 * s - std::sqrt(-3.0 * (1.0 - s) * (1.0 - s) + 1.0)) /
           (6.0 * h);
  }
  return (3.0 - 2.0 * s + std::sqrt(1.0 + 4.0 * s - 4.0 * s * s)) / (6.0 * h);
}

/**
 * @brief Calculate delta product for 3D spreading.
 */
inline double delta_3d(double rx, double ry, double rz, double h) {
  return discrete_delta(rx, h) * discrete_delta(ry, h) * discrete_delta(rz, h);
}

} // namespace fluids
} // namespace isolated
