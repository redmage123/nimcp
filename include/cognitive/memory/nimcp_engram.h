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
 * NIMCP STANDARDS:
 * - Guard clauses (no nested ifs)
 * - WHAT-WHY-HOW documentation
 * - Functions < 50 lines
 * - Single Responsibility Principle
 * - 100% test coverage required
 *
 * @version Phase M1: Memory Engrams - Core System
 * @date 2025-11-13
 */

#ifndef NIMCP_ENGRAM_H
#define NIMCP_ENGRAM_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum neurons per engram (distributed trace) */
#define ENGRAM_MAX_NEURONS 256

/* Default soft cap on simultaneously-active engrams. The engram_system
 * grows the engrams[] array dynamically up to this limit; once
 * active_count crosses the cap, find_free_slot starts evicting the
 * weakest existing engram (preferring DEGRADING, then oldest LABILE)
 * before encoding a new one. This is a runtime-tunable default — set
 * via engram_system_set_max_active() per-system. The constant is the
 * compiled-in default for systems that don't override it. */
#define ENGRAM_MAX_COUNT 524288u  /* 512K — matches the eviction-policy default */

/* Consolidation time constants (seconds) */
#define ENGRAM_SYNAPTIC_CONSOLIDATION_TIME (3600.0f * 6.0f)    /* 6 hours */
#define ENGRAM_SYSTEMS_CONSOLIDATION_TIME (3600.0f * 24.0f * 30.0f)  /* 30 days */

/* Tagging constants (c-fos/Arc expression) */
#define ENGRAM_TAG_DURATION (3600.0f * 4.0f)  /* 4 hours for IEG expression */
#define ENGRAM_TAG_WINDOW (3600.0f * 6.0f)    /* 6 hour tagging window */

/* Reactivation thresholds */
#define ENGRAM_RECALL_THRESHOLD 0.4f          /* Min activation for recall */
#define ENGRAM_RECOGNITION_THRESHOLD 0.6f     /* Min for strong recognition */

/* Decay and interference */
#define ENGRAM_BASE_DECAY_RATE 0.001f         /* Default forgetting rate */
#define ENGRAM_RECONSOLIDATION_WINDOW (3600.0f * 6.0f)  /* 6 hour labile period */

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
 * @brief Individual memory engram
 *
 * WHAT: Distributed pattern of neurons encoding a memory
 * WHY:  Explicit representation enables targeted memory operations
 * HOW:  Track neurons, consolidation state, reactivation
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

    // Reconsolidation
    bool is_reconsolidating;        /**< Temporarily labile? */
    uint64_t reconsolidation_start_us; /**< When recall made it labile */

    // Statistics
    float recall_latency_ms;        /**< Time to retrieve */
    uint32_t successful_recalls;    /**< Count of successful retrievals */

    // Recall acceleration (added with the inverted-index + bloom rework).
    // 256-bit Bloom filter summarising which neuron_ids are members of
    // this engram. k=4 xorshift hash functions per id; build cost is
    // O(neuron_count) at encode, query cost is O(k) per cue neuron and
    // never produces false negatives. Used as a fast skip-test inside
    // engram_recall after the inverted index has narrowed the candidate
    // set, so the expensive O(M*C) calculate_overlap call is only done
    // on survivors. Zero when bloom_built is false (legacy / degraded
    // engram path falls back to direct overlap calc).
    uint64_t bloom[4];
    bool bloom_built;

} memory_engram_t;

/**
 * @brief Complete engram memory system
 */
typedef struct {
    // Engram storage
    memory_engram_t* engrams;       /**< Dynamic array of engrams */
    uint32_t active_count;          /**< How many engrams active */
    uint32_t capacity;              /**< Current array capacity */
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
    bool use_interference;          /**< Enable interference effects */

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

    // Phase 1.5: Memory pool for hot-path engram allocations
    void* engram_pool;              /**< Pool for memory_engram_t structs */

    // Unified memory integration (CoW support for brain cloning)
    unified_mem_manager_t mem_manager;  /**< Unified memory manager */
    unified_mem_handle_t engrams_handle; /**< CoW handle for engram array */

    // Bio-async integration
    void* bio_ctx;                  /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // Recall acceleration: inverted index neuron_id -> list of engram
    // array-indices that contain that neuron. Replaces the linear scan
    // over `capacity` in engram_recall with a sublinear candidate-
    // selection step. Maintained incrementally:
    //   - engram_encode appends the new engram's array-index to each of
    //     its 256 neuron posting lists.
    //   - extinction / decay-to-DEGRADING removes the index from each
    //     posting list it appears in.
    // Opaque pointer here so the public header doesn't expose the hash-
    // table implementation; full type lives in nimcp_engram.c.
    void* inverted_index;

    // Soft cap on active engrams. find_free_slot evicts the weakest
    // existing engram (DEGRADING > oldest LABILE > weakest CONSOLIDATING)
    // before allocating a new one once active_count >= max_active_engrams.
    // expand_engram_array refuses to grow the heap array past this cap.
    // 0 means "unbounded" (legacy behaviour). Default at create-time:
    // ENGRAM_MAX_COUNT (524288). Tunable via engram_system_set_max_active.
    uint32_t max_active_engrams;
    uint64_t total_evictions;       /**< lifetime evictions performed */

} engram_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Create engram system
 *
 * WHAT: Allocate and initialize memory engram system
 * WHY:  Required for explicit memory trace tracking
 * HOW:  Allocate dynamic array, set defaults
 *
 * @return Engram system pointer (NULL on failure)
 */
engram_system_t* engram_system_create(void);

/**
 * @brief Destroy engram system
 *
 * WHAT: Free all engram system resources
 * WHY:  Prevent memory leaks
 * HOW:  Free dynamic arrays, then system struct
 *
 * @param system Engram system to destroy
 */
void engram_system_destroy(engram_system_t* system);

/**
 * @brief Reset engram system
 *
 * WHAT: Clear all engrams, reset to initial state
 * WHY:  Enable fresh start while preserving configuration
 * HOW:  Zero engram array, reset counters
 *
 * @param system Engram system to reset
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
 * HOW:  Tag active neurons, record activations, apply emotion
 *
 * @param system Engram system
 * @param neuron_ids Array of active neuron IDs
 * @param activations Array of neuron activations [0-1]
 * @param count Number of neurons (max ENGRAM_MAX_NEURONS)
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
 * @param activation_out Output: reactivated neurons (allocated by caller)
 * @param activations_out Output: activation levels
 * @param max_activation_count Max output neurons
 * @param confidence_out Output: recall confidence [0-1]
 * @return Engram ID of recalled memory (0 if no match)
 */
uint64_t engram_recall(
    engram_system_t* system,
    const uint32_t* cue_neurons,
    uint32_t cue_count,
    uint32_t* activation_out,
    float* activations_out,
    uint32_t max_activation_count,
    float* confidence_out);

/**
 * @brief Recognition test (familiarity)
 *
 * WHAT: Test if pattern has been seen before
 * WHY:  Recognition vs recall distinction
 * HOW:  Match against engrams without full reactivation
 *
 * @param system Engram system
 * @param pattern Pattern to recognize
 * @param count Pattern length
 * @param familiarity_out Output: familiarity strength [0-1]
 * @return True if recognized (above threshold)
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
 * @brief Sleep-dependent replay
 *
 * WHAT: Reactivate engrams during sleep for consolidation
 * WHY:  Sleep strengthens memories (Wilson & McNaughton, 1994)
 * HOW:  Replay sequences, strengthen synapses
 *
 * @param system Engram system
 * @param replay_count Number of replays to perform
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
 *
 * @param system Engram system
 * @param engram_id Engram to reconsolidate
 */
void engram_trigger_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Block reconsolidation (therapeutic)
 *
 * WHAT: Prevent engram from restabilizing
 * WHY:  PTSD treatment, maladaptive memory weakening
 * HOW:  Simulate protein synthesis inhibitor
 *
 * @param system Engram system
 * @param engram_id Engram to block
 * @return True if successfully blocked
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
 * HOW:  Time-dependent strength decay
 *
 * @param system Engram system
 * @param dt Time step (seconds)
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
 *
 * @param system Engram system
 * @param engram_id Engram to extinguish
 * @param extinction_strength Strength of extinction [0-1]
 */
void engram_extinction(
    engram_system_t* system,
    uint64_t engram_id,
    float extinction_strength);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Get engram by ID
 *
 * WHAT: Retrieve engram pointer from ID
 * WHY:  Access engram for inspection/modification
 * HOW:  Linear search through active engrams
 *
 * @param system Engram system
 * @param engram_id Engram ID to find
 * @return Engram pointer (NULL if not found)
 */
memory_engram_t* engram_get_by_id(
    engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get consolidation state
 *
 * @param system Engram system
 * @param engram_id Engram ID
 * @return Consolidation state
 */
engram_state_t engram_get_state(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get consolidation strength
 *
 * @param system Engram system
 * @param engram_id Engram ID
 * @return Consolidation strength [0-1]
 */
float engram_get_consolidation_strength(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Check if engram is reconsolidating
 *
 * @param system Engram system
 * @param engram_id Engram ID
 * @return True if currently in reconsolidation window
 */
bool engram_is_reconsolidating(
    const engram_system_t* system,
    uint64_t engram_id);

/**
 * @brief Get memory age
 *
 * @param system Engram system
 * @param engram_id Engram ID
 * @param current_time_us Current time in microseconds
 * @return Age in seconds
 */
float engram_get_age_seconds(
    const engram_system_t* system,
    uint64_t engram_id,
    uint64_t current_time_us);

/**
 * @brief Get active engram count
 *
 * @param system Engram system
 * @return Number of active engrams
 */
uint32_t engram_get_active_count(const engram_system_t* system);

/**
 * @brief Set the soft cap on active engrams.
 *
 * WHAT: Update system->max_active_engrams. Once active_count >= cap,
 *       find_free_slot evicts the weakest existing engram (DEGRADING
 *       first, then oldest LABILE) before allocating a new slot.
 * WHY:  The engram pool grows unbounded by default. At brain-scale
 *       comprehend / brain_decide rates a soft cap prevents memory
 *       runaway and keeps the inverted index lookup fast.
 * HOW:  Pure setter; no eviction triggered by the call itself. Pass
 *       0 to disable the cap (legacy unbounded-grow behaviour).
 *
 * @param system Engram system
 * @param cap    Maximum simultaneously-active engrams; 0 disables.
 */
void engram_system_set_max_active(engram_system_t* system, uint32_t cap);

/**
 * @brief Get total evictions performed since system creation / reset.
 *
 * Eviction count is bumped each time find_free_slot drops an existing
 * engram to make room for a new one. Useful for spotting cap pressure.
 */
uint64_t engram_get_total_evictions(const engram_system_t* system);

/**
 * @brief Get engram statistics
 *
 * WHAT: Retrieve system-wide statistics
 * WHY:  Monitor engram system performance
 * HOW:  Return counters and averages
 *
 * @param system Engram system
 * @param total_encodings_out Output: lifetime encodings
 * @param total_recalls_out Output: lifetime recalls
 * @param active_count_out Output: current active engrams
 */
void engram_get_statistics(
    const engram_system_t* system,
    uint64_t* total_encodings_out,
    uint64_t* total_recalls_out,
    uint32_t* active_count_out);

//=============================================================================
// TERNARY ENGRAM WEIGHT API
//=============================================================================

#include "utils/ternary/nimcp_ternary_types.h"

/**
 * @brief Ternary engram weight states
 *
 * WHAT: Discrete engram weight levels
 * WHY:  Model discrete synaptic strength changes
 * HOW:  Map to LTD/BASELINE/LTP synaptic states
 *
 * BIOLOGICAL BASIS:
 * - Synaptic tags have discrete states (PRPs present/absent)
 * - Metaplasticity creates distinct weight bands
 * - All-or-none consolidation at cellular level
 */
typedef enum {
    ENGRAM_WEIGHT_DEPRESSED = -1,   /**< LTD state: weakened synapse */
    ENGRAM_WEIGHT_BASELINE = 0,     /**< Neutral: default synapse */
    ENGRAM_WEIGHT_POTENTIATED = 1   /**< LTP state: strengthened synapse */
} ternary_engram_weight_t;

/**
 * @brief Ternary consolidation states
 *
 * WHAT: Discrete consolidation levels
 * WHY:  Model consolidation as discrete transitions
 */
typedef enum {
    ENGRAM_CONSOL_LABILE = -1,      /**< Labile: vulnerable to disruption */
    ENGRAM_CONSOL_TRANSITIONING = 0,/**< Transitioning: partially stable */
    ENGRAM_CONSOL_STABLE = 1        /**< Stable: fully consolidated */
} ternary_consolidation_state_t;

/**
 * @brief Ternary engram configuration
 */
typedef struct {
    bool use_ternary_weights;       /**< Enable ternary weight mode */
    bool use_ternary_consolidation; /**< Enable ternary consolidation states */
    float potentiation_threshold;   /**< Threshold for potentiated state */
    float depression_threshold;     /**< Threshold for depressed state */
    float consolidation_threshold;  /**< Threshold for stable consolidation */
    float lability_threshold;       /**< Threshold for labile state */
    bool ternary_recall;            /**< Use ternary matching for recall */
    float ternary_recall_threshold; /**< Match threshold for ternary recall */
} ternary_engram_config_t;

/**
 * @brief Ternary engram statistics
 */
typedef struct {
    uint64_t n_potentiated;         /**< Count of potentiated weights */
    uint64_t n_baseline;            /**< Count of baseline weights */
    uint64_t n_depressed;           /**< Count of depressed weights */
    uint64_t n_stable;              /**< Count of stable engrams */
    uint64_t n_labile;              /**< Count of labile engrams */
    float avg_ternary_sparsity;     /**< Fraction of non-baseline weights */
    float avg_consolidation_rate;   /**< Rate of consolidation */
} ternary_engram_stats_t;

/**
 * @brief Get default ternary engram configuration
 *
 * DEFAULTS:
 * - use_ternary_weights: true
 * - potentiation_threshold: 0.7
 * - depression_threshold: 0.3
 * - consolidation_threshold: 0.8
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int ternary_engram_default_config(ternary_engram_config_t* config);

/**
 * @brief Enable ternary mode for engram system
 *
 * @param system Engram system
 * @param config Ternary configuration
 * @return 0 on success, negative on error
 */
int engram_enable_ternary(
    engram_system_t* system,
    const ternary_engram_config_t* config
);

/**
 * @brief Ternarize engram weights
 *
 * WHAT: Convert continuous engram weights to ternary
 * WHY:  Enable discrete weight representation
 * HOW:
 *   - activation > potentiation_threshold => POTENTIATED
 *   - activation < depression_threshold => DEPRESSED
 *   - otherwise => BASELINE
 *
 * BIOLOGICAL BASIS:
 * - Synaptic bistability in potentiation
 * - Tag & capture model has discrete states
 * - Binary engram allocation (allocated vs not)
 *
 * @param system Engram system
 * @param engram_id Engram to ternarize
 * @param config Ternary configuration
 * @param weights_out Output: ternary weights [ENGRAM_MAX_NEURONS]
 * @param n_weights_out Output: number of weights
 * @return 0 on success, negative on error
 */
int engram_ternarize_weights(
    engram_system_t* system,
    uint64_t engram_id,
    const ternary_engram_config_t* config,
    ternary_engram_weight_t* weights_out,
    uint32_t* n_weights_out
);

/**
 * @brief Apply ternary weight update to engram
 *
 * WHAT: Update engram weights based on ternary plasticity
 * WHY:  Discrete LTP/LTD updates
 * HOW:  Apply ternary delta to continuous weights
 *
 * @param system Engram system
 * @param engram_id Engram to update
 * @param weight_changes Array of ternary changes
 * @param n_changes Number of changes
 * @param step_size Magnitude of weight change
 * @return 0 on success, negative on error
 */
int engram_apply_ternary_update(
    engram_system_t* system,
    uint64_t engram_id,
    const ternary_engram_weight_t* weight_changes,
    uint32_t n_changes,
    float step_size
);

/**
 * @brief Get ternary consolidation state
 *
 * WHAT: Get discrete consolidation state of engram
 * WHY:  Binary decision: consolidated or not
 * HOW:  Threshold continuous consolidation_strength
 *
 * @param system Engram system
 * @param engram_id Engram ID
 * @param config Ternary configuration
 * @return Ternary consolidation state
 */
ternary_consolidation_state_t engram_get_ternary_consolidation(
    const engram_system_t* system,
    uint64_t engram_id,
    const ternary_engram_config_t* config
);

/**
 * @brief Ternary pattern matching for recall
 *
 * WHAT: Match cue pattern using ternary logic
 * WHY:  Discrete similarity matching
 * HOW:  Count matching ternary states
 *
 * MATCHING LOGIC:
 * - POTENTIATED matches POTENTIATED => +1
 * - DEPRESSED matches DEPRESSED => +1
 * - Mismatch => -1
 * - BASELINE acts as wildcard => 0
 *
 * @param system Engram system
 * @param cue_weights Ternary cue pattern
 * @param n_cue Number of cue weights
 * @param config Ternary configuration
 * @param best_match_out Output: best matching engram ID
 * @param match_score_out Output: match score [0-1]
 * @return 0 on success, negative on error
 */
int engram_ternary_recall(
    engram_system_t* system,
    const ternary_engram_weight_t* cue_weights,
    uint32_t n_cue,
    const ternary_engram_config_t* config,
    uint64_t* best_match_out,
    float* match_score_out
);

/**
 * @brief Compute ternary engram similarity
 *
 * WHAT: Similarity between two ternary engram patterns
 * WHY:  Pattern separation/completion with discrete weights
 * HOW:  Hamming-like distance with ternary logic
 *
 * @param weights_a First ternary weight pattern
 * @param weights_b Second ternary weight pattern
 * @param n_weights Number of weights
 * @return Similarity score [0-1]
 */
float engram_ternary_similarity(
    const ternary_engram_weight_t* weights_a,
    const ternary_engram_weight_t* weights_b,
    uint32_t n_weights
);

/**
 * @brief Get ternary engram statistics
 *
 * @param system Engram system
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int engram_get_ternary_stats(
    const engram_system_t* system,
    ternary_engram_stats_t* stats
);

/**
 * @brief Convert ternary weight to string name
 *
 * @param weight Ternary weight
 * @return String name
 */
static inline const char* ternary_engram_weight_name(ternary_engram_weight_t weight) {
    switch (weight) {
        case ENGRAM_WEIGHT_DEPRESSED:   return "DEPRESSED";
        case ENGRAM_WEIGHT_BASELINE:    return "BASELINE";
        case ENGRAM_WEIGHT_POTENTIATED: return "POTENTIATED";
        default:                        return "INVALID";
    }
}

/**
 * @brief Convert ternary consolidation to string name
 *
 * @param state Consolidation state
 * @return String name
 */
static inline const char* ternary_consolidation_name(ternary_consolidation_state_t state) {
    switch (state) {
        case ENGRAM_CONSOL_LABILE:       return "LABILE";
        case ENGRAM_CONSOL_TRANSITIONING:return "TRANSITIONING";
        case ENGRAM_CONSOL_STABLE:       return "STABLE";
        default:                         return "INVALID";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENGRAM_H */
