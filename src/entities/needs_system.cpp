#include <isolated/entities/needs_system.hpp>
#include <algorithm>

namespace isolated {
namespace entities {

void NeedsSystem::update(double dt, entt::registry& registry, fluids::LBMEngine& fluids) {
    float dt_f = static_cast<float>(dt);
    
    auto view = registry.view<const Position, Needs>();
    
    for (auto [entity, pos, needs] : view.each()) {
        // Skip dead entities
        if (needs.hypoxia_state == HypoxiaState::DEAD) continue;
        
        // Get ambient O2 at astronaut's position
        int gx = static_cast<int>(pos.x);
        int gy = static_cast<int>(pos.y);
        int gz = pos.z;
        
        // Clamp to grid bounds
        gx = std::clamp(gx, 0, 199);
        gy = std::clamp(gy, 0, 199);
        gz = std::clamp(gz, 0, 0); // Currently only z=0
        
        // Read O2 fraction from LBM
        // O2 fraction = O2 density / total density
        double total_density = fluids.get_density(gx, gy, gz);
        double o2_density = fluids.get_species_density("O2", gx, gy, gz);
        float ambient_o2 = (total_density > 0.0) ? static_cast<float>(o2_density / total_density) : 0.0f;
        
        // === O2 Consumption ===
        // If ambient O2 is sufficient, maintain saturation
        // If ambient O2 is low, saturation drops
        if (ambient_o2 >= MIN_AMBIENT_O2) {
            // Normal breathing: slowly recover O2 saturation
            needs.oxygen = std::min(1.0f, needs.oxygen + 0.1f * dt_f);
        } else {
            // Low O2: saturation drops based on deficit
            float deficit = MIN_AMBIENT_O2 - ambient_o2;
            needs.oxygen = std::max(0.0f, needs.oxygen - O2_CONSUMPTION_RATE * deficit * dt_f * 10.0f);
        }
        
        // Note: CO2 exhale would require modifying the LBM grid which needs additional API
        // For now, we just track hypoxia based on ambient O2
        
        // === Hypoxia State Transitions ===
        HypoxiaState old_state = needs.hypoxia_state;
        
        if (needs.oxygen < HYPOXIA_DEATH_THRESHOLD) {
            needs.hypoxia_state = HypoxiaState::DEAD;
        } else if (needs.oxygen < HYPOXIA_COLLAPSED_THRESHOLD) {
            needs.hypoxia_state = HypoxiaState::COLLAPSED;
        } else if (needs.oxygen < HYPOXIA_CONFUSED_THRESHOLD) {
            needs.hypoxia_state = HypoxiaState::CONFUSED;
        } else {
            needs.hypoxia_state = HypoxiaState::NORMAL;
        }
        
        // TODO: Log state transitions for event system
        (void)old_state; // Unused for now
    }
}

} // namespace entities
} // namespace isolated
