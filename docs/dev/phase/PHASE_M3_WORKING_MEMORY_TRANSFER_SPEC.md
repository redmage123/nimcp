# Phase M3: Working Memory → Engram Transfer Specification

**Version:** 1.0
**Date:** 2025-11-13
**Status:** Implementation Ready
**Depends On:** Phase M1 (Engrams), Phase 10.1 (Working Memory)

## Executive Summary

Phase M3 implements the biological process of transferring information from working memory (temporary storage) to long-term memory (engrams). This bridges the gap between immediate perception and permanent storage, enabling attended and rehearsed information to be consolidated into lasting memory traces.

## Biological Foundation

### Neuroscience Background

**Working Memory (Baddeley & Hitch, 1974):**
- Temporary storage system (7±2 items, Miller's law)
- Holds information for immediate use
- Active maintenance through rehearsal
- Limited capacity requires selective transfer

**Long-Term Memory Consolidation (Atkinson & Shiffrin, 1968):**
- Working memory → Long-term memory transfer
- Rehearsal strengthens transfer probability
- Attention determines what gets encoded
- Emotional salience prioritizes important memories
- Forgetting: unrehearsed items decay from working memory

**Synaptic Consolidation (Dudai, 2004):**
- Protein synthesis required for long-term storage
- Time-dependent process (minutes to hours)
- Repeated activation strengthens encoding
- Emotional arousal enhances consolidation

### Key Mechanisms

1. **Rehearsal-Dependent Transfer**
   - Repeated activation in working memory
   - Each rehearsal increases transfer probability
   - Threshold-based encoding trigger

2. **Attention-Based Selection**
   - Attended items prioritized for transfer
   - Attention weight determines encoding strength
   - Unattended items decay faster

3. **Emotional Enhancement**
   - Emotional arousal boosts transfer probability
   - Amygdala modulation of hippocampal encoding
   - Salient memories preferentially stored

4. **Capacity Management**
   - Working memory has limited slots (7±2)
   - Transfer frees capacity for new information
   - Prevents working memory overflow

## Architecture Design

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    WORKING MEMORY                           │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐        │
│  │ Item │  │ Item │  │ Item │  │ Item │  │ Item │  ...   │
│  │  1   │  │  2   │  │  3   │  │  4   │  │  5   │        │
│  └──────┘  └──────┘  └──────┘  └──────┘  └──────┘        │
│     ↓          ↓          ↓          ↓          ↓          │
│  [Rehearsal] [Attention] [Emotion] [Time] [Decay]         │
└─────────────────────────────────────────────────────────────┘
                            ↓
                  ┌─────────────────┐
                  │ TRANSFER SYSTEM │
                  │   (Phase M3)    │
                  └─────────────────┘
                            ↓
              ┌─────────────────────────┐
              │  Transfer Decision      │
              │  - Rehearsal count      │
              │  - Attention weight     │
              │  - Emotional salience   │
              │  - Time threshold       │
              └─────────────────────────┘
                            ↓
              ┌─────────────────────────┐
              │  Engram Encoding        │
              │  (Phase M1)             │
              │  - Extract features     │
              │  - Create memory trace  │
              │  - Tag with emotion     │
              └─────────────────────────┘
                            ↓
              ┌─────────────────────────┐
              │  Systems Consolidation  │
              │  (Phase M2)             │
              │  - Sleep replay         │
              │  - Cortical transfer    │
              └─────────────────────────┘
```

### Data Structures

```c
/**
 * @struct wm_transfer_criteria_t
 * @brief Criteria for determining when to transfer working memory to engrams
 */
typedef struct {
    uint32_t rehearsal_threshold;     // Min rehearsals for transfer (default: 3)
    float attention_threshold;         // Min attention for transfer (0.5)
    float emotional_threshold;         // Min emotional salience (0.3)
    uint64_t time_threshold_ms;       // Min time in WM for transfer (5000ms)
    float decay_rate;                 // Decay per second (0.1)
} wm_transfer_criteria_t;

/**
 * @struct wm_transfer_stats_t
 * @brief Statistics for working memory transfer system
 */
typedef struct {
    uint64_t total_transfers;         // Total items transferred to engrams
    uint64_t rehearsal_triggered;     // Transfers triggered by rehearsal
    uint64_t attention_triggered;     // Transfers triggered by attention
    uint64_t emotion_triggered;       // Transfers triggered by emotion
    uint64_t time_triggered;          // Transfers triggered by time
    uint64_t total_decayed;           // Items that decayed without transfer
    uint32_t current_wm_items;        // Current working memory item count
} wm_transfer_stats_t;

/**
 * @struct wm_transfer_system_t
 * @brief System managing working memory → engram transfers
 */
typedef struct {
    wm_transfer_criteria_t criteria;  // Transfer decision criteria
    wm_transfer_stats_t stats;        // System statistics

    // System references (not owned)
    void* working_memory;             // Working memory system (Phase 10.1)
    engram_system_t* engram_system;   // Engram system (Phase M1)
    void* emotional_system;           // Emotional tagging system

    // Internal state
    float* last_attention_weights;    // Track attention over time
    uint32_t attention_weight_count;  // Number of attention weights
    uint64_t last_update_time_ms;     // Last system update time

} wm_transfer_system_t;
```

## Core API

### System Management

```c
/**
 * @brief Create working memory transfer system
 * @return New system, or NULL on failure
 */
wm_transfer_system_t* wm_transfer_create(void);

/**
 * @brief Destroy working memory transfer system
 * @param system System to destroy
 */
void wm_transfer_destroy(wm_transfer_system_t* system);

/**
 * @brief Reset transfer system (clear stats, keep criteria)
 * @param system System to reset
 */
void wm_transfer_reset(wm_transfer_system_t* system);
```

### Integration API

```c
/**
 * @brief Connect to working memory system
 * @param system Transfer system
 * @param working_memory Working memory system (Phase 10.1)
 */
void wm_transfer_set_working_memory(
    wm_transfer_system_t* system,
    void* working_memory);

/**
 * @brief Connect to engram system
 * @param system Transfer system
 * @param engram_system Engram system (Phase M1)
 */
void wm_transfer_set_engram_system(
    wm_transfer_system_t* system,
    engram_system_t* engram_system);

/**
 * @brief Connect to emotional tagging system
 * @param system Transfer system
 * @param emotional_system Emotional tagging system
 */
void wm_transfer_set_emotional_system(
    wm_transfer_system_t* system,
    void* emotional_system);
```

### Transfer Operations

```c
/**
 * @brief Evaluate working memory items for transfer to engrams
 * @param system Transfer system
 * @param time_delta_seconds Time since last evaluation
 * @return Number of items transferred
 *
 * WHAT: Checks working memory items against transfer criteria
 * WHY:  Implements selective consolidation to long-term memory
 * HOW:  Evaluates rehearsal, attention, emotion, time; transfers if met
 */
uint32_t wm_transfer_evaluate(
    wm_transfer_system_t* system,
    float time_delta_seconds);

/**
 * @brief Force transfer of specific working memory item
 * @param system Transfer system
 * @param wm_slot Working memory slot index
 * @return true if transferred, false otherwise
 *
 * WHAT: Manually trigger transfer of specific item
 * WHY:  Allow explicit encoding (e.g., important events)
 * HOW:  Bypass normal criteria, directly encode to engram
 */
bool wm_transfer_force_item(
    wm_transfer_system_t* system,
    uint32_t wm_slot);

/**
 * @brief Update attention weights for working memory items
 * @param system Transfer system
 * @param attention_weights Attention for each WM slot (0.0-1.0)
 * @param count Number of weights
 *
 * WHAT: Update which items are receiving attention
 * WHY:  Attention determines transfer priority
 * HOW:  Store weights, use in transfer evaluation
 */
void wm_transfer_update_attention(
    wm_transfer_system_t* system,
    const float* attention_weights,
    uint32_t count);
```

### Configuration API

```c
/**
 * @brief Set transfer criteria
 * @param system Transfer system
 * @param criteria New criteria to use
 */
void wm_transfer_set_criteria(
    wm_transfer_system_t* system,
    const wm_transfer_criteria_t* criteria);

/**
 * @brief Get current transfer criteria
 * @param system Transfer system
 * @param criteria_out Output for current criteria
 */
void wm_transfer_get_criteria(
    const wm_transfer_system_t* system,
    wm_transfer_criteria_t* criteria_out);
```

### Statistics API

```c
/**
 * @brief Get transfer statistics
 * @param system Transfer system
 * @param stats_out Output for statistics
 */
void wm_transfer_get_statistics(
    const wm_transfer_system_t* system,
    wm_transfer_stats_t* stats_out);
```

## Integration Points

### Brain Learning Pipeline

**Location:** `brain_learn_example()`

**Integration:**
1. After learning, update working memory with new information
2. Update attention weights (high for newly learned items)
3. Evaluate transfer criteria
4. Transfer high-priority items to engrams

**Pseudocode:**
```c
float brain_learn_example(...) {
    // Existing learning...

    // Phase M3: Working Memory Transfer
    if (brain->wm_transfer_system && brain->working_memory) {
        // Add learned pattern to working memory
        working_memory_add_item(brain->working_memory, features, num_features);

        // High attention for newly learned items
        float attention_weights[WM_CAPACITY];
        for (int i = 0; i < WM_CAPACITY; i++) {
            attention_weights[i] = (i == newest_item) ? 0.9f : 0.3f;
        }
        wm_transfer_update_attention(brain->wm_transfer_system,
                                     attention_weights, WM_CAPACITY);

        // Evaluate for transfer to engrams
        wm_transfer_evaluate(brain->wm_transfer_system, time_delta);
    }
}
```

### Brain Cognitive Pipeline

**Location:** `brain_decide()`

**Integration:**
1. Update working memory with current input
2. Update attention based on relevance
3. Evaluate transfer criteria
4. Transfer attended/rehearsed items to engrams

**Pseudocode:**
```c
brain_decision_t* brain_decide(...) {
    // Existing inference...

    // Phase M3: Working Memory Transfer
    if (brain->wm_transfer_system && brain->working_memory) {
        // Update working memory with current input
        working_memory_update(brain->working_memory, features, num_features);

        // Attention based on decision confidence
        float attention_weights[WM_CAPACITY];
        compute_attention_weights(attention_weights, decision->confidence);
        wm_transfer_update_attention(brain->wm_transfer_system,
                                     attention_weights, WM_CAPACITY);

        // Evaluate for transfer (rehearsal increases over time)
        wm_transfer_evaluate(brain->wm_transfer_system, TIME_DELTA);
    }
}
```

## Transfer Decision Algorithm

```c
// For each item in working memory:
for (uint32_t i = 0; i < wm_item_count; i++) {
    wm_item_t* item = &working_memory->items[i];

    // Compute transfer score
    float score = 0.0f;

    // 1. Rehearsal contribution
    if (item->rehearsal_count >= criteria.rehearsal_threshold) {
        score += 0.4f;  // 40% weight
    }

    // 2. Attention contribution
    if (item->attention_weight >= criteria.attention_threshold) {
        score += 0.3f;  // 30% weight
    }

    // 3. Emotional contribution
    if (item->emotional_salience >= criteria.emotional_threshold) {
        score += 0.2f;  // 20% weight
    }

    // 4. Time contribution
    uint64_t time_in_wm = current_time - item->creation_time;
    if (time_in_wm >= criteria.time_threshold_ms) {
        score += 0.1f;  // 10% weight
    }

    // Transfer if score sufficient (>= 0.5 = 50%)
    if (score >= 0.5f) {
        transfer_to_engram(item);
        stats.total_transfers++;

        // Update trigger stats
        if (item->rehearsal_count >= criteria.rehearsal_threshold)
            stats.rehearsal_triggered++;
        if (item->attention_weight >= criteria.attention_threshold)
            stats.attention_triggered++;
        // ... etc
    }
}
```

## Default Configuration

```c
wm_transfer_criteria_t DEFAULT_CRITERIA = {
    .rehearsal_threshold = 3,       // 3+ rehearsals triggers transfer
    .attention_threshold = 0.5f,    // 50% attention required
    .emotional_threshold = 0.3f,    // 30% emotional salience
    .time_threshold_ms = 5000,      // 5 seconds in working memory
    .decay_rate = 0.1f              // 10% decay per second
};
```

## Performance Characteristics

### Time Complexity
- **Transfer Evaluation:** O(n) where n = working memory capacity (7-9 items)
- **Per Item Transfer:** O(1) engram encoding
- **Attention Update:** O(n) where n = working memory capacity
- **Total Per Cycle:** O(n) = O(7-9) ≈ O(1) constant time

### Space Complexity
- **Transfer System:** O(1) - Fixed size structures
- **Attention Tracking:** O(n) where n = WM capacity
- **Total Overhead:** ~1KB per brain

### Expected Performance
- **Transfer evaluation:** <100μs per cycle
- **Attention update:** <50μs per cycle
- **Total overhead:** <150μs per decision cycle
- **Memory overhead:** Negligible (<1KB)

## Test Plan

### Unit Tests (Target: 20+ tests)

1. **System Management** (3 tests)
   - Create system
   - Destroy system
   - Reset system

2. **Integration API** (3 tests)
   - Set working memory
   - Set engram system
   - Set emotional system

3. **Transfer Criteria** (5 tests)
   - Rehearsal threshold triggers
   - Attention threshold triggers
   - Emotional threshold triggers
   - Time threshold triggers
   - Combined criteria scoring

4. **Transfer Operations** (5 tests)
   - Evaluate with no transfers
   - Evaluate with single transfer
   - Evaluate with multiple transfers
   - Force transfer
   - Transfer with decay

5. **Attention Management** (2 tests)
   - Update attention weights
   - Attention affects transfer

6. **Statistics** (2 tests)
   - Get statistics
   - Statistics update correctly

### Integration Tests (Target: 10+ tests)

1. **Brain Learning Integration** (3 tests)
   - Learning adds to working memory
   - Learned items transfer to engrams
   - Transfer statistics tracked

2. **Brain Cognitive Integration** (3 tests)
   - Inference updates working memory
   - Attended items transfer
   - Rehearsal triggers transfer

3. **Multi-System Integration** (2 tests)
   - Working memory + engrams + consolidation
   - Full pipeline from WM to cortex

4. **Performance** (2 tests)
   - No excessive overhead
   - Scales with working memory size

### Regression Tests (Target: 15+ tests)

1. **Backward Compatibility** (5 tests)
   - Brain creation still works
   - Learning unaffected
   - Inference unaffected
   - Working memory still works
   - Engrams still work

2. **Stability** (5 tests)
   - Extended use stable
   - No memory leaks
   - Transfer doesn't corrupt brain
   - Multi-pattern learning
   - Sleep cycle compatibility

3. **API Compatibility** (5 tests)
   - All brain APIs work
   - Phase M1 unaffected
   - Phase M2 unaffected
   - Performance acceptable
   - Edge cases handled

## Success Criteria

- ✅ All unit tests passing (20+)
- ✅ All integration tests passing (10+)
- ✅ All regression tests passing (15+)
- ✅ Total: 45+ tests, 100% passing
- ✅ Performance: <200μs overhead per cycle
- ✅ Memory overhead: <2KB per brain
- ✅ Zero breaking changes
- ✅ NIMCP standards: <50 lines per function
- ✅ Comprehensive documentation

## References

1. **Miller, G.A. (1956).** "The magical number seven, plus or minus two: Some limits on our capacity for processing information." *Psychological Review, 63*(2), 81-97.

2. **Baddeley, A.D. & Hitch, G. (1974).** "Working memory." *Psychology of Learning and Motivation, 8*, 47-89.

3. **Atkinson, R.C. & Shiffrin, R.M. (1968).** "Human memory: A proposed system and its control processes." *Psychology of Learning and Motivation, 2*, 89-195.

4. **Dudai, Y. (2004).** "The neurobiology of consolidations, or, how stable is the engram?" *Annual Review of Psychology, 55*, 51-86.

5. **Cowan, N. (2001).** "The magical number 4 in short-term memory: A reconsideration of mental storage capacity." *Behavioral and Brain Sciences, 24*(1), 87-114.

6. **McGaugh, J.L. (2000).** "Memory--a century of consolidation." *Science, 287*(5451), 248-251.

## Implementation Timeline

**Day 1:** Core system implementation (header + source + unit tests)
**Day 2:** Brain learning pipeline integration + integration tests
**Day 3:** Brain cognitive pipeline integration + regression tests
**Day 4:** Documentation and completion report

**Estimated Total:** 1-2 days for full implementation with 100% test coverage
