/**
 * @file circulation.cpp
 * @brief Implementation of the cardiovascular model.
 */

#include <algorithm>
#include <cmath>
#include <isolated/biology/circulation.hpp>
#include <random>

namespace isolated {
namespace biology {

WindkesselCirculation::WindkesselCirculation(double body_mass_kg)
    : body_mass_(body_mass_kg) {
  blood_volume = body_mass_kg * 0.07;
  max_blood_volume_ = blood_volume;
  rr_interval_ = 60.0 / heart_rate;

  chamber_volumes_ = {25.0, 120.0, 25.0, 120.0}; // RA, RV, LA, LV
  valve_states_ = {false, false, false, false};

  create_vessel_network();
}

void WindkesselCirculation::create_vessel_network() {
  major_vessels_["aorta"] = {"aorta", 25.0, 400.0, 2.0, 500.0, 100.0, false};
  major_vessels_["carotid_left"] = {"carotid_left", 6.0,  100.0, 0.5,
                                    400.0,          10.0, false};
  major_vessels_["carotid_right"] = {"carotid_right", 6.0,  100.0, 0.5,
                                     400.0,           10.0, false};
  major_vessels_["femoral_left"] = {"femoral_left", 8.0,  400.0, 0.8,
                                    350.0,          25.0, false};
  major_vessels_["femoral_right"] = {"femoral_right", 8.0,  400.0, 0.8,
                                     350.0,           25.0, false};
  major_vessels_["vena_cava"] = {"vena_cava", 30.0,  350.0, 1.0,
                                 200.0,       150.0, false};
}

double WindkesselCirculation::compute_frank_starling_sv() {
  double esv = end_systolic_volume_ / contractility_;
  esv *= afterload_factor_;
  double sv = end_diastolic_volume_ - esv;

  const double sv_max = 150.0;
  sv = sv_max * (1.0 - std::exp(-sv / sv_max));

  stroke_volume = std::max(10.0, sv);
  return stroke_volume;
}

void WindkesselCirculation::set_preload(double venous_return_factor) {
  end_diastolic_volume_ = 120.0 * venous_return_factor;
}

void WindkesselCirculation::set_afterload(double svr_factor) {
  afterload_factor_ = svr_factor;
}

double WindkesselCirculation::compute_hrv_interval(double dt) {
  if (!hrv_enabled_) {
    return 60.0 / heart_rate;
  }

  hrv_phase_ += dt;

  double lf_component = hrv_lf_power_ * std::sin(2.0 * M_PI * 0.1 * hrv_phase_);
  double hf_component =
      hrv_hf_power_ * std::sin(2.0 * M_PI * 0.25 * hrv_phase_);

  double base_rr = 60.0 / heart_rate;
  double variability_ms = (lf_component + hf_component) * 50.0 / 1000.0;

  rr_interval_ = base_rr + variability_ms;
  return rr_interval_;
}

double WindkesselCirculation::compute_arrhythmia_risk() {
  double risk = 0.0;

  if (potassium < 3.0)
    risk += 0.5;
  else if (potassium < 3.5)
    risk += 0.2;
  else if (potassium > 6.0)
    risk += 0.7;
  else if (potassium > 5.5)
    risk += 0.3;

  if (calcium < 7.5)
    risk += 0.3;
  else if (calcium < 8.5)
    risk += 0.1;
  else if (calcium > 12.0)
    risk += 0.2;

  if (magnesium < 1.2)
    risk += 0.3;
  else if (magnesium < 1.5)
    risk += 0.1;

  arrhythmia_risk_ = std::min(1.0, risk);
  return arrhythmia_risk_;
}

bool WindkesselCirculation::trigger_arrhythmia_check() {
  if (arrhythmia_risk_ > 0) {
    static std::mt19937 rng(42);
    static std::uniform_real_distribution<> dist(0.0, 1.0);
    return dist(rng) < arrhythmia_risk_ * 0.01;
  }
  return false;
}

std::pair<double, double> WindkesselCirculation::get_blood_pressure() const {
  double volume_factor = blood_volume / max_blood_volume_;
  double resistance_factor = 1.0;
  if (volume_factor < 0.85) {
    resistance_factor = 1.0 + (0.85 - volume_factor) * 2.0;
  }

  double sbp = 120.0 * volume_factor * resistance_factor;
  double dbp = 80.0 * volume_factor * resistance_factor * 0.9;

  return {sbp, dbp};
}

double
WindkesselCirculation::get_instantaneous_pressure(double time_in_cycle) const {
  auto [sbp, dbp] = get_blood_pressure();

  if (time_in_cycle < ejection_time_) {
    double t_frac = time_in_cycle / ejection_time_;
    return dbp + (sbp - dbp) * std::sin(M_PI * t_frac / 2.0);
  } else {
    double t_diastole = time_in_cycle - ejection_time_;
    double tau = peripheral_resistance_ * arterial_compliance_;
    return dbp + (sbp - dbp) * std::exp(-t_diastole / tau);
  }
}

HemorrhageClass WindkesselCirculation::get_hemorrhage_class() const {
  double loss_fraction = 1.0 - (blood_volume / max_blood_volume_);
  if (loss_fraction < 0.05)
    return HemorrhageClass::NONE;
  if (loss_fraction < 0.15)
    return HemorrhageClass::CLASS_I;
  if (loss_fraction < 0.30)
    return HemorrhageClass::CLASS_II;
  if (loss_fraction < 0.40)
    return HemorrhageClass::CLASS_III;
  return HemorrhageClass::CLASS_IV;
}

WindkesselCirculation::StepResult WindkesselCirculation::step(double dt) {
  double cycle_duration = 60.0 / heart_rate;
  cardiac_phase_ = std::fmod(cardiac_phase_ + dt, cycle_duration);

  double current_pressure = get_instantaneous_pressure(cardiac_phase_);
  double total_bleed_rate = 0.0;

  for (const auto &[name, vessel] : major_vessels_) {
    total_bleed_rate += vessel.get_bleed_rate_ml_min(current_pressure);
  }

  double blood_loss_ml = total_bleed_rate * (dt / 60.0);
  blood_volume = std::max(0.0, blood_volume - blood_loss_ml / 1000.0);

  // Compensatory response
  auto hemorrhage = get_hemorrhage_class();
  switch (hemorrhage) {
  case HemorrhageClass::CLASS_II:
    heart_rate = std::min(120.0, heart_rate + 0.5);
    break;
  case HemorrhageClass::CLASS_III:
    heart_rate = std::min(140.0, heart_rate + 1.0);
    break;
  case HemorrhageClass::CLASS_IV:
    heart_rate = std::min(160.0, heart_rate + 2.0);
    break;
  default:
    heart_rate = std::max(70.0, heart_rate - 0.1);
    break;
  }

  auto [sbp, dbp] = get_blood_pressure();
  double map = dbp + (sbp - dbp) / 3.0;
  double cardiac_output = (heart_rate * stroke_volume) / 1000.0;

  // Simplified O2 saturation
  double sao2 = 0.97;
  double cao2 = hemoglobin_ * 1.34 * sao2;
  double oxygen_delivery = cardiac_output * cao2 * 10.0;

  return {blood_volume,
          heart_rate,
          sbp,
          dbp,
          map,
          cardiac_output,
          static_cast<int>(hemorrhage),
          sao2,
          oxygen_delivery};
}

} // namespace biology
} // namespace isolated
