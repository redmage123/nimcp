# Memory Engrams Implementation Specification

**Date:** 2025-11-13
**Version:** Phase M (Memory Engrams)
**Status:** Design Specification
**Priority:** ⭐⭐⭐ High Priority

---

## Executive Summary

**WHAT:** Explicit representation of memory traces as distributed patterns of synaptic changes across neuron ensembles
**WHY:** Enable biologically-realistic memory encoding, consolidation, recall, and manipulation for cognitive research
**HOW:** Track engram cells, synaptic weights, consolidation state, and reactivation patterns with integration into existing NIMCP systems

**Scientific Foundation:**
- **Engram Theory:** Richard Semon (1904), proven by Tonegawa et al. (2012-2015)
- **Optogenetics:** Liu et al. (2012) - reactivating engrams triggers memory recall
- **Systems Consolidation:** McClelland et al. (1995) - hippocampus → cortex transfer
- **Reconsolidation:** Nader et al. (2000) - memories become labile when recalled

---

## Table of Contents

1. [Core Data Structures](#core-data-structures)
2. [Biological Mechanisms](#biological-mechanisms)
3. [System Architecture](#system-architecture)
4. [Integration Points](#integration-points)
5. [Implementation Phases](#implementation-phases)
6. [Research Applications](#research-applications)
7. [Testing Strategy](#testing-strategy)
8. [Performance Considerations](#performance-considerations)

---

## Core Data Structures

### 1. Basic Engram Structure

```c
/**
 * @file nimcp_engram.h
 * @brief Memory engram system - explicit memory trace representation
 *
 * WHAT: Physical traces of memory stored as synaptic patterns across neurons
 * WHY:  Enable realistic memory encoding, consolidation, recall, manipulation
 * HOW:  Track engram cells, weights, consolidation state, reactivation
 *
 * BIOLOGICAL BASIS:
 * - Engram cells: Neurons active during encoding and reactivated during recall
 * - Synaptic plasticity: LTP/LTD creates lasting structural changes
 * - Consolidation: Labile → stable transition over hours/days
 * - Systems consolidation: Hippocampus → cortex transfer over weeks/months
 *
 * NEUROSCIENCE REFERENCES:
 * - Tonegawa et al. (2015): "Memory engram cells have come of age"
 * - Liu et al. (2012): "Optogenetic stimulation of engram cells triggers recall"
 * - Ryan et al. (2015): "Engram cells retain memory under retrograde amnesia"
 * - Josselyn & Tonegawa (2020): "Memory engrams: Recalling the past"
 *
 * @version Phase M: Memory Engrams
 * @date 2025-11-13
 */

#ifndef NIMCP_ENGRAM_H
#define NIMCP_ENGRAM_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum neurons per engram (distributed trace) */
#define ENGRAM_MAX_NEURONS 512

/* Maximum engrams tracked simultaneously */
#define ENGRAM_MAX_COUNT 1024

/* Consolidation time constants */
#define ENGRAM_SYNAPTIC_CONSOLIDATION_TIME (3600.0f * 6.0f)    /* 6 hours */
#define ENGRAM_SYSTEMS_CONSOLIDATION_TIME (3600.0f * 24.0f * 30.0f)  /* 30 days */

/* Tagging constants (c-fos/Arc expression) */
#define ENGRAM_TAG_DURATION (3600.0f * 4.0f)  /* 4 hours for IEG expression */
#define ENGRAM_TAG_WINDOW (3600.0f * 6.0f)    /* 6 hour tagging window */

/* Reactivation thresholds */
#define ENGRAM_RECALL_THRESHOLD 0.4f          /* Min activation for recall */
#define ENGRAM_RECOGNITION_THRESHOLD 0.6f     /* Min for strong recognition */

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Engram consolidation state
 */
typedef enum {
    ENGRAM_STATE_ENCODING,          /**< Currently being formed (seconds) */
    ENGRAM_STATE_LABILE,            /**< Unstable, vulnerable (minutes-hours) */
    ENGRAM_STATE_CONSOLIDATING,     /**< Protein synthesis dependent (hours) */
    ENGRAM_STATE_CONSOLIDATED,      /**< Stable, long-term (days-lifetime) */
    ENGRAM_STATE_RECONSOLIDATING,   /**< Temporarily labile after recall */
    ENGRAM_STATE_DEGRADING          /**< Actively weakening/forgotten */
} engram_state_t;

/**
 * @brief Memory system location
 */
typedef enum {
    ENGRAM_LOCATION_HIPPOCAMPUS,    /**< Short-term, spatial, episodic */
    ENGRAM_LOCATION_CORTEX,         /**< Long-term, semantic, consolidated */
    ENGRAM_LOCATION_AMYGDALA,       /**< Emotional memories */
    ENGRAM_LOCATION_STRIATUM,       /**< Procedural, motor memories */
    ENGRAM_LOCATION_CEREBELLUM,     /**< Motor learning, timing */
    ENGRAM_LOCATION_DISTRIBUTED     /**< Across multiple regions */
} engram_location_t;

/**
 * @brief Memory type (Tulving, 1972)
 */
typedef enum {
    MEMORY_TYPE_EPISODIC,           /**< Personal events (what/where/when) */
    MEMORY_TYPE_SEMANTIC,           /**< Facts and knowledge */
    MEMORY_TYPE_PROCEDURAL,         /**< Skills and habits */
    MEMORY_TYPE_EMOTIONAL,          /**< Emotional associations */
    MEMORY_TYPE_WORKING             /**< Temporary active memory */
} memory_type_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Synaptic connection in engram
 */
typedef struct {
    uint32_t pre_neuron_id;         /**< Presynaptic neuron ID */
    uint32_t post_neuron_id;        /**< Postsynaptic neuron ID */
    float weight_at_encoding;       /**< Original synaptic weight */
    float current_weight;           /**< Current weight (may decay) */
    float delta_weight;             /**< Change from baseline */
    bool is_potentiated;            /**< LTP occurred? */
    bool is_depressed;              /**< LTD occurred? */
} engram_synapse_t;

/**
 * @brief Individual memory engram
 *
 * WHAT: Distributed pattern of neurons and synapses encoding a memory
 * WHY:  Explicit representation enables targeted memory operations
 * HOW:  Track neurons, synapses, consolidation state, reactivation
 */
typedef struct {
    // Identification
    uint64_t engram_id;             /**< Unique identifier */
    bool active;                    /**< Is this slot in use? */
    memory_type_t memory_type;      /**< What kind of memory? */

    // Neuron ensemble
    uint32_t neuron_ids[ENGRAM_MAX_NEURONS];  /**< Engram cells */
    float neuron_activation[ENGRAM_MAX_NEURONS]; /**< Current activation */
    uint32_t neuron_count;          /**< How many neurons */

    // Synaptic pattern
    engram_synapse_t* synapses;     /**< Synaptic connections */
    uint32_t synapse_count;         /**< Number of synapses */

    // Consolidation state
    engram_state_t state;           /**< Current consolidation state */
    float consolidation_strength;   /**< 0.0 (labile) to 1.0 (consolidated) */
    engram_location_t primary_location;  /**< Main storage location */
    engram_location_t secondary_location; /**< Systems consolidation target */

    // Temporal dynamics
    uint64_t encoding_time_us;      /**< When was this encoded? */
    uint64_t last_reactivation_us;  /**< Last recall time */
    uint32_t reactivation_count;    /**< How many times recalled */
    float decay_rate;               /**< Forgetting rate */

    // Activity-dependent tagging (IEG expression)
    bool is_tagged;                 /**< c-fos/Arc expression? */
    float tag_strength;             /**< Tag intensity [0-1] */
    uint64_t tag_onset_time_us;     /**< When was tag created? */

    // Content and context
    emotional_tag_t emotion;        /**< Emotional context */
    float vividness;                /**< Subjective clarity [0-1] */
    float confidence;               /**< Retrieval confidence [0-1] */

    // Episodic memory components (optional)
    uint64_t what_content_hash;     /**< What happened (event) */
    uint64_t where_location_hash;   /**< Where it happened (place) */
    uint64_t when_time_hash;        /**< When it happened (time) */
    uint64_t who_agent_hash;        /**< Who was involved (agent) */

    // Interference and competition
    uint64_t* competing_engrams;    /**< Similar/interfering memories */
    uint32_t competition_count;     /**< Number of competitors */

    // Reconsolidation
    bool is_reconsolidating;        /**< Temporarily labile? */
    uint64_t reconsolidation_start_us; /**< When recall made it labile */
    float reconsolidation_window;   /**< How long labile (seconds) */

    // Statistics
    float recall_latency_ms;        /**< Time to retrieve */
    float last_recall_accuracy;     /**< How accurate was recall */

} memory_engram_t;

/**
 * @brief Engram ensemble (related memories)
 */
typedef struct {
    uint64_t* engram_ids;           /**< Related engram IDs */
    uint32_t engram_count;          /**< Number of engrams */
    float coherence;                /**< How strongly linked [0-1] */
    memory_type_t shared_type;      /**< Common memory type */
} engram_ensemble_t;

/**
 * @brief Complete engram memory system
 */
typedef struct {
    // Engram storage
    memory_engram_t engrams[ENGRAM_MAX_COUNT];
    uint32_t active_count;          /**< How many engrams active */
    uint64_t next_engram_id;        /**< ID counter */

    // Consolidation tracking
    uint32_t labile_count;          /**< Engrams in labile state */
    uint32_t consolidating_count;   /**< Engrams consolidating */
    uint32_t consolidated_count;    /**< Stable engrams */

    // Systems consolidation (hippocampus → cortex)
    bool systems_consolidation_enabled;
    float hippocampal_capacity;     /**< Hippocampus space [0-1] */
    float cortical_capacity;        /**< Cortex space [0-1] */

    // Sleep-dependent consolidation
    float sleep_consolidation_rate; /**< Consolidation during sleep */
    uint32_t replays_during_sleep;  /**< Replay count per sleep cycle */

    // Forgetting
    float baseline_decay_rate;      /**< Default forgetting rate */
    bool use_interference;          /**< Enable proactive/retroactive interference */

    // Pattern separation/completion
    float separation_threshold;     /**< Min difference for separate engrams */
    float completion_threshold;     /**< Min overlap for pattern completion */

    // Integration flags
    bool integrate_with_sleep;      /**< Consolidate during sleep? */
    bool integrate_with_emotion;    /**< Emotional enhancement? */
    bool integrate_with_consolidation; /**< Use consolidation system? */

    // Statistics
    uint64_t total_encodings;       /**< Lifetime encoding count */
    uint64_t total_recalls;         /**< Lifetime recall count */
    uint64_t total_consolidations;  /**< Successful consolidations */
    uint64_t total_extinctions;     /**< Memories extinguished */
    float average_consolidation_time; /**< Mean time to consolidate */

} engram_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Create engram system
 */
engram_system_t* engram_system_create(void);

/**
 * @brief Destroy engram system
 */
void engram_system_destroy(engram_system_t* system);

/**
 * @brief Reset engram system
 */
void engram_system_reset(engram_system_t* system);

//=============================================================================
// ENCODING FUNCTIONS
//=============================================================================

/**
 * @brief Encode new memory engram
 *
 * WHAT: Create new engram from current neural activity pattern
 * WHY:  Store experience as explicit memory trace
 * HOW:  Tag active neurons, record synaptic weights, apply emotion
 *
 * @param system Engram system
 * @param neuron_ids Array of active neuron IDs
 * @param activations Array of neuron activations
 * @param count Number of neurons
 * @param memory_type Type of memory being encoded
 * @param emotion Emotional context
 * @return Engram ID (0 if failed)
 */
uint64_t engram_encode(
    engram_system_t* system,
    const uint32_t* neuron_ids,
    const float* activations,
    uint32_t count,
    memory_type_t memory_type,
    emotional_tag_t emotion);

/**
 * @brief Encode episodic memory with context
 *
 * WHAT: Create episodic engram with what/where/when/who components
 * WHY:  Model rich autobiographical memories
 * HOW:  Bind multiple context engrams into unified episode
 */
uint64_t engram_encode_episodic(
    engram_system_t* system,
    uint64_t what_content,
    uint64_t where_location,
    uint64_t when_time,
    uint64_t who_agent,
    emotional_tag_t emotion);

//=============================================================================
// RECALL FUNCTIONS
//=============================================================================

/**
 * @brief Recall memory engram
 *
 * WHAT: Reactivate engram neurons based on cue pattern
 * WHY:  Retrieve stored memory
 * HOW:  Pattern completion from partial cue, reactivate ensemble
 *
 * @param system Engram system
 * @param cue_neurons Partial cue pattern
 * @param cue_count Number of cue neurons
 * @param activation_out Output: reactivated neurons
 * @param max_activation_count Max output neurons
 * @return Engram ID of recalled memory (0 if no match)
 */
uint64_t engram_recall(
    engram_system_t* system,
    const uint32_t* cue_neurons,
    uint32_t cue_count,
    uint32_t* activation_out,
    uint32_t max_activation_count);

/**
 * @brief Recall by content (semantic search)
 *
 * WHAT: Find engrams matching content hash
 * WHY:  Content-addressable memory
 * HOW:  Search by what/where/when/who hashes
 */
uint64_t engram_recall_by_content(
    engram_system_t* system,
    uint64_t content_hash,
    float* confidence_out);

/**
 * @brief Recognition test (familiarity)
 *
 * WHAT: Test if pattern has been seen before
 * WHY:  Recognition vs recall distinction
 * HOW:  Match against engrams without full reactivation
 */
bool engram_recognize(
    engram_system_t* system,
    const uint32_t* pattern,
    uint32_t count,
    float* familiarity_out);

//=============================================================================
// CONSOLIDATION FUNCTIONS
//=============================================================================

/**
 * @brief Update consolidation state
 *
 * WHAT: Advance synaptic and systems consolidation
 * WHY:  Labile → stable transition, hippocampus → cortex transfer
 * HOW:  Time-dependent strengthening, protein synthesis simulation
 *
 * @param system Engram system
 * @param dt Time step (seconds)
 * @param is_sleeping Currently in sleep state?
 */
void engram_consolidate_update(
    engram_system_t* system,
    float dt,
    bool is_sleeping);

/**
 * @brief Systems consolidation (hippocampus → cortex)
 *
 * WHAT: Transfer engram from hippocampus to cortex
 * WHY:  Free hippocampal capacity for new memories
 * HOW:  Gradual transfer over days/weeks
 */
bool engram_systems_consolidate(
    engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Sleep-dependent replay
 *
 * WHAT: Reactivate engrams during sleep for consolidation
 * WHY:  Sleep strengthens memories (Wilson & McNaughton, 1994)
 * HOW:  Replay sequences, strengthen synapses
 */
void engram_sleep_replay(
    engram_system_t* system,
    uint32_t replay_count);

//=============================================================================
// RECONSOLIDATION FUNCTIONS
//=============================================================================

/**
 * @brief Trigger reconsolidation
 *
 * WHAT: Make consolidated engram temporarily labile
 * WHY:  Recalled memories can be updated (Nader et al., 2000)
 * HOW:  Recall triggers reconsolidation window (~6 hours)
 */
void engram_trigger_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Update engram during reconsolidation
 *
 * WHAT: Modify labile engram with new information
 * WHY:  Memory updating, integration of new details
 * HOW:  Update synaptic weights during labile window
 */
bool engram_update_during_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id,
    const float* new_weights,
    uint32_t weight_count);

/**
 * @brief Block reconsolidation (therapeutic application)
 *
 * WHAT: Prevent engram from restabilizing
 * WHY:  PTSD treatment, maladaptive memory weakening
 * HOW:  Simulate protein synthesis inhibitor
 */
bool engram_block_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id);

//=============================================================================
// FORGETTING AND EXTINCTION
//=============================================================================

/**
 * @brief Natural forgetting (decay)
 *
 * WHAT: Gradual weakening of unused engrams
 * WHY:  Realistic forgetting curves (Ebbinghaus)
 * HOW:  Time-dependent synaptic weight decay
 */
void engram_apply_decay(
    engram_system_t* system,
    float dt);

/**
 * @brief Extinction (active unlearning)
 *
 * WHAT: Weaken engram through repeated unreinforced reactivation
 * WHY:  Model extinction learning (e.g., fear extinction)
 * HOW:  LTD-like weakening with each unreinforced recall
 */
void engram_extinction(
    engram_system_t* system,
    uint64_t engram_id,
    float extinction_strength);

/**
 * @brief Interference (proactive/retroactive)
 *
 * WHAT: Competition between similar engrams
 * WHY:  Realistic interference effects
 * HOW:  Similar patterns weaken each other
 */
void engram_apply_interference(
    engram_system_t* system,
    uint64_t engram_id);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Get engram by ID
 */
memory_engram_t* engram_get_by_id(
    engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get consolidation state
 */
engram_state_t engram_get_state(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get consolidation strength
 */
float engram_get_consolidation_strength(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Check if engram is reconsolidating
 */
bool engram_is_reconsolidating(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get memory age
 */
float engram_get_age_seconds(
    const engram_system_t* system,
    uint64_t engram_id,
    uint64_t current_time_us);

/**
 * @brief Find similar engrams (pattern matching)
 */
uint32_t engram_find_similar(
    engram_system_t* system,
    uint64_t query_engram_id,
    uint64_t* similar_ids_out,
    uint32_t max_results,
    float similarity_threshold);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENGRAM_H */
```

---

## Biological Mechanisms

### 1. Engram Formation (Encoding)

**Mechanism:**
```
Sensory Experience
    ↓
Pattern of Neural Activity
    ↓
Coincident Pre/Post Activity
    ↓
NMDA Receptor Activation
    ↓
Ca2+ Influx → CaMKII Activation
    ↓
Early LTP (minutes)
    ↓
c-fos/Arc Gene Expression (IEG Tagging)
    ↓
Protein Synthesis
    ↓
Late LTP (hours-days)
    ↓
Structural Changes (spine growth)
    ↓
Consolidated Engram
```

**Implementation:**
- Detect coincident activity (Hebbian learning)
- Tag active neurons with `is_tagged = true`
- Record synaptic weight changes
- Track consolidation time course

### 2. Consolidation Timeline

**Synaptic Consolidation (0-6 hours):**
- **Phase 1 (0-1h):** Early LTP, labile state
- **Phase 2 (1-4h):** Protein synthesis, IEG expression
- **Phase 3 (4-6h):** Late LTP, structural changes
- **Result:** Stable synaptic changes

**Systems Consolidation (days-weeks):**
- **Hippocampus:** Initial encoding, temporary storage
- **Cortex:** Gradual integration, permanent storage
- **Process:** Repeated replay transfers from hippocampus → cortex

**Implementation:**
```c
void consolidate_engram(memory_engram_t* engram, float dt) {
    float elapsed = get_time_since_encoding(engram);

    // Synaptic consolidation (0-6 hours)
    if (elapsed < SYNAPTIC_CONSOLIDATION_TIME) {
        engram->consolidation_strength = elapsed / SYNAPTIC_CONSOLIDATION_TIME;
        engram->state = ENGRAM_STATE_CONSOLIDATING;
    }
    // Systems consolidation (days-weeks)
    else if (elapsed < SYSTEMS_CONSOLIDATION_TIME) {
        float systems_progress = (elapsed - SYNAPTIC_CONSOLIDATION_TIME) /
                                (SYSTEMS_CONSOLIDATION_TIME - SYNAPTIC_CONSOLIDATION_TIME);

        // Transfer from hippocampus to cortex
        if (systems_progress > 0.5f &&
            engram->primary_location == ENGRAM_LOCATION_HIPPOCAMPUS) {
            engram->secondary_location = ENGRAM_LOCATION_CORTEX;
        }

        engram->state = ENGRAM_STATE_CONSOLIDATED;
    }
}
```

### 3. Recall and Reactivation

**Pattern Completion:**
```
Partial Cue (subset of original neurons)
    ↓
Activate Cue Neurons
    ↓
Recurrent Connections
    ↓
Spread to Connected Neurons
    ↓
Threshold Crossing
    ↓
Full Engram Reactivation
    ↓
Memory Recall
```

**Implementation:**
```c
uint64_t recall_from_cue(engram_system_t* sys, uint32_t* cue, uint32_t count) {
    float best_match = 0.0f;
    uint64_t best_engram_id = 0;

    for (uint32_t i = 0; i < sys->active_count; i++) {
        memory_engram_t* engram = &sys->engrams[i];
        if (!engram->active) continue;

        // Calculate overlap
        uint32_t overlap = count_overlap(cue, count,
                                         engram->neuron_ids,
                                         engram->neuron_count);
        float match = (float)overlap / engram->neuron_count;

        // Pattern completion threshold
        if (match > sys->completion_threshold && match > best_match) {
            best_match = match;
            best_engram_id = engram->engram_id;
        }
    }

    if (best_engram_id > 0) {
        // Trigger reconsolidation
        engram_trigger_reconsolidation(sys, best_engram_id);
    }

    return best_engram_id;
}
```

### 4. Reconsolidation Window

**Process:**
```
Consolidated Memory (stable)
    ↓
Recall/Reactivation
    ↓
Destabilization (labile ~6 hours)
    ↓
Protein Synthesis
    ↓
Restabilization (updated memory)
```

**Key Feature:** Window of opportunity to:
- Update memory with new information
- Weaken maladaptive memories (PTSD treatment)
- Enhance memory with additional rehearsal

---

## System Architecture

### Integration with Existing NIMCP Systems

```
┌─────────────────────────────────────────────────────────────────┐
│                        NIMCP Brain                              │
│                                                                 │
│  ┌────────────────┐         ┌─────────────────┐               │
│  │   Synapses     │────────▶│  Engram System  │               │
│  │  (LTP/LTD)     │  Weight │   (Encoding)    │               │
│  └────────────────┘  Changes└─────────────────┘               │
│                                      │                          │
│                                      │ Tag Neurons              │
│                                      ▼                          │
│  ┌────────────────┐         ┌─────────────────┐               │
│  │ Sleep System   │◀───────▶│  Consolidation  │               │
│  │   (Replay)     │  During │   (Strengthen)  │               │
│  └────────────────┘   Sleep └─────────────────┘               │
│                                      │                          │
│                                      │ Emotion Boost            │
│                                      ▼                          │
│  ┌────────────────┐         ┌─────────────────┐               │
│  │Emotional System│────────▶│  Engram Store   │               │
│  │ (Amygdala Tag) │ Enhance │  (Long-term)    │               │
│  └────────────────┘         └─────────────────┘               │
│                                      │                          │
│                                      │ Recall                   │
│                                      ▼                          │
│  ┌────────────────┐         ┌─────────────────┐               │
│  │Working Memory  │◀───────▶│  Recall/Update  │               │
│  │  (Active WM)   │  Load   │(Reconsolidation)│               │
│  └────────────────┘         └─────────────────┘               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### File Structure

```
src/
├── cognitive/
│   └── memory/
│       ├── nimcp_engram.h              # Core engram API
│       ├── nimcp_engram.c              # Implementation
│       ├── nimcp_engram_consolidation.c # Consolidation logic
│       ├── nimcp_engram_recall.c       # Recall/pattern completion
│       └── nimcp_engram_reconsolidation.c # Reconsolidation logic
├── include/
│   └── cognitive/
│       └── nimcp_engram.h
test/
├── unit/
│   ├── test_engram_encoding.cpp        # Encoding tests
│   ├── test_engram_consolidation.cpp   # Consolidation tests
│   ├── test_engram_recall.cpp          # Recall tests
│   └── test_engram_reconsolidation.cpp # Reconsolidation tests
├── integration/
│   ├── test_engram_sleep_integration.cpp   # Sleep replay
│   ├── test_engram_emotion_integration.cpp # Emotional enhancement
│   └── test_engram_interference.cpp        # Interference effects
└── regression/
    └── test_engram_backward_compat.cpp
```

---

## Integration Points

### 1. Sleep System Integration

**Current State:** NIMCP has `nimcp_sleep_wake.c`

**Integration:**
```c
void sleep_cycle_update(sleep_system_t* sleep, engram_system_t* engrams) {
    if (sleep->current_state == SLEEP_STATE_SWS) {
        // Slow-wave sleep: replay for consolidation
        engram_sleep_replay(engrams, 100);  // 100 replays per cycle
    }
    else if (sleep->current_state == SLEEP_STATE_REM) {
        // REM sleep: integration and emotional processing
        engram_consolidate_update(engrams, sleep->dt, true);
    }
}
```

### 2. Emotional Tagging Integration

**Current State:** NIMCP has `nimcp_emotional_tagging.c`

**Integration:**
```c
uint64_t encode_with_emotion(engram_system_t* engrams,
                              uint32_t* neurons,
                              uint32_t count,
                              emotional_tag_t emotion) {
    uint64_t id = engram_encode(engrams, neurons, count,
                                MEMORY_TYPE_EPISODIC, emotion);

    // Emotional memories consolidate faster
    memory_engram_t* engram = engram_get_by_id(engrams, id);
    if (emotion.arousal > 0.6f) {
        engram->consolidation_strength *= 1.5f;  // Amygdala boost
        engram->decay_rate *= 0.5f;  // Resist forgetting
    }

    return id;
}
```

### 3. Consolidation System Integration

**Current State:** NIMCP has `nimcp_consolidation.c`

**Integration:**
```c
void consolidation_update(consolidation_system_t* consol,
                         engram_system_t* engrams) {
    // Use existing consolidation system to track engram consolidation
    for (uint32_t i = 0; i < engrams->active_count; i++) {
        memory_engram_t* engram = &engrams->engrams[i];
        if (!engram->active) continue;

        if (engram->state == ENGRAM_STATE_CONSOLIDATING) {
            // Update consolidation progress
            float progress = engram->consolidation_strength;
            consolidation_update_memory(consol, engram->engram_id, progress);
        }
    }
}
```

### 4. Neuromodulator Integration

**Dopamine:** Reward prediction error strengthens engrams
**Norepinephrine:** Arousal during encoding enhances consolidation
**Acetylcholine:** Encoding mode (high ACh) vs retrieval mode (low ACh)
**Serotonin:** Mood-congruent memory encoding

```c
void apply_neuromodulator_effects(memory_engram_t* engram,
                                  neuromodulator_system_t* neuromod) {
    float dopamine = neuromod->dopamine_level;
    float norepinephrine = neuromod->norepinephrine_level;
    float acetylcholine = neuromod->acetylcholine_level;

    // High dopamine = stronger encoding
    if (dopamine > 0.6f) {
        engram->consolidation_strength *= (1.0f + dopamine * 0.3f);
    }

    // High norepinephrine = arousal boost
    if (norepinephrine > 0.6f) {
        engram->decay_rate *= 0.7f;  // Slower forgetting
    }

    // High ACh = encoding mode, low ACh = retrieval mode
    if (acetylcholine > 0.7f) {
        // Favor new encoding over recall interference
        engram->state = ENGRAM_STATE_ENCODING;
    }
}
```

---

## Implementation Phases

### **Phase M1: Core Engram System (Week 1-2)**

**Goals:**
- ✅ Basic engram encoding
- ✅ Neuron ensemble tracking
- ✅ Synaptic weight recording
- ✅ Simple recall by ID

**Deliverables:**
- `nimcp_engram.h` - Core API
- `nimcp_engram.c` - Basic implementation
- Unit tests for encoding/recall

**Success Criteria:**
- Encode 1000 engrams without memory leaks
- Recall by ID with <1ms latency
- 100% test coverage for core functions

---

### **Phase M2: Consolidation (Week 3-4)**

**Goals:**
- ✅ Synaptic consolidation (0-6 hours)
- ✅ Systems consolidation (hippocampus → cortex)
- ✅ Integration with sleep system
- ✅ Replay during sleep

**Deliverables:**
- `nimcp_engram_consolidation.c`
- Integration with `nimcp_sleep_wake.c`
- Consolidation unit tests

**Success Criteria:**
- Consolidation progresses realistically over time
- Sleep replay strengthens engrams
- Systems consolidation transfers engrams

---

### **Phase M3: Recall and Pattern Completion (Week 5-6)**

**Goals:**
- ✅ Pattern completion from partial cues
- ✅ Recognition vs recall distinction
- ✅ Confidence/familiarity scoring
- ✅ Context-dependent recall

**Deliverables:**
- `nimcp_engram_recall.c`
- Recall unit tests
- Pattern completion benchmarks

**Success Criteria:**
- Recall from 30% cue with >80% accuracy
- Recognition faster than recall
- Confidence correlates with match quality

---

### **Phase M4: Reconsolidation (Week 7-8)**

**Goals:**
- ✅ Reconsolidation trigger on recall
- ✅ Labile window (6 hours)
- ✅ Memory updating during reconsolidation
- ✅ Reconsolidation blockade (therapeutic)

**Deliverables:**
- `nimcp_engram_reconsolidation.c`
- Reconsolidation tests
- PTSD therapy simulation

**Success Criteria:**
- Recalled memories enter labile state
- Can update memories during window
- Blockade weakens maladaptive memories

---

### **Phase M5: Forgetting and Interference (Week 9-10)**

**Goals:**
- ✅ Natural decay (Ebbinghaus curve)
- ✅ Extinction learning
- ✅ Proactive/retroactive interference
- ✅ Pattern separation

**Deliverables:**
- Forgetting mechanisms
- Interference models
- Extinction protocols

**Success Criteria:**
- Forgetting follows power-law decay
- Similar memories interfere realistically
- Extinction weakens without full erasure

---

### **Phase M6: Episodic Memory (Week 11-12)**

**Goals:**
- ✅ What/where/when/who binding
- ✅ Contextual recall
- ✅ Autobiographical timeline
- ✅ Mental time travel

**Deliverables:**
- Episodic memory API
- Integration with `nimcp_autobiographical_memory.c`
- Rich memory tests

**Success Criteria:**
- Can recall "what happened where and when"
- Context cues aid retrieval
- Temporal ordering preserved

---

## Research Applications

### 1. **PTSD Treatment Simulation**

**Research Question:** Can reconsolidation blockade weaken traumatic memories?

**Protocol:**
```c
// 1. Encode traumatic memory
uint64_t trauma_id = engram_encode(engrams, trauma_neurons, count,
                                  MEMORY_TYPE_EMOTIONAL, trauma_emotion);

// 2. Consolidate
engram_consolidate_update(engrams, dt, false);

// 3. Recall (triggers reconsolidation)
engram_recall(engrams, cue, cue_count, output, max_out);

// 4. Block reconsolidation (simulate propranolol)
engram_block_reconsolidation(engrams, trauma_id);

// 5. Measure trauma memory strength
float strength = engram_get_consolidation_strength(engrams, trauma_id);
// Expected: Reduced strength after blockade
```

### 2. **Alzheimer's Modeling**

**Research Question:** How does amyloid-β affect engram stability?

**Protocol:**
```c
// Simulate amyloid effects on synapses
void apply_amyloid_pathology(engram_system_t* engrams, float amyloid_level) {
    for (uint32_t i = 0; i < engrams->active_count; i++) {
        memory_engram_t* engram = &engrams->engrams[i];

        // Amyloid increases decay rate
        engram->decay_rate *= (1.0f + amyloid_level * 0.5f);

        // Impairs consolidation
        engram->consolidation_strength *= (1.0f - amyloid_level * 0.3f);

        // Disrupts recall
        if (rand_float() < amyloid_level * 0.2f) {
            // Randomly corrupt synapses
            corrupt_engram_synapses(engram, 0.1f);
        }
    }
}
```

### 3. **Optimal Learning Schedules**

**Research Question:** What spacing schedule maximizes consolidation?

**Protocol:**
```c
// Test different spacing intervals
float test_spacing_schedule(uint32_t* intervals, uint32_t count) {
    engram_system_t* engrams = engram_system_create();

    // Encode memory
    uint64_t id = engram_encode(...);

    // Apply spaced repetition
    for (uint32_t i = 0; i < count; i++) {
        wait(intervals[i]);  // Wait interval
        engram_recall(...);  // Recall (strengthens)
        engram_consolidate_update(engrams, intervals[i], false);
    }

    // Measure final strength
    float strength = engram_get_consolidation_strength(engrams, id);

    engram_system_destroy(engrams);
    return strength;
}

// Find optimal schedule
// Result: Expanding intervals (1h, 4h, 1d, 1w) outperform massed practice
```

### 4. **False Memory Creation**

**Research Question:** How do engrams support false memory formation?

**Protocol:**
```c
// DRM paradigm (Deese-Roediger-McDermott)
uint64_t ids[20];

// 1. Encode related words (e.g., bed, rest, awake, tired...)
for (int i = 0; i < 20; i++) {
    ids[i] = engram_encode(engrams, related_words[i], ...);
}

// 2. Consolidate
engram_consolidate_update(engrams, 3600.0f * 6.0f, false);

// 3. Test recall of non-presented lure word ("sleep")
bool falsely_recalled = engram_recognize(engrams, lure_word, count, &confidence);

// Expected: High false recognition due to semantic similarity
```

---

## Testing Strategy

### Unit Tests

**Encoding Tests:**
```cpp
TEST(EngramTest, BasicEncoding) {
    engram_system_t* sys = engram_system_create();

    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_JOY, 0.7f};

    uint64_t id = engram_encode(sys, neurons, activations, 5,
                                MEMORY_TYPE_EPISODIC, emotion);

    ASSERT_NE(id, 0);
    EXPECT_EQ(sys->active_count, 1);

    memory_engram_t* engram = engram_get_by_id(sys, id);
    ASSERT_NE(engram, nullptr);
    EXPECT_EQ(engram->neuron_count, 5);
    EXPECT_EQ(engram->state, ENGRAM_STATE_ENCODING);

    engram_system_destroy(sys);
}
```

**Consolidation Tests:**
```cpp
TEST(EngramTest, SynapticConsolidation) {
    engram_system_t* sys = engram_system_create();
    uint64_t id = engram_encode(sys, neurons, activations, 5, ...);

    // Simulate 6 hours (synaptic consolidation)
    for (int i = 0; i < 6 * 3600; i++) {
        engram_consolidate_update(sys, 1.0f, false);
    }

    memory_engram_t* engram = engram_get_by_id(sys, id);
    EXPECT_NEAR(engram->consolidation_strength, 1.0f, 0.1f);
    EXPECT_EQ(engram->state, ENGRAM_STATE_CONSOLIDATED);

    engram_system_destroy(sys);
}
```

**Recall Tests:**
```cpp
TEST(EngramTest, PatternCompletion) {
    engram_system_t* sys = engram_system_create();

    // Encode full pattern (neurons 1-10)
    uint32_t full[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint64_t id = engram_encode(sys, full, activations, 10, ...);
    engram_consolidate_update(sys, 7200.0f, false);  // Consolidate

    // Recall from partial cue (neurons 1-4 only, 40%)
    uint32_t cue[] = {1, 2, 3, 4};
    uint32_t output[100];
    uint64_t recalled_id = engram_recall(sys, cue, 4, output, 100);

    EXPECT_EQ(recalled_id, id);  // Should complete pattern

    engram_system_destroy(sys);
}
```

**Reconsolidation Tests:**
```cpp
TEST(EngramTest, ReconsolidationWindow) {
    engram_system_t* sys = engram_system_create();
    uint64_t id = engram_encode(sys, neurons, activations, 10, ...);

    // Consolidate
    engram_consolidate_update(sys, 7200.0f, false);
    memory_engram_t* engram = engram_get_by_id(sys, id);
    EXPECT_EQ(engram->state, ENGRAM_STATE_CONSOLIDATED);

    // Recall triggers reconsolidation
    engram_recall(sys, neurons, 5, output, 100);
    EXPECT_TRUE(engram->is_reconsolidating);
    EXPECT_EQ(engram->state, ENGRAM_STATE_RECONSOLIDATING);

    // Window lasts ~6 hours
    engram_consolidate_update(sys, 7200.0f, false);
    EXPECT_EQ(engram->state, ENGRAM_STATE_CONSOLIDATED);
    EXPECT_FALSE(engram->is_reconsolidating);

    engram_system_destroy(sys);
}
```

### Integration Tests

**Sleep Integration:**
```cpp
TEST(EngramIntegrationTest, SleepReplay) {
    engram_system_t* engrams = engram_system_create();
    sleep_system_t* sleep = sleep_system_create();

    // Encode memories before sleep
    uint64_t id1 = engram_encode(engrams, neurons1, ...);
    uint64_t id2 = engram_encode(engrams, neurons2, ...);

    // Enter sleep
    sleep_transition_to(sleep, SLEEP_STATE_SWS);

    // Simulate sleep cycle with replay
    for (int i = 0; i < 90 * 60; i++) {  // 90 min sleep cycle
        sleep_update(sleep, 1.0f);
        if (sleep->current_state == SLEEP_STATE_SWS) {
            engram_sleep_replay(engrams, 10);  // 10 replays/sec
        }
        engram_consolidate_update(engrams, 1.0f, true);
    }

    // Check consolidation improved
    memory_engram_t* e1 = engram_get_by_id(engrams, id1);
    EXPECT_GT(e1->consolidation_strength, 0.8f);
    EXPECT_GT(e1->reactivation_count, 0);  // Was replayed

    sleep_system_destroy(sleep);
    engram_system_destroy(engrams);
}
```

### Regression Tests

**Backward Compatibility:**
```cpp
TEST(EngramRegressionTest, BackwardCompatibility) {
    // Ensure engram system doesn't break existing functionality
    brain_t* brain = brain_create_custom(...);

    // Pre-engram functionality still works
    brain_spike(brain, 0);
    brain_update(brain);
    brain_learn(brain, input, target, count);

    // New engram functionality works
    engram_system_t* engrams = brain_get_engram_system(brain);
    ASSERT_NE(engrams, nullptr);

    uint64_t id = engram_encode(engrams, ...);
    EXPECT_NE(id, 0);

    brain_destroy(brain);
}
```

---

## Performance Considerations

### Memory Usage

**Per Engram:**
```
Base struct: ~200 bytes
Neurons (512 max): 512 * 4 = 2KB
Activations (512 max): 512 * 4 = 2KB
Synapses (variable): ~N synapses * 32 bytes
Metadata: ~100 bytes

Total per engram: ~4-5 KB (without synapses)
With 1000 synapses: ~35 KB per engram
```

**System Total:**
```
1024 engrams * 35 KB = ~35 MB (reasonable)
```

### Computational Complexity

**Encoding:** O(N) where N = number of active neurons
**Recall (pattern completion):** O(M * N) where M = number of engrams, N = neurons per engram
**Consolidation update:** O(M) where M = active engrams
**Forgetting/decay:** O(M)

**Optimizations:**
1. **Spatial indexing:** Hash table for content-based lookup
2. **Lazy consolidation:** Only update engrams that need it
3. **Pruning:** Remove fully decayed engrams
4. **Sparse storage:** Only store active synapses

### Scalability

**Target Performance:**
- Encode 1000 engrams/sec
- Recall <10ms per query
- Consolidation update <1ms for 1000 engrams
- Memory <100 MB for 2000 engrams

---

## Future Enhancements

### Phase M7+: Advanced Features

**Schema Theory:**
- Abstract schemas extracted from multiple episodes
- Semantic memory from episodic instances

**Temporal Context Model:**
- Time cells and temporal gradients
- Mental time travel (past/future simulation)

**Metacognition:**
- Confidence monitoring
- Feeling-of-knowing judgments
- Metamemory accuracy

**Neural Mechanisms:**
- Sharp-wave ripples during replay
- Theta phase precession
- Place cells and grid cells for spatial engrams

---

## References

**Key Papers:**

1. **Tonegawa, S., et al. (2015).** "Memory engram storage and retrieval." *Current Opinion in Neurobiology*, 35, 101-109.
2. **Liu, X., et al. (2012).** "Optogenetic stimulation of a hippocampal engram activates fear memory recall." *Nature*, 484, 381-385.
3. **Nader, K., et al. (2000).** "Fear memories require protein synthesis in the amygdala for reconsolidation after retrieval." *Nature*, 406, 722-726.
4. **Josselyn, S. A., & Tonegawa, S. (2020).** "Memory engrams: Recalling the past and imagining the future." *Science*, 367, 6473.
5. **Ryan, T. J., et al. (2015).** "Engram cells retain memory under retrograde amnesia." *Science*, 348, 1007-1013.
6. **McClelland, J. L., et al. (1995).** "Why there are complementary learning systems in the hippocampus and neocortex." *Psychological Review*, 102, 419-457.

---

## Conclusion

Memory engrams provide NIMCP with:
- ✅ **Biological realism** - proven neuroscience mechanism
- ✅ **Explicit memory traces** - trackable, modifiable memory objects
- ✅ **Research applications** - PTSD, Alzheimer's, learning optimization
- ✅ **Clinical relevance** - therapeutic interventions via reconsolidation
- ✅ **Integration ready** - synergizes with existing sleep, emotion, consolidation systems

**Next Steps:**
1. Review and approve specification
2. Begin Phase M1 implementation (core engram system)
3. Integrate with existing NIMCP systems
4. Validate against neuroscience literature
5. Publish results

**Estimated Timeline:** 12 weeks for full implementation (Phases M1-M6)

---

**Document Status:** ✅ Complete - Ready for Implementation

**Author:** NIMCP Development Team
**Reviewers:** TBD
**Version:** 1.0
**Last Updated:** 2025-11-13
