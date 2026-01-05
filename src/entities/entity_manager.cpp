#include <isolated/entities/entity_manager.hpp>

namespace isolated {
namespace entities {

void EntityManager::init() {
  registry_.clear();
  // Initialize spatial index (200x200 matches default grid size)
  // TODO: Make this configurable or dynamic based on map size
  spatial_index_.init(200, 200);
  
  // Seed the RNG for deterministic spawning
  rng_.seed(seed_);
}

entt::entity EntityManager::spawn_astronaut(float x, float y, int z,
                                            const std::string &name) {
  auto entity = registry_.create();

  registry_.emplace<Position>(entity, x, y, z);
  registry_.emplace<Velocity>(entity, 0.0f, 0.0f);
  registry_.emplace<Astronaut>(entity, name);
  
  // Visuals: '@' symbol, deterministic colors from seeded RNG
  std::uniform_int_distribution<int> dist_rgb(100, 254);
  std::uniform_int_distribution<int> dist_blue(200, 254);
  Color color = {static_cast<unsigned char>(dist_rgb(rng_)),
                 static_cast<unsigned char>(dist_rgb(rng_)),
                 static_cast<unsigned char>(dist_blue(rng_)), 255};
  
  registry_.emplace<Renderable>(entity, '@', color);
  registry_.emplace<Needs>(entity); // Default needs (full O2, hunger, thirst)

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

std::vector<entt::entity> EntityManager::get_entities_in_radius(float x, float y, int z, float radius) const {
  std::vector<entt::entity> result;
  // Broad phase: query spatial index for relevant cells
  int min_x = static_cast<int>(x - radius);
  int max_x = static_cast<int>(x + radius);
  int min_y = static_cast<int>(y - radius);
  int max_y = static_cast<int>(y + radius);

  std::vector<entt::entity> candidates;
  candidates.reserve(16); // Reserve some space
  spatial_index_.query_range(min_x, min_y, max_x, max_y, candidates);

  // Narrow phase: exact distance check
  float r_sq = radius * radius;
  auto view = registry_.view<const Position>();
  
  for (auto entity : candidates) {
      if (!registry_.valid(entity)) continue;
      
      const auto& pos = view.get<const Position>(entity);
      if (pos.z != z) continue;
      
      float dx = pos.x - x;
      float dy = pos.y - y;
      
      if (dx * dx + dy * dy <= r_sq) {
          result.push_back(entity);
      }
  }
  
  return result;
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
