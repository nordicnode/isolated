/**
 * @file worldgen.cpp
 * @brief Implementation of world generation systems.
 */

#include <algorithm>
#include <cmath>
#include <isolated/worldgen/worldgen.hpp>

namespace isolated {
namespace worldgen {

// ============================================================================
// NOISE GENERATOR
// ============================================================================

NoiseGenerator::NoiseGenerator(uint32_t seed) {
  std::mt19937 rng(seed);

  // Initialize permutation table
  for (int i = 0; i < 256; ++i) {
    perm_[i] = static_cast<uint8_t>(i);
  }
  std::shuffle(perm_.begin(), perm_.begin() + 256, rng);

  // Duplicate for overflow
  for (int i = 0; i < 256; ++i) {
    perm_[256 + i] = perm_[i];
  }
}

double NoiseGenerator::grad(int hash, double x, double y) {
  int h = hash & 7;
  double u = h < 4 ? x : y;
  double v = h < 4 ? y : x;
  return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
}

double NoiseGenerator::grad(int hash, double x, double y, double z) {
  int h = hash & 15;
  double u = h < 8 ? x : y;
  double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

double NoiseGenerator::noise2d(double x, double y) const {
  int X = static_cast<int>(std::floor(x)) & 255;
  int Y = static_cast<int>(std::floor(y)) & 255;

  x -= std::floor(x);
  y -= std::floor(y);

  double u = fade(x);
  double v = fade(y);

  int A = perm_[X] + Y;
  int B = perm_[X + 1] + Y;

  return lerp(
      lerp(grad(perm_[A], x, y), grad(perm_[B], x - 1, y), u),
      lerp(grad(perm_[A + 1], x, y - 1), grad(perm_[B + 1], x - 1, y - 1), u),
      v);
}

double NoiseGenerator::noise3d(double x, double y, double z) const {
  int X = static_cast<int>(std::floor(x)) & 255;
  int Y = static_cast<int>(std::floor(y)) & 255;
  int Z = static_cast<int>(std::floor(z)) & 255;

  x -= std::floor(x);
  y -= std::floor(y);
  z -= std::floor(z);

  double u = fade(x);
  double v = fade(y);
  double w = fade(z);

  int A = perm_[X] + Y;
  int AA = perm_[A] + Z;
  int AB = perm_[A + 1] + Z;
  int B = perm_[X + 1] + Y;
  int BA = perm_[B] + Z;
  int BB = perm_[B + 1] + Z;

  return lerp(
      lerp(lerp(grad(perm_[AA], x, y, z), grad(perm_[BA], x - 1, y, z), u),
           lerp(grad(perm_[AB], x, y - 1, z), grad(perm_[BB], x - 1, y - 1, z),
                u),
           v),
      lerp(lerp(grad(perm_[AA + 1], x, y, z - 1),
                grad(perm_[BA + 1], x - 1, y, z - 1), u),
           lerp(grad(perm_[AB + 1], x, y - 1, z - 1),
                grad(perm_[BB + 1], x - 1, y - 1, z - 1), u),
           v),
      w);
}

double NoiseGenerator::fbm2d(double x, double y, int octaves, double lacunarity,
                             double persistence) const {
  double value = 0.0;
  double amplitude = 1.0;
  double frequency = 1.0;
  double max_value = 0.0;

  for (int i = 0; i < octaves; ++i) {
    value += amplitude * noise2d(x * frequency, y * frequency);
    max_value += amplitude;
    amplitude *= persistence;
    frequency *= lacunarity;
  }

  return value / max_value;
}

double NoiseGenerator::fbm3d(double x, double y, double z, int octaves,
                             double lacunarity, double persistence) const {
  double value = 0.0;
  double amplitude = 1.0;
  double frequency = 1.0;
  double max_value = 0.0;

  for (int i = 0; i < octaves; ++i) {
    value += amplitude * noise3d(x * frequency, y * frequency, z * frequency);
    max_value += amplitude;
    amplitude *= persistence;
    frequency *= lacunarity;
  }

  return value / max_value;
}

// ============================================================================
// GEOLOGY GENERATOR
// ============================================================================

GeologyGenerator::GeologyGenerator(size_t width, size_t height, size_t depth,
                                   const Config &config, uint32_t seed)
    : width_(width), height_(height), depth_(depth), config_(config),
      noise_(seed), rng_(seed) {
  grid_.resize(width * height * depth, RockType::AIR);
}

void GeologyGenerator::generate() {
  std::uniform_int_distribution<int> ore_dist(0, 99);

  for (size_t z = 0; z < depth_; ++z) {
    double depth_factor = static_cast<double>(z) / depth_;

    for (size_t y = 0; y < height_; ++y) {
      for (size_t x = 0; x < width_; ++x) {
        // Base terrain using noise
        double n = noise_.fbm3d(x * config_.base_layer_scale,
                                y * config_.base_layer_scale,
                                z * config_.base_layer_scale, 4);

        // Determine rock type based on depth and noise
        RockType type = RockType::AIR;

        if (n + depth_factor > 0.3) {
          if (depth_factor < 0.2) {
            type = RockType::REGOLITH;
          } else if (depth_factor < 0.6) {
            type = RockType::BASALT;
          } else {
            type = RockType::GRANITE;
          }

          // Ore deposits
          if (ore_dist(rng_) < config_.ore_frequency) {
            double ore_n =
                noise_.noise3d(x * config_.ore_scale, y * config_.ore_scale,
                               z * config_.ore_scale);
            if (ore_n > config_.ore_threshold) {
              int ore_type = ore_dist(rng_) % 3;
              switch (ore_type) {
              case 0:
                type = RockType::ORE_IRON;
                break;
              case 1:
                type = RockType::ORE_URANIUM;
                break;
              case 2:
                type = RockType::ORE_WATER_ICE;
                break;
              }
            }
          }
        }

        grid_[idx(x, y, z)] = type;
      }
    }
  }
}

RockType GeologyGenerator::get(size_t x, size_t y, size_t z) const {
  return grid_[idx(x, y, z)];
}

// ============================================================================
// CAVERN GENERATOR
// ============================================================================

CavernGenerator::CavernGenerator(size_t width, size_t height,
                                 const Config &config, uint32_t seed)
    : width_(width), height_(height), config_(config), rng_(seed) {
  map_.resize(width * height, false);
}

void CavernGenerator::generate() {
  std::bernoulli_distribution fill_dist(config_.initial_fill);

  // Initial random fill
  for (size_t i = 0; i < map_.size(); ++i) {
    map_[i] = fill_dist(rng_);
  }

  // Smooth with cellular automata
  for (int i = 0; i < config_.smoothing_iterations; ++i) {
    smooth();
  }
}

void CavernGenerator::smooth() {
  std::vector<bool> new_map = map_;

  for (size_t y = 1; y < height_ - 1; ++y) {
    for (size_t x = 1; x < width_ - 1; ++x) {
      int neighbors = count_neighbors(x, y);
      size_t i = x + width_ * y;

      if (map_[i]) {
        // Currently solid
        new_map[i] = neighbors >= config_.death_threshold;
      } else {
        // Currently open
        new_map[i] = neighbors >= config_.birth_threshold;
      }
    }
  }

  map_ = std::move(new_map);
}

int CavernGenerator::count_neighbors(size_t x, size_t y) const {
  int count = 0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0)
        continue;
      size_t nx = x + dx;
      size_t ny = y + dy;
      if (map_[nx + width_ * ny]) {
        ++count;
      }
    }
  }
  return count;
}

bool CavernGenerator::is_open(size_t x, size_t y) const {
  return !map_[x + width_ * y];
}

// ============================================================================
// MINERAL SYSTEM
// ============================================================================

MineralSystem::MineralSystem(size_t width, size_t height, size_t depth)
    : width_(width), height_(height), depth_(depth) {}

void MineralSystem::generate_deposits(const GeologyGenerator &geology,
                                      size_t count) {
  std::uniform_int_distribution<size_t> x_dist(0, width_ - 1);
  std::uniform_int_distribution<size_t> y_dist(0, height_ - 1);
  std::uniform_int_distribution<size_t> z_dist(0, depth_ - 1);
  std::uniform_real_distribution<double> quantity_dist(100, 10000);
  std::uniform_real_distribution<double> purity_dist(0.2, 0.95);

  deposits_.clear();

  for (size_t i = 0; i < count; ++i) {
    size_t x = x_dist(rng_);
    size_t y = y_dist(rng_);
    size_t z = z_dist(rng_);

    RockType type = geology.get(x, y, z);

    // Only create deposits for ore types
    if (type >= RockType::ORE_IRON) {
      MineralDeposit deposit;
      deposit.x = x;
      deposit.y = y;
      deposit.z = z;
      deposit.type = type;
      deposit.quantity = quantity_dist(rng_);
      deposit.purity = purity_dist(rng_);
      deposits_.push_back(deposit);
    }
  }
}

double MineralSystem::extract(size_t deposit_idx, double amount_kg) {
  if (deposit_idx >= deposits_.size())
    return 0.0;

  MineralDeposit &d = deposits_[deposit_idx];
  double extracted = std::min(d.quantity, amount_kg);
  d.quantity -= extracted;
  return extracted * d.purity;
}

} // namespace worldgen
} // namespace isolated
