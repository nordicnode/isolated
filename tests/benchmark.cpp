/**
 * @file benchmark.cpp
 * @brief Comprehensive benchmark suite for all simulation systems.
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

// Core systems
#include <isolated/core/constants.hpp>
#include <isolated/fluids/lattice.hpp>
#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/fluids/multiphase.hpp>
#include <isolated/thermal/heat_engine.hpp>
#include <isolated/thermal/materials.hpp>
#include <isolated/worldgen/worldgen.hpp>

// Biology systems
#include <isolated/biology/blood_chemistry.hpp>
#include <isolated/biology/circulation.hpp>
#include <isolated/biology/coagulation.hpp>
#include <isolated/biology/immune.hpp>
#include <isolated/biology/integumentary.hpp>
#include <isolated/biology/metabolism.hpp>
#include <isolated/biology/muscular.hpp>
#include <isolated/biology/nervous.hpp>
#include <isolated/biology/respiration.hpp>
#include <isolated/biology/thermoregulation.hpp>
#include <isolated/biology/ventilation.hpp>

using namespace isolated;
using Clock = std::chrono::high_resolution_clock;

struct BenchmarkResult {
  std::string name;
  double total_ms;
  double per_step_us;
  size_t iterations;
};

template <typename Func>
BenchmarkResult run_benchmark(const std::string &name, size_t iterations,
                              Func &&func) {
  auto start = Clock::now();
  for (size_t i = 0; i < iterations; ++i) {
    func();
  }
  auto end = Clock::now();

  double total_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  double per_step_us = (total_ms * 1000.0) / iterations;

  return {name, total_ms, per_step_us, iterations};
}

void print_result(const BenchmarkResult &r) {
  std::cout << std::left << std::setw(30) << r.name << std::right
            << std::setw(10) << std::fixed << std::setprecision(2) << r.total_ms
            << " ms  " << std::setw(10) << r.per_step_us << " µs/step  ("
            << r.iterations << " iters)\n";
}

int main() {
  std::cout
      << "╔══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║        ISOLATED SIMULATION - BENCHMARK SUITE                 ║\n";
  std::cout
      << "╚══════════════════════════════════════════════════════════════╝\n\n";

  std::vector<BenchmarkResult> results;
  const double dt = 0.01;
  const size_t PHYSICS_ITERS = 100;
  const size_t BIO_ITERS = 10000;

  // =========================================================================
  // PHYSICS BENCHMARKS
  // =========================================================================
  std::cout << "═══ PHYSICS & FLUIDS ═══\n";

  // LBM Fluid Engine (100x100)
  {
    fluids::LBMConfig config;
    config.nx = 100;
    config.ny = 100;
    config.collision_mode = fluids::CollisionMode::MRT;
    config.enable_les = true;
    fluids::LBMEngine lbm(config);

    results.push_back(run_benchmark("LBM 100x100 MRT+LES", PHYSICS_ITERS,
                                    [&]() { lbm.step(dt); }));
    print_result(results.back());
  }

  // LBM Fluid Engine (200x200)
  {
    fluids::LBMConfig config;
    config.nx = 200;
    config.ny = 200;
    config.collision_mode = fluids::CollisionMode::MRT;
    config.enable_les = true;
    fluids::LBMEngine lbm(config);

    results.push_back(run_benchmark("LBM 200x200 MRT+LES", PHYSICS_ITERS / 2,
                                    [&]() { lbm.step(dt); }));
    print_result(results.back());
  }

  // LBM GPU Engine (100x100)
  {
    fluids::LBMConfig config;
    config.nx = 100;
    config.ny = 100;
    config.collision_mode = fluids::CollisionMode::MRT;
    config.use_gpu = true;
    fluids::LBMEngine lbm(config);

    // Warmup GPU
    lbm.step(dt);

    results.push_back(
        run_benchmark("LBM 100x100 [GPU]", PHYSICS_ITERS * 10, [&]() {
          lbm.step(dt);
          lbm.wait();
        }));
    print_result(results.back());
  }

  // LBM GPU Engine (500x500)
  {
    fluids::LBMConfig config;
    config.nx = 500;
    config.ny = 500;
    // GPU scales better, use larger grid to show benefit
    config.collision_mode = fluids::CollisionMode::MRT;
    config.use_gpu = true;
    fluids::LBMEngine lbm(config);

    // Warmup
    lbm.step(dt);

    results.push_back(run_benchmark("LBM 500x500 [GPU]", PHYSICS_ITERS, [&]() {
      lbm.step(dt);
      lbm.wait();
    }));
    print_result(results.back());
  }

  // Sparse Readback Benchmark
  {
    fluids::LBMConfig config;
    config.nx = 500;
    config.ny = 500;
    config.use_gpu = true;
    fluids::LBMEngine lbm(config);
    lbm.step(dt); // Initialize GPU buffers
    lbm.wait();

    // Simulate 100 agent positions
    std::vector<std::pair<size_t, size_t>> agent_pos;
    for (int i = 0; i < 100; ++i) {
      agent_pos.push_back({50 + i, 250});
    }
    std::vector<fluids::cuda::FluidSample> samples;

    results.push_back(
        run_benchmark("Sparse Readback (100 agents)", PHYSICS_ITERS * 10,
                      [&]() { lbm.sample_at_positions(agent_pos, samples); }));
    print_result(results.back());
  }

  // Thermal Engine
  {
    thermal::ThermalConfig config;
    config.nx = 100;
    config.ny = 100;
    config.enable_radiation = true;
    thermal::ThermalEngine thermal(config);
    thermal.set_temperature(50, 50, 0, 500.0);

    results.push_back(run_benchmark("Thermal 100x100 +Radiation", PHYSICS_ITERS,
                                    [&]() { thermal.step(dt); }));
    print_result(results.back());
  }

  // Multiphase - Phase Change
  {
    fluids::PhaseChangeSystem::Config cfg;
    fluids::PhaseChangeSystem phase(100, 100, cfg);
    std::vector<double> h2o(10000, 0.01);
    std::vector<double> temp(10000, 290.0);

    results.push_back(run_benchmark("PhaseChange 100x100", PHYSICS_ITERS,
                                    [&]() { phase.step(dt, h2o, temp); }));
    print_result(results.back());
  }

  // Multiphase - Aerosols
  {
    fluids::AerosolSystem::Config cfg;
    fluids::AerosolSystem aerosol(100, 100, cfg);
    aerosol.spawn_particles(50, 50, 1000);
    std::vector<double> ux(10000, 0.1);
    std::vector<double> uy(10000, 0.05);
    std::vector<uint8_t> solid(10000, 0);

    results.push_back(
        run_benchmark("Aerosols 1000 particles", PHYSICS_ITERS,
                      [&]() { aerosol.step(dt, ux, uy, solid); }));
    print_result(results.back());
  }

  // Multiphase - Combustion
  {
    fluids::CombustionSystem::Config cfg;
    fluids::CombustionSystem fire(100, 100, cfg);
    fire.add_fuel(50, 50, 10.0);
    fire.ignite(50, 50);
    std::vector<double> o2(10000, 0.21);
    std::vector<double> temp(10000, 600.0);

    results.push_back(run_benchmark("Combustion 100x100", PHYSICS_ITERS,
                                    [&]() { fire.step(dt, o2, temp); }));
    print_result(results.back());
  }

  std::cout << "\n═══ WORLD GENERATION ═══\n";

  // Noise Generation
  {
    worldgen::NoiseGenerator noise(42);
    double sum = 0;

    results.push_back(run_benchmark("Perlin Noise 100k samples", 1, [&]() {
      for (int i = 0; i < 100000; ++i) {
        sum += noise.fbm2d(i * 0.01, i * 0.02, 4);
      }
    }));
    print_result(results.back());
  }

  // Geology Generation
  {
    worldgen::GeologyGenerator::Config cfg;
    worldgen::GeologyGenerator geo(64, 64, 32, cfg);

    results.push_back(
        run_benchmark("Geology 64x64x32", 1, [&]() { geo.generate(); }));
    print_result(results.back());
  }

  // Cavern Generation
  {
    worldgen::CavernGenerator::Config cfg;
    worldgen::CavernGenerator cavern(100, 100, cfg);

    results.push_back(run_benchmark("Cavern 100x100 automata", 10,
                                    [&]() { cavern.generate(); }));
    print_result(results.back());
  }

  // =========================================================================
  // BIOLOGY BENCHMARKS
  // =========================================================================
  std::cout << "\n═══ BIOLOGY - CARDIOVASCULAR ═══\n";

  // Circulation
  {
    biology::WindkesselCirculation circ(70.0);

    results.push_back(run_benchmark("Windkessel Circulation", BIO_ITERS,
                                    [&]() { circ.step(dt); }));
    print_result(results.back());
  }

  // Blood Chemistry
  {
    biology::BloodChemistrySystem blood;

    results.push_back(
        run_benchmark("Blood Chemistry", BIO_ITERS, [&]() { blood.step(dt); }));
    print_result(results.back());
  }

  // Coagulation
  {
    biology::CoagulationSystem coag;
    coag.activate_extrinsic_pathway(0.5);
    coag.form_clot(0, 0.5);

    results.push_back(run_benchmark("Coagulation Cascade", BIO_ITERS,
                                    [&]() { coag.step(dt); }));
    print_result(results.back());
  }

  std::cout << "\n═══ BIOLOGY - RESPIRATORY ═══\n";

  // Respiration
  {
    biology::RespiratorySystem::Config cfg;
    biology::RespiratorySystem resp(cfg);

    results.push_back(run_benchmark("Respiratory System", BIO_ITERS,
                                    [&]() { resp.step(dt, 150, 0.3); }));
    print_result(results.back());
  }

  // Ventilation
  {
    biology::VentilationSystem::Config cfg;
    biology::VentilationSystem vent(cfg);

    results.push_back(run_benchmark("Ventilation Mechanics", BIO_ITERS,
                                    [&]() { vent.step(dt, 95, 40, 0.21); }));
    print_result(results.back());
  }

  std::cout << "\n═══ BIOLOGY - METABOLIC ═══\n";

  // Metabolism
  {
    biology::MetabolismSystem::Config cfg;
    biology::MetabolismSystem metab(cfg);

    results.push_back(run_benchmark("Metabolism", BIO_ITERS,
                                    [&]() { metab.step(dt, 1.5, 22.0); }));
    print_result(results.back());
  }

  // Muscular
  {
    biology::MuscularSystem muscle;

    results.push_back(run_benchmark("Muscular System", BIO_ITERS,
                                    [&]() { muscle.step(dt, 1.5, 0.9); }));
    print_result(results.back());
  }

  std::cout << "\n═══ BIOLOGY - REGULATORY ═══\n";

  // Thermoregulation
  {
    biology::ThermoregulationSystem thermo;
    biology::ThermoregulationSystem::Environment env;

    results.push_back(run_benchmark("Thermoregulation", BIO_ITERS,
                                    [&]() { thermo.step(dt, env, 1.2); }));
    print_result(results.back());
  }

  // Nervous (ANS)
  {
    biology::AutonomicNervousSystem ans;

    results.push_back(run_benchmark("Autonomic Nervous", BIO_ITERS, [&]() {
      ans.step(dt, 93, 0.97, 37.0, 90);
    }));
    print_result(results.back());
  }

  std::cout << "\n═══ BIOLOGY - OTHER ═══\n";

  // Immune
  {
    biology::ImmuneSystem immune;
    immune.add_infection(0, 50.0);

    results.push_back(
        run_benchmark("Immune System", BIO_ITERS, [&]() { immune.step(dt); }));
    print_result(results.back());
  }

  // Integumentary
  {
    biology::IntegumentarySystem skin;
    biology::Wound wound;
    wound.area_cm2 = 5.0;
    wound.bleed_rate_ml_min = 10.0;
    skin.add_wound(wound);

    results.push_back(run_benchmark("Integumentary (Wounds)", BIO_ITERS,
                                    [&]() { skin.step(dt, 1.0, 1.0); }));
    print_result(results.back());
  }

  // =========================================================================
  // SUMMARY
  // =========================================================================
  std::cout
      << "\n╔══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║                        SUMMARY                               ║\n";
  std::cout
      << "╚══════════════════════════════════════════════════════════════╝\n\n";

  double total_physics = 0, total_bio = 0;
  for (const auto &r : results) {
    if (r.name.find("LBM") != std::string::npos ||
        r.name.find("Thermal") != std::string::npos ||
        r.name.find("Phase") != std::string::npos ||
        r.name.find("Aerosol") != std::string::npos ||
        r.name.find("Combustion") != std::string::npos) {
      total_physics += r.per_step_us;
    } else if (r.name.find("Perlin") == std::string::npos &&
               r.name.find("Geology") == std::string::npos &&
               r.name.find("Cavern") == std::string::npos) {
      total_bio += r.per_step_us;
    }
  }

  std::cout << "Physics systems per-step:  " << std::fixed
            << std::setprecision(0) << total_physics << " µs\n";
  std::cout << "Biology systems per-step:  " << total_bio << " µs\n";
  std::cout << "Combined per-step total:   " << (total_physics + total_bio)
            << " µs (" << (total_physics + total_bio) / 1000.0 << " ms)\n\n";

  double target_fps = 60.0;
  double budget_us = 1000000.0 / target_fps;
  double usage_percent = (total_physics + total_bio) / budget_us * 100.0;

  std::cout << "Target frame budget (60 FPS): " << std::setprecision(0)
            << budget_us << " µs\n";
  std::cout << "Simulation usage: " << std::setprecision(1) << usage_percent
            << "%\n";

  if (usage_percent < 50) {
    std::cout << "Status: ✅ EXCELLENT - Plenty of headroom for rendering\n";
  } else if (usage_percent < 80) {
    std::cout << "Status: ✅ GOOD - Some headroom available\n";
  } else if (usage_percent < 100) {
    std::cout << "Status: ⚠️  TIGHT - Consider optimization\n";
  } else {
    std::cout << "Status: ❌ OVER BUDGET - Optimization required\n";
  }

  return 0;
}
