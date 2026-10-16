# Phase M2: Systems Consolidation Specification

**Author:** NIMCP Development Team
**Date:** 2025-11-13
**Status:** Planning
**Dependencies:** Phase M1 Memory Engrams (COMPLETE)

---

## Executive Summary

Phase M2 implements **systems consolidation** - the gradual transfer of memory traces from the hippocampus to the neocortex over time. This enables true long-term memory storage, semantic knowledge formation, and memory independence from the hippocampus.

**Key Features:**
- Hippocampus → Cortex memory transfer during sleep
- Sleep replay mechanism (pattern reactivation)
- Cortical semantic memory storage
- Gradual memory transformation (episodic → semantic)
- Integration with Phase M1 engrams and sleep-wake cycle

---

## 1. Background: Systems Consolidation Theory

### Neuroscience Foundation

**Two-Stage Memory Model** (McClelland et al., 1995; Squire & Alvarez, 1995):

```
STAGE 1: RAPID ENCODING (Hours - Days)
├── Hippocampus rapidly encodes episodic memories
├── Pattern separation and completion (Phase M1 ✅)
└── Memory traces remain hippocampus-dependent

STAGE 2: SLOW CONSOLIDATION (Weeks - Years)
├── Gradual transfer to neocortex during sleep
├── Memory traces become cortex-dependent
├── Episodic details fade, semantic core remains
└── Hippocampus becomes less critical for recall
```

### Biological Evidence

1. **Temporal Gradient in Amnesia**
   - Hippocampal damage → recent memory loss (unconsolidated)
   - Old memories preserved (consolidated to cortex)
   - Supports gradual transfer hypothesis

2. **Sleep Replay** (Wilson & McNaughton, 1994)
   - Hippocampal cells replay recent experiences during sleep
   - 10-20x faster than real-time (time compression)
   - Coordinated with cortical slow oscillations

3. **Cortical Reorganization** (Takashima et al., 2006)
   - fMRI shows hippocampal activation decreases over time
   - Cortical activation increases for same memories
   - Complete transfer takes weeks to months

4. **Slow-Wave Sleep Critical** (Born & Wilhelm, 2012)
   - SWS (slow-wave sleep) essential for consolidation
   - Sharp-wave ripples (SWRs) coordinate replay
   - Disrupting SWS impairs consolidation

---

## 2. Architecture Design

### 2.1 Core Components

```c
┌─────────────────────────────────────────────────────────────┐
│                     SYSTEMS CONSOLIDATION                     │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐         ┌──────────────┐                  │
│  │ HIPPOCAMPUS  │ replay  │   CORTEX     │                  │
│  │  (Phase M1)  │────────>│  (Phase M2)  │                  │
│  │   Engrams    │ during  │   Semantic   │                  │
│  │              │  sleep  │   Memory     │                  │
│  └──────────────┘         └──────────────┘                  │
│         │                         ▲                          │
│         │                         │                          │
│         │    ┌──────────────┐    │                          │
│         └───>│ SLEEP REPLAY │────┘                          │
│              │  Mechanism   │                                │
│              └──────────────┘                                │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Data Structures

```c
/**
 * Cortical memory node (semantic storage)
 *
 * WHAT: Long-term semantic memory representation
 * WHY:  Stores consolidated memories independent of hippocampus
 * HOW:  Graph-based network of concepts with strengths
 */
typedef struct cortical_memory_node {
    uint64_t id;                          // Unique identifier

    // Content representation
    float* features;                      // Semantic features (abstract)
    uint32_t feature_dim;                 // Feature dimensionality

    // Consolidation tracking
    float consolidation_strength;         // 0.0 = new, 1.0 = fully consolidated
    float hippocampal_dependency;         // 1.0 = hippocampus-dependent, 0.0 = independent
    uint64_t creation_time_ms;            // When first transferred
    uint64_t last_reactivation_ms;        // Last replay/retrieval
    uint32_t reactivation_count;          // Number of replays

    // Semantic relationships
    struct cortical_memory_node** neighbors;  // Connected concepts
    float* neighbor_strengths;            // Connection strengths
    uint32_t neighbor_count;              // Number of neighbors
    uint32_t neighbor_capacity;           // Allocated capacity

    // Source tracking
    uint64_t source_engram_id;            // Original hippocampal engram (M1)
    bool is_semantic;                     // true = semantic, false = episodic remnant
} cortical_memory_node_t;

/**
 * Replay event during sleep
 *
 * WHAT: Reactivation of hippocampal pattern for consolidation
 * WHY:  Transfer mechanism from hippocampus to cortex
 * HOW:  Coordinated replay strengthens cortical traces
 */
typedef struct replay_event {
    uint64_t engram_id;                   // Which engram to replay
    float replay_strength;                // Intensity [0-1]
    uint64_t timestamp_ms;                // When replayed
    bool completed;                       // Transfer success
    float cortical_activation;            // Cortical response strength
} replay_event_t;

/**
 * Systems consolidation system
 *
 * WHAT: Manages hippocampus → cortex transfer
 * WHY:  Enable long-term memory and semantic knowledge
 * HOW:  Sleep replay + gradual strengthening + graph storage
 */
typedef struct systems_consolidation_system {
    // Cortical storage
    cortical_memory_node_t** cortical_nodes;  // Array of cortical memories
    uint32_t node_count;                      // Current count
    uint32_t node_capacity;                   // Max capacity (e.g., 10,000)

    // Replay tracking
    replay_event_t* replay_queue;         // Scheduled replays
    uint32_t replay_queue_size;           // Queue length
    uint32_t replay_queue_capacity;       // Max queue size

    // Consolidation parameters
    float transfer_rate;                  // Speed of consolidation (per sleep cycle)
    float replay_frequency;               // Replays per sleep hour
    float semantic_threshold;             // When memory becomes semantic (0.8)

    // Statistics
    uint64_t total_transfers;             // Total memories transferred
    uint64_t total_replays;               // Total replay events
    float avg_consolidation_time_days;    // Average time to full consolidation

    // Integration
    engram_system_t* engram_system;       // Phase M1 hippocampal storage
    sleep_system_t* sleep_system;         // Sleep-wake cycle integration
} systems_consolidation_t;
```

---

## 3. Core Functionality

### 3.1 Sleep Replay

**Function:** Reactivate hippocampal engrams during sleep for transfer

**Algorithm:**
```c
/**
 * @brief Trigger sleep replay of recent engrams
 *
 * WHAT: Reactivate hippocampal memories during sleep
 * WHY:  Enable transfer to cortex through coordinated replay
 * HOW:  Select recent engrams, replay at 10-20x speed, strengthen cortex
 *
 * BIOLOGICAL BASIS:
 * - Sharp-wave ripples (SWRs) during slow-wave sleep
 * - 10-20x time compression (Wilson & McNaughton, 1994)
 * - Coordinated hippocampal-cortical replay
 * - Preferential replay of salient/recent memories
 *
 * @param system Consolidation system
 * @param sleep_state Current sleep state
 * @param time_delta_seconds Time since last update
 */
void systems_consolidation_replay(
    systems_consolidation_t* system,
    sleep_state_t sleep_state,
    float time_delta_seconds);
```

**Replay Selection Criteria:**
1. **Recency:** Prefer recent engrams (last 24 hours)
2. **Salience:** High emotional arousal → higher priority
3. **Retrieval frequency:** Recently recalled → replay more
4. **Consolidation state:** Prioritize LABILE engrams

**Replay Process:**
```
1. Select engram from hippocampus (Phase M1)
2. Reactivate neural pattern (10-20x compressed)
3. Check for existing cortical node
   ├─ If exists: Strengthen existing node
   └─ If new: Create cortical node
4. Update consolidation strength
5. Adjust hippocampal dependency (gradual decrease)
6. Record replay event for statistics
```

### 3.2 Cortical Transfer

**Function:** Create and strengthen cortical memory nodes

**Algorithm:**
```c
/**
 * @brief Transfer engram to cortical storage
 *
 * WHAT: Create or strengthen cortical memory representation
 * WHY:  Enable long-term, hippocampus-independent storage
 * HOW:  Abstract features, create node, link to semantic network
 *
 * BIOLOGICAL BASIS:
 * - Neocortical slow learning (McClelland et al., 1995)
 * - Gradual strengthening over weeks
 * - Loss of episodic detail, retention of semantic core
 * - Formation of semantic categories
 *
 * @param system Consolidation system
 * @param engram_id Source hippocampal engram
 * @param replay_strength Strength of current replay
 * @return Cortical node ID (0 on failure)
 */
uint64_t systems_consolidation_transfer(
    systems_consolidation_t* system,
    uint64_t engram_id,
    float replay_strength);
```

**Transfer Process:**
```
1. Retrieve engram from hippocampus
2. Abstract features (semantic extraction)
   ├─ Remove episodic details (time, place, context)
   └─ Keep semantic core (meaning, relationships)
3. Find similar cortical nodes
   ├─ If similarity > 0.8: Merge with existing
   └─ If similarity < 0.8: Create new node
4. Update consolidation strength
   └─ Δstrength = replay_strength * transfer_rate * time_delta
5. Update hippocampal dependency
   └─ dependency = 1.0 - consolidation_strength
6. Link to semantic network
   └─ Create edges to related concepts
```

### 3.3 Semantic Abstraction

**Function:** Extract semantic core from episodic memories

**Algorithm:**
```c
/**
 * @brief Extract semantic features from episodic engram
 *
 * WHAT: Transform episodic details into semantic abstractions
 * WHY:  Cortex stores meaning, not specific episodes
 * HOW:  Feature averaging, detail removal, category formation
 *
 * BIOLOGICAL BASIS:
 * - Episodic memories lose detail over time
 * - Semantic knowledge emerges from repeated episodes
 * - Neocortex favors categorical representations
 * - Multiple episodes → single semantic concept
 *
 * @param engram Source hippocampal engram
 * @param features_out Output semantic features
 * @param feature_dim Feature dimensionality
 * @return True on success
 */
bool extract_semantic_features(
    const engram_t* engram,
    float* features_out,
    uint32_t feature_dim);
```

**Abstraction Rules:**
1. **Temporal context removed:** Time of encoding discarded
2. **Spatial context removed:** Location information discarded
3. **Emotional details removed:** Only arousal level retained
4. **Perceptual details blurred:** Specific sensory data averaged
5. **Semantic core retained:** Meaning, category, relationships preserved

**Example:**
```
EPISODIC (Hippocampus):
- "I ate a red apple at 2pm in the kitchen"
- Neuron IDs: [1, 2, 3, 4, 5]
- Activations: [0.8, 0.7, 0.9, 0.6, 0.85]
- Emotion: Joy (arousal 0.5)
- Time: 2025-11-13 14:00:00

SEMANTIC (Cortex):
- "Apples are fruit, usually red, sweet, edible"
- Features: [food_category: 0.9, sweet: 0.7, fruit: 0.85, ...]
- Consolidated: 0.8 (mostly independent)
- Neighbors: [fruit, food, tree, orchard]
```

### 3.4 Consolidation Update

**Function:** Progress consolidation over time

**Algorithm:**
```c
/**
 * @brief Update consolidation state for all cortical nodes
 *
 * WHAT: Gradually strengthen cortical traces, weaken hippocampal dependency
 * WHY:  Implement time-dependent consolidation process
 * HOW:  Exponential approach to full consolidation
 *
 * BIOLOGICAL BASIS:
 * - Consolidation takes weeks to months (Takashima et al., 2006)
 * - Exponential time course with τ = 2-4 weeks
 * - Accelerated during sleep (2-5x faster)
 * - Asymptotic approach to independence
 *
 * @param system Consolidation system
 * @param time_delta_seconds Time since last update
 * @param is_sleeping Currently in sleep state
 */
void systems_consolidation_update(
    systems_consolidation_t* system,
    float time_delta_seconds,
    bool is_sleeping);
```

**Update Rules:**
```
For each cortical node:
  1. Compute time-dependent strengthening
     └─ Δstrength = (1 - strength) * (time_delta / τ) * sleep_multiplier

  2. Update consolidation strength
     └─ strength += Δstrength
     └─ Clamp to [0.0, 1.0]

  3. Update hippocampal dependency
     └─ dependency = 1.0 - strength

  4. Check for semantic transition
     └─ If strength > semantic_threshold (0.8):
        └─ Mark as semantic memory
        └─ Prune episodic details

  5. Update reactivation decay
     └─ If no recent replay: slightly decrease strength
     └─ Models forgetting of unused memories

Constants:
  τ_awake = 30 days (slow consolidation when awake)
  τ_sleep = 10 days (3x faster during sleep)
  semantic_threshold = 0.8
  forgetting_rate = 0.001 per day (slow decay)
```

---

## 4. Integration Points

### 4.1 Phase M1 Engram System

**Integration:** Read engrams for replay and transfer

```c
// In brain_decide() after engram recall
if (brain->systems_consolidation && brain->sleep_system) {
    sleep_state_t sleep_state = sleep_get_current_state(brain->sleep_system);

    // During sleep: trigger replay
    if (sleep_state == SLEEP_STATE_DEEP_NREM) {
        systems_consolidation_replay(
            brain->systems_consolidation,
            sleep_state,
            0.1f  // Time delta
        );
    }

    // Always update consolidation
    bool is_sleeping = (sleep_state != SLEEP_STATE_AWAKE);
    systems_consolidation_update(
        brain->systems_consolidation,
        0.1f,
        is_sleeping
    );
}
```

### 4.2 Sleep-Wake Cycle

**Integration:** Trigger replay during SWS, accelerate consolidation

**Sleep State Effects:**
- **AWAKE:** Slow consolidation (τ = 30 days), no replay
- **DROWSY:** Normal consolidation, no replay
- **LIGHT_NREM:** Moderate consolidation, occasional replay
- **DEEP_NREM:** Fast consolidation (τ = 10 days), active replay (1-2 per minute)
- **REM:** No consolidation (cortex busy), minimal replay

### 4.3 Brain Structure

**Add to brain_struct:**
```c
struct brain_struct {
    // ... existing fields ...

    // Phase M1: Memory Engrams
    engram_system_t* engram_system;

    // Phase M2: Systems Consolidation
    systems_consolidation_t* systems_consolidation;
};
```

---

## 5. API Reference

### 5.1 System Management

```c
// Create systems consolidation system
systems_consolidation_t* systems_consolidation_create(
    engram_system_t* engram_system,
    sleep_system_t* sleep_system,
    uint32_t cortical_capacity);

// Destroy system
void systems_consolidation_destroy(
    systems_consolidation_t* system);

// Reset system (clear all cortical memories)
void systems_consolidation_reset(
    systems_consolidation_t* system);
```

### 5.2 Core Operations

```c
// Trigger sleep replay
void systems_consolidation_replay(
    systems_consolidation_t* system,
    sleep_state_t sleep_state,
    float time_delta_seconds);

// Transfer engram to cortex
uint64_t systems_consolidation_transfer(
    systems_consolidation_t* system,
    uint64_t engram_id,
    float replay_strength);

// Update consolidation state
void systems_consolidation_update(
    systems_consolidation_t* system,
    float time_delta_seconds,
    bool is_sleeping);
```

### 5.3 Query Functions

```c
// Get cortical memory by ID
cortical_memory_node_t* systems_consolidation_get_node(
    const systems_consolidation_t* system,
    uint64_t node_id);

// Find cortical memory similar to pattern
uint64_t systems_consolidation_find_similar(
    const systems_consolidation_t* system,
    const float* features,
    uint32_t feature_dim,
    float* similarity_out);

// Check consolidation status
float systems_consolidation_get_strength(
    const systems_consolidation_t* system,
    uint64_t node_id);

// Get semantic neighbors
uint32_t systems_consolidation_get_neighbors(
    const systems_consolidation_t* system,
    uint64_t node_id,
    uint64_t* neighbor_ids_out,
    float* strengths_out,
    uint32_t max_neighbors);
```

### 5.4 Statistics

```c
// Get system statistics
void systems_consolidation_get_stats(
    const systems_consolidation_t* system,
    uint64_t* total_transfers_out,
    uint64_t* total_replays_out,
    uint32_t* cortical_node_count_out,
    float* avg_consolidation_strength_out);
```

---

## 6. Performance Characteristics

### 6.1 Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Replay | O(r * (n + k)) | r = replays, n = engram size, k = cortical neighbors |
| Transfer | O(n + m) | n = feature extraction, m = similarity search |
| Update | O(c) | c = cortical node count |
| Find Similar | O(c * d) | c = nodes, d = feature dimensionality |

### 6.2 Space Complexity

| Component | Memory | Default |
|-----------|--------|---------|
| Cortical Node | ~200 bytes + features | 10,000 nodes = 2-5 MB |
| Replay Queue | ~100 bytes per event | 1,000 events = 100 KB |
| Total System | ~3-6 MB | For 10,000 cortical memories |

### 6.3 Consolidation Timeline

```
Day 0:     Encoding (Phase M1) ──────────> Hippocampus-dependent (100%)
Day 1:     First replays        ──────────> 10% consolidated
Day 7:     Multiple replays     ──────────> 40% consolidated
Day 14:    Semantic emerging    ──────────> 70% consolidated
Day 30:    Mostly consolidated  ──────────> 90% consolidated (semantic)
Day 60+:   Fully consolidated   ──────────> 95%+ independent of hippocampus
```

---

## 7. Test Plan

### 7.1 Unit Tests

**File:** `test/unit/test_systems_consolidation.cpp`

Tests (15 planned):
1. ✅ SystemCreation_Success
2. ✅ CorticalNodeCreation_ValidFields
3. ✅ ReplayEvent_Scheduling
4. ✅ SemanticExtraction_LossOfDetail
5. ✅ ConsolidationUpdate_TimeDependent
6. ✅ SleepAcceleration_3xFaster
7. ✅ HippocampalDependency_Decreases
8. ✅ SemanticTransition_At80Percent
9. ✅ SimilaritySearch_FindsMatches
10. ✅ SemanticNetwork_LinkFormation
11. ✅ ReplayQueue_FIFO
12. ✅ StatisticsTracking_Accurate
13. ✅ MemoryBounds_Enforced
14. ✅ NullSafety_AllFunctions
15. ✅ Cleanup_NoLeaks

### 7.2 Integration Tests

**File:** `test/integration/test_systems_consolidation_integration.cpp`

Tests (12 planned):
1. ✅ EnggramToCortex_FullPipeline
2. ✅ SleepReplay_TriggersTransfer
3. ✅ MultipleReplays_StrengthensMemory
4. ✅ ConsolidationTimeline_30Days
5. ✅ EpisodicToSemantic_Transformation
6. ✅ SemanticNetwork_Emergence
7. ✅ HippocampalLesion_OldMemoriesPreserved
8. ✅ SleepDeprivation_ImpairsConsolidation
9. ✅ RecentMemories_MoreReplay
10. ✅ SalientMemories_FasterConsolidation
11. ✅ MultipleEngrams_OneCorticalNode
12. ✅ Integration_WithBrain

### 7.3 Regression Tests

**File:** `test/regression/test_systems_consolidation_backward_compat.cpp`

Tests (8 planned):
1. ✅ BrainCreation_StillWorks
2. ✅ Phase_M1_Unaffected
3. ✅ NoPerformanceRegression
4. ✅ SleepCycle_Compatible
5. ✅ Memory_Bounded
6. ✅ API_BackwardCompatible
7. ✅ Consolidation_Stable
8. ✅ Integration_Transparent

---

## 8. Implementation Plan

### Phase 1: Core Data Structures (Day 1)
- [ ] Define cortical_memory_node_t
- [ ] Define replay_event_t
- [ ] Define systems_consolidation_t
- [ ] Implement create/destroy functions
- [ ] Basic unit tests

### Phase 2: Sleep Replay (Day 2)
- [ ] Implement replay selection algorithm
- [ ] Implement replay queue management
- [ ] Implement coordinated hippocampal-cortical replay
- [ ] Replay unit tests
- [ ] Integration with sleep system

### Phase 3: Cortical Transfer (Day 3)
- [ ] Implement semantic feature extraction
- [ ] Implement cortical node creation
- [ ] Implement similarity search
- [ ] Implement transfer algorithm
- [ ] Transfer unit tests

### Phase 4: Consolidation Update (Day 4)
- [ ] Implement time-dependent strengthening
- [ ] Implement sleep acceleration
- [ ] Implement semantic transition
- [ ] Implement forgetting/decay
- [ ] Update unit tests

### Phase 5: Brain Integration (Day 5)
- [ ] Add to brain_struct
- [ ] Wire into brain_decide()
- [ ] Wire into sleep cycle
- [ ] Integration tests
- [ ] Performance testing

### Phase 6: Testing & Documentation (Day 6-7)
- [ ] Complete all unit tests (15)
- [ ] Complete integration tests (12)
- [ ] Regression tests (8)
- [ ] Performance benchmarks
- [ ] API documentation
- [ ] Completion report

---

## 9. Success Criteria

**Phase M2 is complete when:**

✅ All 35 tests passing (15 unit + 12 integration + 8 regression)
✅ Systems consolidation integrated into brain
✅ Sleep replay functional and tested
✅ Cortical transfer working over time
✅ Semantic abstraction verified
✅ Performance overhead < 10%
✅ Code coverage > 85%
✅ Documentation complete
✅ Zero breaking changes to Phase M1

---

## 10. References

1. **McClelland, J.L., McNaughton, B.L., & O'Reilly, R.C.** (1995). "Why there are complementary learning systems in the hippocampus and neocortex." *Psychological Review*, 102(3), 419-457.

2. **Squire, L.R. & Alvarez, P.** (1995). "Retrograde amnesia and memory consolidation." *Current Opinion in Neurobiology*, 5(2), 169-177.

3. **Wilson, M.A. & McNaughton, B.L.** (1994). "Reactivation of hippocampal ensemble memories during sleep." *Science*, 265(5172), 676-679.

4. **Takashima, A. et al.** (2006). "Shift from hippocampal to neocortical centered retrieval network with consolidation." *Journal of Neuroscience*, 26(43), 11061-11069.

5. **Born, J. & Wilhelm, I.** (2012). "System consolidation of memory during sleep." *Psychological Research*, 76(2), 192-203.

6. **Dudai, Y., Karni, A., & Born, J.** (2015). "The consolidation and transformation of memory." *Neuron*, 88(1), 20-32.

---

**Status:** SPECIFICATION COMPLETE - Ready for Implementation 🚀
