#pragma once

/**
 * @file geology_dynamics.hpp
 * @brief Dynamic geology events.
 *
 * Features:
 * - Structural collapse / cave-ins
 * - Seismic events
 * - Fault lines and hydrothermal vents
 * - Long-term tectonic changes
 */

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace isolated {
namespace worldgen {

// ============================================================================
// STRUCTURAL INTEGRITY
// ============================================================================

struct StructuralCell {
  double integrity = 1.0; // 0-1 (0 = collapsed)
  double load = 0.0;      // Stress on this cell
  double support = 1.0;   // Support from surrounding rock
  bool is_void = false;   // Excavated/cave
  bool collapsed = false;
};

// ============================================================================
// SEISMIC EVENT
// ============================================================================

struct SeismicEvent {
  double x, y, z;   // Epicenter
  double magnitude; // Richter scale
  double depth;     // Depth below surface (m)
  double timestamp; // When it occurred

  double intensity_at(double px, double py, double pz) const {
    double dist = std::sqrt((px - x) * (px - x) + (py - y) * (py - y) +
                            (pz - z) * (pz - z));
    // Intensity decays with distance
    return magnitude / (1.0 + dist / 100.0);
  }
};

// ============================================================================
// FAULT LINE
// ============================================================================

struct FaultLine {
  double x1, y1, x2, y2;   // Surface trace
  double depth = 1000.0;   // Depth extent (m)
  double slip_rate = 0.01; // m/year
  double stress_accumulation = 0.0;
  double last_rupture = 0.0; // Timestamp

  bool has_hydrothermal = false;
  double heat_flux = 0.0; // W/m² along fault

  double distance_to(double px, double py) const {
    // Point-line distance
    double dx = x2 - x1, dy = y2 - y1;
    double t = std::clamp(
        ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    double nx = x1 + t * dx, ny = y1 + t * dy;
    return std::sqrt((px - nx) * (px - nx) + (py - ny) * (py - ny));
  }
};

// ============================================================================
// GEOLOGY DYNAMICS SYSTEM
// ============================================================================

class GeologyDynamicsSystem {
public:
  struct Config {
    double collapse_threshold = 0.3; // Integrity below this = collapse
    double stress_propagation = 0.1; // How stress spreads
    double seismic_damping = 0.9;    // Energy decay per step
    double tectonic_rate = 1e-9;     // Very slow changes
  };

  explicit GeologyDynamicsSystem(const Config &config = Config{})
      : config_(config), rng_(42) {}

  void initialize(size_t nx, size_t ny, size_t nz) {
    nx_ = nx;
    ny_ = ny;
    nz_ = nz;
    cells_.resize(nx * ny * nz);
  }

  void add_fault_line(const FaultLine &fault) { faults_.push_back(fault); }

  /**
   * @brief Update geology dynamics.
   * @param dt Time step (seconds)
   */
  void step(double dt) {
    accumulate_fault_stress(dt);
    check_for_earthquakes();
    propagate_stress();
    check_collapses();
  }

  // === Cave-in mechanics ===

  void excavate(size_t x, size_t y, size_t z) {
    auto &cell = at(x, y, z);
    cell.is_void = true;
    cell.integrity = 0.0;

    // Increase load on neighbors
    apply_excavation_stress(x, y, z);
  }

  void add_support(size_t x, size_t y, size_t z, double strength) {
    at(x, y, z).support += strength;
  }

  // === Seismic events ===

  void trigger_earthquake(double x, double y, double z, double magnitude) {
    SeismicEvent event{x, y, z, magnitude, z, current_time_};
    events_.push_back(event);

    // Apply damage
    apply_seismic_damage(event);
  }

  // === Accessors ===

  StructuralCell &at(size_t x, size_t y, size_t z) {
    return cells_[x + nx_ * (y + ny_ * z)];
  }

  const StructuralCell &at(size_t x, size_t y, size_t z) const {
    return cells_[x + nx_ * (y + ny_ * z)];
  }

  const std::vector<SeismicEvent> &recent_events() const { return events_; }
  const std::vector<FaultLine> &faults() const { return faults_; }

  std::vector<std::tuple<size_t, size_t, size_t>> get_collapsed_cells() const {
    std::vector<std::tuple<size_t, size_t, size_t>> result;
    for (size_t z = 0; z < nz_; ++z) {
      for (size_t y = 0; y < ny_; ++y) {
        for (size_t x = 0; x < nx_; ++x) {
          if (at(x, y, z).collapsed) {
            result.push_back({x, y, z});
          }
        }
      }
    }
    return result;
  }

private:
  Config config_;
  size_t nx_ = 0, ny_ = 0, nz_ = 0;
  std::vector<StructuralCell> cells_;
  std::vector<FaultLine> faults_;
  std::vector<SeismicEvent> events_;
  std::mt19937 rng_;
  double current_time_ = 0.0;

  void accumulate_fault_stress(double dt) {
    for (auto &fault : faults_) {
      fault.stress_accumulation += fault.slip_rate * dt / (365.25 * 86400.0);
    }
  }

  void check_for_earthquakes() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (auto &fault : faults_) {
      // Probability increases with accumulated stress
      double prob = fault.stress_accumulation * 0.001;

      if (dist(rng_) < prob) {
        // Rupture!
        double magnitude = 3.0 + fault.stress_accumulation * 2.0;
        magnitude = std::min(magnitude, 8.0);

        double x = (fault.x1 + fault.x2) / 2.0;
        double y = (fault.y1 + fault.y2) / 2.0;

        trigger_earthquake(x, y, fault.depth / 2.0, magnitude);
        fault.stress_accumulation = 0.0;
        fault.last_rupture = current_time_;
      }
    }
  }

  void apply_seismic_damage(const SeismicEvent &event) {
    for (size_t z = 0; z < nz_; ++z) {
      for (size_t y = 0; y < ny_; ++y) {
        for (size_t x = 0; x < nx_; ++x) {
          double intensity = event.intensity_at(x, y, z);
          auto &cell = at(x, y, z);

          // Damage proportional to intensity
          if (intensity > 0.5) {
            cell.integrity -= (intensity - 0.5) * 0.1;
            cell.load += intensity * 0.2;
          }
        }
      }
    }
  }

  void apply_excavation_stress(size_t cx, size_t cy, size_t cz) {
    // Stress redistributes to surrounding cells
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0 && dz == 0)
            continue;

          int nx = cx + dx, ny = cy + dy, nz = cz + dz;
          if (nx < 0 || ny < 0 || nz < 0)
            continue;
          if ((size_t)nx >= nx_ || (size_t)ny >= ny_ || (size_t)nz >= nz_)
            continue;

          at(nx, ny, nz).load += 0.1;
        }
      }
    }
  }

  void propagate_stress() {
    // Stress spreads from high-load to low-load cells
    for (size_t z = 0; z < nz_; ++z) {
      for (size_t y = 0; y < ny_; ++y) {
        for (size_t x = 0; x < nx_; ++x) {
          auto &cell = at(x, y, z);

          // Reduce integrity under load
          if (cell.load > cell.support) {
            cell.integrity -=
                (cell.load - cell.support) * config_.stress_propagation;
          }

          // Clamp values
          cell.integrity = std::clamp(cell.integrity, 0.0, 1.0);
          cell.load *= 0.99; // Gradual stress relaxation
        }
      }
    }
  }

  void check_collapses() {
    for (size_t z = 0; z < nz_; ++z) {
      for (size_t y = 0; y < ny_; ++y) {
        for (size_t x = 0; x < nx_; ++x) {
          auto &cell = at(x, y, z);

          if (!cell.collapsed && cell.integrity < config_.collapse_threshold) {
            cell.collapsed = true;
            cell.is_void = true;

            // Cascade: add load to cells below
            if (z > 0) {
              at(x, y, z - 1).load += 0.5;
            }
          }
        }
      }
    }
  }
};

} // namespace worldgen
} // namespace isolated
