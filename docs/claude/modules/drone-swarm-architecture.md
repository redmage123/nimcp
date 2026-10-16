# Edge Brain Architecture — Distillation, Deployment & Distributed Cognition

**Status**: Design Document
**Date**: 2026-03-20

## Overview

A fully trained master brain (1M+ neurons, full cognitive stack) produces
optimized child brains for resource-constrained devices. The master handles
distillation, optimization, and ongoing federated learning. Child brains
operate autonomously and can communicate with peers and the master.

**Target platforms**: phones, drones, IoT devices, robots, embedded MCUs,
wearables, edge servers — anything with less compute than the master.

```
                    ┌─────────────────────┐
                    │    MASTER BRAIN     │
                    │    1M+ neurons      │
                    │    Full cognition   │
                    │    Training server  │
                    │    Distillation     │
                    │    Optimization     │
                    └────────┬────────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
    ┌─────┴─────┐     ┌─────┴─────┐     ┌─────┴─────┐
    │  Phone    │     │  Drone    │     │  Robot    │
    │  100K     │     │  50K      │     │  200K     │
    │  neurons  │     │  neurons  │     │  neurons  │
    └───────────┘     └─────┬─────┘     └───────────┘
                            │ peer mesh
                    ┌───────┼───────┐
                    │ Drone │ Drone │ ... × N
                    │  50K  │  50K  │
                    └───────┘───────┘
```

The master brain's job is to ensure each child brain is **fully optimized**
for its target platform — right neuron count, right subsystems, right
memory footprint, right inference latency.

---

## Part 1: Platform-Aware Distillation

The master brain auto-optimizes based on target device capabilities.
The device reports its constraints; the master figures out the rest.

### Device Profile

```c
typedef struct {
    // Hardware constraints (reported by device)
    uint32_t ram_mb;                  // Available RAM
    uint32_t vram_mb;                 // GPU VRAM (0 = CPU only)
    uint32_t cpu_cores;               // Available cores
    float cpu_gflops;                 // Compute budget
    uint32_t storage_mb;              // For checkpoint
    float power_budget_watts;         // 0 = unlimited
    float target_inference_ms;        // Max acceptable latency
    float target_hz;                  // Decision frequency (e.g., 50 Hz)

    // Capabilities (what the device has)
    bool has_gpu;
    bool has_camera;
    bool has_microphone;
    bool has_imu;
    bool has_motor_control;
    bool has_network;                 // Can communicate with master/peers
    bool has_persistent_storage;      // Can save checkpoints

    // Mission profile
    nimcp_device_role_t role;         // SENSOR, ACTUATOR, COORDINATOR, GENERAL
    const char* device_name;          // "drone_scout_3", "phone_user_app"
} nimcp_device_profile_t;

typedef enum {
    NIMCP_DEVICE_SENSOR,              // Observe and report (IoT, camera)
    NIMCP_DEVICE_ACTUATOR,            // Act on environment (robot, drone)
    NIMCP_DEVICE_COORDINATOR,         // Lead a group (swarm leader, hub)
    NIMCP_DEVICE_GENERAL              // General purpose (phone, tablet)
} nimcp_device_role_t;
```

### Master Auto-Optimization

```c
// Master brain analyzes device profile and produces optimal child config
nimcp_status_t nimcp_brain_optimize_for_device(
    nimcp_brain_t master,
    const nimcp_device_profile_t* device,
    nimcp_brain_t* child,             // Output: optimized child brain
    nimcp_optimization_report_t* report);  // What was included/excluded

typedef struct {
    uint32_t neuron_count;            // How many neurons fit
    uint32_t subsystems_enabled;      // Bitmask of enabled subsystems
    uint32_t subsystems_disabled;     // Bitmask of disabled subsystems
    float estimated_ram_mb;           // Predicted RAM usage
    float estimated_inference_ms;     // Predicted inference time
    float accuracy_retention;         // Estimated % of master accuracy kept
    const char* warnings[16];         // Any concerns
    uint32_t num_warnings;
} nimcp_optimization_report_t;
```

The master applies these rules automatically:

```
1. NEURON BUDGET:
   neurons = min(
     ram_mb * 150,              // ~6.5 KB/neuron → ~150 neurons/MB
     target_inference_ms * 50000, // ~50K neurons per ms on typical CPU
     MAX_NEURONS
   )

2. SUBSYSTEM SELECTION:
   always_on = {security, immune, ethics, stdp, homeostasis}
   if (has_camera)     enable visual_cortex
   if (has_microphone) enable audio_cortex
   if (has_imu)        enable somatosensory
   if (has_motor_control) enable cerebellum, basal_ganglia
   if (has_network)    enable swarm_registry
   if (role == COORDINATOR) enable executive_control, working_memory
   if (ram_mb > 512)   enable lnn, pr_memory
   if (power_budget < 5W) disable gpu, bio_async

3. DISTILLATION:
   - Importance-score all master neurons
   - Select top-N for device's neuron budget
   - Preserve neurons relevant to device's sensors
   - Knowledge distillation training (teacher-student)
   - Verify accuracy on device-relevant test set

4. CHECKPOINT:
   - Produce device-specific checkpoint
   - Include only enabled subsystem state
   - Compress for storage_mb constraint
```

### Platform Examples

| Device | RAM | CPU | Neurons | Subsystems | Inference |
|--------|-----|-----|---------|------------|-----------|
| ESP32 MCU | 4 MB | 1 core, 240 MHz | 500 | Core only | <1 ms |
| Raspberry Pi | 1 GB | 4 cores | 100K | Core + vision + WM | 2-5 ms |
| Phone (mid-range) | 4 GB | 8 cores | 200K | Core + vision + audio + WM + LNN | 5-10 ms |
| Drone (Jetson Nano) | 4 GB + 4 GB GPU | 4 cores + GPU | 300K | Core + vision + motor + GPU | 1-3 ms |
| Robot (Jetson Orin) | 16 GB + 8 GB GPU | 8 cores + GPU | 500K | Full edge stack | 2-5 ms |
| Edge server | 32 GB + 16 GB GPU | 16 cores + GPU | 1M | Near-full cognitive | 5-15 ms |

---

## Part 1b: What to Disable by Platform

### Subsystem Tiers

**ALWAYS ON** (~15-25 MB) — safety + core learning:
- Core neural network (forward/backward)
- Security subsystem (input validation, adversarial protection)
- Immune system (exploit detection, self-healing)
- Ethics engine + core directives (safety gate)
- STDP + homeostatic plasticity (continuous learning)
- Plasticity coordinator

**SENSOR-DEPENDENT** (~5-20 MB each) — enable per drone hardware:
- Visual cortex → camera
- Audio cortex → microphone
- Somatosensory → IMU, accelerometer, force sensors
- Multimodal integration → if 2+ sensor types

**MISSION-DEPENDENT** (~5-15 MB each):
- Working memory → sequential decision tasks
- Cerebellum → precision motor timing
- Basal ganglia → action selection (multi-actuator)
- Executive control → multi-task drones
- LNN → temporal pattern learning
- Dragonfly system → target pursuit/tracking
- Neuromodulation → reward-based learning

**DISABLE ON DRONES** (saves ~200-400 MB):
- All emotional systems (joy, grief, empathy, shadow emotions)
- Theory of mind, mirror neurons
- Global workspace, introspection, self-model
- Autobiographical memory
- Symbolic logic, imagination engine, RCOG
- Inner dialogue, recursive cognition
- Glial systems (astrocytes, oligodendrocytes, microglia)
- Pink noise, neuropeptides, endocannabinoid, glymphatic
- Cortical columns, brain regions module
- FEP orchestrator
- Shannon/quantum information theory
- All cognitive engines (unless specifically needed)

### Drone Configuration Profile

```c
nimcp_resize_config_t drone_config = {
    .mode = NIMCP_RESIZE_CONTRACT,
    .target_neuron_count = 50000,         // 50K neurons
    .enable_knowledge_transfer = true,    // Preserve learned representations
    .preserve_io_layers = true,
};

brain_config_t drone_brain_config = {
    .init_mode = BRAIN_INIT_FAST,         // 6 waves only
    .minimal_mode = true,                 // Kill emotions/self-awareness
    .lazy_init_mode = true,               // Init on first access

    // Core (always on)
    .enable_security = true,
    .enable_immune = true,
    .enable_ethics = true,
    .enable_training = true,
    .enable_plasticity = true,

    // Sensor-specific (configure per drone)
    .enable_visual_cortex = true,         // This drone has a camera
    .enable_audio_cortex = false,
    .enable_somatosensory = true,         // IMU data

    // Mission-specific
    .enable_working_memory = true,
    .enable_cerebellum = true,            // Flight control
    .enable_basal_ganglia = true,         // Action selection

    // Swarm
    .enable_distributed = true,           // P2P comms

    // Everything else: OFF
    .enable_glial = false,
    .enable_emotional_tagging = false,
    .enable_theory_of_mind = false,
    .enable_global_workspace = false,
    .enable_introspection = false,
    // ... all other flags false
};
```

### Resource Budget per Drone

| Component | RAM | Notes |
|-----------|-----|-------|
| Core neural net (50K neurons) | ~50 MB | 1 KB/neuron hot path |
| Sparse synapses (embedded) | ~300 MB | 6.1 KB/neuron × 50K |
| Security + immune + ethics | ~15 MB | Safety rails |
| Visual cortex | ~15 MB | If camera |
| Somatosensory | ~5 MB | IMU input |
| Working memory | ~2 MB | 7-item buffer |
| Cerebellum + basal ganglia | ~15 MB | Motor control |
| Plasticity + STDP | ~5 MB | Learning |
| Swarm comms | ~5 MB | P2P messaging |
| **Total** | **~400 MB** | Fits in 512 MB drone |

Without GPU: CPU inference at 50K neurons ≈ 0.5-2 ms (500-1000 Hz possible).

---

## Part 2: Knowledge Distillation (Mothership → Drone)

The key challenge: compress a 1M neuron trained brain into a 50K neuron
drone brain without losing critical learned behavior.

### Distillation Pipeline

```
┌──────────────────────────────────────────────────────┐
│                   MOTHERSHIP (1M)                     │
│                                                      │
│  1. Run importance scoring on all neurons             │
│  2. Identify top-50K by activity × connectivity       │
│  3. Extract subnetwork (keep all connections within)  │
│  4. Compress layer topology to 5-layer diamond        │
│  5. Run knowledge distillation training:              │
│     - Teacher: full 1M network                        │
│     - Student: extracted 50K subnetwork               │
│     - Loss: KL divergence on output distributions     │
│  6. Export drone brain checkpoint                      │
└──────────────────────────────────────────────────────┘
```

### API

```c
typedef struct {
    uint32_t target_neurons;          // 50000
    float importance_threshold;       // Auto-select if 0
    uint32_t distillation_steps;      // 5000 training steps
    float temperature;                // 2.0 (softens teacher output)
    bool preserve_specialists;        // Keep domain-specific neurons

    // Subsystem selection
    bool include_snn;                 // Include spiking network
    bool include_lnn;                 // Include liquid network
    bool include_cnn;                 // Include CNN (visual)

    // Layer config for student
    uint32_t num_layers;              // 0 = auto (diamond)
    uint32_t* layer_sizes;            // NULL = auto
} nimcp_distill_config_t;

// Distill mothership brain into smaller drone brain
nimcp_status_t nimcp_brain_distill(
    nimcp_brain_t teacher,            // Trained 1M brain
    nimcp_brain_t* student,           // Output: new smaller brain
    const nimcp_distill_config_t* config);

// Batch distill: create N drone brains (can specialize each)
nimcp_status_t nimcp_brain_distill_batch(
    nimcp_brain_t teacher,
    nimcp_brain_t* students,          // Array of N output brains
    const nimcp_distill_config_t* configs,  // Per-drone configs
    uint32_t count);                  // N drones
```

### Specialization

Each drone can be distilled with different emphasis:

```c
// Scout drone: vision-heavy, minimal motor
distill_configs[0].preserve_specialists = true;
distill_configs[0].include_cnn = true;   // Visual cortex
distill_configs[0].include_snn = false;  // No spiking needed

// Pursuit drone: motor-heavy, fast reactions
distill_configs[1].include_snn = true;   // Fast spiking for reflexes
distill_configs[1].target_neurons = 30000; // Smaller, faster

// Coordinator drone: larger brain, planning focus
distill_configs[2].target_neurons = 100000;
distill_configs[2].include_lnn = true;   // Temporal reasoning
```

---

## Part 3: Swarm Communication Architecture

### Message Types

```c
typedef enum {
    // Perception sharing
    SWARM_MSG_PERCEPT,           // "I see X at location Y"
    SWARM_MSG_THREAT,            // "Threat detected at Z"
    SWARM_MSG_MAP_UPDATE,        // Spatial map fragment

    // Coordination
    SWARM_MSG_TASK_CLAIM,        // "I'll handle task T"
    SWARM_MSG_TASK_COMPLETE,     // "Task T done, result R"
    SWARM_MSG_TASK_FAIL,         // "Task T failed, need help"
    SWARM_MSG_HEARTBEAT,        // "I'm alive at position P"

    // Learning
    SWARM_MSG_GRADIENT,          // Weight update from local learning
    SWARM_MSG_EXPERIENCE,        // Training example to share
    SWARM_MSG_MODEL_SYNC,        // Periodic weight sync request

    // Mothership
    SWARM_MSG_REPORT,            // Drone → mothership status
    SWARM_MSG_DIRECTIVE,         // Mothership → drone command
    SWARM_MSG_WEIGHT_PUSH,       // Mothership → drone updated weights
    SWARM_MSG_EXPERIENCE_PULL,   // Mothership ← drone experiences
} nimcp_swarm_msg_type_t;
```

### Communication Topology

```
TIER 1: Peer Mesh (drone ↔ drone)
  - UDP multicast on local network
  - Lightweight: percepts, heartbeats, task claims
  - Latency: <10 ms
  - Bandwidth: ~1 KB/message, 10-50 msg/sec

TIER 2: Mothership Link (drone ↔ mothership)
  - TCP or MQTT over wireless backhaul
  - Heavier: gradient sync, model updates, experience replay
  - Latency: 50-500 ms (acceptable for async learning)
  - Bandwidth: ~10-100 KB/sync, every 30-300 sec

TIER 3: Federated Learning (mothership aggregates)
  - Mothership collects gradients from all drones
  - Aggregates (FedAvg or FedProx)
  - Pushes updated weights back to fleet
  - Cycle: every 5-60 minutes
```

### Swarm Learning Protocol

```
┌──────────┐                    ┌──────────────┐
│  Drone   │                    │  Mothership   │
│          │   EXPERIENCE_PULL  │              │
│  Learn   │◄──────────────────│  Aggregate   │
│  locally │                    │  gradients   │
│          │   GRADIENT_PUSH    │              │
│  Every   │──────────────────►│  FedAvg      │
│  N steps │                    │  across all  │
│          │   WEIGHT_PUSH      │  10 drones   │
│  Apply   │◄──────────────────│              │
│  updates │                    │  Push new    │
│          │                    │  weights     │
└──────────┘                    └──────────────┘

Drone local loop (every step):
  1. Observe environment → forward pass → action
  2. Receive reward/feedback → backward pass → local weight update
  3. Store experience in local replay buffer (circular, 1000 items)

Drone sync loop (every 100 steps):
  4. Compute gradient delta since last sync
  5. Send SWARM_MSG_GRADIENT to mothership
  6. Receive SWARM_MSG_WEIGHT_PUSH if available
  7. Blend: new_weights = 0.7 * local + 0.3 * mothership

Mothership aggregation loop (every 60 seconds):
  8. Collect gradients from all drones that reported
  9. FedAvg: avg_gradient = mean(drone_gradients)
  10. Apply to mothership brain (full 1M network)
  11. Distill updated weights for drone topology
  12. Push SWARM_MSG_WEIGHT_PUSH to all drones
```

### Federated Learning Details

**Why not just copy mothership weights to drones?**
The mothership has 1M neurons, drones have 50K. Direct copy is impossible.
Instead:

1. **Mothership trains** on aggregated drone experiences (full network)
2. **Distill output layer** weights directly (same I/O dimensions)
3. **Distill hidden representations** via intermediate matching:
   - Pick corresponding layers (drone layer 2 ↔ mothership layer 3)
   - Minimize MSE between teacher and student hidden activations
   - Only update drone's hidden weights, not output weights
4. **Push compressed update** (~500 KB per sync, not full checkpoint)

**Weight blending on drone side:**
```c
// Blend received weights with local weights
// Higher local weight preserves drone's unique experiences
for (uint32_t i = 0; i < num_params; i++) {
    drone_weight[i] = blend_ratio * drone_weight[i]
                    + (1.0 - blend_ratio) * mothership_weight[i];
}
// blend_ratio = 0.7 (favor local) or configurable
```

---

## Part 4: Drone-to-Drone Communication

### Percept Sharing

When a drone sees something relevant, it broadcasts to peers:

```c
typedef struct {
    uint32_t sender_id;
    float position[3];              // GPS or relative
    float confidence;               // 0-1
    uint32_t object_class;          // What was detected
    float feature_vector[64];       // Compressed representation
    uint64_t timestamp;
} nimcp_swarm_percept_t;
```

Receiving drones integrate this as a **virtual sensory input**:
- Feed the percept's feature vector through the visual cortex
- Tag with source drone ID and confidence
- Working memory holds recent peer percepts (decays in 5 sec)

### Collective Decision Making

For group decisions (e.g., "which area to search next"):

```
1. Each drone broadcasts its preferred action + confidence
2. Drones collect votes for 500 ms
3. Weighted vote: action = argmax(sum(confidence_i * vote_i))
4. All drones execute winning action
5. Dissenting drones can defect if confidence > 0.9
   (preserves exploration diversity)
```

### Emergent Specialization

Over time, drones naturally specialize through their unique experiences:
- Drone that explores dark areas → develops better low-light vision
- Drone that tracks fast objects → faster reaction circuits
- Drone near mothership → relay specialist

The federated learning loop preserves general capability (via mothership
weight blending) while allowing local specialization (via local gradients).

---

## Part 5: Mothership Role

The mothership brain serves multiple functions:

### 1. Training Server
- Aggregates experiences from all drones
- Trains on combined dataset (10x more data than any single drone)
- Runs overnight consolidation (memory replay, synapse pruning)
- Periodically re-distills updated drone brains

### 2. Strategic Planner
- Maintains global map from all drone percepts
- Assigns tasks to drones based on capability + position
- Sends DIRECTIVE messages for mission-level commands
- Uses full cognitive stack (reasoning, planning, prediction)

### 3. Knowledge Repository
- Stores all experiences in PR memory (long-term)
- Maintains knowledge graph of environment
- Can answer queries from drones ("What's in sector 7?")

### 4. Safety Monitor
- Monitors drone health via heartbeats
- Detects anomalous behavior (compromised drone?)
- Can force-update weights to override bad local learning
- Ethics engine validates drone decisions

### Communication Budget

| Message | Size | Frequency | Total/min |
|---------|------|-----------|-----------|
| Heartbeat (per drone) | 64 B | 1/sec | 38 KB |
| Percept (per drone) | 512 B | 5/sec | 1.5 MB |
| Gradient sync (per drone) | 50 KB | 1/100 steps | ~30 KB |
| Weight push (mothership→all) | 500 KB | 1/min | 500 KB |
| Directive | 128 B | As needed | ~1 KB |
| **Total (10 drones)** | | | **~20 MB/min** |

Fits comfortably in a 5 Mbps wireless link.

---

## Part 6: Implementation Phases

### Phase 1: Distillation
- `nimcp_brain_distill()` — importance scoring + subnetwork extraction
- Knowledge distillation training loop (teacher-student)
- Drone brain checkpoint format
- Test: distill 1M→50K, verify accuracy retention

### Phase 2: Drone Configuration
- `BRAIN_CONFIG_EMBEDDED` profile with subsystem disable flags
- `BRAIN_INIT_FAST` with drone-specific wave selection
- CPU-only inference path optimization
- Test: 50K brain on ARM/Jetson, measure latency

### Phase 3: Swarm Communication
- UDP multicast peer mesh
- TCP mothership link
- Message serialization (protobuf or msgpack)
- Heartbeat + failure detection
- Test: 3 drones + 1 mothership, percept sharing

### Phase 4: Federated Learning
- Gradient delta computation + compression
- FedAvg aggregation on mothership
- Weight blending on drone
- Distilled weight push (mothership→drone topology)
- Test: 10 drones learn collaboratively, compare to single-brain

### Phase 5: Collective Intelligence
- Collective decision protocol
- Emergent specialization tracking
- Strategic planning on mothership
- Safety monitoring + anomaly detection

---

## Part 7: Quantization & Compression

### INT8 Quantization

The single biggest optimization for edge. FP32 → INT8 cuts memory 4x and
doubles inference speed on most hardware (ARM NEON, x86 AVX-512, GPU tensor cores).

```c
typedef enum {
    NIMCP_QUANT_NONE,          // FP32 (default, training)
    NIMCP_QUANT_FP16,          // Half precision (2x compression)
    NIMCP_QUANT_INT8_SYMMETRIC,// INT8 symmetric per-tensor
    NIMCP_QUANT_INT8_AFFINE,   // INT8 asymmetric per-channel (best accuracy)
    NIMCP_QUANT_INT4,          // 4-bit (8x compression, ~5% accuracy loss)
    NIMCP_QUANT_TERNARY        // {-1, 0, +1} (already supported in synapse_handle_t)
} nimcp_quantization_t;

typedef struct {
    nimcp_quantization_t weight_precision;    // INT8_AFFINE recommended
    nimcp_quantization_t activation_precision;// INT8_SYMMETRIC recommended
    bool calibrate;                          // Run calibration pass (100 samples)
    uint32_t calibration_samples;            // Default: 100
    float accuracy_threshold;                // Abort if accuracy drops below this
} nimcp_quantize_config_t;
```

**Master performs quantization during distillation:**
1. Distill to target neuron count (FP32)
2. Calibrate: run 100 representative inputs, record activation ranges
3. Quantize weights: per-channel INT8 with scale/zero-point
4. Quantize activations: per-tensor INT8 with dynamic range
5. Validate: compare INT8 vs FP32 accuracy on test set
6. If accuracy loss > threshold: fall back to FP16 or mixed precision

**Memory savings:**

| Neurons | FP32 | FP16 | INT8 | INT4 |
|---------|------|------|------|------|
| 50K | ~325 MB | ~162 MB | ~81 MB | ~41 MB |
| 100K | ~650 MB | ~325 MB | ~162 MB | ~81 MB |
| 200K | ~1.3 GB | ~650 MB | ~325 MB | ~162 MB |

A 100K neuron brain in INT8 uses less memory than 25K in FP32.

**Ternary weights** (already in synapse_handle_t): for ultra-constrained
devices (ESP32, MCU), ternary {-1, 0, +1} with a learned scale factor
gives ~32x compression with ~10-15% accuracy loss.

---

## Part 8: Delta Weight Pushes

### Problem
Full re-distillation on every sync is expensive (minutes on master,
megabytes over the wire). Most syncs change <5% of weights.

### Solution: Compressed Delta Updates

```c
typedef struct {
    uint32_t version_from;           // Base version this delta applies to
    uint32_t version_to;             // New version after applying
    uint32_t num_changes;            // Number of changed weights
    uint32_t* layer_indices;         // Which layers changed
    uint32_t* neuron_indices;        // Which neurons within those layers
    float* weight_deltas;            // Delta values (new - old)
    uint32_t compressed_size;        // After compression
} nimcp_weight_delta_t;

// Master computes delta between current and last-pushed weights
nimcp_status_t nimcp_brain_compute_weight_delta(
    nimcp_brain_t master,
    uint32_t device_id,
    nimcp_weight_delta_t* delta);

// Device applies delta to local weights
nimcp_status_t nimcp_brain_apply_weight_delta(
    nimcp_brain_t device_brain,
    const nimcp_weight_delta_t* delta);
```

**Compression pipeline:**
1. Compute raw delta: `delta = new_weights - old_weights`
2. Sparsify: zero out deltas with |delta| < threshold (keep top-k%)
3. Quantize deltas to INT8
4. Run-length encode zero regions
5. LZ4 compress

**Typical savings:**
- Full checkpoint: 500 KB - 5 MB per device
- Delta push: 10-50 KB (50-100x smaller)
- Over 10 devices, 60-second cycle: 100-500 KB/min vs 5-50 MB/min

---

## Part 9: Rollback Safety

### Problem
A federated weight push might make a device worse — new weights conflict
with local specialization, or master training diverged.

### Solution: Automatic Rollback

```c
typedef struct {
    float* previous_weights;         // Saved before applying update
    uint32_t previous_version;
    float baseline_loss;             // Loss measured before update
    uint32_t validation_steps;       // Steps to evaluate (default: 50)
    float rollback_threshold;        // Rollback if loss > baseline × this
    bool rollback_triggered;
} nimcp_rollback_state_t;
```

**Protocol:**
```
1. Before applying weight push:
   - Save current weights to rollback buffer
   - Measure baseline loss over last 50 steps

2. Apply weight push (blended)

3. Monitor for validation_steps (50 steps):
   - Track running loss average

4. If running_loss > baseline_loss × rollback_threshold (default 2.0):
   - Restore previous weights from rollback buffer
   - Report ROLLBACK to master with loss metrics
   - Master flags this device for re-distillation

5. If loss is acceptable:
   - Free rollback buffer
   - Confirm update to master
```

**Rollback memory cost:** one copy of device weights (~50-500 KB for
INT8 quantized 50K neuron brain). Freed after validation window.

---

## Part 10: Early Exit (Adaptive Computation)

### Problem
Not every input needs the full network. Simple inputs (clear sky, no
obstacles) waste compute going through all 7 layers.

### Solution: Exit Heads at Intermediate Layers

```
Layer 1 → Layer 2 → [EXIT HEAD 1] → Layer 3 → Layer 4 → [EXIT HEAD 2] → ...
                     confidence > 0.9?              confidence > 0.8?
                     → output early                 → output early
                     → skip remaining               → skip remaining
```

**During distillation**, master trains exit heads at layers 2, 4:
- Each exit head: small linear projection (layer_size → output_size)
- Trained with same targets as final layer
- Learns to produce confident output when input is "easy"

```c
typedef struct {
    uint32_t num_exits;              // 2-3 exit points
    uint32_t* exit_layers;           // Which layers have exit heads
    float* confidence_thresholds;    // Per-exit confidence threshold
    float** exit_weights;            // Per-exit linear projection weights
    float** exit_biases;
    bool enabled;                    // Can be toggled at runtime
} nimcp_early_exit_config_t;
```

**Runtime behavior:**
- After each exit layer, compute exit head output
- If max(softmax(output)) > threshold: return early, skip remaining layers
- If no exit fires: run full network as normal

**Savings:** 40-60% average compute reduction. Easy inputs (70-80% of
typical workload) exit after layer 2. Hard inputs still get full depth.

**Power-aware:** At low battery, lower confidence thresholds to force
more early exits (faster, less accurate).

---

## Part 11: Offline Degradation Policy

### Problem
What happens when a device loses contact with master for hours or days?
Local learning continues, but drift accumulates without master correction.

### Solution: Confidence Decay + Conservative Mode

```c
typedef struct {
    uint64_t last_sync_timestamp;        // When last synced with master
    uint32_t steps_since_sync;           // Local steps without sync
    float confidence_decay_rate;         // Per-step decay (default: 0.0001)
    float min_confidence_multiplier;     // Floor (default: 0.5)
    float current_confidence;            // 1.0 after sync, decays over time

    // Conservative mode thresholds
    uint32_t cautious_after_steps;       // Enter cautious mode (default: 1000)
    uint32_t conservative_after_steps;   // Enter conservative mode (default: 5000)
    uint32_t frozen_after_steps;         // Stop learning, inference only (default: 20000)
} nimcp_offline_policy_t;
```

**Degradation ladder:**

| Steps Since Sync | Mode | Behavior |
|-----------------|------|----------|
| 0-1000 | NORMAL | Full learning, full confidence |
| 1000-5000 | CAUTIOUS | Learning rate × 0.5, flag uncertain decisions |
| 5000-20000 | CONSERVATIVE | Learning rate × 0.1, early exit thresholds raised, report uncertainty |
| 20000+ | FROZEN | No learning (weights frozen), inference only, max caution |

**On reconnect:** Immediately sync gradients to master, receive fresh
weights, reset confidence to 1.0, return to NORMAL mode.

**Confidence multiplier** applied to all output decisions:
```c
float decision_confidence = raw_confidence * offline_policy.current_confidence;
// At 10000 steps offline: decision_confidence ≈ raw × 0.63
// Device reports lower confidence → upstream systems can handle uncertainty
```

---

## Part 12: Device-to-Device Transfer (Gossip Learning)

### Problem
All learning flows through the master. If Drone A discovers a new obstacle
type, Drone B won't learn about it until the next master sync cycle
(potentially minutes away).

### Solution: Peer-to-Peer Weight Gossip

```c
typedef struct {
    uint32_t sender_id;
    uint32_t experience_hash;        // Deduplicate — don't re-learn same thing
    float urgency;                   // 0-1; higher = more important
    uint32_t num_weights;
    uint32_t* weight_indices;        // Which weights changed
    float* weight_deltas;            // By how much
    float sender_confidence;         // How confident sender is in this update
} nimcp_gossip_update_t;
```

**Protocol:**
```
1. Device A learns something significant:
   - Loss on new input was >5x average (novel situation)
   - Weight changes in relevant layer > threshold

2. Device A broadcasts GOSSIP_UPDATE to peer mesh:
   - Include only the changed weight indices + deltas
   - Tag with urgency score (based on loss magnitude)

3. Receiving devices (B, C, ...):
   - Check experience_hash — skip if already seen
   - If urgency > local threshold:
     Apply delta × gossip_blend_ratio (default 0.1)
   - If urgency < threshold: queue for later consideration

4. Gossip propagation:
   - Each device forwards high-urgency updates to its peers
   - TTL (time-to-live) prevents infinite propagation (default: 3 hops)
   - Master also receives gossip and incorporates into next aggregate
```

**Urgency scoring:**
```c
float urgency = min(1.0, loss_on_new_input / (5.0 * ema_loss));
// Loss 10x normal → urgency = 1.0 (broadcast immediately)
// Loss 2x normal → urgency = 0.4 (queue)
// Loss normal → urgency = 0.2 (don't broadcast)
```

**Bandwidth:** Gossip updates are tiny (~1-5 KB). Broadcast rate limited
to 1/sec to prevent flooding. High-urgency bypasses rate limit.

---

## Part 13: OTA Update Safety

### Problem
Pushing new weights over wireless to an active device (flying drone,
moving robot, in-use phone) must not cause mid-operation failures.

### Solution: Staged Atomic Updates

```
1. DOWNLOAD stage:
   - Receive delta/checkpoint to staging area (separate from active weights)
   - Verify checksum (SHA-256)
   - If download interrupted: discard, retry later

2. VALIDATE stage:
   - Load staged weights into shadow buffer (not active)
   - Run inference on 10 built-in test inputs
   - Compare output to expected (stored in checkpoint metadata)
   - If any test fails: reject update, report to master

3. SWAP stage (atomic):
   - Wait for idle window (between inference cycles)
   - Device must NOT be in critical operation (mid-flight maneuver,
     obstacle avoidance active, motor command in progress)
   - Atomic pointer swap: active_weights ↔ staged_weights
   - Takes <1 ms

4. VERIFY stage:
   - Run 50 steps with new weights
   - If rollback triggers (Part 9): swap back, report failure

5. CLEANUP:
   - Free old weights buffer
   - Report success to master
   - Update local version counter
```

**Critical operation detection:**
```c
bool nimcp_device_is_safe_to_update(nimcp_brain_t brain) {
    // Not mid-motor-command
    if (brain->motor_active) return false;
    // Not in obstacle avoidance
    if (brain->threat_level > 0.5) return false;
    // Not mid-inference
    if (brain->inference_in_progress) return false;
    // Battery sufficient
    if (brain->battery_pct < 15) return false;
    return true;
}
```

---

## Part 14: Catastrophic Forgetting Protection (EWC)

### Problem
When master pushes new weights, the device loses locally-learned
specializations. Blend ratio helps but doesn't distinguish between
important and unimportant local weights.

### Solution: Elastic Weight Consolidation

```c
typedef struct {
    float* fisher_diagonal;          // Per-weight importance score
    float* anchor_weights;           // Weights at last consolidation
    float ewc_lambda;                // Regularization strength (default: 1000)
    uint32_t num_params;
} nimcp_ewc_state_t;
```

**How it works:**
1. After each sync cycle, compute Fisher Information Matrix (diagonal):
   ```
   F_i = E[(dL/dw_i)^2]  over recent local experiences
   ```
   This measures how important each weight is for local performance.

2. When applying master weight push:
   ```
   For each weight w_i:
     penalty = ewc_lambda * F_i * (w_new - w_anchor)^2
     If penalty > threshold:
       // This weight is locally important — protect it
       w_final = 0.9 * w_local + 0.1 * w_master
     Else:
       // This weight isn't locally important — accept master's value
       w_final = 0.3 * w_local + 0.7 * w_master
   ```

3. The device keeps its hard-won local specializations while still
   absorbing general improvements from the master.

**Memory cost:** 2 float arrays of num_params each. For 50K neuron
brain with ~5M params: ~40 MB FP32, ~10 MB INT8. Can be computed
lazily (only for weights that actually get pushed).

---

## Part 15: Model Versioning & Compatibility

### Problem
If the master retrains and changes architecture (adds a layer, changes
layer sizes), existing devices can't apply weight deltas.

### Solution: Architecture Versioning

```c
typedef struct {
    uint32_t major;       // Architecture change (layers, sizes) — incompatible
    uint32_t minor;       // Weight update — compatible delta
    uint32_t patch;       // Config/subsystem change — compatible
    uint32_t arch_hash;   // Hash of layer_sizes array
} nimcp_model_version_t;

typedef struct {
    nimcp_model_version_t device_version;
    nimcp_model_version_t master_version;
    bool architecturally_compatible;
    bool delta_compatible;
    const char* migration_path;      // "re-distill" or "delta" or "none"
} nimcp_compatibility_check_t;
```

**Compatibility rules:**
- Same `major` + same `arch_hash` → delta push OK
- Different `major` or `arch_hash` → full re-distillation required
- Master tracks each device's current version
- Before pushing: `nimcp_check_compatibility(device_ver, master_ver)`
- If incompatible: queue device for re-distillation (background task)

**Migration protocol:**
```
1. Master retrains → new major version (e.g., 2.0.0)
2. Existing devices still on 1.x.x
3. Master maintains BOTH versions temporarily:
   - v1 weights for delta pushes to old devices
   - v2 weights for new distillations
4. Re-distill devices one at a time (avoid fleet-wide disruption)
5. Once all devices on v2: drop v1 support
```

---

## Part 16: Device Telemetry & Health Monitoring

### Problem
Master has no visibility into whether distilled brains are performing
well on actual device workloads.

### Solution: Lightweight Telemetry

```c
typedef struct {
    uint32_t device_id;
    uint64_t timestamp;

    // Performance metrics
    float avg_inference_ms;          // Running average
    float p99_inference_ms;          // 99th percentile
    float avg_loss;                  // Recent loss (last 100 steps)
    float loss_trend;                // Positive = degrading

    // Confidence metrics
    float avg_confidence;            // Mean output confidence
    float low_confidence_pct;        // % of decisions with confidence < 0.5
    float anomaly_rate;              // % of inputs flagged by immune system

    // Resource metrics
    float ram_usage_pct;
    float cpu_usage_pct;
    float battery_pct;               // 0 if plugged in
    float temperature_c;             // Device temperature

    // Learning metrics
    uint32_t steps_since_sync;
    float local_accuracy;            // On validation set if available
    uint32_t rollbacks_triggered;
    nimcp_offline_mode_t offline_mode;
} nimcp_device_telemetry_t;
```

**Master monitors and acts:**
```
For each device telemetry report:
  IF loss_trend > 0.1 for 3 consecutive reports:
    → Flag for re-distillation (brain is degrading)

  IF anomaly_rate > 20%:
    → Alert: device may be under attack or receiving bad input

  IF avg_confidence < 0.3:
    → Device is uncertain — may need more neurons or different subsystems

  IF rollbacks_triggered > 2 in last hour:
    → Stop pushing updates to this device, investigate

  IF temperature_c > device_max:
    → Send REDUCE_COMPUTE directive (lower inference Hz, enable early exit)

  IF battery_pct < 10%:
    → Send POWER_SAVE directive (freeze learning, minimum subsystems)
```

**Telemetry bandwidth:** ~200 bytes per report, every 30 seconds = ~400 B/min.
Negligible.

---

## Part 17: Privacy-Preserving Federated Learning

### Problem
Gradient inversion attacks can reconstruct training data from shared
gradients. If devices handle sensitive data (phone cameras, medical
sensors), raw gradients leak private information.

### Solution: Differential Privacy + Secure Aggregation

**Differential Privacy (per-device):**
```c
typedef struct {
    float noise_scale;               // Gaussian noise σ (default: 0.01)
    float gradient_clip_norm;        // Max gradient norm (default: 1.0)
    float privacy_budget_epsilon;    // Total privacy budget
    float privacy_spent;             // Consumed so far
} nimcp_dp_config_t;
```

Before sending gradients to master:
```
1. Clip: ||gradient|| ← min(||gradient||, clip_norm)
2. Add noise: gradient += N(0, σ² × clip_norm²)
3. Track privacy budget: ε_spent += ε_per_sync
4. If ε_spent > ε_budget: stop sending gradients (privacy exhausted)
```

**Secure Aggregation (master-side):**
```
1. Each device encrypts gradient with master's public key
2. Master can only decrypt the SUM of all gradients, not individual
3. Uses Shamir's Secret Sharing or homomorphic encryption
4. Master sees avg_gradient = (1/N) × Σ encrypted_gradients
5. Cannot attribute any gradient to a specific device
```

**Trade-off:** More noise = more privacy but slower learning.
Recommended: ε = 1.0 per day, σ = 0.01 (minimal accuracy impact
with 10+ devices contributing).

---

## Part 18: Power-Aware Compute Scaling

### Problem
A device at 80% battery should use its full brain. At 20% battery,
it should automatically shed load to extend operational time.

### Solution: Dynamic Power Modes

```c
typedef enum {
    NIMCP_POWER_FULL,           // All subsystems, full inference rate
    NIMCP_POWER_BALANCED,       // Non-essential subsystems off, normal rate
    NIMCP_POWER_SAVING,         // Minimal subsystems, reduced rate
    NIMCP_POWER_CRITICAL        // Inference only, no learning, minimum rate
} nimcp_power_mode_t;

typedef struct {
    nimcp_power_mode_t mode;
    float inference_hz;              // Target decision frequency
    float learning_rate_scale;       // Multiplier on learning rate
    uint32_t subsystem_mask;         // Which subsystems are active
    bool early_exit_enabled;         // Force early exit
    float early_exit_threshold;      // Lower = more aggressive
    bool gpu_enabled;                // Kill GPU to save power
} nimcp_power_config_t;
```

**Auto-scaling thresholds (battery-powered devices):**

| Battery | Mode | Hz | Learning | Subsystems | Early Exit |
|---------|------|-----|----------|------------|------------|
| 80-100% | FULL | 50 Hz | Full | All enabled | Optional |
| 50-80% | BALANCED | 50 Hz | Full | Non-essential off | Threshold 0.9 |
| 20-50% | SAVING | 20 Hz | LR × 0.1 | Core only | Threshold 0.7 |
| 5-20% | CRITICAL | 5 Hz | Frozen | Security + ethics only | Threshold 0.5 |
| <5% | SHUTDOWN | 1 Hz | None | Emergency stop behavior | Always exit early |

**For plugged-in devices (phones charging, robots on dock):**
Override to FULL regardless of battery level.

**For thermal throttling:**
```c
if (device_temp > thermal_limit * 0.9) {
    // Approaching thermal limit — reduce compute
    power_mode = max(power_mode, NIMCP_POWER_SAVING);
    disable_gpu();  // GPU generates most heat
}
```

**Runtime API:**
```c
// Auto-manage based on battery + temperature
nimcp_brain_set_power_auto(brain, true);

// Manual override
nimcp_brain_set_power_mode(brain, NIMCP_POWER_SAVING);

// Query current
nimcp_power_mode_t mode = nimcp_brain_get_power_mode(brain);
```

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Distillation accuracy loss | Device can't do what master can | Measure per-domain accuracy, increase device size if needed |
| Network partition (device offline) | Operates on stale weights | Offline degradation policy (Part 11), confidence decay |
| Adversarial device (hijacked) | Bad gradients poison master | Byzantine-fault-tolerant aggregation (median not mean) |
| Weight blending instability | Oscillating weights | Lower blend ratio + EWC protection (Part 14) |
| Bandwidth saturation | Delayed sync | Delta pushes (Part 8), gradient compression |
| Heterogeneous devices | Different capabilities | Per-device profiles + auto-optimization |
| Battery death during sync | Corrupted state | OTA safety protocol (Part 13), atomic swap |
| Catastrophic forgetting | Local specialization lost | EWC (Part 14), per-weight importance scoring |
| Model version mismatch | Can't apply delta | Version checking (Part 15), auto re-distillation |
| Privacy breach via gradients | User data leaked | Differential privacy + secure aggregation (Part 17) |
| Thermal runaway | Device damage | Power-aware scaling (Part 18), thermal throttling |
| Update makes device worse | Degraded performance | Automatic rollback (Part 9), 50-step validation |
| Stale gossip propagation | Conflicting peer updates | TTL on gossip messages, experience dedup hash |
