# RealCraft Development Milestones

This document outlines the complete development roadmap for RealCraft, organized into phases, milestones, and individual tasks.

## Status Legend

**Phase/Milestone Status:**
- `[ ]` Not Started
- `[~]` In Progress
- `[x]` Completed

**Complexity Ratings:**
- `[Low]` - Straightforward implementation, well-understood patterns
- `[Medium]` - Moderate complexity, some challenges expected
- `[High]` - Complex implementation, significant R&D or optimization needed

---

## Phase 0: Project Foundation `[x]`

**Goal:** Establish build system, project structure, and development infrastructure

### Milestone 0.1: Build System & Tooling `[Low]` `[x]`

- [x] Create CMakeLists.txt with cross-platform configuration
- [x] Configure C++20 compiler settings and optimizations
- [x] Implement platform detection (macOS, Windows, Linux)
- [x] Set up dependency management (vcpkg or Conan)
- [x] Create build scripts for Debug/Release configurations
- [x] Configure CI/CD pipeline (GitHub Actions)

### Milestone 0.2: Project Structure `[Low]` `[x]`

- [x] Create directory layout:
  - [x] `src/` - Source files organized by subsystem
  - [x] `include/` - Public headers
  - [x] `shaders/` - MSL, HLSL, GLSL shader sources
  - [x] `assets/` - Textures, sounds, data files
  - [x] `tests/` - Unit and integration tests
  - [x] `tools/` - Build and development utilities
- [x] Create .gitignore with appropriate exclusions
- [x] Create .clang-format for code style consistency
- [x] Create .editorconfig for editor settings
- [x] Set up pre-commit hooks for formatting

### Milestone 0.3: Third-Party Dependencies `[Medium]` `[x]`

- [x] Evaluate and select physics library (Bullet vs PhysX)
- [x] Integrate physics library
- [x] Integrate noise library (FastNoise2)
- [x] Integrate math library (GLM)
- [x] Integrate testing framework (Google Test or Catch2)
- [x] Integrate logging library (spdlog or similar)
- [x] Document all dependencies and versions

---

## Phase 1: Core Engine & Platform Abstraction `[x]`

**Goal:** Create foundational engine layer with window management, input, and graphics API abstraction

### Milestone 1.1: Platform Abstraction Layer `[Medium]` `[x]`

- [x] Implement window creation and management
  - [x] GLFW integration or native window APIs
  - [x] Fullscreen/windowed mode switching
  - [x] Multi-monitor support
- [x] Implement input handling
  - [x] Keyboard input with key states
  - [x] Mouse input with position and buttons
  - [x] Mouse capture for FPS controls
  - [x] Input mapping/rebinding system
- [x] Implement file I/O abstraction
  - [x] Cross-platform path handling
  - [x] Async file loading
  - [x] Resource caching
- [x] Implement timer/clock utilities
  - [x] High-resolution timers
  - [x] Frame time measurement
  - [x] Delta time calculation

### Milestone 1.2: Graphics API Abstraction `[High]` `[x]`

- [x] Design graphics abstraction interface
  - [x] Device/context management
  - [x] Command buffer abstraction
  - [x] Resource (buffer, texture) abstraction
  - [x] Pipeline state abstraction
- [x] Implement Metal backend (macOS)
  - [x] Device initialization
  - [x] Swap chain setup
  - [x] Command encoding
  - [x] Resource creation
- [~] Implement Vulkan backend (Windows/Linux)
  - [~] Stub implementation (throws NotImplemented)
- [x] Implement shader compilation pipeline
  - [x] GLSL to SPIR-V compilation (glslang)
  - [x] SPIR-V to MSL cross-compilation (SPIRV-Cross)
  - [x] Shader reflection for uniforms
  - [ ] Hot-reload support for development (deferred)

### Milestone 1.3: Engine Core `[Medium]` `[x]`

- [x] Implement main game loop
  - [x] Fixed timestep for physics
  - [x] Variable timestep for rendering
  - [x] Frame limiting options
- [x] Implement configuration/settings system
  - [x] Config file parsing (JSON/TOML)
  - [x] Runtime settings modification
  - [x] Settings persistence
- [x] Implement logging system
  - [x] Multiple log levels
  - [x] File and console output
  - [x] Performance-friendly implementation
- [x] Implement memory management
  - [x] Custom allocators for subsystems
  - [x] Memory tracking in debug builds

---

## Phase 2: Voxel World Foundation `[ ]`

**Goal:** Implement core voxel data structures and chunk system

### Milestone 2.1: Voxel Data Structures `[Medium]`

- [ ] Define block type system
  - [ ] Block ID registry
  - [ ] Block property definitions (solid, transparent, liquid)
  - [ ] Material properties (weight, strength, hardness)
  - [ ] Block states (orientation, variants)
- [ ] Implement voxel storage format
  - [ ] Basic 3D array storage
  - [ ] Run-length encoding for compression
  - [ ] Palette-based storage for variety
- [ ] Create block registry
  - [ ] Register core block types (stone, dirt, grass, water, etc.)
  - [ ] Block texture mappings
  - [ ] Block behavior hooks

### Milestone 2.2: Chunk System `[Medium]`

- [ ] Implement Chunk class
  - [ ] Define chunk dimensions (e.g., 32x32x256)
  - [ ] Voxel data storage
  - [ ] Chunk metadata (biome, modified flag)
- [ ] Implement coordinate systems
  - [ ] World coordinates (double precision)
  - [ ] Chunk coordinates (integer)
  - [ ] Local voxel coordinates
  - [ ] World-to-chunk conversion
- [ ] Implement origin shifting
  - [ ] Track player position for shift
  - [ ] Rebase all world objects on shift
  - [ ] Maintain precision at large distances
- [ ] Implement chunk neighbors/adjacency
  - [ ] Neighbor lookup
  - [ ] Cross-chunk voxel access

### Milestone 2.3: World Manager `[Medium]`

- [ ] Implement chunk lifecycle management
  - [ ] Chunk creation
  - [ ] Chunk loading from disk
  - [ ] Chunk unloading and saving
- [ ] Implement active chunk tracking
  - [ ] Player-centered loading radius
  - [ ] Priority queue for loading order
  - [ ] View distance settings
- [ ] Implement thread-safe chunk access
  - [ ] Read/write locks per chunk
  - [ ] Atomic chunk state transitions
  - [ ] Safe concurrent modification
- [ ] Implement chunk modification events
  - [ ] Block change notifications
  - [ ] Dirty chunk tracking
  - [ ] Observer pattern for subsystems

---

## Phase 3: Basic Rendering Pipeline `[ ]`

**Goal:** Implement rasterized voxel rendering as foundation before path tracing

### Milestone 3.1: Voxel Mesh Generation `[Medium]`

- [ ] Implement naive mesh generation
  - [ ] Generate quads for exposed faces
  - [ ] Face normal calculation
  - [ ] UV coordinate generation
- [ ] Implement greedy meshing
  - [ ] Merge coplanar adjacent faces
  - [ ] Reduce vertex/triangle count
  - [ ] Handle block type boundaries
- [ ] Implement hidden face culling
  - [ ] Skip faces between solid blocks
  - [ ] Handle transparent block adjacency
  - [ ] Cross-chunk face culling
- [ ] Implement mesh updates
  - [ ] Incremental mesh updates on block change
  - [ ] Batch updates for efficiency
  - [ ] Background mesh generation
- [ ] Implement mesh LOD
  - [ ] Reduced detail for distant chunks
  - [ ] LOD transition handling

### Milestone 3.2: Basic Rasterization Pipeline `[Medium]`

- [ ] Implement vertex/fragment shaders
  - [ ] Basic vertex transformation
  - [ ] Texture sampling
  - [ ] Basic lighting in fragment shader
- [ ] Implement camera system
  - [ ] First-person camera
  - [ ] Free/spectator camera
  - [ ] Camera matrices (view, projection)
  - [ ] FOV settings
- [ ] Implement frustum culling
  - [ ] Extract frustum planes
  - [ ] AABB-frustum intersection
  - [ ] Per-chunk culling
- [ ] Implement depth buffering
  - [ ] Proper depth testing
  - [ ] Handle transparent objects

### Milestone 3.3: Basic Lighting `[Medium]`

- [ ] Implement directional sunlight
  - [ ] Sun direction from time of day
  - [ ] Diffuse lighting calculation
  - [ ] Basic specular highlights
- [ ] Implement ambient occlusion
  - [ ] Per-vertex AO from neighbor blocks
  - [ ] Smooth AO interpolation
- [ ] Implement day/night cycle
  - [ ] Time system
  - [ ] Sun/moon position
  - [ ] Sky color transitions
  - [ ] Light level changes
- [ ] Implement basic shadow mapping
  - [ ] Directional shadow maps
  - [ ] Cascaded shadow maps for distance
  - [ ] Shadow filtering (PCF)

### Milestone 3.4: Texture System `[Low]`

- [ ] Implement texture atlas
  - [ ] Pack block textures into atlas
  - [ ] Atlas coordinate calculation
  - [ ] Mipmap generation
- [ ] Implement texture coordinates
  - [ ] Per-face UV mapping
  - [ ] Texture rotation for variety
- [ ] Implement basic PBR properties
  - [ ] Albedo textures
  - [ ] Normal maps
  - [ ] Roughness/metallic values

---

## Phase 4: World Generation System `[ ]`

**Goal:** Implement procedural terrain generation with noise and erosion

### Milestone 4.1: Noise-Based Terrain `[Medium]`

- [ ] Implement noise generation
  - [ ] Integrate FastNoise2
  - [ ] Perlin/Simplex noise setup
  - [ ] Noise parameter configuration
- [ ] Implement fractal octave composition
  - [ ] Multiple octave layers
  - [ ] Persistence and lacunarity
  - [ ] Domain warping
- [ ] Implement heightmap generation
  - [ ] 2D heightmap from noise
  - [ ] Height scaling and offset
  - [ ] Terrain type blending
- [ ] Implement 3D density
  - [ ] 3D noise for caves/overhangs
  - [ ] Density threshold for solid blocks
  - [ ] Blending with heightmap

### Milestone 4.2: Biome System `[Medium]`

- [ ] Implement climate model
  - [ ] Temperature noise map
  - [ ] Rainfall/humidity noise map
  - [ ] Altitude influence
- [ ] Implement biome distribution
  - [ ] Biome selection from climate
  - [ ] Biome definitions (forest, desert, tundra, etc.)
  - [ ] Biome-specific block palettes
- [ ] Implement smooth biome transitions
  - [ ] Edge blending
  - [ ] Gradient transitions
- [ ] Implement biome-specific generation
  - [ ] Per-biome noise parameters
  - [ ] Biome-specific features

### Milestone 4.3: Hydraulic Erosion `[High]`

- [ ] Implement CPU erosion (particle-based)
  - [ ] Raindrop simulation
  - [ ] Sediment pickup and deposit
  - [ ] Terrain modification
  - [ ] Parameter tuning
- [ ] Implement GPU compute erosion
  - [ ] Compute shader for erosion
  - [ ] Heightmap upload/download
  - [ ] Parallel droplet simulation
- [ ] Implement river channel carving
  - [ ] Water flow accumulation
  - [ ] Channel depth calculation
  - [ ] River path generation
- [ ] Implement sediment deposition
  - [ ] Alluvial plains
  - [ ] Delta formation
  - [ ] Sediment block types

### Milestone 4.4: Cave Generation `[Medium]`

- [ ] Implement 3D cave noise
  - [ ] Perlin worms algorithm
  - [ ] Cellular automata alternative
  - [ ] Cave density control
- [ ] Implement cave system features
  - [ ] Cave entrance placement
  - [ ] Chamber generation
  - [ ] Tunnel connections
- [ ] Implement underground water
  - [ ] Water table level
  - [ ] Underground rivers
  - [ ] Flooded caves
- [ ] Implement cave decoration
  - [ ] Stalactites and stalagmites
  - [ ] Crystal formations
  - [ ] Moss/fungi in damp areas

### Milestone 4.5: Vegetation & Features `[Medium]`

- [ ] Implement tree generation
  - [ ] L-system tree generation
  - [ ] Tree templates by biome
  - [ ] Tree size variation
  - [ ] Realistic root systems (optional)
- [ ] Implement ground cover
  - [ ] Grass placement
  - [ ] Flower distribution
  - [ ] Bush/shrub generation
- [ ] Implement ore distribution
  - [ ] Ore vein generation
  - [ ] Depth-based ore types
  - [ ] Cluster sizes
- [ ] Implement structure placement
  - [ ] Ruins/abandoned structures (optional)
  - [ ] Natural formations (rock spires, etc.)

### Milestone 4.6: Chunk Border Handling `[Medium]`

- [ ] Implement cross-chunk erosion
  - [ ] Erosion buffer zones
  - [ ] Consistent results across borders
- [ ] Implement seamless terrain
  - [ ] Height continuity
  - [ ] Biome edge handling
- [ ] Implement river continuation
  - [ ] Cross-chunk water flow
  - [ ] River network consistency

---

## Phase 5: Physics Engine `[ ]`

**Goal:** Implement collision, rigid body dynamics, and structural integrity

### Milestone 5.1: Collision Detection `[Medium]`

- [ ] Implement voxel collision mesh
  - [ ] Generate collision shapes from chunks
  - [ ] Optimize for static terrain
  - [ ] Update on block changes
- [ ] Implement collider types
  - [ ] AABB colliders
  - [ ] Capsule colliders (player, entities)
  - [ ] Convex hull colliders (debris)
- [ ] Implement ray casting
  - [ ] Ray-voxel intersection
  - [ ] Hit information (position, normal, block)
  - [ ] Ray length limits
- [ ] Implement broad-phase acceleration
  - [ ] Spatial partitioning (octree)
  - [ ] Chunk-based culling

### Milestone 5.2: Rigid Body Dynamics `[Medium]`

- [ ] Integrate physics library (Bullet)
  - [ ] World setup
  - [ ] Collision configuration
  - [ ] Solver configuration
- [ ] Implement gravity and forces
  - [ ] Global gravity
  - [ ] Per-object forces
  - [ ] Impulse application
- [ ] Implement collision response
  - [ ] Bounce/friction coefficients
  - [ ] Material-based response
- [ ] Implement fixed timestep simulation
  - [ ] Accumulator pattern
  - [ ] Interpolation for rendering
  - [ ] Substep configuration

### Milestone 5.3: Player Physics `[Medium]`

- [ ] Implement capsule controller
  - [ ] Player collision shape
  - [ ] Stair stepping
  - [ ] Slope handling
- [ ] Implement ground detection
  - [ ] Ground contact check
  - [ ] Ground normal
  - [ ] Edge detection
- [ ] Implement jump mechanics
  - [ ] Jump impulse
  - [ ] Variable jump height
  - [ ] Coyote time (optional)
- [ ] Implement movement physics
  - [ ] Acceleration/deceleration
  - [ ] Friction on surfaces
  - [ ] Air control limits
  - [ ] Swimming physics

### Milestone 5.4: Structural Integrity System `[High]`

- [ ] Implement block connectivity graph
  - [ ] Track connected blocks
  - [ ] Efficient graph updates
  - [ ] Connection types (face, edge, corner)
- [ ] Implement support propagation
  - [ ] Support from ground
  - [ ] Support transfer through blocks
  - [ ] Support distance limits
- [ ] Implement material properties
  - [ ] Weight per block type
  - [ ] Strength/support capacity
  - [ ] Brittleness
- [ ] Implement collapse detection
  - [ ] Check on block removal
  - [ ] Identify unsupported clusters
  - [ ] Cascade collapse handling

### Milestone 5.5: Debris & Destruction `[High]`

- [ ] Implement voxel-to-rigid-body conversion
  - [ ] Create rigid body from voxel cluster
  - [ ] Calculate mass and center of gravity
  - [ ] Approximate collision shape
- [ ] Implement debris fragmentation
  - [ ] Break large clusters into fragments
  - [ ] Realistic break patterns
  - [ ] Fragment size limits
- [ ] Implement impact damage
  - [ ] Damage on debris collision
  - [ ] Block breaking from impacts
  - [ ] Entity damage from debris
- [ ] Implement debris lifecycle
  - [ ] Despawn timer
  - [ ] Debris limit
  - [ ] Debris-to-block conversion (optional)

### Milestone 5.6: Fluid Simulation `[High]`

- [ ] Implement cellular automata water
  - [ ] Water level per block
  - [ ] Flow to neighbors
  - [ ] Infinite source blocks
- [ ] Implement water pressure
  - [ ] Pressure from depth
  - [ ] Pressure-based flow rate
  - [ ] Equalization
- [ ] Implement buoyancy
  - [ ] Buoyant force calculation
  - [ ] Object density
  - [ ] Floating/sinking behavior
- [ ] Implement water currents
  - [ ] Flow direction tracking
  - [ ] Entity push from current
  - [ ] Current visualization

---

## Phase 6: Core Gameplay Systems `[ ]`

**Goal:** Implement player interaction, inventory, and basic gameplay loop

### Milestone 6.1: Player Controller `[Medium]`

- [ ] Implement first-person camera control
  - [ ] Mouse look
  - [ ] Look sensitivity settings
  - [ ] Head bob (optional)
- [ ] Implement movement
  - [ ] WASD movement
  - [ ] Sprint (shift)
  - [ ] Crouch (ctrl)
  - [ ] Jump (space)
- [ ] Implement player state machine
  - [ ] Walking/running/crouching
  - [ ] Swimming
  - [ ] Falling
  - [ ] Climbing (ladders)

### Milestone 6.2: Block Interaction `[Medium]`

- [ ] Implement block targeting
  - [ ] Ray cast from camera
  - [ ] Target block highlight
  - [ ] Reach distance limit
  - [ ] Adjacent face detection for placement
- [ ] Implement block breaking
  - [ ] Mining progress system
  - [ ] Tool effectiveness
  - [ ] Breaking animation/particles
  - [ ] Block drop on break
- [ ] Implement block placement
  - [ ] Place on targeted face
  - [ ] Placement validation
  - [ ] Orientation handling
  - [ ] Place sound

### Milestone 6.3: Inventory System `[Medium]`

- [ ] Implement inventory data structure
  - [ ] Slot-based inventory
  - [ ] Item stacks
  - [ ] Stack size limits
- [ ] Implement item types
  - [ ] Block items
  - [ ] Tool items
  - [ ] Consumable items
- [ ] Implement hotbar
  - [ ] Quick slot selection (1-9)
  - [ ] Scroll wheel selection
  - [ ] Active item tracking
- [ ] Implement item pickup
  - [ ] Dropped item entities
  - [ ] Pickup radius
  - [ ] Auto-stack on pickup

### Milestone 6.4: Basic UI `[Low]`

- [ ] Implement crosshair
  - [ ] Center screen crosshair
  - [ ] Style options
- [ ] Implement hotbar display
  - [ ] Item icons
  - [ ] Stack counts
  - [ ] Selection highlight
- [ ] Implement health/hunger bars
  - [ ] Health bar (survival)
  - [ ] Hunger bar (survival)
  - [ ] Status effects display
- [ ] Implement debug overlay
  - [ ] FPS counter
  - [ ] Position/chunk info
  - [ ] Performance stats
  - [ ] Toggle with F3

---

## Phase 7: Advanced Rendering (Path Tracing) `[ ]`

**Goal:** Implement ray-traced/path-traced rendering pipeline

### Milestone 7.1: Acceleration Structures `[High]`

- [ ] Implement Sparse Voxel Octree (SVO)
  - [ ] Octree construction from chunks
  - [ ] Node compression
  - [ ] Memory-efficient storage
- [ ] Implement SVO incremental updates
  - [ ] Partial tree updates on block change
  - [ ] Lazy evaluation
  - [ ] Background updates
- [ ] Implement BVH for dynamic objects
  - [ ] Build BVH for entities/debris
  - [ ] Fast rebuild for moving objects

### Milestone 7.2: Path Tracing Core `[High]`

- [ ] Implement ray generation
  - [ ] Camera ray computation
  - [ ] Jittered sampling for AA
  - [ ] Ray differentials (optional)
- [ ] Implement SVO ray traversal
  - [ ] DDA/parametric traversal
  - [ ] Node stack for recursion
  - [ ] Early termination
- [ ] Implement primary ray intersection
  - [ ] First hit detection
  - [ ] Hit normal computation
  - [ ] Material lookup
- [ ] Implement direct lighting
  - [ ] Sun/sky sampling
  - [ ] Shadow rays
  - [ ] Multiple importance sampling

### Milestone 7.3: Global Illumination `[High]`

- [ ] Implement indirect bounces
  - [ ] Recursive ray tracing
  - [ ] Bounce limit (2-3)
  - [ ] Color bleeding
- [ ] Implement Russian roulette
  - [ ] Probabilistic path termination
  - [ ] Throughput-based probability
- [ ] Implement importance sampling
  - [ ] Cosine-weighted hemisphere
  - [ ] Light importance sampling
  - [ ] BSDF importance sampling
- [ ] Implement sky/environment lighting
  - [ ] Procedural sky model
  - [ ] Environment map sampling
  - [ ] Sky contribution to GI

### Milestone 7.4: Materials & BSDFs `[High]`

- [ ] Implement Lambertian diffuse
  - [ ] Diffuse reflection
  - [ ] Albedo from texture
- [ ] Implement microfacet specular (GGX)
  - [ ] Roughness-based reflection
  - [ ] Fresnel effect
  - [ ] Smith masking
- [ ] Implement emissive materials
  - [ ] Self-illuminating blocks
  - [ ] Emission strength
  - [ ] Light source contribution
- [ ] Implement refraction
  - [ ] Glass/ice refraction
  - [ ] Water refraction
  - [ ] IOR handling
  - [ ] Total internal reflection

### Milestone 7.5: Denoising & Temporal `[High]`

- [ ] Implement temporal accumulation
  - [ ] Sample accumulation over frames
  - [ ] Moving average
  - [ ] Convergence tracking
- [ ] Implement motion vectors
  - [ ] Per-pixel motion
  - [ ] Object motion
- [ ] Integrate AI denoiser
  - [ ] OIDN integration
  - [ ] Normal/albedo auxiliary buffers
  - [ ] Temporal stability
- [ ] Implement reprojection
  - [ ] History buffer
  - [ ] Disocclusion handling
  - [ ] Ghosting mitigation

### Milestone 7.6: Upscaling & Performance `[Medium]`

- [ ] Implement DLSS integration (NVIDIA)
  - [ ] DLSS SDK integration
  - [ ] Quality modes
  - [ ] Motion vector input
- [ ] Implement FSR integration (AMD)
  - [ ] FSR 2.0 integration
  - [ ] Platform-agnostic upscaling
- [ ] Implement MetalFX (Apple)
  - [ ] Temporal upscaling
  - [ ] Apple Silicon optimization
- [ ] Implement dynamic resolution
  - [ ] Frame time-based scaling
  - [ ] Target framerate maintenance

### Milestone 7.7: Atmospheric Effects `[Medium]`

- [ ] Implement volumetric fog
  - [ ] Ray marching through fog
  - [ ] Density variation
  - [ ] Fog color/scattering
- [ ] Implement god rays
  - [ ] Light shaft rendering
  - [ ] Shadow sampling for rays
  - [ ] Intensity control
- [ ] Implement procedural sky
  - [ ] Atmospheric scattering
  - [ ] Sun/moon rendering
  - [ ] Cloud generation
- [ ] Implement underwater rendering
  - [ ] Color absorption
  - [ ] Caustics (optional)
  - [ ] Visibility falloff

### Milestone 7.8: Post-Processing `[Low]`

- [ ] Implement HDR tone mapping
  - [ ] ACES tone mapping
  - [ ] Exposure control
  - [ ] White balance
- [ ] Implement bloom
  - [ ] Bright pass extraction
  - [ ] Gaussian blur
  - [ ] Additive blend
- [ ] Implement exposure adaptation
  - [ ] Average luminance calculation
  - [ ] Eye adaptation speed
- [ ] Implement optional effects
  - [ ] Depth of field
  - [ ] Motion blur
  - [ ] Vignette

---

## Phase 8: Survival Mode `[ ]`

**Goal:** Implement survival mechanics, crafting, and AI

### Milestone 8.1: Survival Stats `[Medium]`

- [ ] Implement health system
  - [ ] Health points
  - [ ] Damage types
  - [ ] Health regeneration
  - [ ] Death handling
- [ ] Implement hunger/thirst
  - [ ] Hunger meter
  - [ ] Hunger drain rate
  - [ ] Starvation damage
  - [ ] Thirst (optional)
- [ ] Implement fall damage
  - [ ] Damage from fall height
  - [ ] Safe fall distance
  - [ ] Surface hardness modifier
- [ ] Implement environmental damage
  - [ ] Cold damage in snow biomes
  - [ ] Heat damage near lava
  - [ ] Drowning

### Milestone 8.2: Crafting System `[Medium]`

- [ ] Implement recipe system
  - [ ] Recipe data format
  - [ ] Shaped vs shapeless recipes
  - [ ] Recipe registry
- [ ] Implement crafting UI
  - [ ] Crafting grid
  - [ ] Recipe preview
  - [ ] Available recipes list
- [ ] Implement workstation blocks
  - [ ] Crafting table
  - [ ] Furnace
  - [ ] Anvil (optional)
- [ ] Implement smelting/cooking
  - [ ] Fuel consumption
  - [ ] Smelt time
  - [ ] Recipe results

### Milestone 8.3: Tool & Weapon System `[Medium]`

- [ ] Implement tool durability
  - [ ] Durability points
  - [ ] Use consumption
  - [ ] Tool breaking
  - [ ] Repair mechanics
- [ ] Implement tool tiers
  - [ ] Wood/stone/iron/diamond tiers
  - [ ] Mining speed per tier
  - [ ] Block requirements
- [ ] Implement weapons
  - [ ] Melee weapons
  - [ ] Attack damage
  - [ ] Attack speed/cooldown
- [ ] Implement tool recipes
  - [ ] Pickaxe, axe, shovel, sword
  - [ ] Tier-specific materials

### Milestone 8.4: Day/Night Cycle `[Low]`

- [ ] Implement time system
  - [ ] Day length setting
  - [ ] Time speed control
  - [ ] Sleeping to skip night
- [ ] Implement lighting changes
  - [ ] Sun angle from time
  - [ ] Ambient light levels
  - [ ] Torch importance at night
- [ ] Implement gameplay effects
  - [ ] Hostile spawns at night
  - [ ] Vision reduction
  - [ ] Sleep requirement (optional)

### Milestone 8.5: Entity System `[Medium]`

- [ ] Implement entity base class
  - [ ] Position/rotation
  - [ ] Health
  - [ ] Collision
  - [ ] AI component hook
- [ ] Implement spawn system
  - [ ] Biome-based spawning
  - [ ] Spawn rate control
  - [ ] Max entity limits
  - [ ] Spawn distance from player
- [ ] Implement pathfinding
  - [ ] A* on voxel grid
  - [ ] Path caching
  - [ ] Jump gap handling
  - [ ] Swim paths
- [ ] Implement entity-world collision
  - [ ] Entity vs terrain
  - [ ] Entity vs entity
  - [ ] Stuck detection

### Milestone 8.6: Animal AI `[Medium]`

- [ ] Implement passive animals
  - [ ] Wander behavior
  - [ ] Flee when attacked
  - [ ] Breeding (optional)
  - [ ] Drop items on death
- [ ] Implement hostile creatures
  - [ ] Player detection
  - [ ] Chase behavior
  - [ ] Attack behavior
  - [ ] Day/night activity
- [ ] Implement behavior state machines
  - [ ] Idle state
  - [ ] Patrol/wander state
  - [ ] Alert state
  - [ ] Combat state
  - [ ] Flee state
- [ ] Implement drops and loot
  - [ ] Death drops
  - [ ] Drop tables
  - [ ] XP drops (optional)

---

## Phase 9: Creative Mode `[ ]`

**Goal:** Implement creative mode features and building tools

### Milestone 9.1: Creative Mode Core `[Low]`

- [ ] Implement unlimited resources
  - [ ] Access to all blocks
  - [ ] No inventory limits
  - [ ] Block count display off
- [ ] Implement instant break/place
  - [ ] No mining time
  - [ ] Instant placement
  - [ ] No tool requirements
- [ ] Implement invincibility
  - [ ] No health system
  - [ ] No hunger
  - [ ] No fall damage
- [ ] Implement mode switching
  - [ ] Command or menu option
  - [ ] Inventory transition

### Milestone 9.2: Flight System `[Low]`

- [ ] Implement flight toggle
  - [ ] Double-jump to fly
  - [ ] Flight indicator
- [ ] Implement free movement
  - [ ] 3D movement controls
  - [ ] No gravity while flying
  - [ ] Ascend/descend keys
- [ ] Implement speed controls
  - [ ] Sprint to fly faster
  - [ ] Speed multiplier settings
- [ ] Implement collision toggle
  - [ ] No-clip mode option
  - [ ] Phase through blocks

### Milestone 9.3: Building Tools `[Medium]`

- [ ] Implement block palette
  - [ ] Full block browser
  - [ ] Category organization
  - [ ] Search functionality
  - [ ] Recently used blocks
- [ ] Implement physics toggle
  - [ ] Enable/disable structural physics
  - [ ] Floating blocks allowed
  - [ ] Per-session setting
- [ ] Implement large-scale operations
  - [ ] Fill command
  - [ ] Copy/paste regions
  - [ ] Undo/redo stack
  - [ ] Selection tools (optional)

---

## Phase 10: Audio Engine `[ ]`

**Goal:** Implement spatial audio and sound effects

### Milestone 10.1: Audio System Core `[Medium]`

- [ ] Integrate audio library
  - [ ] OpenAL or alternative
  - [ ] Audio context setup
  - [ ] Device enumeration
- [ ] Implement sound loading
  - [ ] WAV/OGG loading
  - [ ] Sound caching
  - [ ] Streaming for music
- [ ] Implement 3D spatial audio
  - [ ] Source positioning
  - [ ] Listener position/orientation
  - [ ] Distance attenuation
  - [ ] Occlusion (optional)
- [ ] Implement audio mixing
  - [ ] Volume categories (master, music, SFX)
  - [ ] Channel limits
  - [ ] Priority system

### Milestone 10.2: Sound Effects `[Low]`

- [ ] Implement block sounds
  - [ ] Break sounds per material
  - [ ] Place sounds
  - [ ] Step sounds per surface
- [ ] Implement entity sounds
  - [ ] Creature sounds
  - [ ] Damage sounds
  - [ ] Ambient creature sounds
- [ ] Implement environmental sounds
  - [ ] Wind
  - [ ] Water flowing
  - [ ] Rain
  - [ ] Cave ambience

### Milestone 10.3: Music System `[Low]`

- [ ] Implement music playback
  - [ ] Background music tracks
  - [ ] Music queue
  - [ ] Shuffle/repeat
- [ ] Implement biome music
  - [ ] Per-biome music selection
  - [ ] Transition on biome change
- [ ] Implement crossfading
  - [ ] Smooth track transitions
  - [ ] Fade duration settings

---

## Phase 11: Save System & Polish `[ ]`

**Goal:** Implement persistence, UI polish, and optimization

### Milestone 11.1: Save System `[Medium]`

- [ ] Implement region storage
  - [ ] Region file format
  - [ ] Chunk serialization
  - [ ] Block data compression (zlib)
- [ ] Implement chunk persistence
  - [ ] Save on unload
  - [ ] Load on demand
  - [ ] Dirty chunk tracking
- [ ] Implement player data
  - [ ] Position/rotation
  - [ ] Inventory
  - [ ] Health/hunger
  - [ ] Game mode
- [ ] Implement autosave
  - [ ] Periodic save timer
  - [ ] Background saving
  - [ ] Save on exit

### Milestone 11.2: UI Polish `[Medium]`

- [ ] Implement main menu
  - [ ] New world
  - [ ] Load world
  - [ ] Settings
  - [ ] Exit
- [ ] Implement pause menu
  - [ ] Resume
  - [ ] Settings access
  - [ ] Save and quit
- [ ] Implement settings screen
  - [ ] Graphics settings
  - [ ] Audio settings
  - [ ] Control settings
  - [ ] Gameplay settings
- [ ] Implement world management
  - [ ] World list
  - [ ] World creation options
  - [ ] Delete world
  - [ ] World info

### Milestone 11.3: Performance Optimization `[High]`

- [ ] Implement profiling
  - [ ] Frame time breakdown
  - [ ] Per-system timing
  - [ ] Memory usage tracking
- [ ] Optimize rendering
  - [ ] Draw call batching
  - [ ] Culling improvements
  - [ ] Shader optimization
- [ ] Optimize physics
  - [ ] Collision optimization
  - [ ] Sleep inactive bodies
  - [ ] Spatial partitioning
- [ ] Optimize world gen
  - [ ] Generation caching
  - [ ] LOD generation
  - [ ] Background thread tuning
- [ ] Optimize memory
  - [ ] Chunk compression in memory
  - [ ] Asset streaming
  - [ ] Pool allocators

### Milestone 11.4: Quality Settings `[Low]`

- [ ] Implement graphics presets
  - [ ] Low/Medium/High/Ultra
  - [ ] Custom settings
- [ ] Implement individual settings
  - [ ] Ray tracing toggle
  - [ ] Bounce count
  - [ ] Draw distance
  - [ ] Shadow quality
  - [ ] Resolution scale
- [ ] Implement performance targets
  - [ ] Frame rate limits
  - [ ] VSync options

### Milestone 11.5: Accessibility `[Low]`

- [ ] Implement UI scaling
  - [ ] Font sizes
  - [ ] HUD scale
- [ ] Implement colorblind modes
  - [ ] Deuteranopia
  - [ ] Protanopia
  - [ ] Tritanopia
- [ ] Implement control rebinding
  - [ ] Keyboard mapping
  - [ ] Mouse sensitivity
  - [ ] Invert options
- [ ] Implement subtitles
  - [ ] Sound direction indicators
  - [ ] Environmental audio cues

---

## Phase 12: Testing & Release Prep `[ ]`

**Goal:** Comprehensive testing and release preparation

### Milestone 12.1: Testing `[Medium]`

- [ ] Implement unit tests
  - [ ] Voxel data structures
  - [ ] Coordinate conversions
  - [ ] Physics calculations
  - [ ] World generation
- [ ] Implement integration tests
  - [ ] Chunk loading/saving
  - [ ] Block interactions
  - [ ] Crafting system
- [ ] Implement performance benchmarks
  - [ ] Rendering benchmarks
  - [ ] Physics stress tests
  - [ ] World gen speed tests
- [ ] Implement edge case testing
  - [ ] Extreme coordinates
  - [ ] Large structures
  - [ ] Memory limits
  - [ ] Long play sessions

### Milestone 12.2: Platform Testing `[Medium]`

- [ ] macOS testing
  - [ ] M1/M2/M3 chips
  - [ ] Various memory configs
  - [ ] Different macOS versions
- [ ] Windows testing (if applicable)
  - [ ] NVIDIA GPUs
  - [ ] AMD GPUs
  - [ ] Intel GPUs
  - [ ] Various driver versions
- [ ] Linux testing (if applicable)
  - [ ] Major distributions
  - [ ] Vulkan drivers

### Milestone 12.3: Documentation `[Low]`

- [ ] Create player guide
  - [ ] Controls reference
  - [ ] Gameplay basics
  - [ ] Tips and tricks
- [ ] Create settings documentation
  - [ ] Graphics options explained
  - [ ] Performance recommendations
- [ ] Update README
  - [ ] System requirements
  - [ ] Installation instructions
  - [ ] Known issues

### Milestone 12.4: Release `[Low]`

- [ ] Implement build packaging
  - [ ] macOS .app bundle
  - [ ] Windows installer (optional)
  - [ ] Linux package (optional)
- [ ] Create release assets
  - [ ] Icons and branding
  - [ ] Screenshots
  - [ ] Trailer (optional)
- [ ] Finalize versioning
  - [ ] Version number scheme
  - [ ] Changelog
- [ ] Create release notes
  - [ ] Feature summary
  - [ ] Known issues
  - [ ] Future roadmap

---

## Phase Dependencies

```
Phase 0 (Foundation)
    └── Phase 1 (Core Engine)
            ├── Phase 2 (Voxel World)
            │       └── Phase 4 (World Gen)
            │               └── Phase 5 (Physics) ──┐
            │                                       │
            ├── Phase 3 (Basic Rendering) ──────────┤
            │       └── Phase 7 (Path Tracing)      │
            │                                       │
            └── Phase 10 (Audio)                    │
                                                    │
                                           Phase 6 (Core Gameplay)
                                                    │
                                    ┌───────────────┴───────────────┐
                                    │                               │
                            Phase 8 (Survival)              Phase 9 (Creative)
                                    │                               │
                                    └───────────────┬───────────────┘
                                                    │
                                           Phase 11 (Polish)
                                                    │
                                           Phase 12 (Release)
```

## Development Notes

1. **Parallel Development**: Phases 7 (Path Tracing) and 10 (Audio) can be developed in parallel with other phases after their dependencies are met.

2. **Iterative Approach**: Basic versions of features should be implemented first, then refined. Don't aim for perfection on first pass.

3. **Playtesting**: Regular playtesting should begin after Phase 6 (Core Gameplay) is complete. Feedback should inform later phases.

4. **Performance Budget**: Keep performance in mind throughout. Profile regularly, especially for rendering and physics.

5. **Platform Priority**: macOS with Metal is the primary target. Cross-platform support is secondary but should be considered in architecture decisions.

6. **Scope Management**: This document represents the full vision. If time/resources are limited, prioritize Core Gameplay (Phase 6) as the minimum viable product, with path tracing and survival mechanics as stretch goals.
