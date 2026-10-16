//=============================================================================
// nimcp_information_forager.h - Autonomous Curiosity-Driven Learning
//=============================================================================

/**
 * @file nimcp_information_forager.h
 * @brief Autonomous information foraging: brain-driven knowledge acquisition
 *
 * WHAT: Tick-based state machine that identifies knowledge gaps, ranks them
 *       by expected information gain, generates queries, and drives its own
 *       data acquisition through a registered callback.
 *
 * WHY:  Passive training pipelines push data to the brain. The forager lets
 *       the brain PULL data it actually needs — making learning self-directed,
 *       efficient, and biologically grounded.
 *
 * HOW:  Composes five existing NIMCP subsystems into an autonomous loop:
 *       - Curiosity engine: knowledge gap detection, question generation
 *       - Salience evaluator: novelty/surprise scoring
 *       - Ensemble uncertainty: epistemic uncertainty quantification
 *       - Hypothalamus drives: curiosity drive level + satisfaction
 *       - Epistemic filter: data quality gating
 *
 * STATE MACHINE:
 *   IDLE ──► SEEKING ──► EVALUATING ──► LEARNING ──► CONSOLIDATING ──► IDLE
 *     ▲                       │                            │
 *     └───────────────────────┘ (no viable targets)        │
 *     └────────────────────────────────────────────────────┘
 *
 * BIOLOGICAL BASIS:
 * - IDLE: Default mode network, low dopamine tonic activity
 * - SEEKING: Orienting response, norepinephrine spike (Aston-Jones & Cohen)
 * - EVALUATING: Prefrontal gating, cost-benefit analysis (Shenhav et al. 2013)
 * - LEARNING: Hippocampal encoding, phasic dopamine reward (Schultz 1998)
 * - CONSOLIDATING: Sleep-like consolidation window (Walker & Stickgold 2004)
 *
 * EXAMPLE:
 * @code
 *   // Create forager connected to brain's curiosity + salience systems
 *   forager_config_t cfg = forager_default_config();
 *   information_forager_t f = forager_create(brain, curiosity, salience, &cfg);
 *
 *   // Register external data source (Python HTTP callback)
 *   forager_register_data_callback(f, my_fetch_fn, my_ctx);
 *
 *   // Optional: connect deeper subsystems
 *   forager_connect_ensemble(f, ensemble);
 *   forager_connect_epistemic_filter(f, filter);
 *   forager_connect_drives(f, drives);
 *
 *   // Tick from training loop (~100ms intervals)
 *   while (training) {
 *       forager_tick(f, 100);
 *       // ... other training work ...
 *   }
 *
 *   // Inspect what the brain wants to learn
 *   forager_target_t targets[5];
 *   int n = forager_get_top_targets(f, targets, 5);
 *   for (int i = 0; i < n; i++)
 *       printf("Want to learn: %s (IG=%.3f)\n", targets[i].topic, targets[i].expected_ig);
 *
 *   forager_destroy(f);
 * @endcode
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#ifndef NIMCP_INFORMATION_FORAGER_H
#define NIMCP_INFORMATION_FORAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/salience/nimcp_salience.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define FORAGER_MAX_QUEUE_DEPTH          64
#define FORAGER_MAX_TOPIC_LEN           256
#define FORAGER_MAX_QUERY_LEN           512
#define FORAGER_MAX_SOURCE_HINT_LEN     128
#define FORAGER_TARGET_DECAY_RATE       0.995f
#define FORAGER_DEFAULT_EXPLORATION      0.3f
#define FORAGER_MIN_IG_THRESHOLD         0.05f
#define FORAGER_QUALITY_THRESHOLD        0.4f
#define FORAGER_MAX_ATTEMPTS              5
#define FORAGER_CONSOLIDATION_TICKS      10
#define FORAGER_CURIOSITY_DRIVE_THRESHOLD 0.4f
#define FORAGER_SALIENCE_MIN_THRESHOLD    0.2f

//=============================================================================
// Forward Declarations (avoid circular includes)
//=============================================================================

/* Optional subsystem connections — headers not required */
struct ensemble_context_struct;
struct epistemic_filter_struct;
struct hypo_drive_system;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque information forager handle
 */
typedef struct information_forager_struct* information_forager_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Foraging loop state machine
 *
 * BIOLOGICAL BASIS:
 * - IDLE: Default mode network, low dopamine tonic activity
 * - SEEKING: Orienting response, norepinephrine spike
 * - EVALUATING: Prefrontal gating, salience evaluation
 * - LEARNING: Hippocampal encoding, dopamine reward signal
 * - CONSOLIDATING: Sleep-like memory consolidation window
 * - PAUSED: External pause (training recess, etc.)
 */
typedef enum {
    FORAGER_STATE_IDLE = 0,
    FORAGER_STATE_SEEKING,
    FORAGER_STATE_EVALUATING,
    FORAGER_STATE_LEARNING,
    FORAGER_STATE_CONSOLIDATING,
    FORAGER_STATE_PAUSED
} forager_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single foraging target in the priority queue
 *
 * Represents one topic the brain wants to learn about, ranked by expected
 * information gain. Targets age out over time if not pursued.
 */
typedef struct {
    uint32_t target_id;                                 /**< Unique monotonic ID */
    char topic[FORAGER_MAX_TOPIC_LEN];                  /**< Topic string */
    float expected_ig;                                   /**< Expected information gain [0-1] */
    float curiosity_intensity;                           /**< From knowledge_gap_t */
    float epistemic_uncertainty;                         /**< From ensemble system or heuristic */
    float familiarity;                                   /**< 1 - gap_size */
    float prerequisite_satisfaction;                      /**< Readiness [0-1] */
    float acquisition_cost_estimate;                     /**< Estimated effort [0-1] */
    char query[FORAGER_MAX_QUERY_LEN];                  /**< Generated search query */
    char source_hint[FORAGER_MAX_SOURCE_HINT_LEN];      /**< Hint for data callback */
    uint32_t attempts;                                   /**< Fetch attempts so far */
    uint64_t created_tick;                               /**< Tick when target was created */
    float age_decay;                                     /**< Cumulative decay factor */
    bool active;                                         /**< Slot in use */
} forager_target_t;

/**
 * @brief Data acquisition callback
 *
 * WHAT: Called by forager when it needs external data for a target
 * WHY:  C module stays network-agnostic; external code does HTTP/IO
 * HOW:  Receives query + source_hint, writes result text to output params
 *
 * The callback is synchronous. For async architectures, use
 * forager_feed_result() instead.
 *
 * @param query        Search query string (NUL-terminated)
 * @param source_hint  Hint for data source (e.g. "wikipedia", "arxiv")
 * @param user_data    Context from registration
 * @param result_text  Output: heap-allocated text (caller frees with free())
 * @param result_len   Output: byte length of result_text
 * @return 0 on success, -1 on failure (no data available)
 */
typedef int (*forager_data_callback_t)(
    const char* query,
    const char* source_hint,
    void* user_data,
    char** result_text,
    size_t* result_len
);

/**
 * @brief Forager configuration
 */
typedef struct {
    uint32_t max_queue_depth;             /**< Max targets in queue (default 64) */
    uint32_t top_n_gaps;                  /**< Knowledge gaps to detect per seek (default 5) */
    float exploration_rate;               /**< Epsilon for explore/exploit (default 0.3) */
    float ig_threshold;                   /**< Min IG to enqueue a target (default 0.05) */
    float quality_threshold;              /**< Min quality to learn from data (default 0.4) */
    float target_decay_rate;              /**< Per-tick aging factor (default 0.995) */
    float curiosity_boost_factor;         /**< LR boost multiplier (default 0.4) */
    uint32_t max_attempts;                /**< Max fetch retries per target (default 5) */
    uint32_t consolidation_ticks;         /**< Ticks in consolidation state (default 10) */
    uint32_t seek_interval_ticks;         /**< Min ticks between SEEKING phases (default 50) */
    bool enable_prerequisite_check;       /**< Check prerequisites before pursuing (default true) */
    bool enable_drive_integration;        /**< Integrate with hypothalamus drives (default true) */
} forager_config_t;

/**
 * @brief Forager statistics
 */
typedef struct {
    /* Counters */
    uint64_t total_ticks;
    uint64_t targets_created;
    uint64_t targets_completed;
    uint64_t targets_expired;              /**< Aged out of queue */
    uint64_t targets_failed;               /**< Exceeded max_attempts */
    uint64_t data_callbacks_made;
    uint64_t learn_events;
    uint64_t quality_rejections;           /**< Failed epistemic filter */

    /* Running averages */
    float avg_expected_ig;
    float avg_realized_ig;
    float ig_prediction_error;             /**< |expected - realized| EMA */
    float avg_queue_depth;

    /* Current state */
    forager_state_t current_state;
    uint32_t active_targets;
    uint64_t ticks_in_current_state;
} forager_stats_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default forager configuration
 *
 * @return Config with sensible defaults
 */
NIMCP_EXPORT forager_config_t forager_default_config(void);

/**
 * @brief Create an information forager
 *
 * WHAT: Allocate and initialize the foraging state machine
 * WHY:  Brain needs a coordinator to drive self-directed learning
 * HOW:  Wire together curiosity + salience into a tick-driven loop
 *
 * @param brain     Parent brain (non-NULL, not owned)
 * @param curiosity Curiosity engine (non-NULL, not owned)
 * @param salience  Salience evaluator (non-NULL, not owned)
 * @param config    Configuration (NULL for defaults)
 * @return Forager handle, or NULL on error
 */
NIMCP_EXPORT information_forager_t forager_create(
    brain_t brain,
    curiosity_engine_t curiosity,
    salience_evaluator_t salience,
    const forager_config_t* config
);

/**
 * @brief Destroy information forager
 *
 * @param forager Forager to destroy (NULL-safe)
 */
NIMCP_EXPORT void forager_destroy(information_forager_t forager);

//=============================================================================
// Main Loop
//=============================================================================

/**
 * @brief Advance foraging by one tick
 *
 * WHAT: Run one iteration of the foraging state machine
 * WHY:  Non-blocking, integrates into existing training loops
 * HOW:  State-dependent actions (see state machine diagram in file header)
 *
 * Call this at regular intervals (~100ms) from the training loop.
 * The forager will autonomously detect gaps, seek data, and learn.
 *
 * @param forager  Forager handle
 * @param delta_ms Milliseconds since last tick (for timing)
 * @return 1 if actively working, 0 if idle, -1 on error
 */
NIMCP_EXPORT int forager_tick(information_forager_t forager, uint64_t delta_ms);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register data acquisition callback
 *
 * WHAT: Set the function called when the forager needs external data
 * WHY:  C module stays network-agnostic; Python/JS does the actual HTTP
 * HOW:  Store callback pointer, invoke during EVALUATING state
 *
 * @param forager   Forager handle
 * @param callback  Data fetch function (NULL to unregister)
 * @param user_data Context passed to callback
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_register_data_callback(
    information_forager_t forager,
    forager_data_callback_t callback,
    void* user_data
);

//=============================================================================
// Target Inspection
//=============================================================================

/**
 * @brief Get top foraging targets by priority
 *
 * WHAT: Peek at what the brain wants to learn next
 * WHY:  External systems can inspect/override foraging priorities
 * HOW:  Copy top-N targets sorted by effective priority
 *
 * @param forager      Forager handle
 * @param out_targets  Output array (caller-allocated, at least max_count)
 * @param max_count    Maximum targets to return
 * @return Number of targets copied (0 to max_count), -1 on error
 */
NIMCP_EXPORT int forager_get_top_targets(
    information_forager_t forager,
    forager_target_t* out_targets,
    uint32_t max_count
);

//=============================================================================
// Manual Data Feed (Async Alternative to Callback)
//=============================================================================

/**
 * @brief Feed data for a specific target
 *
 * WHAT: Provide data for a queued target without using the callback
 * WHY:  Supports async architectures where data arrives asynchronously
 * HOW:  Validates quality, applies curiosity-boosted learning, updates gaps
 *
 * @param forager       Forager handle
 * @param target_id     ID of target (from forager_get_top_targets)
 * @param text          Text data to learn from
 * @param text_len      Length of text
 * @param quality_score External quality estimate [0-1] (0.5 if unknown)
 * @return 0 on success (learned), 1 on rejection (quality), -1 on error
 */
NIMCP_EXPORT int forager_feed_result(
    information_forager_t forager,
    uint32_t target_id,
    const char* text,
    size_t text_len,
    float quality_score
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get forager statistics
 *
 * @param forager Forager handle
 * @return Statistics snapshot (zeroed on error)
 */
NIMCP_EXPORT forager_stats_t forager_get_stats(information_forager_t forager);

//=============================================================================
// Control
//=============================================================================

/**
 * @brief Set exploration rate
 *
 * WHAT: Adjust explore vs exploit balance
 * WHY:  Higher exploration = more novel topics; lower = deepen existing
 * HOW:  Epsilon-greedy: with probability epsilon, pursue random gap
 *
 * @param forager Forager handle
 * @param rate    Exploration rate [0-1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_set_exploration_rate(information_forager_t forager, float rate);

/**
 * @brief Pause foraging
 *
 * Enters PAUSED state. Tick becomes a no-op. Queue preserved.
 *
 * @param forager Forager handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_pause(information_forager_t forager);

/**
 * @brief Resume foraging
 *
 * Returns to IDLE state. Next tick resumes normal operation.
 *
 * @param forager Forager handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_resume(information_forager_t forager);

//=============================================================================
// Optional Subsystem Connections
//=============================================================================

/**
 * @brief Connect ensemble uncertainty system
 *
 * WHAT: Enable real epistemic uncertainty instead of heuristic fallback
 * WHY:  Better IG estimation → more targeted learning
 *
 * @param forager  Forager handle
 * @param ensemble Ensemble context (not owned)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_connect_ensemble(
    information_forager_t forager,
    struct ensemble_context_struct* ensemble
);

/**
 * @brief Connect epistemic filter
 *
 * WHAT: Enable quality gating on incoming data
 * WHY:  Prevent learning from biased/unreliable sources
 *
 * @param forager Forager handle
 * @param filter  Epistemic filter (not owned)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_connect_epistemic_filter(
    information_forager_t forager,
    struct epistemic_filter_struct* filter
);

/**
 * @brief Connect hypothalamus drive system
 *
 * WHAT: Enable drive-level curiosity integration
 * WHY:  Curiosity drive rises with idle time, satisfied by learning
 *
 * @param forager Forager handle
 * @param drives  Drive system handle (not owned)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int forager_connect_drives(
    information_forager_t forager,
    struct hypo_drive_system* drives
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFORMATION_FORAGER_H */
