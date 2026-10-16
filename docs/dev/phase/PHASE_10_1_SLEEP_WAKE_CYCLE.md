# Phase 10.1: Sleep-Wake Cycle Implementation

## Overview

**WHAT**: Biologically-inspired sleep-wake cycle for memory consolidation and synaptic homeostasis
**WHY**: Prevent catastrophic forgetting, improve generalization, maintain synaptic health
**HOW**: Multi-stage sleep states with memory replay and synaptic scaling

## Architecture

### Sleep States

```
┌─────────────────────────────────────────────────────────────────┐
│                      SLEEP-WAKE CYCLE                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  AWAKE (Beta/Gamma 13-100Hz)                                   │
│  ├── Learning: Encode experiences into hippocampus             │
│  ├── Processing: Active sensory input, decision-making         │
│  └── Accumulate: Sleep pressure (adenosine) [0 → 1]           │
│                                                                 │
│  ↓ When sleep_pressure > threshold (default: 0.8)              │
│                                                                 │
│  DROWSY (Alpha 8-13Hz)                                         │
│  ├── Transition state: Reduced attention                       │
│  └── Duration: 1-5 minutes                                     │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  LIGHT SLEEP (Theta 4-8Hz, Sleep Spindles 12-16Hz)            │
│  ├── Sort: Identify important vs trivial memories              │
│  ├── Protect: Sleep spindles block external input              │
│  └── Duration: 10-20 minutes                                   │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  DEEP SLEEP (Delta 0.5-4Hz) ★ MOST IMPORTANT                  │
│  ├── Replay: Hippocampal memories → cortex (10-20x speed)     │
│  ├── Consolidate: Strengthen important synapses               │
│  ├── Downscale: Reduce all weights by 10-20% (homeostasis)    │
│  ├── Prune: Remove weak synapses (< threshold)                │
│  └── Duration: 20-40 minutes                                   │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  REM SLEEP (Theta + low muscle tone)                          │
│  ├── Recombine: Form novel connections between concepts        │
│  ├── Emotional: Integrate affective content (amygdala active)  │
│  ├── Creative: Random activation patterns (dreams)             │
│  └── Duration: 10-30 minutes                                   │
│                                                                 │
│  ↓ Cycle repeats 4-6 times                                     │
│  ↓ Early sleep: More deep (consolidation)                      │
│  ↓ Late sleep: More REM (creativity)                           │
│                                                                 │
│  WAKE UP                                                        │
│  └── Reset sleep pressure to 0                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Data Structures

### Sleep State Enum

```c
/**
 * @brief Sleep-wake states
 *
 * Maps to brain oscillation frequencies and functional modes
 */
typedef enum {
    SLEEP_STATE_AWAKE,        /**< Beta/Gamma: 13-100Hz - active processing */
    SLEEP_STATE_DROWSY,       /**< Alpha: 8-13Hz - relaxed, eyes closed */
    SLEEP_STATE_LIGHT_NREM,   /**< NREM 1-2: Theta 4-8Hz - sorting */
    SLEEP_STATE_DEEP_NREM,    /**< NREM 3: Delta 0.5-4Hz - consolidation */
    SLEEP_STATE_REM           /**< REM: Theta + atonia - creativity */
} sleep_state_t;
```

### Sleep System Configuration

```c
/**
 * @brief Sleep system configuration parameters
 */
typedef struct {
    // Sleep pressure dynamics
    float adenosine_accumulation_rate;  /**< Per learning step [0.0001] */
    float sleep_pressure_threshold;     /**< When to sleep [0.8] */
    float adenosine_clearance_rate;     /**< During sleep [0.05/min] */

    // Stage durations (milliseconds)
    uint32_t drowsy_duration_ms;        /**< Default: 120,000 (2 min) */
    uint32_t light_sleep_duration_ms;   /**< Default: 900,000 (15 min) */
    uint32_t deep_sleep_duration_ms;    /**< Default: 1,800,000 (30 min) */
    uint32_t rem_duration_ms;           /**< Default: 600,000 (10 min) */

    // Memory replay parameters
    uint32_t replay_batch_size;         /**< Memories per replay [100] */
    float replay_speed_multiplier;      /**< 10-20x faster than awake [15.0] */
    float replay_noise;                 /**< Variability in replay [0.1] */
    bool prioritize_emotional;          /**< Replay emotional memories first */
    bool prioritize_novel;              /**< Replay novel memories first */

    // Synaptic homeostasis
    float synaptic_downscaling_factor;  /**< Multiply all weights [0.85] */
    float synaptic_pruning_threshold;   /**< Remove if w < threshold [0.01] */
    bool enable_homeostasis;            /**< Enable weight downscaling */

    // REM parameters
    float rem_creativity_noise;         /**< Random activation [0.3] */
    bool enable_rem;                    /**< Enable REM stage */

    // Oscillation control
    bool sync_to_oscillations;          /**< Match brain oscillation frequencies */

} sleep_config_t;
```

### Sleep System State

```c
/**
 * @brief Runtime state of sleep-wake system
 */
typedef struct {
    sleep_config_t config;

    // Current state
    sleep_state_t current_state;
    uint64_t state_entered_at;          /**< Timestamp when entered state */
    uint64_t total_awake_time;          /**< Total time awake (ms) */
    uint64_t total_sleep_time;          /**< Total time asleep (ms) */

    // Sleep pressure (adenosine)
    float sleep_pressure;               /**< Current level [0,1] */
    uint32_t learning_steps_since_sleep; /**< Count since last sleep */

    // Memory replay tracking
    uint32_t* replayed_memory_indices;  /**< Which memories replayed */
    uint32_t num_replayed;              /**< Count replayed this cycle */
    uint32_t total_replays;             /**< Lifetime replay count */

    // Homeostasis statistics
    float total_weight_before_scaling;  /**< Total synaptic weight pre-sleep */
    float total_weight_after_scaling;   /**< Total synaptic weight post-sleep */
    uint32_t synapses_pruned;           /**< Count pruned last cycle */

    // Sleep cycle tracking
    uint32_t sleep_cycles_completed;    /**< Total sleep cycles */
    uint32_t deep_sleep_count;          /**< Times in deep sleep */
    uint32_t rem_count;                 /**< Times in REM */

    // Performance metrics
    float consolidation_efficiency;     /**< How much memory retained [0,1] */
    float energy_saved;                 /**< From synaptic scaling */

} sleep_system_t;
```

### Memory Record for Replay

```c
/**
 * @brief Memory trace for consolidation
 */
typedef struct {
    float* input;                    /**< Input pattern */
    uint32_t input_size;
    float* target;                   /**< Target output */
    uint32_t target_size;

    float emotional_strength;        /**< Emotional salience [0,1] */
    float novelty_score;            /**< How novel [0,1] */
    float confidence;               /**< Initial confidence [0,1] */

    uint64_t timestamp;             /**< When encoded */
    uint32_t replay_count;          /**< Times replayed */

    bool consolidated;              /**< Transferred to cortex */

} memory_trace_t;
```

## Core API

### Lifecycle Functions

```c
/**
 * @brief Create sleep-wake system
 *
 * WHAT: Initialize sleep tracking and configuration
 * WHY:  Enable memory consolidation and synaptic homeostasis
 * HOW:  Allocate structures, set defaults, attach to brain
 *
 * @param config Sleep configuration (NULL for defaults)
 * @return Sleep system handle or NULL on failure
 */
sleep_system_t* sleep_system_create(const sleep_config_t* config);

/**
 * @brief Destroy sleep system
 *
 * @param sleep Sleep system to destroy
 */
void sleep_system_destroy(sleep_system_t* sleep);

/**
 * @brief Get default sleep configuration
 *
 * DEFAULTS:
 * - Sleep threshold: 0.8 (80% pressure)
 * - Deep sleep: 30 minutes
 * - Replay speed: 15x
 * - Downscaling: 85% of original weights
 * - Pruning: Remove weights < 0.01
 *
 * @return Default configuration
 */
sleep_config_t sleep_default_config(void);
```

### Sleep Pressure Management

```c
/**
 * @brief Update sleep pressure after learning
 *
 * WHAT: Accumulate adenosine-like sleep pressure
 * WHY:  Model metabolic cost of learning
 * HOW:  Increment pressure by learning_rate * accumulation_rate
 *
 * @param sleep Sleep system
 * @param learning_steps Number of learning steps performed
 */
void sleep_accumulate_pressure(
    sleep_system_t* sleep,
    uint32_t learning_steps
);

/**
 * @brief Get current sleep pressure
 *
 * @param sleep Sleep system
 * @return Sleep pressure [0,1], where 1.0 = desperate for sleep
 */
float sleep_get_pressure(const sleep_system_t* sleep);

/**
 * @brief Check if sleep is needed
 *
 * @param sleep Sleep system
 * @return true if pressure exceeds threshold
 */
bool sleep_is_needed(const sleep_system_t* sleep);
```

### Sleep Cycle Control

```c
/**
 * @brief Enter sleep state
 *
 * WHAT: Transition to specified sleep state
 * WHY:  Initiate consolidation, homeostasis, or creativity
 * HOW:  Set state, record timestamp, configure oscillations
 *
 * @param sleep Sleep system
 * @param brain Brain to put to sleep
 * @param target_state Desired sleep state
 * @return true on success
 */
bool sleep_enter_state(
    sleep_system_t* sleep,
    brain_t brain,
    sleep_state_t target_state
);

/**
 * @brief Run automatic sleep cycle
 *
 * WHAT: Execute full sleep cycle (drowsy → light → deep → REM → awake)
 * WHY:  Automate entire consolidation process
 * HOW:  Progress through states with configured durations
 *
 * PIPELINE:
 * 1. Drowsy (2 min): Reduce oscillation frequency
 * 2. Light NREM (15 min): Sort memories by importance
 * 3. Deep NREM (30 min): Replay memories, downscale weights
 * 4. REM (10 min): Creative recombination
 * 5. Wake: Reset pressure, return to awake state
 *
 * @param sleep Sleep system
 * @param brain Brain to sleep
 * @param num_cycles Number of cycles to perform (default: 1)
 * @return true on success
 */
bool sleep_run_cycle(
    sleep_system_t* sleep,
    brain_t brain,
    uint32_t num_cycles
);

/**
 * @brief Wake brain from sleep
 *
 * @param sleep Sleep system
 * @param brain Brain to wake
 * @return true on success
 */
bool sleep_wake_up(sleep_system_t* sleep, brain_t brain);
```

### Memory Consolidation (Deep Sleep)

```c
/**
 * @brief Replay hippocampal memories into cortex
 *
 * WHAT: Reactivate memory traces and strengthen cortical synapses
 * WHY:  Transfer short-term (hippocampus) → long-term (cortex)
 * HOW:  Select memories, replay at high speed, apply learning
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal sharp-wave ripples (100-250Hz) during deep sleep
 * - Replay at 10-20x real-time speed
 * - Coordinated with cortical slow oscillations
 *
 * SELECTION PRIORITY:
 * 1. High emotional salience (amygdala-tagged)
 * 2. High novelty (prediction errors)
 * 3. Recent experiences (temporal recency)
 * 4. Low replay count (under-consolidated)
 *
 * @param sleep Sleep system
 * @param brain Brain with memories
 * @param memories Array of memory traces
 * @param num_memories Total memories available
 * @param num_to_replay How many to replay this session
 * @return Number successfully replayed
 */
uint32_t sleep_replay_memories(
    sleep_system_t* sleep,
    brain_t brain,
    memory_trace_t* memories,
    uint32_t num_memories,
    uint32_t num_to_replay
);

/**
 * @brief Sort memories by consolidation priority
 *
 * PRIORITY FORMULA:
 * priority = emotional_strength * 0.4 +
 *           novelty_score * 0.3 +
 *           recency * 0.2 +
 *           (1 - replay_count/max_replays) * 0.1
 *
 * @param memories Array of memories
 * @param num_memories Size of array
 * @param current_time Current timestamp (for recency)
 */
void sleep_prioritize_memories(
    memory_trace_t* memories,
    uint32_t num_memories,
    uint64_t current_time
);
```

### Synaptic Homeostasis (Deep Sleep)

```c
/**
 * @brief Apply synaptic downscaling to all weights
 *
 * WHAT: Multiply all synaptic weights by scaling factor (e.g., 0.85)
 * WHY:  Prevent saturation, save energy, improve signal-to-noise
 * HOW:  Iterate through all synapses, scale weights proportionally
 *
 * BIOLOGICAL BASIS (Tononi & Cirelli, 2014):
 * - Synapses potentiate during awake (learning)
 * - Net weight increase → saturation, energy cost
 * - Sleep globally downscales → renormalization
 * - Strong synapses survive, weak ones pruned
 *
 * ALGORITHM:
 * ```
 * for each synapse:
 *     w_new = w_old * downscaling_factor
 *     if w_new < pruning_threshold:
 *         remove_synapse()
 * ```
 *
 * BENEFITS:
 * - Maintains dynamic range (prevents saturation)
 * - Improves generalization (weak features removed)
 * - Reduces energy consumption (~20%)
 *
 * @param sleep Sleep system
 * @param brain Brain to downscale
 * @return Statistics about scaling (weights before/after, synapses pruned)
 */
homeostasis_stats_t sleep_synaptic_homeostasis(
    sleep_system_t* sleep,
    brain_t brain
);

/**
 * @brief Prune weak synapses below threshold
 *
 * @param sleep Sleep system
 * @param brain Brain to prune
 * @return Number of synapses removed
 */
uint32_t sleep_prune_weak_synapses(
    sleep_system_t* sleep,
    brain_t brain
);
```

### Creative Recombination (REM Sleep)

```c
/**
 * @brief Generate novel connections during REM
 *
 * WHAT: Activate random neuron combinations to form new associations
 * WHY:  Creativity, insight, problem-solving (pattern recombination)
 * HOW:  Random activation + low threshold → spontaneous connections
 *
 * BIOLOGICAL BASIS:
 * - REM: High acetylcholine (attention) + low norepinephrine (logic)
 * - Allows "impossible" combinations → creativity
 * - Explains dreams: Random activation of memories
 *
 * ALGORITHM:
 * ```
 * for num_activations:
 *     pattern_A = select_random_memory()
 *     pattern_B = select_random_memory()
 *     combined = pattern_A * 0.5 + pattern_B * 0.5 + noise
 *     brain_process(combined)  // Creates new synapses
 * ```
 *
 * @param sleep Sleep system
 * @param brain Brain to activate
 * @param num_activations Number of random combinations
 * @return Number of novel connections formed
 */
uint32_t sleep_rem_recombination(
    sleep_system_t* sleep,
    brain_t brain,
    uint32_t num_activations
);
```

### Statistics and Monitoring

```c
/**
 * @brief Get sleep statistics
 *
 * @param sleep Sleep system
 * @param stats Output structure
 */
void sleep_get_statistics(
    const sleep_system_t* sleep,
    sleep_stats_t* stats
);

typedef struct {
    uint64_t total_awake_time_ms;
    uint64_t total_sleep_time_ms;
    uint32_t sleep_cycles_completed;
    uint32_t total_memories_replayed;
    uint32_t total_synapses_pruned;
    float avg_consolidation_efficiency;
    float energy_savings_percent;
    float current_sleep_pressure;
} sleep_stats_t;
```

## Integration with Existing NIMCP Systems

### 1. Brain Oscillations

```c
// During sleep state transitions, update oscillation frequencies
void sleep_sync_oscillations(sleep_system_t* sleep, brain_t brain) {
    switch (sleep->current_state) {
        case SLEEP_STATE_AWAKE:
            brain_oscillations_set_frequency(brain, FREQ_GAMMA, 40.0f);
            break;
        case SLEEP_STATE_DROWSY:
            brain_oscillations_set_frequency(brain, FREQ_ALPHA, 10.0f);
            break;
        case SLEEP_STATE_LIGHT_NREM:
            brain_oscillations_set_frequency(brain, FREQ_THETA, 6.0f);
            break;
        case SLEEP_STATE_DEEP_NREM:
            brain_oscillations_set_frequency(brain, FREQ_DELTA, 2.0f);
            break;
        case SLEEP_STATE_REM:
            brain_oscillations_set_frequency(brain, FREQ_THETA, 5.0f);
            break;
    }
}
```

### 2. Memory Consolidation System

```c
// Consolidation module provides memory traces
consolidation_memory_t* consolidated = consolidation_get_recent(brain);

// Convert to memory_trace_t for replay
memory_trace_t trace = {
    .input = consolidated->pattern,
    .emotional_strength = consolidated->salience,
    .novelty_score = consolidated->novelty,
    ...
};

sleep_replay_memories(sleep, brain, &trace, 1, 1);
```

### 3. Neuromodulators

```c
// During deep sleep: Low acetylcholine (no encoding, only consolidation)
neuromodulator_set_level(brain, NEUROMOD_ACETYLCHOLINE, 0.2f);

// During REM: High acetylcholine, low norepinephrine (creativity without logic)
neuromodulator_set_level(brain, NEUROMOD_ACETYLCHOLINE, 0.8f);
neuromodulator_set_level(brain, NEUROMOD_NOREPINEPHRINE, 0.1f);
```

### 4. Wellbeing System

```c
// Sleep deprivation affects wellbeing
if (sleep_pressure > 0.9f) {
    wellbeing_set_distress(brain, DISTRESS_FATIGUE, 0.8f);
}

// After good sleep, restore wellbeing
if (sleep_cycles_completed > 0) {
    wellbeing_restore(brain, 0.9f);
}
```

### 5. Introspection

```c
// High sleep pressure → high uncertainty
if (sleep_pressure > 0.7f) {
    output->introspection_uncertainty += sleep_pressure * 0.3f;
}
```

## Example Usage

### Basic Sleep Cycle

```c
// Create brain with sleep enabled
brain_config_t config = brain_default_config("learner",
    BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 784, 10);
config.enable_sleep_wake_cycle = true;

brain_t brain = brain_create_custom(&config);

// Train during "day"
for (int i = 0; i < 1000; i++) {
    brain_learn_example(brain, training_data[i].input, 784,
                       training_data[i].label, 1.0f);
}

// Check if sleep needed
if (sleep_is_needed(brain->sleep_system)) {
    printf("Brain tired, initiating sleep...\n");

    // Automatic sleep cycle
    sleep_run_cycle(brain->sleep_system, brain, 1);

    printf("Awake and refreshed!\n");
}

// Continue training with consolidated knowledge
```

### Manual Sleep Stages

```c
sleep_system_t* sleep = brain->sleep_system;

// Enter deep sleep manually
sleep_enter_state(sleep, brain, SLEEP_STATE_DEEP_NREM);

// Replay specific memories
memory_trace_t important_memories[10];
// ... populate memories ...

uint32_t replayed = sleep_replay_memories(
    sleep, brain, important_memories, 10, 10
);
printf("Replayed %u memories\n", replayed);

// Apply homeostasis
homeostasis_stats_t stats = sleep_synaptic_homeostasis(sleep, brain);
printf("Pruned %u weak synapses\n", stats.synapses_pruned);

// Wake up
sleep_wake_up(sleep, brain);
```

### Monitoring Sleep Quality

```c
sleep_stats_t stats;
sleep_get_statistics(brain->sleep_system, &stats);

printf("Sleep Report:\n");
printf("  Awake time: %.1f hours\n",
       stats.total_awake_time_ms / 3600000.0f);
printf("  Sleep time: %.1f hours\n",
       stats.total_sleep_time_ms / 3600000.0f);
printf("  Cycles: %u\n", stats.sleep_cycles_completed);
printf("  Memories consolidated: %u\n", stats.total_memories_replayed);
printf("  Synapses pruned: %u\n", stats.total_synapses_pruned);
printf("  Energy saved: %.1f%%\n", stats.energy_savings_percent);
printf("  Current pressure: %.1f%%\n",
       stats.current_sleep_pressure * 100.0f);
```

## Benefits

### 1. Prevents Catastrophic Forgetting

**Problem**: Sequential learning overwrites old knowledge
**Solution**: Replay old memories during sleep → maintain performance on old tasks

### 2. Improves Generalization

**Problem**: Overfitting to training data
**Solution**: Synaptic downscaling removes noise, keeps signal

### 3. Enables Continual Learning

**Problem**: Can't learn indefinitely without forgetting
**Solution**: Sleep consolidates important knowledge, prunes trivial

### 4. Reduces Energy

**Problem**: Large networks consume energy
**Solution**: Prune 10-20% of weak synapses → same performance, less compute

### 5. Enhances Creativity

**Problem**: Stuck in local optima
**Solution**: REM random recombination explores solution space

## Implementation Timeline

**Week 1-2**: Core data structures, lifecycle functions
**Week 3**: Sleep pressure accumulation, state transitions
**Week 4**: Memory replay implementation
**Week 5**: Synaptic homeostasis (downscaling, pruning)
**Week 6**: REM creative recombination
**Week 7**: Integration with oscillations, consolidation
**Week 8**: Testing, optimization, documentation

## References

- **Tononi & Cirelli (2014)**: Synaptic homeostasis hypothesis
- **Wilson & McNaughton (1994)**: Hippocampal replay in rats
- **Stickgold & Walker (2013)**: Sleep-dependent memory consolidation
- **Rasch & Born (2013)**: Memory consolidation during sleep
- **Cai et al. (2009)**: REM sleep enhances creative problem-solving
