#pragma once

/**
 * @file gas_transport.hpp
 * @brief Gas transport pathologies: CO poisoning, narcosis, O2 toxicity, DCS.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace isolated {
namespace biology {

/**
 * @brief Decompression sickness bubble location.
 */
enum class BubbleLocation : uint8_t {
  NONE = 0,
  JOINTS = 1,      // Type I - pain only
  SPINAL = 2,      // Type II - neurological
  PULMONARY = 3,   // "Chokes"
  CEREBRAL = 4,    // Arterial gas embolism
  CUTANEOUS = 5    // Skin bends
};

/**
 * @brief Gas transport pathology system.
 */
class GasTransportSystem {
public:
  struct State {
    // Carbon Monoxide Poisoning
    double cohb_percent = 0.5;        // Carboxyhemoglobin (normal <1%)
    double co_exposure_ppm = 0.0;     // Ambient CO level
    double effective_o2_capacity = 1.0; // Fraction of Hb available for O2
    
    // Nitrogen Narcosis
    double pn2_ata = 0.79;            // Partial pressure N2 (atmospheres)
    double narcosis_severity = 0.0;   // 0-1 (Martini's law)
    double cognitive_impairment = 0.0;
    double depth_meters = 0.0;
    
    // Oxygen Toxicity
    double po2_ata = 0.21;            // Partial pressure O2
    double cns_o2_toxicity = 0.0;     // CNS clock (0-1, seizure at 1.0)
    double pulmonary_o2_toxicity = 0.0; // UPTD accumulation
    bool seizure_risk = false;
    
    // Decompression Sickness
    double tissue_n2_loading = 1.0;   // Nitrogen saturation ratio
    double bubble_formation = 0.0;    // Bubble score
    double ascent_rate_m_min = 0.0;
    BubbleLocation dcs_location = BubbleLocation::NONE;
    bool requires_recompression = false;
  };

  GasTransportSystem() = default;

  State step(double dt, double ambient_pressure_ata, double fio2, double fico);

  // CO Poisoning
  void expose_to_co(double ppm);
  double compute_cohb_equilibrium(double co_ppm, double o2_fraction) const;
  double compute_o2_delivery_impairment() const;

  // Nitrogen Narcosis
  void update_depth(double depth_m, double ambient_pressure);
  double compute_narcosis_equivalent_depth(double pn2) const;
  double compute_cognitive_effect() const;

  // Oxygen Toxicity
  void update_po2(double po2_ata);
  double compute_cns_clock_rate(double po2) const;
  double compute_uptd_rate(double po2) const;
  bool check_seizure_risk() const;

  // Decompression Sickness
  void simulate_ascent(double start_depth, double end_depth, double time_min);
  double compute_supersaturation(double tissue_pn2, double ambient_pn2) const;
  void compute_bubble_formation();
  bool needs_decompression_stop(double current_depth) const;

  const State &get_state() const { return state_; }

private:
  State state_;

  // Tissue compartments for decompression (Bühlmann ZHL-16C simplified)
  static constexpr int N_COMPARTMENTS = 6;
  double tissue_n2_[N_COMPARTMENTS] = {0.79, 0.79, 0.79, 0.79, 0.79, 0.79};
  // Half-times in minutes
  static constexpr double HALF_TIMES[N_COMPARTMENTS] = {5.0, 12.5, 27.0, 54.3, 109.0, 180.0};
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void GasTransportSystem::expose_to_co(double ppm) {
  state_.co_exposure_ppm = ppm;
}

inline double GasTransportSystem::compute_cohb_equilibrium(double co_ppm,
                                                            double o2_frac) const {
  // Haldane equation: COHb/O2Hb = M * (pCO/pO2)
  // M ≈ 210-250 (CO affinity is 200-250x that of O2)
  constexpr double M = 230.0;
  double pco = co_ppm / 1e6;  // Convert ppm to fraction
  double ratio = M * (pco / std::max(0.01, o2_frac));
  return 100.0 * ratio / (1.0 + ratio);  // Percent COHb
}

inline double GasTransportSystem::compute_o2_delivery_impairment() const {
  // COHb shifts O2-Hb dissociation curve left AND reduces capacity
  double capacity_loss = state_.cohb_percent / 100.0;
  double left_shift_effect = 1.0 + state_.cohb_percent * 0.02;
  return std::clamp(1.0 - capacity_loss * left_shift_effect, 0.0, 1.0);
}

inline void GasTransportSystem::update_depth(double depth_m, double ambient_p) {
  state_.depth_meters = depth_m;
  state_.pn2_ata = 0.79 * ambient_p;  // Assuming air
}

inline double GasTransportSystem::compute_narcosis_equivalent_depth(double pn2) const {
  // Narcosis starts ~30m (4 ATA), equivalent to ~1 martini per 10m after
  if (pn2 < 3.16) return 0.0;  // ~30m threshold
  return (pn2 - 3.16) / 0.79 * 10.0;  // Equivalent depth beyond 30m
}

inline double GasTransportSystem::compute_cognitive_effect() const {
  // Mental impairment from narcosis
  // EAD 30m: mild euphoria, 50m: severe impairment, 70m: unconsciousness
  double ead = compute_narcosis_equivalent_depth(state_.pn2_ata);
  return std::clamp(ead / 40.0, 0.0, 1.0);
}

inline void GasTransportSystem::update_po2(double po2) {
  state_.po2_ata = po2;
}

inline double GasTransportSystem::compute_cns_clock_rate(double po2) const {
  // NOAA CNS oxygen toxicity limits
  // 1.6 ATA: 45 min limit, 1.4 ATA: 150 min, 1.2 ATA: ~5 hours
  if (po2 <= 0.5) return 0.0;
  if (po2 < 1.0) return 0.0;
  if (po2 < 1.2) return 1.0 / 300.0;   // 300 min to 100%
  if (po2 < 1.4) return 1.0 / 150.0;
  if (po2 < 1.6) return 1.0 / 45.0;
  return 1.0 / 10.0;  // Rapid onset above 1.6 ATA
}

inline double GasTransportSystem::compute_uptd_rate(double po2) const {
  // Unit Pulmonary Toxicity Dose (Lambertsen)
  // UPTD = t * ((pO2 - 0.5) / 0.5)^0.83
  if (po2 <= 0.5) return 0.0;
  return std::pow((po2 - 0.5) / 0.5, 0.83);
}

inline bool GasTransportSystem::check_seizure_risk() const {
  return state_.cns_o2_toxicity > 0.8 || state_.po2_ata > 1.6;
}

inline void GasTransportSystem::simulate_ascent(double start_depth,
                                                 double end_depth,
                                                 double time_min) {
  state_.ascent_rate_m_min = (start_depth - end_depth) / std::max(0.1, time_min);
}

inline double GasTransportSystem::compute_supersaturation(double tissue_pn2,
                                                           double ambient_pn2) const {
  if (ambient_pn2 < 0.01) return 0.0;
  return (tissue_pn2 - ambient_pn2) / ambient_pn2;
}

inline void GasTransportSystem::compute_bubble_formation() {
  double max_super = 0.0;
  double ambient_pn2 = state_.pn2_ata;
  
  for (int i = 0; i < N_COMPARTMENTS; ++i) {
    double super = compute_supersaturation(tissue_n2_[i], ambient_pn2);
    max_super = std::max(max_super, super);
  }
  
  // Bubble formation when supersaturation > ~1.6 (M-value exceeded)
  state_.bubble_formation = std::max(0.0, (max_super - 1.6) / 0.5);
  
  if (state_.bubble_formation > 0.3) {
    state_.dcs_location = BubbleLocation::JOINTS;  // Type I
  }
  if (state_.bubble_formation > 0.6) {
    state_.dcs_location = BubbleLocation::SPINAL;  // Type II
  }
  if (state_.bubble_formation > 0.8) {
    state_.requires_recompression = true;
  }
}

inline bool GasTransportSystem::needs_decompression_stop(double depth) const {
  double ambient_p = 1.0 + depth / 10.0;
  double ceiling = 0.0;
  
  for (int i = 0; i < N_COMPARTMENTS; ++i) {
    // Simplified M-value calculation
    double m_value = 1.6 + 0.5 * (ambient_p - 1.0);
    double min_p = tissue_n2_[i] / m_value;
    ceiling = std::max(ceiling, (min_p - 1.0) * 10.0);
  }
  
  return depth < ceiling;
}

inline GasTransportSystem::State GasTransportSystem::step(
    double dt, double ambient_pressure, double fio2, double fico) {
  
  double dt_min = dt / 60.0;
  
  // === CO Poisoning ===
  if (state_.co_exposure_ppm > 0 || fico > 0) {
    double co_ppm = state_.co_exposure_ppm + fico * 1e6;
    double target_cohb = compute_cohb_equilibrium(co_ppm, fio2);
    // Half-time for CO binding ~4-5 hours
    double tau = 270.0;  // minutes
    state_.cohb_percent += (target_cohb - state_.cohb_percent) * dt_min / tau;
  } else {
    // CO elimination (faster with O2 therapy)
    double elimination_rate = (fio2 > 0.9) ? 30.0 : 270.0;
    state_.cohb_percent *= std::exp(-dt_min / elimination_rate);
  }
  state_.cohb_percent = std::clamp(state_.cohb_percent, 0.0, 80.0);
  state_.effective_o2_capacity = compute_o2_delivery_impairment();
  
  // === Nitrogen Narcosis ===
  state_.pn2_ata = 0.79 * ambient_pressure;
  state_.narcosis_severity = compute_cognitive_effect();
  state_.cognitive_impairment = state_.narcosis_severity;
  
  // === Oxygen Toxicity ===
  state_.po2_ata = fio2 * ambient_pressure;
  
  // CNS clock
  double cns_rate = compute_cns_clock_rate(state_.po2_ata);
  state_.cns_o2_toxicity += cns_rate * dt_min;
  // Recovery when pO2 < 0.5
  if (state_.po2_ata < 0.5) {
    state_.cns_o2_toxicity -= 0.01 * dt_min;
  }
  state_.cns_o2_toxicity = std::clamp(state_.cns_o2_toxicity, 0.0, 1.5);
  
  // Pulmonary toxicity (slower, cumulative)
  double uptd_rate = compute_uptd_rate(state_.po2_ata);
  state_.pulmonary_o2_toxicity += uptd_rate * dt_min;
  
  state_.seizure_risk = check_seizure_risk();
  
  // === Decompression ===
  // Update tissue compartments (Haldane model)
  for (int i = 0; i < N_COMPARTMENTS; ++i) {
    double k = std::log(2.0) / HALF_TIMES[i];
    double target = state_.pn2_ata;
    tissue_n2_[i] += (target - tissue_n2_[i]) * (1.0 - std::exp(-k * dt_min));
  }
  
  // Compute tissue loading (leading compartment)
  double max_loading = 0.0;
  for (int i = 0; i < N_COMPARTMENTS; ++i) {
    max_loading = std::max(max_loading, tissue_n2_[i] / 0.79);
  }
  state_.tissue_n2_loading = max_loading;
  
  compute_bubble_formation();
  
  return state_;
}

} // namespace biology
} // namespace isolated
