#pragma once

/**
 * @file hematology.hpp
 * @brief Hematology system modeling.
 *
 * Features:
 * - RBC production and destruction
 * - Anemia effects on oxygen carrying
 * - Polycythemia at altitude
 * - Hemoglobin variants
 */

#include <algorithm>
#include <cmath>

namespace isolated {
namespace biology {

// ============================================================================
// HEMATOLOGY STATE
// ============================================================================

struct HematologyState {
  // Red blood cells
  double rbc_count = 5.0;        // million/µL (4.5-5.5 normal)
  double hemoglobin = 14.0;      // g/dL (12-17 normal)
  double hematocrit = 0.42;      // fraction (0.37-0.52)
  double reticulocyte_pct = 1.0; // % young RBCs (0.5-2.5)

  // RBC production
  double erythropoietin = 10.0; // mU/mL (normal 4-24)
  double iron_stores = 1000.0;  // mg (1000-3000 normal)
  double b12_level = 400.0;     // pg/mL (200-900)
  double folate_level = 10.0;   // ng/mL (3-17)

  // Pathology
  double hemolysis_rate = 0.0; // 0-1 abnormal destruction
  bool sickle_cell = false;
  bool thalassemia = false;

  // Oxygen carrying capacity
  double o2_capacity() const {
    // 1.34 mL O2 per gram Hb
    return hemoglobin * 1.34; // mL O2 / dL blood
  }

  bool is_anemic() const { return hemoglobin < 12.0; }
  bool is_polycythemic() const { return hematocrit > 0.55; }
};

// ============================================================================
// HEMATOLOGY SYSTEM
// ============================================================================

class HematologySystem {
public:
  struct Config {
    double rbc_lifespan = 120.0;       // days
    double epo_response_time = 7.0;    // days for RBC production
    double altitude_adaptation = 14.0; // days for full adaptation
  };

  explicit HematologySystem(const Config &config = Config{})
      : config_(config) {}

  /**
   * @brief Update hematology state.
   * @param dt Time step (seconds)
   * @param pao2 Arterial oxygen (mmHg)
   * @param kidney_function Kidney function (0-1)
   * @param altitude Altitude (meters)
   */
  void step(double dt, double pao2, double kidney_function, double altitude) {
    update_epo(pao2, kidney_function);
    update_rbc_production(dt);
    update_rbc_destruction(dt);
    update_altitude_adaptation(dt, altitude);
    compute_indices();
  }

  // === Pathology ===

  void apply_blood_loss(double volume_ml) {
    // Acute hemorrhage
    double fraction_lost = volume_ml / 5000.0; // ~5L total blood
    state_.hemoglobin *= (1.0 - fraction_lost);
    state_.rbc_count *= (1.0 - fraction_lost);
    state_.hematocrit *= (1.0 - fraction_lost);
  }

  void apply_hemolysis(double severity) {
    state_.hemolysis_rate = std::min(1.0, state_.hemolysis_rate + severity);
  }

  void apply_iron_deficiency(double severity) {
    state_.iron_stores -= severity * 500.0;
    state_.iron_stores = std::max(0.0, state_.iron_stores);
  }

  void apply_b12_deficiency() { state_.b12_level *= 0.9; }

  void set_sickle_cell(bool trait) {
    state_.sickle_cell = trait;
    if (trait) {
      state_.hemolysis_rate += 0.1; // Chronic hemolysis
    }
  }

  // === Blood transfusion ===

  void transfuse_prbc(double units) {
    // Each unit raises Hgb ~1 g/dL
    state_.hemoglobin += units;
    state_.hematocrit += units * 0.03;
    state_.rbc_count += units * 0.3;
  }

  // === Accessors ===

  const HematologyState &state() const { return state_; }
  HematologyState &state() { return state_; }

  double oxygen_carrying_capacity() const { return state_.o2_capacity(); }
  double anemia_severity() const {
    if (state_.hemoglobin >= 12.0)
      return 0.0;
    return std::min(1.0, (12.0 - state_.hemoglobin) / 6.0);
  }

private:
  Config config_;
  HematologyState state_;
  double altitude_days_ = 0.0;

  void update_epo(double pao2, double kidney_function) {
    // Hypoxia stimulates EPO release from kidneys
    double target_epo = 10.0; // Baseline

    if (pao2 < 70.0) {
      // Hypoxia → increased EPO
      double hypoxia_factor = (70.0 - pao2) / 30.0;
      target_epo += hypoxia_factor * 50.0;
    }

    // Kidney function affects EPO production
    target_epo *= kidney_function;

    // EPO adjusts slowly
    state_.erythropoietin += (target_epo - state_.erythropoietin) * 0.1;
  }

  void update_rbc_production(double dt) {
    // EPO stimulates reticulocyte production
    double production_rate = state_.erythropoietin / 10.0; // Relative to normal

    // Requires iron, B12, folate
    if (state_.iron_stores < 100.0) {
      production_rate *= state_.iron_stores / 100.0;
    }
    if (state_.b12_level < 200.0) {
      production_rate *= state_.b12_level / 200.0;
    }

    // Reticulocyte count indicates production
    state_.reticulocyte_pct = 1.0 + (production_rate - 1.0) * 2.0;
    state_.reticulocyte_pct = std::clamp(state_.reticulocyte_pct, 0.1, 15.0);

    // RBC count increases slowly
    double daily_production = production_rate * 0.01; // 1% per day normally
    state_.rbc_count += daily_production * dt / 86400.0;
    state_.hemoglobin += daily_production * 0.3 * dt / 86400.0;
  }

  void update_rbc_destruction(double dt) {
    // Normal RBC lifespan ~120 days
    double destruction_rate = 1.0 / config_.rbc_lifespan;
    destruction_rate += state_.hemolysis_rate * 0.1; // Pathologic destruction

    state_.rbc_count -= state_.rbc_count * destruction_rate * dt / 86400.0;
    state_.hemoglobin -= state_.hemoglobin * destruction_rate * dt / 86400.0;

    // Minimum values
    state_.rbc_count = std::max(1.0, state_.rbc_count);
    state_.hemoglobin = std::max(3.0, state_.hemoglobin);
  }

  void update_altitude_adaptation(double dt, double altitude) {
    if (altitude > 2500.0) {
      altitude_days_ += dt / 86400.0;

      // Polycythemia develops over weeks
      double adaptation =
          std::min(1.0, altitude_days_ / config_.altitude_adaptation);
      double altitude_factor = (altitude - 2500.0) / 5000.0;

      double target_hct = 0.42 + altitude_factor * adaptation * 0.15;
      state_.hematocrit += (target_hct - state_.hematocrit) * 0.01;
    } else {
      altitude_days_ = 0.0;
      // Return to normal
      state_.hematocrit += (0.42 - state_.hematocrit) * 0.01;
    }
  }

  void compute_indices() {
    // Hematocrit from RBC and Hgb
    state_.hematocrit = state_.rbc_count / 5.0 * 0.42;
    state_.hematocrit = std::clamp(state_.hematocrit, 0.1, 0.7);
  }
};

} // namespace biology
} // namespace isolated
