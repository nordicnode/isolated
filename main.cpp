/**
 * @file main.cpp
 * @brief Entry point for the Isolated simulation.
 */

#include <chrono>
#include <iostream>

#include <isolated/biology/blood_chemistry.hpp>
#include <isolated/biology/circulation.hpp>
#include <isolated/core/constants.hpp>
#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/thermal/heat_engine.hpp>

using namespace isolated;

int main() {
  std::cout << "=== Isolated Simulation (C++) ===" << std::endl;
  std::cout << "High-Performance Habitat Simulator" << std::endl;
  std::cout << std::endl;

  // Initialize systems
  std::cout << "[1] Initializing LBM Fluid System..." << std::endl;
  fluids::LBMConfig lbm_config;
  lbm_config.nx = 100;
  lbm_config.ny = 100;
  lbm_config.nz = 1;
  lbm_config.enable_les = true;
  lbm_config.collision_mode = fluids::CollisionMode::MRT;

  fluids::LBMEngine fluids(lbm_config);
  fluids.initialize_uniform({{"O2", 0.21}, {"N2", 0.79}, {"CO2", 0.0004}});
  std::cout << "   LBM: " << lbm_config.nx << "x" << lbm_config.ny
            << " grid, MRT+LES enabled" << std::endl;

  std::cout << "[2] Initializing Thermal System..." << std::endl;
  thermal::ThermalConfig thermal_config;
  thermal_config.nx = 100;
  thermal_config.ny = 100;
  thermal_config.nz = 1;
  thermal_config.enable_radiation = true;

  thermal::ThermalEngine thermal(thermal_config);
  thermal.set_material(50, 50, 0, "granite");
  thermal.set_temperature(50, 50, 0, 500.0);
  std::cout << "   Thermal: Radiation enabled, hot cell at (50,50)"
            << std::endl;

  std::cout << "[3] Initializing Biology Systems..." << std::endl;
  biology::WindkesselCirculation circulation(70.0);
  biology::BloodChemistrySystem blood_chem;
  std::cout << "   Biology: Windkessel + Blood Chemistry initialized"
            << std::endl;

  std::cout << std::endl;
  std::cout << "=== Running Simulation Benchmark ===" << std::endl;

  // Benchmark loop
  const int N_STEPS = 100;
  const double dt = 0.01;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < N_STEPS; ++i) {
    fluids.step(dt);
    thermal.step(dt);
    circulation.step(dt);
    blood_chem.step(dt);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << std::endl;
  std::cout << "Benchmark Results:" << std::endl;
  std::cout << "  Steps: " << N_STEPS << std::endl;
  std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
  std::cout << "  Per step: " << duration.count() / static_cast<double>(N_STEPS)
            << " ms" << std::endl;

  // Final state check
  std::cout << std::endl;
  std::cout << "Final State:" << std::endl;
  std::cout << "  Fluid density at center: " << fluids.get_density(50, 50, 0)
            << std::endl;
  std::cout << "  Temperature at hot cell: "
            << thermal.get_temperature(50, 50, 0) << " K" << std::endl;
  std::cout << "  Heart rate: " << circulation.heart_rate << " bpm"
            << std::endl;
  std::cout << "  Blood pH: " << blood_chem.abg.pH << std::endl;

  std::cout << std::endl;
  std::cout << "=== Simulation Complete ===" << std::endl;

  return 0;
}
