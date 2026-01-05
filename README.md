# Isolated (C++)

High-performance habitat simulation framework written in modern C++20 with **GPU-accelerated physics** (OpenGL Compute Shaders) and a **Dwarf Fortress-inspired 3D tile interface**.

![Dwarf Fortress-style terrain rendering with real-time simulation](docs/screenshot.png)

## Features

### 🎮 Visual Simulation
- **Raylib** tile-based rendering (Dwarf Fortress-style)
- **Dear ImGui** unified sidebar UI with DF theme
- **3D chunk-based world** with Z-level navigation (Q/E keys)
- Real-time temperature, pressure, and oxygen overlays
- **Click-to-lock tile inspector** — left-click to examine any cell
- Camera pan/zoom controls
- Simulation pause, step, and time scale controls

### 🌍 World System (NEW)
- **GPU Terrain Generation** — Procedural noise on GPU via OpenGL Compute Shaders
- **Chunk-based streaming** — 64³ voxel chunks loaded around camera
- **Material types** — Granite, Basalt, Limestone, Soil, Water, Ores, Gases
- **Geological stratification** — Depth-based rock layers with thermal gradient
- **Z-level rendering** — Navigate vertically through underground layers

### 🚀 GPU-Accelerated Physics
- **LBM Fluids** — D2Q9 lattice with BGK collision (OpenGL Compute)
- **Thermal Engine** — 2D heat diffusion on GPU
- **Terrain Generation** — Simplex noise and FBM on GPU
- Multi-species gas tracking (O₂, N₂, CO₂, H₂O, CO)
- CPU fallback for systems without OpenGL 4.3+

### 🔥 Multiphase Physics
- Magnus-Tetens condensation/evaporation
- Lagrangian aerosol transport
- Procedural combustion model with smoke

### 🌡️ Thermal Systems
- 3D finite difference conduction
- Stefan-Boltzmann radiation
- Enthalpy-based phase changes
- Geothermal gradient in terrain

### 🧬 Human Physiology

**Cardiovascular**
- Windkessel circulation, Frank-Starling, HRV
- Coagulation cascade (intrinsic/extrinsic pathways)

**Respiratory**
- Hill equation hemoglobin, alveolar gas exchange
- V/Q mismatch, dead space, work of breathing

**Other Systems**
- Metabolic: ATP/glycogen, substrate utilization
- Blood Chemistry: Henderson-Hasselbalch, electrolytes
- Thermoregulation: Heat balance, sweating, shivering

## Build

### Windows (MSVC)

```powershell
mkdir build; cd build
cmake ..
cmake --build . --config Release
.\Release\isolated.exe
```

### Linux (GCC/Clang)

```bash
# Dependencies (Ubuntu)
sudo apt install libeigen3-dev libfmt-dev libomp-dev cmake

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run
./isolated
```

### Dependencies (auto-fetched via CMake)
- **Raylib** 5.5 — Graphics
- **Dear ImGui** (docking branch) — UI
- **rlImGui** — Raylib/ImGui binding
- **Eigen3** — Linear algebra
- **fmt** — String formatting
- **OpenMP** — CPU parallelization
- **OpenGL 4.3+** — GPU Compute Shaders

## Controls

| Key | Action |
|-----|--------|
| **WASD / Arrows** | Pan camera |
| **Mouse Wheel** | Zoom |
| **Q / E** | Z-level down/up |
| **Left Click** | Lock tile inspector to cell |
| **Right Click** | Unlock tile inspector |
| **1 / 2 / 3 / 0** | Toggle overlay (Pressure/Temp/O2/None) |
| **Space** | Pause/Resume simulation |
| **+/-** | Adjust time scale |
| **F3** | Toggle event log |

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   GPU (Compute) │     │   CPU (OpenMP)  │     │   Renderer      │
├─────────────────┤     ├─────────────────┤     ├─────────────────┤
│ LBM Fluids      │◄───►│ ChunkManager    │◄───►│ Raylib Tiles    │
│ Thermal         │     │ Biology Systems │     │ ImGui Sidebar   │
│ Terrain Gen     │     │ Entity System   │     │ Overlays        │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the complete development plan including:
- Massive world scaling (10,000 × 10,000 × 150)
- Advanced terrain (erosion, caves, rivers)
- Chunk-based physics synchronization
- Astronaut AI (Utility AI → GOAP)

## License

MIT
