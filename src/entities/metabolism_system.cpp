#include <isolated/entities/metabolism_system.hpp>
#include <algorithm>
#include <cmath>

namespace isolated {
namespace entities {

void MetabolismSystem::update(double dt, entt::registry& registry, thermal::ThermalEngine& thermal) {
    float dt_f = static_cast<float>(dt);
    
    // Process all entities with Metabolism component
    auto view = registry.view<const Position, const Velocity, Metabolism>();
    
    for (auto [entity, pos, vel, metabolism] : view.each()) {
        // 1. Calculate metabolic rate based on activity
        float speed = std::sqrt(vel.dx*vel.dx + vel.dy*vel.dy);
        float current_rate = (speed > 0.1f) ? WALKING_METABOLIC_RATE : RESTING_METABOLIC_RATE;
        
        // Update stored rate for UI
        metabolism.metabolic_rate_watts = current_rate;
        
        // 2. Burn calories
        // 1 Watt = 1 Joule/sec
        // 1 kcal = 4184 Joules
        float joules_burned = current_rate * dt_f;
        float kcal_burned = joules_burned / 4184.0f;
        
        metabolism.caloric_balance -= kcal_burned;
        
        // 3. Inject heat into environment (Thermodynamic Coupling)
        int gx = static_cast<int>(pos.x);
        int gy = static_cast<int>(pos.y);
        int gz = pos.z;
        
        // Clamp to grid
        gx = std::clamp(gx, 0, 199);
        gy = std::clamp(gy, 0, 199);
        gz = std::clamp(gz, 0, 0);
        
        // Inject Q = Power * dt
        thermal.inject_heat(gx, gy, gz, joules_burned);
        
        // 4. Update core temperature (Simplified)
        // Ideally: dTemp = (Q_gen - Q_loss) / (mass * cp)
        // For now, assume perfect thermoregulation unless extreme conditions
        // TODO: Add complex thermoregulation model
    }
}

} // namespace entities
} // namespace isolated
