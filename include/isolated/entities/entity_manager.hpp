#pragma once

#include "entt/entt.hpp"
#include <isolated/entities/components.hpp>
#include <string>

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
  entt::entity get_entity_at(float x, float y, int z, float radius = 0.5f) const;

  // Accessors
  entt::registry &registry() { return registry_; }
  const entt::registry &registry() const { return registry_; }

  size_t count_astronauts() const {
    return registry_.view<const Astronaut>().size();
  }

private:
  entt::registry registry_;

  // System methods
  void update_movement(double dt);
};

} // namespace entities
} // namespace isolated
