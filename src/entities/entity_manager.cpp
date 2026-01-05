#include <isolated/entities/entity_manager.hpp>

namespace isolated {
namespace entities {

void EntityManager::init() {
  registry_.clear();
  // Reserved for system entities or singletons if needed
}

entt::entity EntityManager::spawn_astronaut(float x, float y, int z,
                                            const std::string &name) {
  auto entity = registry_.create();

  registry_.emplace<Position>(entity, x, y, z);
  registry_.emplace<Velocity>(entity, 0.0f, 0.0f);
  registry_.emplace<Astronaut>(entity, name);
  
  // Visuals: '@' symbol, cycling colors for variety
  Color color = {static_cast<unsigned char>(100 + (std::rand() % 155)),
                 static_cast<unsigned char>(100 + (std::rand() % 155)),
                 static_cast<unsigned char>(200 + (std::rand() % 55)), 255};
  
  registry_.emplace<Renderable>(entity, '@', color);

  return entity;
}

void EntityManager::update(double dt) {
  update_movement(dt);
}

void EntityManager::update_movement(double dt) {
  auto view = registry_.view<Position, const Velocity>();
  
  // Simple Euler integration
  float dt_float = static_cast<float>(dt);
  
  view.each([dt_float](Position &pos, const Velocity &vel) {
    pos.x += vel.dx * dt_float;
    pos.y += vel.dy * dt_float;
  });
}

entt::entity EntityManager::get_entity_at(float target_x, float target_y, int target_z, float radius) const {
  auto view = registry_.view<const Position>();
  entt::entity found = entt::null;
  float min_dist_sq = radius * radius;

  for (auto [entity, pos] : view.each()) {
    if (pos.z != target_z) continue;

    float dx = pos.x - target_x;
    float dy = pos.y - target_y;
    float dist_sq = dx * dx + dy * dy;

    if (dist_sq <= min_dist_sq) {
      min_dist_sq = dist_sq;
      found = entity;
    }
  }
  return found;
}

} // namespace entities
} // namespace isolated
