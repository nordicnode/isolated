#pragma once

/**
 * @file biomes.hpp
 * @brief Underground biome and ecosystem modeling.
 *
 * Features:
 * - Underground ecosystems (fungus, bacteria, algae)
 * - Bioluminescent regions
 * - Fossil deposits
 * - Environmental zones
 */

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace isolated {
namespace worldgen {

// ============================================================================
// ORGANISM TYPES
// ============================================================================

enum class OrganismType {
  BACTERIA,
  FUNGUS,
  ALGAE,
  LICHEN,
  CAVE_FAUNA,  // Insects, crustaceans
  EXTREMOPHILE // Thermophiles, halophiles
};

struct Organism {
  OrganismType type = OrganismType::BACTERIA;
  double population = 1000.0; // Relative units
  double growth_rate = 0.1;   // Per day
  double death_rate = 0.05;   // Per day

  // Environmental requirements
  double min_temp = 5.0; // °C
  double max_temp = 40.0;
  double min_humidity = 0.3; // 0-1
  double optimal_ph = 7.0;

  // Light requirements
  bool photosynthetic = false;
  bool bioluminescent = false;
  double light_output = 0.0; // lux if bioluminescent

  // Resource production/consumption
  double o2_production = 0.0; // mol/day
  double co2_consumption = 0.0;
  double organic_production = 0.0;
};

// ============================================================================
// ECOSYSTEM
// ============================================================================

struct Ecosystem {
  std::vector<Organism> organisms;

  double temperature = 20.0; // °C
  double humidity = 0.5;     // 0-1
  double ph = 7.0;
  double organic_matter = 100.0; // kg
  double mineral_nutrients = 50.0;

  // Light
  double ambient_light = 0.0;   // lux (0 in deep caves)
  double bioluminescence = 0.0; // Total light from organisms

  // Gas levels
  double o2_level = 0.21;
  double co2_level = 0.0004;
  double h2s_level = 0.0; // Hydrogen sulfide (toxic)

  double total_population() const {
    double sum = 0.0;
    for (const auto &org : organisms) {
      sum += org.population;
    }
    return sum;
  }

  double total_light() const { return ambient_light + bioluminescence; }
};

// ============================================================================
// BIOLUMINESCENT REGION
// ============================================================================

struct BioluminescentRegion {
  size_t x, y, z;
  size_t radius = 10;
  double intensity = 50.0; // lux at center
  std::string dominant_species = "glow_fungus";

  double light_at(size_t px, size_t py, size_t pz) const {
    double dx = static_cast<double>(px) - x;
    double dy = static_cast<double>(py) - y;
    double dz = static_cast<double>(pz) - z;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > radius)
      return 0.0;
    // Inverse square falloff
    return intensity / (1.0 + dist * dist / (radius * radius));
  }
};

// ============================================================================
// FOSSIL DEPOSIT
// ============================================================================

enum class FossilType {
  INVERTEBRATE, // Trilobites, ammonites
  FISH,
  AMPHIBIAN,
  REPTILE,
  MAMMAL,
  PLANT,
  TRACE // Footprints, burrows
};

struct FossilDeposit {
  size_t x, y, z;
  size_t extent = 5;
  FossilType type = FossilType::INVERTEBRATE;
  double age_million_years = 300.0;
  double density = 1.0; // Fossils per m³
  double quality = 0.5; // 0-1 preservation
  bool discovered = false;

  double value() const {
    // Scientific/economic value
    double age_factor = std::log10(age_million_years + 1);
    double quality_factor = quality * quality;
    return age_factor * quality_factor * density * 1000.0;
  }
};

// ============================================================================
// BIOME SYSTEM
// ============================================================================

class BiomeSystem {
public:
  struct Config {
    double ecosystem_update_rate = 86400.0; // Update every day of sim time
    double nutrient_regeneration = 0.01;    // Per day
    double light_decay = 0.1;               // Per meter depth
  };

  BiomeSystem() : rng_(42) { config_ = Config{}; }

  void add_ecosystem(const std::string &id, const Ecosystem &eco) {
    ecosystems_[id] = eco;
  }

  void add_bioluminescent_region(const BioluminescentRegion &region) {
    glow_regions_.push_back(region);
  }

  void add_fossil_deposit(const FossilDeposit &deposit) {
    fossils_.push_back(deposit);
  }

  /**
   * @brief Update all ecosystems.
   * @param dt Time step (seconds)
   */
  void step(double dt) {
    accumulated_time_ += dt;

    if (accumulated_time_ >= config_.ecosystem_update_rate) {
      update_ecosystems();
      accumulated_time_ = 0.0;
    }

    update_bioluminescence();
  }

  // === Ecosystem seeding ===

  void seed_cave_ecosystem(const std::string &id, double depth) {
    Ecosystem eco;
    eco.temperature = 15.0 - depth * 0.01; // Cooler with depth
    eco.humidity = 0.8;
    eco.ambient_light = 0.0;

    // Add cave bacteria
    Organism bacteria;
    bacteria.type = OrganismType::BACTERIA;
    bacteria.population = 1e6;
    bacteria.growth_rate = 0.2;
    eco.organisms.push_back(bacteria);

    // Add cave fungus
    Organism fungus;
    fungus.type = OrganismType::FUNGUS;
    fungus.population = 1000;
    fungus.growth_rate = 0.05;
    fungus.bioluminescent = (depth > 100);
    fungus.light_output = fungus.bioluminescent ? 10.0 : 0.0;
    eco.organisms.push_back(fungus);

    ecosystems_[id] = eco;
  }

  void seed_hydrothermal_ecosystem(const std::string &id, double temperature) {
    Ecosystem eco;
    eco.temperature = temperature;
    eco.humidity = 1.0;
    eco.h2s_level = 0.001; // Sulfur from vents

    // Chemosynthetic bacteria
    Organism chemotroph;
    chemotroph.type = OrganismType::EXTREMOPHILE;
    chemotroph.population = 1e8;
    chemotroph.growth_rate = 0.5;
    chemotroph.min_temp = 50.0;
    chemotroph.max_temp = 120.0;
    chemotroph.o2_production = 0.1; // Some produce O2
    eco.organisms.push_back(chemotroph);

    // Tube worms (simplified as fauna)
    Organism tubeworm;
    tubeworm.type = OrganismType::CAVE_FAUNA;
    tubeworm.population = 100;
    tubeworm.growth_rate = 0.01;
    eco.organisms.push_back(tubeworm);

    ecosystems_[id] = eco;
  }

  // === Fossil generation ===

  void generate_fossil_layer(size_t z, FossilType type, double age) {
    std::uniform_real_distribution<double> pos(0.0, 256.0);
    std::uniform_real_distribution<double> quality(0.1, 0.9);

    int count = 3 + (rng_() % 5);
    for (int i = 0; i < count; ++i) {
      FossilDeposit deposit;
      deposit.x = static_cast<size_t>(pos(rng_));
      deposit.y = static_cast<size_t>(pos(rng_));
      deposit.z = z;
      deposit.type = type;
      deposit.age_million_years = age;
      deposit.quality = quality(rng_);
      deposit.density = 0.5 + quality(rng_);
      fossils_.push_back(deposit);
    }
  }

  // === Queries ===

  double get_bioluminescence_at(size_t x, size_t y, size_t z) const {
    double total = 0.0;
    for (const auto &region : glow_regions_) {
      total += region.light_at(x, y, z);
    }
    return total;
  }

  const FossilDeposit *get_fossil_at(size_t x, size_t y, size_t z) const {
    for (const auto &deposit : fossils_) {
      double dx = static_cast<double>(x) - deposit.x;
      double dy = static_cast<double>(y) - deposit.y;
      double dz = static_cast<double>(z) - deposit.z;
      if (std::abs(dx) <= deposit.extent && std::abs(dy) <= deposit.extent &&
          std::abs(dz) <= deposit.extent) {
        return &deposit;
      }
    }
    return nullptr;
  }

  Ecosystem *get_ecosystem(const std::string &id) {
    auto it = ecosystems_.find(id);
    return it != ecosystems_.end() ? &it->second : nullptr;
  }

  const std::vector<BioluminescentRegion> &glow_regions() const {
    return glow_regions_;
  }
  const std::vector<FossilDeposit> &fossils() const { return fossils_; }

private:
  Config config_;
  std::unordered_map<std::string, Ecosystem> ecosystems_;
  std::vector<BioluminescentRegion> glow_regions_;
  std::vector<FossilDeposit> fossils_;
  std::mt19937 rng_;
  double accumulated_time_ = 0.0;

  void update_ecosystems() {
    for (auto &[id, eco] : ecosystems_) {
      update_single_ecosystem(eco);
    }
  }

  void update_single_ecosystem(Ecosystem &eco) {
    // Nutrient regeneration
    eco.mineral_nutrients +=
        config_.nutrient_regeneration * eco.mineral_nutrients;

    // Update each organism population
    for (auto &org : eco.organisms) {
      // Check environmental compatibility
      double env_factor = calculate_environment_factor(eco, org);

      // Logistic growth
      double carrying_capacity = eco.organic_matter * 100.0;
      double growth = org.growth_rate * env_factor * org.population *
                      (1.0 - org.population / carrying_capacity);
      double death = org.death_rate * org.population;

      org.population += growth - death;
      org.population = std::max(0.0, org.population);

      // Gas exchange
      eco.o2_level += org.o2_production * org.population / 1e9;
      eco.co2_level -= org.co2_consumption * org.population / 1e9;
    }

    // Clamp gas levels
    eco.o2_level = std::clamp(eco.o2_level, 0.0, 0.5);
    eco.co2_level = std::clamp(eco.co2_level, 0.0, 0.1);
  }

  double calculate_environment_factor(const Ecosystem &eco,
                                      const Organism &org) {
    double factor = 1.0;

    // Temperature
    if (eco.temperature < org.min_temp || eco.temperature > org.max_temp) {
      return 0.0;
    }
    double temp_opt = (org.min_temp + org.max_temp) / 2.0;
    factor *= 1.0 - std::abs(eco.temperature - temp_opt) /
                        (org.max_temp - org.min_temp);

    // Humidity
    if (eco.humidity < org.min_humidity) {
      factor *= eco.humidity / org.min_humidity;
    }

    // Light for photosynthetic organisms
    if (org.photosynthetic && eco.total_light() < 10.0) {
      factor *= eco.total_light() / 10.0;
    }

    return std::clamp(factor, 0.0, 1.0);
  }

  void update_bioluminescence() {
    for (auto &[id, eco] : ecosystems_) {
      eco.bioluminescence = 0.0;
      for (const auto &org : eco.organisms) {
        if (org.bioluminescent) {
          eco.bioluminescence += org.light_output * org.population / 1000.0;
        }
      }
    }
  }
};

} // namespace worldgen
} // namespace isolated
