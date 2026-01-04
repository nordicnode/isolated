#pragma once

/**
 * @file worldgen.hpp
 * @brief Procedural world generation: noise, geology, caverns, minerals.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace isolated {
namespace worldgen {

// ============================================================================
// NOISE GENERATION
// ============================================================================

/**
 * @brief Simplex/Perlin noise generator for procedural terrain.
 */
class NoiseGenerator {
public:
  explicit NoiseGenerator(uint32_t seed = 42);

  double noise2d(double x, double y) const;
  double noise3d(double x, double y, double z) const;

  // Fractal Brownian Motion
  double fbm2d(double x, double y, int octaves = 4, double lacunarity = 2.0,
               double persistence = 0.5) const;
  double fbm3d(double x, double y, double z, int octaves = 4,
               double lacunarity = 2.0, double persistence = 0.5) const;

private:
  std::array<uint8_t, 512> perm_;

  static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
  static double lerp(double a, double b, double t) { return a + t * (b - a); }
  static double grad(int hash, double x, double y);
  static double grad(int hash, double x, double y, double z);
};

// ============================================================================
// GEOLOGY SYSTEM
// ============================================================================

enum class RockType : uint8_t {
  AIR = 0,
  REGOLITH = 1,
  BASALT = 2,
  GRANITE = 3,
  ICE = 4,
  ORE_IRON = 5,
  ORE_URANIUM = 6,
  ORE_WATER_ICE = 7
};

/**
 * @brief Geology layer generator.
 */
class GeologyGenerator {
public:
  struct Config {
    double base_layer_scale = 0.02;
    double ore_scale = 0.1;
    double ore_threshold = 0.7;
    int ore_frequency = 5; // % chance
  };

  GeologyGenerator(size_t width, size_t height, size_t depth,
                   const Config &config, uint32_t seed = 42);

  void generate();

  RockType get(size_t x, size_t y, size_t z) const;
  const std::vector<RockType> &get_grid() const { return grid_; }

private:
  size_t width_, height_, depth_;
  Config config_;
  NoiseGenerator noise_;
  std::vector<RockType> grid_;
  std::mt19937 rng_;

  size_t idx(size_t x, size_t y, size_t z) const {
    return x + width_ * (y + height_ * z);
  }
};

// ============================================================================
// CAVERN SYSTEM
// ============================================================================

/**
 * @brief Cavern and tunnel generator using cellular automata.
 */
class CavernGenerator {
public:
  struct Config {
    double initial_fill = 0.45;
    int smoothing_iterations = 5;
    int birth_threshold = 5;
    int death_threshold = 4;
  };

  CavernGenerator(size_t width, size_t height, const Config &config,
                  uint32_t seed = 42);

  void generate();
  void smooth();

  bool is_open(size_t x, size_t y) const;
  const std::vector<bool> &get_map() const { return map_; }

private:
  size_t width_, height_;
  Config config_;
  std::vector<bool> map_;
  std::mt19937 rng_;

  int count_neighbors(size_t x, size_t y) const;
};

// ============================================================================
// MINERAL DEPOSIT
// ============================================================================

struct MineralDeposit {
  size_t x, y, z;
  RockType type;
  double quantity; // kg
  double purity;   // 0-1
};

/**
 * @brief Mineral deposit tracker and generator.
 */
class MineralSystem {
public:
  MineralSystem(size_t width, size_t height, size_t depth);

  void generate_deposits(const GeologyGenerator &geology, size_t count = 20);

  const std::vector<MineralDeposit> &get_deposits() const { return deposits_; }

  double extract(size_t deposit_idx, double amount_kg);

private:
  size_t width_, height_, depth_;
  std::vector<MineralDeposit> deposits_;
  std::mt19937 rng_{42};
};

} // namespace worldgen
} // namespace isolated
