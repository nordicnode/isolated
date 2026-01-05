#pragma once

#include "entt/entt.hpp"
#include <isolated/entities/components.hpp>
#include <isolated/entities/spatial_index.hpp>
#include <string>
#include <random>

namespace isolated {
namespace entities {

/**
 * @brief Manages the ECS registry and high-level entity operations.
 */
class EntityManager {
public:
  EntityManager() = default;
  ~EntityManager() = default;

  // Lifecycle
  void init();

  // Spawning
  entt::entity spawn_astronaut(float x, float y, int z, const std::string &name = "Bob");

  // Systems
  void update(double dt);

  // Queries
  // Queries
  entt::entity get_entity_at(float x, float y, int z, float radius = 0.5f) const;
  
  // Returns all entities within the given radius of the point.
  // Note: Only checks 'Position' components.
  std::vector<entt::entity> get_entities_in_radius(float x, float y, int z, float radius) const;

  // Accessors
  entt::registry &registry() { return registry_; }
  const entt::registry &registry() const { return registry_; }

  size_t count_astronauts() const {
    return registry_.view<const Astronaut>().size();
  }

private:
  entt::registry registry_;
  SpatialIndex spatial_index_;
  
  // Seeded RNG for deterministic spawning
  std::mt19937 rng_;
  uint32_t seed_ = 42;

  // System methods
  void update_movement(double dt);
  void update_spatial_index();
};

} // namespace entities
} // namespace isolated
