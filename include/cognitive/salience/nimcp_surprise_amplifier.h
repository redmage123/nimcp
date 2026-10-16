/**
 * @file nimcp_surprise_amplifier.h
 * @brief Surprise Amplification System for Society of Thought Reasoning
 * @version 1.0.0
 * @date 2026-01-26
 *
 * WHAT: Amplifies prediction errors and inter-agent conflicts into attention,
 *       curiosity, and executive re-evaluation signals
 * WHY:  Kim et al. (2026) showed that amplifying a surprise/realization feature
 *       nearly doubled reasoning accuracy (27.1% -> 54.8%). This module implements
 *       that mechanism for NIMCP's cognitive architecture.
 * HOW:  Monitors FEP prediction errors, inter-agent conflicts, and hypothesis
 *       invalidations. When surprise exceeds threshold, amplifies signal and
 *       routes to attention, curiosity, global workspace, and executive systems.
 *
 * THEORETICAL FOUNDATION:
 * ====================================================================================
 *
 * SURPRISE AS REASONING CATALYST (Kim et al., 2026):
 * ---------------------------------------------------
 * Sparse autoencoder analysis of DeepSeek-R1 identified Layer 15 Feature 30939
 * as a "surprise/realization" discourse marker. Artificially steering this feature
 * with strength +10 nearly doubled accuracy on arithmetic reasoning tasks.
 *
 * The surprise signal serves as a reasoning catalyst that:
 * 1. Forces re-examination of current beliefs
 * 2. Broadens attention to previously ignored evidence
 * 3. Triggers curiosity-driven exploration of alternatives
 * 4. Interrupts premature convergence on suboptimal conclusions
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Locus coeruleus: Norepinephrine release on unexpected events (global attention boost)
 * - Anterior cingulate cortex: Conflict monitoring triggers re-evaluation
 * - VTA / substantia nigra: Dopaminergic prediction error signals
 * - P300 ERP component: Electrophysiological marker of surprise/context updating
 * - Hippocampal mismatch detection: Novelty signals from CA1 comparator
 *
 * DESIGN PATTERNS:
 * - Observer: Subscribes to FEP prediction errors, agent conflicts
 * - Strategy: Different amplification profiles for different surprise sources
 * - Mediator: Routes amplified signals to attention, curiosity, GW, executive
 *
 * KG WIRING INTEGRATION:
 * ```
 * Surprise Amplifier Wiring
 * ─────────────────────────────────────────────────────────
 * Input:   BIO_MSG_SOCIETY_SURPRISE_SIGNAL (from FEP, agents)
 * Output:  BIO_MSG_SOCIETY_REALIZATION (to GW, attention, curiosity)
 * Output:  BIO_MSG_ATTENTION_SHIFT (to attention system)
 * Handler: BIO_MSG_SOCIETY_SURPRISE_SIGNAL (priority 100)
 * Handler: BIO_MSG_SOCIETY_CONFLICT_DETECTED (priority 150)
 * ```
 *
 * ERROR CODE RANGE: 28000-28099 (Module-specific)
 * BIO-ASYNC MODULE ID: 0x1E01
 *
 * REFERENCE:
 * Kim, J., Lai, S., Scherrer, N., Aguera y Arcas, B., Evans, J. (2026).
 * "Reasoning Models Generate Societies of Thought." arXiv:2601.10825
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_AMPLIFIER_H
#define NIMCP_SURPRISE_AMPLIFIER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 *
 * Use forward declarations for types only used as pointers in the API.
 * The implementation file includes the full headers.
 * ============================================================================ */

/** FEP system - anonymous typedef struct, use void* */
struct fep_system_fwd;

/** Salience evaluator (cognitive/salience/nimcp_salience.h) */
struct salience_evaluator_struct;

/** Global workspace (cognitive/global_workspace/nimcp_global_workspace.h) */
struct global_workspace_struct;

/** Curiosity engine (cognitive/curiosity/nimcp_curiosity.h) */
struct curiosity_engine_struct;

/** Executive controller (cognitive/executive) */
struct executive_controller;

/** Bio-async router (async/nimcp_bio_router.h) */
struct bio_router_struct;

/** Health agent (utils/fault_tolerance/nimcp_health_agent.h) */
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28000-28099)
 * ============================================================================ */

#define NIMCP_SURPRISE_ERROR_BASE               28000
#define NIMCP_SURPRISE_ERROR_NULL_POINTER       (NIMCP_SURPRISE_ERROR_BASE + 1)
#define NIMCP_SURPRISE_ERROR_INVALID_PARAM      (NIMCP_SURPRISE_ERROR_BASE + 2)
#define NIMCP_SURPRISE_ERROR_NO_MEMORY          (NIMCP_SURPRISE_ERROR_BASE + 3)
#define NIMCP_SURPRISE_ERROR_NOT_INITIALIZED    (NIMCP_SURPRISE_ERROR_BASE + 4)
#define NIMCP_SURPRISE_ERROR_REFRACTORY         (NIMCP_SURPRISE_ERROR_BASE + 5)
#define NIMCP_SURPRISE_ERROR_MAX_CONCURRENT     (NIMCP_SURPRISE_ERROR_BASE + 6)
#define NIMCP_SURPRISE_ERROR_BIO_ASYNC          (NIMCP_SURPRISE_ERROR_BASE + 10)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum concurrent surprise events tracked */
#define SURPRISE_MAX_CONCURRENT         8

/** Maximum surprise event history ring buffer size */
#define SURPRISE_HISTORY_SIZE           64

/** Default surprise threshold */
#define SURPRISE_DEFAULT_THRESHOLD      0.3f

/** Default amplification gain */
#define SURPRISE_DEFAULT_GAIN           2.0f

/** Default attention boost factor */
#define SURPRISE_DEFAULT_ATTENTION_BOOST 1.5f

/** Default curiosity boost factor */
#define SURPRISE_DEFAULT_CURIOSITY_BOOST 1.2f

/** Default decay rate per second */
#define SURPRISE_DEFAULT_DECAY_RATE     0.95f

/** Default refractory period in milliseconds */
#define SURPRISE_DEFAULT_REFRACTORY_MS  100

/* ============================================================================
 * Bio-Async Module ID and Message Types
 *
 * Canonical definitions are in async/nimcp_bio_messages.h:
 *   Module ID: BIO_MODULE_SURPRISE_AMPLIFIER (0x1E01)
 *   Messages (0x6E80-0x6E9F range):
 *     BIO_MSG_SOCIETY_SURPRISE_SIGNAL  (0x6E80) - Input: raw surprise signal
 *     BIO_MSG_SOCIETY_CONFLICT_DETECTED (0x6E82) - Input: inter-agent conflict
 *     BIO_MSG_SOCIETY_REALIZATION      (0x6E8C) - Output: amplified realization
 * ============================================================================ */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Surprise source types
 *
 * WHAT: Identifies what triggered a surprise signal
 * WHY:  Different sources have different amplification profiles
 * HOW:  Tagged on each surprise event for routing and statistics
 */
typedef enum {
    SURPRISE_SOURCE_FEP_PREDICTION_ERROR = 0,   /**< FEP prediction error exceeded threshold */
    SURPRISE_SOURCE_INTER_AGENT_CONFLICT = 1,   /**< Two reasoning agents contradicted */
    SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED = 2, /**< Evidence invalidated current hypothesis */
    SURPRISE_SOURCE_NOVELTY_DETECTION = 3,      /**< Novel pattern detected */
    SURPRISE_SOURCE_BAYESIAN_DIVERGENCE = 4,    /**< Large KL divergence prior/posterior */
    SURPRISE_SOURCE_COUNT = 5
} surprise_source_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief A single surprise event
 *
 * WHAT: Record of one surprise signal firing
 * WHY:  Carries information needed to route the surprise to downstream systems
 * HOW:  Created when surprise threshold is exceeded, consumed by connected systems
 */
typedef struct {
    surprise_source_t source;           /**< What caused the surprise */
    float magnitude;                    /**< Surprise intensity [0.0 - 1.0] */
    float raw_prediction_error;         /**< Raw prediction error value */
    float attention_boost;              /**< Computed attention boost amount */
    float curiosity_boost;              /**< Computed curiosity drive increase */
    uint64_t timestamp_ns;              /**< When the surprise occurred */
    uint32_t source_module_id;          /**< Bio-async module that produced signal */
    uint32_t conflicting_module_id;     /**< If inter-agent conflict, the other module */
} surprise_event_t;

/**
 * @brief Surprise amplifier configuration
 *
 * WHAT: Parameters controlling surprise detection and amplification
 * WHY:  Different cognitive states may need different sensitivity profiles
 * HOW:  Configurable thresholds, gains, and routing options
 */
typedef struct {
    float base_threshold;               /**< Minimum error to trigger [0.3] */
    float amplification_gain;           /**< Signal multiplier [2.0] */
    float attention_boost_factor;       /**< Attention boost on surprise [1.5] */
    float curiosity_boost_factor;       /**< Curiosity boost on surprise [1.2] */
    float decay_rate;                   /**< Per-second exponential decay [0.95] */
    float conflict_weight;              /**< Inter-agent conflict weight [1.5] */
    float novelty_weight;               /**< Novelty-based surprise weight [1.0] */
    float hypothesis_weight;            /**< Hypothesis invalidation weight [1.3] */
    float bayesian_weight;              /**< Bayesian divergence weight [1.1] */
    uint32_t refractory_period_ms;      /**< Min ms between events [100] */
    uint32_t max_concurrent;            /**< Max simultaneous signals [4] */
    bool enable_gw_broadcast;           /**< Broadcast to global workspace [true] */
    bool enable_executive_interrupt;    /**< Interrupt executive on high [true] */
    float executive_interrupt_threshold; /**< Threshold for interrupt [0.7] */
    bool enable_bio_async;              /**< Enable bio-async routing [true] */
    bool enable_logging;                /**< Enable diagnostic logging [true] */
} surprise_amplifier_config_t;

/**
 * @brief Surprise amplifier statistics
 *
 * WHAT: Accumulated metrics about surprise amplifier activity
 * WHY:  Monitoring, debugging, and diversity metrics for Society of Thought
 * HOW:  Counters and running averages updated on each event
 */
typedef struct {
    uint64_t total_surprises;           /**< Total surprise events fired */
    uint64_t fep_triggered;             /**< Surprises from FEP prediction errors */
    uint64_t conflict_triggered;        /**< Surprises from inter-agent conflicts */
    uint64_t hypothesis_triggered;      /**< Surprises from hypothesis invalidation */
    uint64_t novelty_triggered;         /**< Surprises from novelty detection */
    uint64_t bayesian_triggered;        /**< Surprises from Bayesian divergence */
    float avg_magnitude;                /**< Running average surprise magnitude */
    float max_magnitude;                /**< Peak surprise magnitude observed */
    float avg_attention_boost;          /**< Average attention boost delivered */
    float avg_curiosity_boost;          /**< Average curiosity boost delivered */
    uint64_t gw_broadcasts;             /**< Times broadcast to global workspace */
    uint64_t executive_interrupts;      /**< Times executive was interrupted */
    uint64_t refractory_suppressed;     /**< Events suppressed by refractory period */
    uint64_t total_updates;             /**< Total update ticks processed */
} surprise_amplifier_stats_t;

/**
 * @brief Opaque surprise amplifier system handle
 */
typedef struct surprise_amplifier surprise_amplifier_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration with sensible defaults */
surprise_amplifier_config_t surprise_amplifier_default_config(void);

/** @brief Create a surprise amplifier system (NULL config = defaults) */
surprise_amplifier_t* surprise_amplifier_create(
    const surprise_amplifier_config_t* config);

/** @brief Destroy a surprise amplifier system (NULL-safe) */
void surprise_amplifier_destroy(surprise_amplifier_t* amp);

/** @brief Reset state, preserving config and connections */
int surprise_amplifier_reset(surprise_amplifier_t* amp);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect FEP system for prediction error monitoring */
int surprise_amplifier_connect_fep(surprise_amplifier_t* amp, void* fep);

/** @brief Connect salience evaluator for novelty integration */
int surprise_amplifier_connect_salience(surprise_amplifier_t* amp,
                                        struct salience_evaluator_struct* salience);

/** @brief Connect global workspace for surprise broadcasting */
int surprise_amplifier_connect_gw(surprise_amplifier_t* amp,
                                   struct global_workspace_struct* gw);

/** @brief Connect curiosity engine for curiosity boost delivery */
int surprise_amplifier_connect_curiosity(surprise_amplifier_t* amp,
                                          struct curiosity_engine_struct* curiosity);

/** @brief Connect executive controller for interrupt delivery */
int surprise_amplifier_connect_executive(surprise_amplifier_t* amp,
                                          struct executive_controller* executive);

/** @brief Connect bio-async router for message-based integration */
int surprise_amplifier_connect_bio_async(surprise_amplifier_t* amp,
                                          struct bio_router_struct* router);

/** @brief Disconnect bio-async router */
int surprise_amplifier_disconnect_bio_async(surprise_amplifier_t* amp);

/* ============================================================================
 * Input Signal API
 * ============================================================================ */

/** @brief Signal a prediction error from FEP system */
int surprise_amplifier_on_prediction_error(surprise_amplifier_t* amp,
                                            float prediction_error,
                                            uint32_t source_module);

/** @brief Signal a conflict between two reasoning agents */
int surprise_amplifier_on_agent_conflict(surprise_amplifier_t* amp,
                                          uint32_t agent_a_id,
                                          float confidence_a,
                                          uint32_t agent_b_id,
                                          float confidence_b,
                                          float divergence);

/** @brief Signal that a hypothesis has been invalidated */
int surprise_amplifier_on_hypothesis_invalidated(surprise_amplifier_t* amp,
                                                  float prior_confidence,
                                                  float posterior_confidence);

/** @brief Signal a novelty detection event */
int surprise_amplifier_on_novelty(surprise_amplifier_t* amp,
                                   float novelty_score,
                                   uint32_t source_module);

/** @brief Signal a Bayesian surprise (large KL divergence) */
int surprise_amplifier_on_bayesian_surprise(surprise_amplifier_t* amp,
                                             float kl_divergence,
                                             uint32_t source_module);

/* ============================================================================
 * Update API
 * ============================================================================ */

/** @brief Update state: decay surprise level, process pending events */
int surprise_amplifier_update(surprise_amplifier_t* amp, float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current surprise level [0-1], -1.0f on error */
float surprise_amplifier_get_current_level(surprise_amplifier_t* amp);

/** @brief Check if amplifier is in refractory period */
bool surprise_amplifier_is_in_refractory(surprise_amplifier_t* amp);

/** @brief Get the most recent surprise event */
int surprise_amplifier_get_last_event(surprise_amplifier_t* amp,
                                       surprise_event_t* event_out);

/** @brief Get recent event history */
int surprise_amplifier_get_history(const surprise_amplifier_t* amp,
                                    surprise_event_t* events_out,
                                    uint32_t max_events,
                                    uint32_t* count_out);

/** @brief Get accumulated statistics (zeroed if amp is NULL) */
surprise_amplifier_stats_t surprise_amplifier_get_stats(
    const surprise_amplifier_t* amp);

/** @brief Check if bio-async is connected */
bool surprise_amplifier_is_bio_async_connected(const surprise_amplifier_t* amp);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_amplifier_set_health_agent(surprise_amplifier_t* amp,
                                         struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_AMPLIFIER_H */
