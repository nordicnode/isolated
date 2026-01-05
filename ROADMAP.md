# Isolated — Long-Horizon Development Roadmap

> **Vision**: A zero-player simulation game where stranded astronauts autonomously struggle to survive on a hostile alien world. The player watches as emergent stories unfold from deeply interconnected systems.

---

## ⚠️ Core Design Principles

### 1. Visualization IS Gameplay
In a zero-player game, if the player cannot see the simulation, the game doesn't exist. You cannot debug pathfinding, fluid dynamics, or AI with text logs. **Rendering comes first.**

### 2. Don't Reinvent Wheels
Use battle-tested libraries: **EnTT** for ECS, **Raylib** for rendering, **Dear ImGui** for debug UI. Save months of development time.

### 3. Utility AI Before GOAP
Start with Utility AI driving FSMs. GOAP is computationally expensive and hard to debug. Add planning only when Utility fails to solve complex puzzles.

### 4. The Log IS The Story
Don't try to "detect narrative arcs." Instead, generate a readable Black Box log that tells the story through events. The player reads and interprets.

### 5. Cold = Slow, Not Damage
Thermodynamics should affect simulation speed: fluid viscosity, reaction rates, metabolic rate. Death comes from failure to act, not HP drain.

---

## 📊 Project Status Overview

### Currently Implemented ✅
| System | Status | Notes |
|--------|--------|-------|
| LBM Fluid Dynamics | ✅ Complete | D3Q19, MRT, LES, CUDA acceleration |
| Thermal Engine | ✅ Complete | Conduction, radiation, phase changes |
| Cardiovascular | ✅ Complete | Windkessel, Frank-Starling, coagulation |
| Respiratory | ✅ Complete | V/Q mismatch, gas transport pathologies |
| Neurological | ✅ Complete | Cerebral autoregulation, ICP, GCS |
| Blood Chemistry | ✅ Complete | pH balance, electrolytes |
| World Generation | ✅ Partial | Noise, geology, hydrology basics |

### Major Gaps 🔴
- ~~**No Visualization** — Pure simulation engine, no way to see it~~ ✅ RESOLVED
- **No Entity System** — No way to represent/manage astronauts
- **No AI/Behavior** — Astronauts don't think, plan, or act
- **No Needs/Moods** — No hunger, thirst, sleep, psychological states
- **No Items/Building** — No equipment, construction, resources
- **No Save/Load** — No persistence

---

## 🎯 Development Phases (Revised)

```
Phase 1: Visual Foundation + EnTT       [Weeks 1-3]    ← VISUALIZATION FIRST
Phase 2: Astronaut Needs & Survival     [Weeks 4-9]
Phase 3: Utility AI & Behavior          [Weeks 10-16]
Phase 4: Social & Psychological         [Weeks 17-23]
Phase 5: Building & Crafting            [Weeks 24-30]
Phase 6: Advanced World Systems         [Weeks 31-37]
Phase 7: Event Log & Black Box          [Weeks 38-42]
Phase 8: Polish & Advanced Rendering    [Weeks 43-50]
```

---

# Phase 1: Visual Foundation + Entity System

> **Goal**: See the simulation. Click on things. Inspect state. Debug visually.

## 1.1 Raylib + Dear ImGui Setup

### 1.1.1 Core Rendering (Week 1) ✅
- [x] ~~🔴~~ **Raylib window initialization** — Basic window with game loop
- [x] ~~🔴~~ **Tile-based world rendering** — Colored rectangles, not sprites
- [x] ~~🔴~~ **Camera controls** — Pan, zoom with mouse/keyboard
- [x] ~~🔴~~ **Z-level switching** — View different vertical layers
- [x] ~~🟡~~ **Fluid pressure overlay** — Color gradient from LBM density
- [x] ~~🟡~~ **Temperature overlay** — Heat map from thermal engine
- [ ] 🟢 **Velocity vector field** — Arrows showing flow direction

### 1.1.2 Dear ImGui Debug UI (Week 1-2) ✅
- [x] ~~🔴~~ **ImGui integration with Raylib** — rlImGui binding
- [ ] 🔴 **Entity inspector panel** — Click astronaut, see all components
- [x] ~~🔴~~ **Simulation controls** — Pause, step, speed (1x/2x/5x/10x)
- [x] ~~🔴~~ **Cell inspector** — Hover tile, see pressure/temp/gas composition
- [x] ~~🟡~~ **Performance panel** — FPS, step time, memory usage
- [ ] 🟡 **Console/log panel** — Scrolling event log
- [ ] 🟢 **System toggles** — Enable/disable individual systems

### 1.1.3 Visual Feedback
- [ ] 🔴 **Astronaut representation** — Colored circle with direction
- [ ] 🔴 **Pathfinding visualization** — Show A* path as line
- [ ] 🟡 **AI state display** — Text above entity showing current action
- [ ] 🟡 **Hazard highlights** — Red overlay on dangerous tiles
- [ ] 🟢 **Selection highlight** — Outline selected entity

## 1.2 EnTT Integration

### 1.2.1 Core Setup (Week 2)
- [ ] 🔴 **Add EnTT as dependency** — Header-only, just include
- [ ] 🔴 **Basic component types** — Position, Velocity, Health, Astronaut
- [ ] 🔴 **Entity creation/destruction** — Spawn/despawn astronauts
- [ ] 🔴 **System registration** — Process entities each frame
- [ ] 🟡 **Component observers** — React to component add/remove
- [ ] 🟢 **Entity prefabs** — Templates for common entities

### 1.2.2 Spatial Integration (Week 2-3)
- [ ] 🔴 **Grid-based spatial index** — Simple 2D grid of entity lists
- [ ] 🔴 **Sync positions to grid** — Update on movement
- [ ] 🔴 **Neighbor queries** — "Who is in adjacent cells?"
- [ ] 🟡 **LBM grid alignment** — Entity positions map to fluid cells
- [ ] 🟡 **Static vs dynamic separation** — Buildings don't move
- [ ] 🟢 **Range queries** — "Who is within 5 tiles?"

> **Note**: Don't build an Octree. Your LBM d_solid mask handles static geometry. Use a simple grid-linked-list for dynamic entities. Add hierarchy only if verticality exceeds 2.5D layers.

## 1.3 Time & Simulation Control

### 1.3.1 Game Loop (Week 3)
- [ ] 🔴 **Fixed timestep simulation** — Physics at constant rate
- [ ] 🔴 **Variable render rate** — Rendering independent of simulation
- [ ] 🔴 **Pause/resume** — Freeze simulation, UI still works
- [ ] 🔴 **Time acceleration** — 1x, 2x, 5x, 10x, 100x
- [ ] 🟡 **Step-by-step mode** — Advance one tick at a time
- [ ] 🟢 **Slow motion** — 0.5x, 0.25x for debugging

### 1.3.2 Determinism
- [ ] 🔴 **Seeded RNG** — Reproducible random numbers
- [ ] 🟡 **Fixed system ordering** — Same results on replay
- [ ] 🟢 **Replay recording** — Record for debugging

---

# Phase 2: Astronaut Needs & Survival

> **Goal**: Astronauts have needs. Needs not met = death. Visible in UI.

## 2.1 Core Physiological Needs

### 2.1.1 Oxygen & Breathing
- [ ] 🔴 **O2 consumption component** — Rate varies by activity level
- [ ] 🔴 **CO2 as entity gas source** — Exhale into LBM simulation
- [ ] 🔴 **Hypoxia stages** — Confusion → collapse → death (timed)
- [ ] 🔴 **Visual: breathing indicator** — Colored bar in entity inspector
- [ ] 🟡 **Suit O2 tank** — Limited supply when suited
- [ ] 🟢 **Breath-holding** — Short vacuum exposure

### 2.1.2 Hydration
- [ ] 🔴 **Water need (2-3L/day)** — Increases with activity/temp
- [ ] 🔴 **Dehydration stages** — Thirst → weakness → death
- [ ] 🔴 **Visual: hydration bar** — In astronaut panel
- [ ] 🟡 **Sweat rate from thermoregulation** — Hot work = more loss
- [ ] 🟢 **Water quality** — Contaminated = illness

### 2.1.3 Nutrition
- [ ] 🔴 **Caloric need** — BMR + activity-based burn
- [ ] 🔴 **Starvation stages** — Hunger → weakness → death
- [ ] 🔴 **Visual: hunger bar** — In astronaut panel
- [ ] 🟡 **Macronutrients** — Carbs, protein, fat effects
- [ ] 🟢 **Micronutrient deficiencies** — Scurvy, anemia

### 2.1.4 Sleep & Fatigue
- [ ] 🔴 **Fatigue accumulation** — Activity drains, rest restores
- [ ] 🔴 **Sleep debt** — Cumulative exhaustion
- [ ] 🔴 **Sleep deprivation effects** — Cognitive decline, hallucinations
- [ ] 🟡 **Circadian rhythm** — Natural sleep/wake tendency
- [ ] 🟢 **Sleep quality factors** — Noise, comfort

### 2.1.5 Waste
- [ ] 🟡 **Bladder/bowel timers** — Requires facilities
- [ ] 🟡 **Illness effects** — Diarrhea, vomiting
- [ ] 🟢 **Waste as resource** — Composting

## 2.2 Environmental Hazards

### 2.2.1 Pressure Integration
- [ ] 🔴 **Vacuum exposure timeline** — 15s conscious, 90s death
- [ ] 🔴 **Pressure from LBM density** — Real simulation data
- [ ] 🔴 **Visual: pressure danger overlay** — Red where low
- [ ] 🟡 **Suit breach mechanics** — Rate of pressure loss
- [ ] 🟢 **Ear/sinus pain** — Rapid pressure changes

### 2.2.2 Temperature Integration
- [ ] 🔴 **Body temp from thermal engine** — Real heat flow
- [ ] 🔴 **Hypothermia: SLOW DOWN effect** — Not just damage
  - Fluid viscosity in body increases
  - Metabolic rate drops
  - Reaction time degrades
  - Death from failure to act
- [ ] 🔴 **Hyperthermia stages** — Sweating → heat stroke
- [ ] 🔴 **Visual: temperature danger overlay** — Blue/red
- [ ] 🟡 **Suit thermal regulation** — Active heating/cooling

### 2.2.3 Radiation
- [ ] 🔴 **Background radiation from geology** — From worldgen
- [ ] 🔴 **Acute radiation syndrome** — Staged progression
- [ ] 🟡 **Solar flare events** — Periodic high radiation
- [ ] 🟡 **Visual: radiation overlay** — Yellow/orange zones
- [ ] 🟢 **Dosimetry tracking** — Cumulative exposure

### 2.2.4 Toxic Gases
- [ ] 🔴 **CO poisoning integration** — Already in respiration system
- [ ] 🟡 **Visual: toxic gas overlay** — Gas concentration colors
- [ ] 🟢 **Chemical leak events** — Ammonia, etc.

## 2.3 Injury & Trauma

### 2.3.1 Physical Injuries
- [ ] 🔴 **Wound component** — Location, severity, bleeding rate
- [ ] 🔴 **Bleeding rate affects circulation** — Ties to cardiovascular
- [ ] 🔴 **Fractures** — Mobility reduction
- [ ] 🟡 **Visual: injury markers** — Red indicators on astronaut
- [ ] 🟢 **Internal bleeding** — From blunt trauma

### 2.3.2 Healing & Infection
- [ ] 🔴 **Healing rate** — Based on nutrition/rest
- [ ] 🔴 **Infection from untreated wounds** — Ties to immune system
- [ ] 🟡 **Bandaging action** — Stops bleeding
- [ ] 🟢 **Surgical intervention** — Advanced treatment

---

# Phase 3: Utility AI & Behavior

> **Goal**: Astronauts make decisions. Utility scores drive FSM transitions. Debuggable.

## 3.1 Utility AI Core

### 3.1.1 Consideration System
- [ ] 🔴 **Need-based utility curves** — Hunger urgency = exponential
- [ ] 🔴 **Action scoring (0-1)** — Rate each possible action
- [ ] 🔴 **Visual: utility debugger** — Show all scores for selected entity
- [ ] 🔴 **Weighted random selection** — Some randomness for variety
- [ ] 🟡 **Personality modifiers** — Brave = lower danger weight
- [ ] 🟡 **Mood modifiers** — Depressed = lower social score
- [ ] 🟢 **History avoidance** — Don't repeat same action

### 3.1.2 State Machine Layer
- [ ] 🔴 **FSM per astronaut** — Idle, Working, Eating, Sleeping, Fleeing
- [ ] 🔴 **Utility drives transitions** — Highest score wins
- [ ] 🔴 **Visual: current state display** — Text above astronaut
- [ ] 🟡 **Interruptible states** — Emergency breaks current action
- [ ] 🟡 **State duration tracking** — How long in each state
- [ ] 🟢 **Nested sub-states** — Eating → FindFood → WalkToFood → Consume

### 3.1.3 Future: GOAP Planning (Deferred)
- [ ] 🟢 **GOAP for complex multi-step puzzles** — Only if Utility fails
- [ ] 🟢 **Action preconditions/effects** — "Eat requires HasFood"
- [ ] 🟢 **A* plan search** — Find action sequence
- [ ] 🟢 **Plan caching** — Reuse when state unchanged

## 3.2 Perception & Awareness

### 3.2.1 Sensory Systems
- [ ] 🔴 **Vision cone** — Angle + range, blocked by walls
- [ ] 🔴 **Hearing range** — Distance-based, through walls (muffled)
- [ ] 🔴 **Visual: perception debug** — Show vision cone
- [ ] 🟡 **Memory of positions** — "Saw Bob there 5 min ago"
- [ ] 🟡 **Impaired senses** — Injury affects vision/hearing
- [ ] 🟢 **Attention focus** — Miss peripheral events

### 3.2.2 Threat Detection
- [ ] 🔴 **Danger assessment** — Fire, vacuum, radiation
- [ ] 🔴 **Fear response** — Triggers Fleeing state
- [ ] 🔴 **Visual: threat indicators** — Red arrows toward danger
- [ ] 🟡 **Risk tolerance** — Personality affects threshold
- [ ] 🟢 **Warning others** — Alert nearby astronauts

## 3.3 Navigation & Movement

### 3.3.1 A* Pathfinding
- [ ] 🔴 **A* on 2D grid** — Basic pathfinding
- [ ] 🔴 **Hazard cost** — Fire/vacuum increase path cost
- [ ] 🔴 **Visual: path line** — Show planned route
- [ ] 🔴 **Dynamic replanning** — Reroute when path blocked
- [ ] 🟡 **Multi-level paths** — Ladders, stairs
- [ ] 🟢 **Group coordination** — Avoid collisions

### 3.3.2 Movement
- [ ] 🔴 **Steering behaviors** — Seek, arrive at destination
- [ ] 🔴 **Speed variation** — Walk, run (stamina cost)
- [ ] 🔴 **Visual: velocity vector** — Small arrow on entity
- [ ] 🟡 **Obstacle avoidance** — Don't walk into walls
- [ ] 🟢 **Climbing/swimming** — Special movement modes

## 3.4 Task Execution

### 3.4.1 Work Actions
- [ ] 🔴 **Action duration** — Tasks take time
- [ ] 🔴 **Progress bar** — Visual task completion
- [ ] 🔴 **Skill-based speed** — Better skill = faster
- [ ] 🟡 **Tool requirements** — Some tasks need tools
- [ ] 🟢 **Quality of output** — Skill affects quality

### 3.4.2 Resource Interaction
- [ ] 🔴 **Find nearest resource** — Pathfind to closest
- [ ] 🔴 **Carry capacity** — Limited inventory
- [ ] 🔴 **Consume/deposit actions** — Interact with world
- [ ] 🟡 **Prioritized gathering** — Urgent needs first
- [ ] 🟢 **Resource memory** — Remember locations

### 3.4.3 Emergency Response
- [ ] 🔴 **Fire suppression** — Get extinguisher, fight fire
- [ ] 🔴 **Help injured** — Move to, apply aid
- [ ] 🟡 **Evacuation** — Leave dangerous area
- [ ] 🟢 **Triage decisions** — Who to save first

---

# Phase 4: Social & Psychological Systems

> **Goal**: Astronauts have personalities, moods, relationships. Interactions = information packets.

## 4.1 Personality System

### 4.1.1 Core Traits
- [ ] 🔴 **Big Five traits** — Openness, Conscientiousness, Extraversion, Agreeableness, Neuroticism
- [ ] 🔴 **Trait → Utility weights** — Personality affects decisions
- [ ] 🔴 **Visual: personality panel** — Bar chart in inspector
- [ ] 🟡 **Bravery** — Risk tolerance
- [ ] 🟡 **Optimism** — Baseline mood tendency
- [ ] 🟢 **Trait evolution** — Extreme events shift traits

## 4.2 Mood & Mental Health

### 4.2.1 Mood System
- [ ] 🔴 **Baseline mood from needs** — Fed, rested, safe = happier
- [ ] 🔴 **Mood events** — Death, injury, success affects mood
- [ ] 🔴 **Visual: mood indicator** — Color/icon on astronaut
- [ ] 🟡 **Mood decay** — Events fade over time
- [ ] 🟡 **Mood → productivity** — Happy works faster
- [ ] 🟢 **Mood contagion** — Spreads between nearby astronauts

### 4.2.2 Stress & Breakdown
- [ ] 🔴 **Stress accumulation** — Threats, overwork, isolation
- [ ] 🔴 **Breakdown threshold** — Too much stress = crisis
- [ ] 🔴 **Visual: stress meter** — In astronaut panel
- [ ] 🟡 **Stress relief activities** — Recreation, rest, socializing
- [ ] 🟢 **PTSD from trauma** — Flashbacks, avoidance

## 4.3 Relationships & Information

### 4.3.1 Relationship Values
- [ ] 🔴 **Relationship score (-100 to +100)** — Per pair
- [ ] 🔴 **Change from interactions** — Help = +, conflict = -
- [ ] 🔴 **Visual: relationship graph** — Network diagram
- [ ] 🟡 **Trust component** — Separate from liking
- [ ] 🟢 **Relationship types** — Friend, rival, romantic

### 4.3.2 Information Packets (Memetics)
> **Don't simulate words. Simulate information flow.**

- [ ] 🔴 **Packet structure** — {Topic, Location, Time, Source}
- [ ] 🔴 **Pass packet on interaction** — "Fire in Kitchen, now"
- [ ] 🔴 **Memory creation** — Recipient stores packet
- [ ] 🔴 **Visual: info packet log** — What entity knows
- [ ] 🟡 **Staleness check** — Old info = ignored
- [ ] 🟡 **Trust filter** — Distrust source = maybe ignore
- [ ] 🟢 **Misinformation** — Errors in retelling

### 4.3.3 Social Interactions
- [ ] 🔴 **Proximity triggers interaction** — When nearby
- [ ] 🟡 **Helping/cooperation** — Builds relationship
- [ ] 🟡 **Conflict/arguments** — Damages relationship
- [ ] 🟢 **Group dynamics** — Factions, hierarchy

---

# Phase 5: Building & Crafting

## 5.1 Resource System
- [ ] 🔴 **Material types** — Raw, processed, manufactured
- [ ] 🔴 **Inventory system** — Personal + container storage
- [ ] 🔴 **Visual: inventory panel** — Grid view
- [ ] 🟡 **Weight/volume limits** — Capacity constraints
- [ ] 🟢 **Item degradation** — Perishables, durability

## 5.2 Crafting
- [ ] 🔴 **Recipe definitions** — Inputs → outputs
- [ ] 🔴 **Workstation requirements** — Forge, printer, kitchen
- [ ] 🔴 **Visual: crafting menu** — Available recipes
- [ ] 🟡 **Skill affects speed/quality** — Better crafters
- [ ] 🟢 **Byproducts** — Waste, heat

## 5.3 Construction
- [ ] 🔴 **Wall/floor/door placement** — Basic structures
- [ ] 🔴 **Sealed room detection** — Is room airtight?
- [ ] 🔴 **Pressure integration** — LBM reacts to construction
- [ ] 🔴 **Visual: blueprint mode** — Plan before building
- [ ] 🟡 **Structural integrity** — Load-bearing calculations
- [ ] 🟡 **Power grid** — Generation, distribution
- [ ] 🟢 **Life support equipment** — O2 gen, CO2 scrub

## 5.4 Equipment
- [ ] 🔴 **Spacesuits** — Pressure, O2, thermal protection
- [ ] 🔴 **Medical supplies** — Bandages, medicines
- [ ] 🔴 **Visual: equipment loadout** — What astronaut wears
- [ ] 🟡 **Tool durability** — Wear and repair
- [ ] 🟢 **Heavy machinery** — Excavators, vehicles

---

# Phase 6: Advanced World Systems

## 6.1 Planetary Environment
- [ ] 🔴 **Day/night cycle** — Affects temperature, solar power
- [ ] 🔴 **Visual: lighting changes** — Tint based on time
- [ ] 🟡 **Weather events** — Dust storms, wind
- [ ] 🟡 **Seasons** — Long-term temperature variation
- [ ] 🟢 **Geological events** — Earthquakes, eruptions

## 6.2 Alien Life
- [ ] 🟡 **Microorganisms** — Pathogens, beneficial
- [ ] 🟡 **Flora** — Plants for food, oxygen
- [ ] 🟢 **Fauna** — Creatures, predators
- [ ] 🟢 **Ecosystem simulation** — Food chains

## 6.3 Advanced Physics
- [ ] 🟡 **Electrical systems** — Circuits, shorts, fires
- [ ] 🟡 **Chemical reactions** — Beyond combustion
- [ ] 🟢 **Nuclear systems** — Reactor simulation

---

# Phase 7: Event Log & Black Box

> **The Log IS the Story.** Don't detect arcs—generate readable events.

## 7.1 Event System
- [ ] 🔴 **Event triggers** — Condition-based, timed, random
- [ ] 🔴 **Event library** — Dozens of scenarios
- [ ] 🔴 **Event chaining** — One leads to another
- [ ] 🟡 **Difficulty scaling** — Match colony age
- [ ] 🟢 **Rare legendary events** — Memorable moments

## 7.2 Black Box Log

### 7.2.1 Log Generation
- [ ] 🔴 **Timestamped entries** — [T+1042] format
- [ ] 🔴 **Entity actions** — "Cpt. Miller entered Hypoxia Stage 2"
- [ ] 🔴 **Object interactions** — "dropped 'Welding Tool'"
- [ ] 🔴 **Task outcomes** — "attempted 'Repair Hull' → FAILED (Tremors)"
- [ ] 🔴 **Visual: scrolling log panel** — Real-time updates

### 7.2.2 Log Filtering
- [ ] 🟡 **Per-entity filter** — Show only one astronaut
- [ ] 🟡 **Category filter** — Medical, social, work, etc.
- [ ] 🟡 **Severity filter** — Critical, warning, info
- [ ] 🟢 **Search** — Find specific events

### 7.2.3 Chronicle
- [ ] 🟡 **Major event summary** — Daily/weekly recap
- [ ] 🟡 **Death narratives** — How they died, what they meant
- [ ] 🟢 **Export to file** — Save story as text

## 7.3 Memory System
- [ ] 🔴 **Per-astronaut event log** — What they witnessed
- [ ] 🔴 **Emotional memory** — How events made them feel
- [ ] 🟡 **Fading memories** — Old matters less
- [ ] 🟢 **Traumatic persistence** — Some never fade

---

# Phase 8: Polish & Advanced Rendering

> **Now make it pretty.** The simulation works, now enhance visuals.

## 8.1 Advanced Rendering
- [ ] 🟡 **Sprite-based graphics** — Replace rectangles with art
- [ ] 🟡 **Animation states** — Walk, work, sleep, injured
- [ ] 🟡 **Particle effects** — Smoke, sparks, debris
- [ ] 🟡 **Dynamic lighting** — Shadows, light sources
- [ ] 🟢 **3D isometric option** — Depth perspective
- [ ] 🟢 **Weather effects** — Dust, precipitation

## 8.2 Audio
- [ ] 🟡 **Ambient sounds** — Machinery, silence, wind
- [ ] 🟡 **Action sounds** — Footsteps, tools, doors
- [ ] 🟡 **Alert sounds** — Alarms, emergencies
- [ ] 🟢 **Adaptive music** — Match simulation mood

## 8.3 Persistence
- [ ] 🔴 **Full save/load** — Serialize all state
- [ ] 🔴 **Visual: save/load menu** — Slot management
- [ ] 🟡 **Autosave** — Periodic automatic saves
- [ ] 🟡 **Save compression** — Reduce file size
- [ ] 🟢 **Replay system** — Watch past events

## 8.4 Performance
- [ ] 🟡 **LOD for distant entities** — Reduce detail
- [ ] 🟡 **Chunk streaming** — Load/unload world
- [ ] 🟢 **GPU rendering** — Hardware acceleration
- [ ] 🟢 **Profiler integration** — Find bottlenecks

---

# 📈 Summary Statistics

## By Phase

| Phase | 🔴 Critical | 🟡 Important | 🟢 Nice-to-have | Total |
|-------|-------------|--------------|-----------------|-------|
| 1. Visual + EnTT | 18 | 11 | 6 | 35 |
| 2. Survival | 24 | 14 | 9 | 47 |
| 3. Utility AI | 24 | 16 | 12 | 52 |
| 4. Social/Psych | 16 | 12 | 8 | 36 |
| 5. Building | 12 | 8 | 5 | 25 |
| 6. Advanced World | 2 | 6 | 6 | 14 |
| 7. Event Log | 12 | 10 | 4 | 26 |
| 8. Polish | 2 | 12 | 8 | 22 |
| **Total New** | **110** | **89** | **58** | **257** |

## Combined with Existing

| Category | Complete | Remaining | Total |
|----------|----------|-----------|-------|
| Existing Systems | 41 | 47 | 88 |
| New Systems | 0 | 257 | 257 |
| **Project Total** | **41** | **304** | **345** |

---

# 🚀 Sprint Plan: Weeks 1-6

## Sprint 1 (Week 1-2): See the Simulation

### Dependencies
```bash
# Raylib (rendering)
# EnTT (ECS) - header-only
# Dear ImGui + rlImGui (debug UI)
```

### Tasks
1. [x] Raylib window with tile grid rendering
2. [x] Camera pan/zoom
3. [ ] EnTT registry with Position, Velocity components
4. [ ] Astronaut entity as colored circle
5. [ ] ImGui integration: entity inspector, simulation controls
6. [x] LBM pressure overlay (color gradient)
7. [x] Thermal overlay (heat map)
8. [ ] Cell click inspector (gas composition, temp)

### Deliverable
**A window showing the fluid/thermal simulation with clickable debug UI.**

---

## Sprint 2 (Week 3-4): Astronaut Lives

### Tasks
1. [ ] Oxygen consumption component
2. [ ] Hunger/thirst/fatigue needs with decay
3. [ ] Death from unmet needs (visible: entity turns gray)
4. [ ] Needs panel in ImGui
5. [ ] Food/water/O2 source objects
6. [ ] Pathfinding to nearest resource
7. [ ] Consume action (restores need)
8. [ ] Pause/step/speed controls working

### Deliverable
**An astronaut that pathfinds to resources to stay alive, dies if it fails.**

---

## Sprint 3 (Week 5-6): Utility AI

### Tasks
1. [ ] Utility curves for each need
2. [ ] Action scoring system
3. [ ] FSM with states: Idle, MovingToResource, Consuming, Sleeping
4. [ ] Utility debugger panel (show all scores)
5. [ ] Current state display above astronaut
6. [ ] Basic save/load (EnTT snapshot)
7. [ ] Event log panel (scrolling text)

### Deliverable
**An autonomous astronaut making visible decisions based on needs.**

---

# 📚 Technology Stack

| Component | Library | Reason |
|-----------|---------|--------|
| ECS | **EnTT** | Industry-standard, header-only, fast |
| Rendering | **Raylib** | Simple, cross-platform, C/C++ |
| Debug UI | **Dear ImGui + rlImGui** | Immediate mode, powerful |
| Pathfinding | **Custom A*** | Simple, integrates with hazards |
| Spatial Index | **Grid-based linked list** | Simple, sufficient for 2.5D |
| AI | **Utility + FSM** | Debuggable, tweakable |
| Physics | **Existing LBM/Thermal** | Already GPU-accelerated |

---

*This roadmap prioritizes visual feedback from day one, leverages proven libraries, and builds toward emergent storytelling through systems, not scripted narratives.*
