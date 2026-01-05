/**
 * @file main.cpp
 * @brief Entry point for the Isolated simulation with visual renderer.
 */

#include <algorithm>
#include <chrono>
#include <iostream>

#include "raylib.h"

#include <isolated/biology/blood_chemistry.hpp>
#include <isolated/biology/circulation.hpp>
#include <isolated/core/constants.hpp>
#include <isolated/fluids/lbm_engine.hpp>
#include <isolated/renderer/debug_ui.hpp>
#include <isolated/renderer/renderer.hpp>
#include <isolated/thermal/heat_engine.hpp>
#include <isolated/entities/entity_manager.hpp>
#include <isolated/entities/needs_system.hpp>
#include <isolated/entities/metabolism_system.hpp>
#include <isolated/core/lod_zone_manager.hpp>
#include <isolated/world/chunk_manager.hpp>
#include <isolated/world/terrain_generator.hpp>
#include <isolated/gpu/gpu_compute.hpp>

using namespace isolated;

int main() {
  std::cout << "=== Isolated Simulation (C++) ===" << std::endl;
  std::cout << "Initializing systems..." << std::endl;

  // Initialize LBM Fluid System
  fluids::LBMConfig lbm_config;
  lbm_config.nx = 200;  // Reasonable size for CPU simulation
  lbm_config.ny = 200;
  lbm_config.nz = 1;
  lbm_config.enable_les = true;
  lbm_config.collision_mode = fluids::CollisionMode::MRT;
  lbm_config.use_gpu = true; // Use GPU if CUDA available (stubs used otherwise)

  fluids::LBMEngine fluids(lbm_config);
  fluids.initialize_uniform({{"O2", 0.21}, {"N2", 0.79}, {"CO2", 0.0004}});
  std::cout << "[OK] LBM: " << lbm_config.nx << "x" << lbm_config.ny
            << " grid, MRT+LES enabled" << std::endl;

  // Initialize Thermal System
  thermal::ThermalConfig thermal_config;
  thermal_config.nx = 200;
  thermal_config.ny = 200;
  thermal_config.nz = 1;
  thermal_config.enable_radiation = true;
  thermal_config.use_gpu = true; // Use GPU if CUDA available (stubs used otherwise)

  thermal::ThermalEngine thermal(thermal_config);

  // Set up some interesting initial conditions
  // Hot spot in center
  thermal.set_material(100, 100, 0, "granite");
  thermal.set_temperature(100, 100, 0, 500.0);

  // Cold region in corner
  for (int x = 20; x < 40; ++x) {
    for (int y = 20; y < 40; ++y) {
      thermal.set_temperature(x, y, 0, 220.0);
    }
  }

  std::cout << "[OK] Thermal: Radiation enabled, hot/cold zones set"
            << std::endl;

  // Initialize Biology Systems
  biology::WindkesselCirculation circulation(70.0);
  biology::BloodChemistrySystem blood_chem;
  std::cout << "[OK] Biology: Windkessel + Blood Chemistry initialized"
            << std::endl;

  // Initialize Renderer
  renderer::RendererConfig render_config;
  render_config.window_width = 1280;
  render_config.window_height = 720;
  render_config.tile_size = 4;  // 4px per cell
  render_config.title = "Isolated - Visual Simulation";
  render_config.target_fps = 60;

  renderer::Renderer game_renderer;
  game_renderer.init(render_config);
  std::cout << "[OK] Renderer: " << render_config.window_width << "x"
            << render_config.window_height << " window" << std::endl;

  // Initialize Debug UI
  renderer::DebugUI debug_ui;
  debug_ui.init();
  std::cout << "[OK] Debug UI: Dear ImGui initialized" << std::endl;
  
  // Initialize GPU Compute for thermal physics
  gpu::ThermalComputeKernel gpu_thermal;
  bool gpu_thermal_ready = gpu_thermal.init(200, 200);
  if (gpu_thermal_ready) {
    std::cout << "[OK] GPU: Thermal compute kernel ready (200x200)" << std::endl;
    gpu_thermal.upload_temperature(thermal.temperature_field());
  } else {
    std::cout << "[WARN] GPU: Thermal compute shader failed, using CPU fallback" << std::endl;
  }
  
  // Initialize GPU Compute for LBM fluid simulation
  gpu::LBMComputeKernel gpu_lbm;
  bool gpu_lbm_ready = gpu_lbm.init(200, 200);
  if (gpu_lbm_ready) {
    std::cout << "[OK] GPU: LBM compute kernel ready (200x200)" << std::endl;
  } else {
    std::cout << "[WARN] GPU: LBM compute shader failed, using CPU fallback" << std::endl;
  }

  // Initialize Entity Manager (ECS)
  entities::EntityManager entity_manager;
  entity_manager.init();
  
  // Spawn test entities
  entity_manager.spawn_astronaut(50, 50, 51, "Bob");       // Z=51 is surface level
  entity_manager.spawn_astronaut(60, 40, 51, "Alice");
  entity_manager.spawn_astronaut(100, 100, 51, "Commander");
  
  std::cout << "[OK] ECS: EnTT initialized, 3 astronauts spawned" << std::endl;
  
  // Initialize LOD Zone Manager for physics optimization (Temporal slicing)
  core::LODConfig lod_config;
  lod_config.nx = 200;
  lod_config.ny = 200;
  lod_config.num_regions = 4;      // Grid divided into 4 quadrants
  lod_config.viewport_padding = 50; // Cells around camera always update
  core::LODZoneManager lod_manager(lod_config);
  std::cout << "[OK] LOD: Temporal slicing (4 regions, viewport priority)" << std::endl;
  
  // Initialize Chunk-based World System (for massive worlds)
  world::TerrainConfig terrain_config;
  terrain_config.seed = 42;
  terrain_config.terrain_scale = 0.02;
  terrain_config.height_amplitude = 30.0;
  terrain_config.sea_level = 50.0;  // Surface at Z=50±30, so always Z > 20
  world::TerrainGenerator terrain_gen(terrain_config);
  
  world::ChunkManagerConfig chunk_config;
  chunk_config.load_radius = 1;      // 3x3x1 = 9 chunks (minimal for performance)
  chunk_config.unload_radius = 2;    // Unload quickly
  chunk_config.max_loaded = 30;      // Cap memory usage
  chunk_config.save_path = "./world_data/";
  world::ChunkManager chunk_manager(chunk_config);
  
  // Initialize GPU Terrain Generator
  static gpu::TerrainComputeKernel gpu_terrain;
  bool gpu_terrain_ready = gpu_terrain.init(64); // 64^3 chunk size
  if (gpu_terrain_ready) {
      std::cout << "[OK] GPU: Terrain compute kernel ready" << std::endl;
  } else {
      std::cout << "[WARN] GPU: Terrain compute failed, using CPU fallback" << std::endl;
  }
  
  // Wire terrain generator into chunk manager
  chunk_manager.set_terrain_generator([&terrain_gen, gpu_terrain_ready](world::Chunk& chunk) {
      if (gpu_terrain_ready) {
          auto [ox, oy, oz] = chunk.world_origin();
          // Use chunk origin for coordinate inputs
          gpu_terrain.generate_chunk(ox, oy, oz, 42.0); // Seed 42
          
          // Download to temp buffer and cast to Material enum
          std::vector<uint8_t> mat_raw(world::CHUNK_SIZE * world::CHUNK_SIZE * world::CHUNK_SIZE);
          gpu_terrain.download_chunk(mat_raw, chunk.temperature, chunk.density);
          
          for(size_t i=0; i < mat_raw.size(); ++i) {
             chunk.material[i] = static_cast<world::Material>(mat_raw[i]);
          }
          chunk.generated = true;
      } else {
          terrain_gen.generate(chunk);
      }
  });
  
  // Pre-load chunks around surface (Z=50 is sea_level)
  chunk_manager.update(100.0f, 100.0f, 50.0f);
  std::cout << "[OK] World: ChunkManager initialized, " << chunk_manager.loaded_count() 
            << " chunks loaded" << std::endl;
  std::cout << std::endl;
  std::cout << "=== Simulation Running ===" << std::endl;
  std::cout << "Controls:" << std::endl;
  std::cout << "  WASD/Arrows: Pan camera" << std::endl;
  std::cout << "  Mouse Wheel: Zoom" << std::endl;
  std::cout << "  Q/E: Z-level up/down" << std::endl;
  std::cout << "  1/2/3/0: Overlay (Pressure/Temp/O2/None)" << std::endl;
  std::cout << "  Space: Pause/Resume" << std::endl;
  std::cout << "  N: Step (when paused)" << std::endl;
  std::cout << "  F1: Toggle cell inspector" << std::endl;
  std::cout << "  F2: Toggle performance panel" << std::endl;
  std::cout << "  F3: Toggle event log" << std::endl;
  std::cout << std::endl;

  // Simulation parameters - Fixed Timestep
  const double fixed_dt = 0.01; // 100 Hz simulation tick
  double accumulator = 0.0;
  int step_count = 0;
  double sim_time = 0.0;
  double sim_step_time_ms = 0.0;

  // Expose pause/time_scale for ImGui control
  bool paused = false;
  float time_scale = 1.0f;

  // Main game loop
  while (!game_renderer.should_close()) {
    // F3 toggles event log visibility
    if (IsKeyPressed(KEY_F3))
      debug_ui.toggle_log();

    // Always update input (keyboard for overlays/pause works regardless of ImGui)
    // Note: Renderer's camera drag uses is_capturing_mouse internally
    game_renderer.update_input();
    
    // INPUT: Select Entity
    // (Only if not clicking the ImGui sidebar)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        if (mouse_pos.x > 220.0f) { // Sidebar width
            Vector2 mouse_world = GetScreenToWorld2D(mouse_pos, game_renderer.get_camera());
            float world_x = mouse_world.x / render_config.tile_size;
            float world_y = mouse_world.y / render_config.tile_size;
            
            // Query ECS - Use radius 1.5 to cover full diagonal of the tile
            entt::entity hit = entity_manager.get_entity_at(world_x, world_y, game_renderer.get_z_level(), 1.5f);
            game_renderer.select_entity(hit);
        }
    }

    // Keyboard shortcuts for simulation control (in addition to ImGui)
    if (IsKeyPressed(KEY_SPACE))
      paused = !paused;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
      time_scale = std::min(time_scale * 2.0f, 10.0f);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
      time_scale = std::max(time_scale / 2.0f, 0.1f);

    // Update chunk loading EVERY FRAME (not just during simulation)
    // This ensures chunks load when navigating Z-levels even when paused
    {
      const Camera2D& cam = game_renderer.get_camera();
      float world_cell_x = cam.target.x / render_config.tile_size;
      float world_cell_y = cam.target.y / render_config.tile_size;
      float world_cell_z = static_cast<float>(game_renderer.get_z_level());
      chunk_manager.update(world_cell_x, world_cell_y, world_cell_z);
    }

    // Fixed timestep accumulator
    // Add frame time (scaled by time_scale) to accumulator
    double frame_time = static_cast<double>(GetFrameTime());
    if (!paused) {
      accumulator += frame_time * time_scale;
    }
    
    // Step-by-step mode: if paused and step requested, add exactly one tick
    bool step_requested = game_renderer.should_step();
    if (paused && step_requested) {
      accumulator += fixed_dt;
      game_renderer.clear_step_request();
    }
    
    // Run simulation steps while accumulator has enough time
    auto step_start = std::chrono::high_resolution_clock::now();
    int steps_this_frame = 0;
    const int max_steps_per_frame = 10; // Prevent spiral of death
    
    while (accumulator >= fixed_dt && steps_this_frame < max_steps_per_frame) {
      // Update LOD viewport from camera (temporal slicing)
      if (steps_this_frame == 0) {
        const Camera2D& cam = game_renderer.get_camera();
        int cam_x = static_cast<int>(cam.target.x / 4); // Convert to grid coords
        int cam_y = static_cast<int>(cam.target.y / 4);
        core::ViewportBounds vp;
        vp.x_min = std::max(0, cam_x - 50);
        vp.x_max = std::min(199, cam_x + 50);
        vp.y_min = std::max(0, cam_y - 50);
        vp.y_max = std::min(199, cam_y + 50);
        lod_manager.set_viewport(vp);
        
        // Update chunk loading based on camera (convert pixels to world cells)
        float world_cell_x = cam.target.x / render_config.tile_size;
        float world_cell_y = cam.target.y / render_config.tile_size;
        float world_cell_z = static_cast<float>(game_renderer.get_z_level());
        chunk_manager.update(world_cell_x, world_cell_y, world_cell_z);
      }
      
      // LBM Fluid physics: GPU accelerated
      if (gpu_lbm_ready) {
        gpu_lbm.step(fixed_dt, 1.7); // omega ~1.7 for viscosity
        // No sync needed every step - GPU handles it internally
      } else {
        fluids.step(fixed_dt);
      }
      
      // Thermal physics: GPU accelerated with chunk sync
      if (gpu_thermal_ready) {
        // Sync chunk data TO physics buffer (every 10 steps to reduce overhead)
        static std::vector<double> physics_temp_buffer;
        static std::vector<double> physics_density_buffer;
        int z_level = game_renderer.get_z_level();
        
        if (step_count % 10 == 0) {
          chunk_manager.sync_to_physics(physics_temp_buffer, physics_density_buffer, 
                                        200, 200, z_level);
          gpu_thermal.upload_temperature(physics_temp_buffer);
        }
        
        gpu_thermal.step(fixed_dt);
        
        // Sync results back FROM physics to chunks (every 10 steps)
        if (step_count % 10 == 0) {
          gpu_thermal.download_temperature(physics_temp_buffer);
          chunk_manager.sync_from_physics(physics_temp_buffer, physics_density_buffer,
                                          200, 200, z_level);
          // Also update CPU thermal for visualization
          thermal.temperature_field() = physics_temp_buffer;
        }
      } else {
        thermal.step(fixed_dt);
      }
      
      // Biological systems: throttled (don't need per-step accuracy)
      if (step_count % 10 == 0) {
        circulation.step(fixed_dt * 10);  // Compensate for fewer updates
        blood_chem.step(fixed_dt * 10);
      }
      entity_manager.update(fixed_dt);
      
      // Update astronaut needs (throttled - every 5 steps)
      if (step_count % 5 == 0) {
        entities::NeedsSystem::update(fixed_dt * 5, entity_manager.registry(), fluids);
        entities::MetabolismSystem::update(fixed_dt * 5, entity_manager.registry(), thermal);
      }
      
      sim_time += fixed_dt;
      accumulator -= fixed_dt;
      step_count++;
      steps_this_frame++;
    }
    
    // If we hit max steps, drain remaining accumulator to prevent spiral
    if (steps_this_frame >= max_steps_per_frame && accumulator > fixed_dt) {
      accumulator = 0.0; // Reset to prevent runaway
    }
    
    auto step_end = std::chrono::high_resolution_clock::now();
    sim_step_time_ms = std::chrono::duration<double, std::milli>(step_end - step_start).count();

    // Render
    game_renderer.begin_frame();
    // Draw 3D chunk world instead of flat grid
    game_renderer.draw_chunks(&chunk_manager); 
    // game_renderer.draw_grid(fluids, thermal); // Disable old flat grid
    game_renderer.draw_entities(&entity_manager.registry());
    
    // Draw cursor highlight if inspecting
    if (debug_ui.is_tile_locked()) {
        auto [ix, iy, iz] = debug_ui.get_inspected_tile();
        game_renderer.draw_cursor(ix, iy, iz, {255, 200, 0, 255}); // Gold highlight
    }

    // Exit camera mode for ImGui (screen-space rendering)
    EndMode2D();

    // Draw ImGui sidebar
    debug_ui.begin_frame();
    debug_ui.draw_sidebar(fluids, thermal, game_renderer.get_camera(),
                          render_config.tile_size, game_renderer.get_z_level(),
                          paused, time_scale,
                          sim_step_time_ms, sim_time,
                          &chunk_manager,
                          &entity_manager.registry(),
                          game_renderer.get_selected_entity());
    debug_ui.draw_right_sidebar(
        game_renderer.get_z_level(),
        game_renderer.get_overlay_type(),
        chunk_manager.loaded_count());
    debug_ui.end_frame();

    game_renderer.end_frame();
  }

  // Cleanup
  debug_ui.shutdown();
  game_renderer.shutdown();

  std::cout << std::endl;
  std::cout << "=== Simulation Complete ===" << std::endl;
  std::cout << "Total steps: " << step_count << std::endl;
  std::cout << "Simulated time: " << sim_time << " s" << std::endl;

  return 0;
}
