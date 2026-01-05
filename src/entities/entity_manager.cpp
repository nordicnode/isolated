#include <isolated/entities/entity_manager.hpp>

namespace isolated {
namespace entities {

void EntityManager::init() {
  registry_.clear();
  // Initialize spatial index (200x200 matches default grid size)
  // TODO: Make this configurable or dynamic based on map size
  spatial_index_.init(200, 200);
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
  update_spatial_index();
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
  // Fast lookup using spatial index
  int gx = static_cast<int>(target_x);
  int gy = static_cast<int>(target_y);
  
  const auto& entities = spatial_index_.get_entities_at(gx, gy);
  
  if (entities.empty()) return entt::null;
  
  // If multiple entities in cell, find closest to exact click
  entt::entity found = entt::null;
  float min_dist_sq = radius * radius;
  
  auto view = registry_.view<const Position>();

  for (auto entity : entities) {
      if (!registry_.valid(entity)) continue;
      
      const auto& pos = view.get<const Position>(entity);
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

void EntityManager::update_spatial_index() {
  spatial_index_.clear();
  auto view = registry_.view<const Position>();
  for (auto [entity, pos] : view.each()) {
      spatial_index_.insert(entity, static_cast<int>(pos.x), static_cast<int>(pos.y));
  }
}

} // namespace entities
} // namespace isolated
