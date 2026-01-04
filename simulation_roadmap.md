# Simulation Improvement Roadmap

## 🔥 Priority Legend
- 🔴 **Critical** — High impact, unlocks other features
- 🟡 **Important** — Significant accuracy/performance gain
- 🟢 **Nice-to-have** — Polish and edge cases

---

## 1. Physics & Fluids

### 1.1 3D Simulation ✅
- [x] 🔴 Implement D3Q19 lattice for 3D LBM
- [x] 🔴 Add gravity term to equilibrium distribution
- [x] 🔴 Vertical gas stratification (CO2 sinks, hot air rises)
- [x] 🟡 Pressure-driven vertical flow through shafts
- [x] 🟢 Multi-level breach propagation

### 1.2 Turbulence & Mixing ✅
- [x] 🟡 Large Eddy Simulation (LES) for turbulent flows
- [x] 🟡 Entrainment model for jets (O2 from vents)
- [x] 🟢 Kelvin-Helmholtz instabilities at density interfaces

### 1.3 Stability & Accuracy ✅
- [x] 🔴 CFL-based adaptive timestep
- [x] 🟡 Multiple Relaxation Time (MRT) for better stability
- [x] 🟡 Regularized LBM for high Re flows
- [x] 🟢 Immersed boundary method for curved geometries

### 1.4 Multi-phase ✅
- [x] 🟡 Water vapor / condensation
- [x] 🟡 Droplet/aerosol transport (dust, smoke)
- [x] 🟢 Fire/combustion model

---

## 2. Thermodynamics

### 2.1 Heat Transfer ✅
- [x] 🔴 Per-tile thermal conductivity from geology
- [x] 🔴 Radiant heat transfer (Stefan-Boltzmann from magma)
- [x] 🟡 Convective coupling coefficient based on velocity
- [x] 🟢 Anisotropic conduction in layered rock

### 2.2 Phase Changes ✅
- [x] 🔴 Water freezing/boiling with latent heat
- [x] 🟡 CO2 sublimation (dry ice formation)
- [x] 🟡 Rock melite → magma transition
- [x] 🟢 Evaporative cooling in humid environments

### 2.3 Thermal Sources ✅
- [x] 🟡 Equipment/machinery heat generation
- [x] 🟡 Biological heat from entities
- [x] 🟢 Radioactive decay heat in ores

---

## 3. Biology - Cardiovascular

### 3.1 Cardiac Model ✅
- [x] 🔴 Frank-Starling mechanism (preload → stroke volume)
- [x] 🔴 Heart rate variability (HRV) model
- [x] 🟡 4-chamber heart model with valve dynamics
- [x] 🟡 Cardiac arrhythmia from electrolytes
- [ ] 🟢 Pacemaker cell automaticity

### 3.2 Blood Chemistry ✅
- [x] 🔴 pH / acid-base balance (Henderson-Hasselbalch)
- [x] 🔴 Electrolytes (Na⁺, K⁺, Ca²⁺) affecting heart
- [x] 🟡 Lactate → bicarbonate buffering
- [x] 🟡 Anion gap calculation
- [x] 🟢 Base excess/deficit tracking

### 3.3 Coagulation ✅
- [x] 🟡 Clotting cascade model
- [x] 🟡 Platelet dynamics
- [x] 🟢 Anticoagulant effects (if items exist)

---

## 4. Biology - Respiratory

### 4.1 Ventilation ✅
- [x] 🟡 V/Q mismatch modeling
- [x] 🟡 Dead space ventilation
- [x] 🟡 Work of breathing / respiratory fatigue
- [x] 🟢 Positive pressure effects (mask/helmet)

### 4.2 Gas Transport ✅
- [x] 🔴 CO poisoning (carboxyhemoglobin)
- [x] 🟡 Nitrogen narcosis at depth
- [x] 🟡 Oxygen toxicity at high PO2
- [x] 🟢 Decompression sickness (bends)

---

## 5. Biology - Neurological

### 5.1 Brain Perfusion ✅
- [x] 🔴 Cerebral autoregulation
- [x] 🟡 Intracranial pressure (ICP) modeling
- [x] 🟡 Herniation from trauma/swelling
- [x] 🟢 Glasgow Coma Scale computation

### 5.2 Sensory Systems ✅
- [x] 🟡 Vision impairment (hypoxia, injury)
- [x] 🟡 Hearing damage (blast, noise)
- [x] 🟡 Vestibular (balance, vertigo)
- [x] 🟢 Proprioception loss from nerve damage

### 5.3 Cognitive Effects ✅
- [x] 🟡 Confusion from hypoxia/glucose
- [x] 🟡 Reaction time degradation
- [x] 🟢 Memory effects (concussion)

---

## 6. Biology - Additional Systems

### 6.1 Lymphatic ✅
- [x] 🟡 Edema from capillary leak
- [x] 🟡 Lymph node filtering
- [x] 🟢 Lymphatic drainage impairment

### 6.2 Hematology ✅
- [x] 🟡 RBC production/destruction
- [x] 🟡 Anemia effects on O2 carrying
- [x] 🟢 Polycythemia at altitude

### 6.3 Reproductive (if relevant)
- [ ] 🟢 Hormonal effects on physiology
- [ ] 🟢 Pregnancy complications

---

## 7. World Generation

### 7.1 Hydrology ✅
- [x] 🔴 Underground aquifers
- [x] 🔴 Flooding mechanics
- [x] 🟡 Water table + percolation
- [x] 🟡 Pressure-driven water flow
- [x] 🟢 Erosion over time

### 7.2 Geology Dynamics ✅
- [x] 🟡 Cave-ins / structural collapse
- [x] 🟡 Seismic events
- [x] 🟡 Fault lines with hydrothermal vents
- [x] 🟢 Tectonic long-term changes

### 7.3 Biomes ✅
- [x] 🟡 Underground ecosystems (fungus, bacteria)
- [x] 🟡 Bioluminescent regions
- [x] 🟢 Fossil deposits

---

## 8. Performance

### 8.1 Computation ✅
- [x] 🔴 SIMD vectorization (AVX2/AVX512)
- [x] 🔴 OpenMP parallelization
- [x] 🟡 GPU acceleration (CUDA/OpenCL)
- [x] 🟡 Cache-friendly data layouts
- [x] 🟢 Profile-guided optimization (PGO)

### 8.2 Memory
- [x] 🟡 Structure of Arrays (SoA) layout for LBM
- [ ] 🟡 Custom allocators for hot paths
- [ ] 🟡 Sparse storage for mostly-empty regions
- [ ] 🟢 Memory-mapped files for large worlds

### 8.3 LOD Enhancements
- [ ] 🟡 Hierarchical simulation (coarse → fine)
- [ ] 🟡 Predictive chunk loading
- [ ] 🟢 Temporal coherence caching

---

## 9. Architecture

### 9.1 Data-Oriented Design
- [x] 🟡 Header-only system design
- [ ] 🟡 Component storage optimization
- [ ] 🟡 Template-based compile-time dispatch
- [ ] 🟢 Archetype-based ECS storage

### 9.2 Event System
- [ ] 🟡 Deferred event processing
- [ ] 🟡 Event priority queues
- [ ] 🟢 Event replay for debugging

### 9.3 Serialization
- [ ] 🔴 Binary save/load (cereal or custom)
- [ ] 🟡 Delta compression for saves
- [ ] 🟢 Cross-platform compatibility

---

## 10. Gameplay Integration

### 10.1 UI/Rendering
- [ ] 🟡 Real-time pressure/temp overlays
- [ ] 🟡 Medical status display
- [ ] 🟢 3D isometric view

### 10.2 AI/Behavior
- [ ] 🟡 GOAP action planning
- [ ] 🟡 Pathfinding with hazard avoidance
- [ ] 🟢 NPC medical triage decisions

### 10.3 Items/Equipment
- [ ] 🟡 O2 tanks, suits, masks
- [ ] 🟡 Medical supplies (bandages, drugs)
- [ ] 🟢 Tools affecting world (mining, welding)

---

## Summary

| Category | 🔴 Critical | 🟡 Important | 🟢 Nice-to-have | Done |
|----------|-------------|--------------|-----------------|------|
| Physics | 4 | 5 | 3 | **12/12** |
| Thermo | 2 | 5 | 3 | **9/10** |
| Biology-Cardio | 4 | 4 | 3 | **11/11** |
| Biology-Resp | 1 | 4 | 2 | **5/7** |
| Biology-Neuro | 1 | 5 | 3 | 0/9 |
| Biology-Other | 0 | 4 | 3 | 0/7 |
| Worldgen | 2 | 5 | 3 | 0/10 |
| Performance | 2 | 3 | 2 | **3/7** |
| Architecture | 1 | 4 | 2 | **1/7** |
| Gameplay | 0 | 5 | 3 | 0/8 |
| **Total** | **17** | **44** | **27** | **41/88** |
