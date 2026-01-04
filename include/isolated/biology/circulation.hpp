#pragma once

/**
 * @file circulation.hpp
 * @brief Cardiovascular model with Windkessel and Frank-Starling mechanics.
 */

#include <array>
#include <cmath>
#include <string>
#include <unordered_map>

namespace isolated {
namespace biology {

/**
 * @brief Hemorrhage classification.
 */
enum class HemorrhageClass {
  NONE = 0,
  CLASS_I = 1,   // <15% loss
  CLASS_II = 2,  // 15-30% loss
  CLASS_III = 3, // 30-40% loss
  CLASS_IV = 4   // >40% loss
};

/**
 * @brief Blood vessel segment.
 */
struct VesselSegment {
  std::string name;
  double diameter_mm;
  double length_mm;
  double wall_thickness_mm;
  double elasticity_kpa;
  double blood_volume_ml;
  bool is_severed = false;

  double get_bleed_rate_ml_min(double pressure_mmhg) const {
    if (!is_severed)
      return 0.0;
    double radius_cm = diameter_mm / 20.0;
    return 60.0 * pressure_mmhg * std::pow(radius_cm, 4) * 100.0;
  }
};

/**
 * @brief Windkessel circulation model with Frank-Starling mechanics.
 */
class WindkesselCirculation {
public:
  explicit WindkesselCirculation(double body_mass_kg = 70.0);

  // Simulation step
  struct StepResult {
    double blood_volume_L;
    double heart_rate;
    double systolic;
    double diastolic;
    double map;
    double cardiac_output;
    int hemorrhage_class;
    double sao2;
    double oxygen_delivery;
  };

  StepResult step(double dt);

  // Frank-Starling mechanism
  double compute_frank_starling_sv();
  void set_preload(double venous_return_factor);
  void set_afterload(double svr_factor);

  // HRV
  double compute_hrv_interval(double dt);

  // Electrolyte effects
  double compute_arrhythmia_risk();
  bool trigger_arrhythmia_check();

  // Accessors
  std::pair<double, double> get_blood_pressure() const;
  double get_instantaneous_pressure(double time_in_cycle) const;
  HemorrhageClass get_hemorrhage_class() const;

  // Public state
  double heart_rate = 70.0;
  double stroke_volume = 70.0;
  double blood_volume;
  double potassium = 4.0;
  double calcium = 9.5;
  double magnesium = 2.0;

private:
  double body_mass_;
  double max_blood_volume_;
  double ejection_time_ = 0.3;

  // Frank-Starling
  double end_diastolic_volume_ = 120.0;
  double end_systolic_volume_ = 50.0;
  double contractility_ = 1.0;
  double afterload_factor_ = 1.0;

  // HRV
  bool hrv_enabled_ = true;
  double hrv_lf_power_ = 0.5;
  double hrv_hf_power_ = 0.3;
  double rr_interval_;
  double hrv_phase_ = 0.0;

  // Arrhythmia
  double arrhythmia_risk_ = 0.0;

  // Windkessel
  double peripheral_resistance_ = 1.0;
  double arterial_compliance_ = 1.5;

  // 4-chamber
  std::array<double, 4> chamber_volumes_;
  std::array<bool, 4> valve_states_;

  // State
  double cardiac_phase_ = 0.0;

  // Hemoglobin
  double hemoglobin_ = 14.0;
  double ph_ = 7.4;
  double arterial_po2_ = 95.0;

  // Vessels
  std::unordered_map<std::string, VesselSegment> major_vessels_;

  void create_vessel_network();
};

} // namespace biology
} // namespace isolated
