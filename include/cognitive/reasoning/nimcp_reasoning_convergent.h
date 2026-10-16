/**
 * @file nimcp_reasoning_convergent.h
 * @brief Convergent Evidence Accumulation Architecture for Reasoning
 *
 * WHAT: Replaces the fixed wave pipeline with a convergent evidence accumulation
 *       architecture where ~44 brain modules participate as autonomous contributors
 * WHY:  The brain doesn't reason in fixed pipelines — all cortical regions process
 *       simultaneously, contribute evidence continuously, and convergence emerges
 *       from parallel accumulation (Global Workspace Theory)
 * HOW:  Static contributor registry + evidence accumulator with EMA convergence
 *       detection. Modules are categorized into 3 tiers:
 *       Tier 1: Evidence Producers (contribute reasoning steps)
 *       Tier 2: Confidence Modulators (adjust confidence, no steps)
 *       Tier 3: Context Providers (supply context, Wave 0)
 *
 * ARCHITECTURE:
 *   Wave 0 (main thread): Context providers run instantly
 *   Wave 1 (parallel):    Evidence producers + modulators run in thread pool
 *   Wave 2 (sequential):  Dependent phases run sequentially (may early-exit)
 *   Evidence accumulator: Mutex-protected, running confidence + EMA delta
 *   Convergence: When EMA delta < threshold after >= 3 submissions
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_CONVERGENT_H
#define NIMCP_REASONING_CONVERGENT_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;
struct reasoning_engine;
typedef struct reasoning_engine reasoning_engine_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum contributors in one convergent session */
#define REASONING_MAX_CONTRIBUTORS 128

/** Minimum submissions before convergence can trigger */
#define REASONING_MIN_CONVERGENCE_SUBMISSIONS 3

/** Default EMA smoothing factor */
#define REASONING_DEFAULT_EMA_ALPHA 0.3f

/** Default convergence threshold (EMA delta below this = converged) */
#define REASONING_DEFAULT_CONVERGENCE_THRESHOLD 0.005f

/** Default convergence timeout in milliseconds */
#define REASONING_DEFAULT_CONVERGENCE_TIMEOUT_MS 500

/** Maximum confidence history for accumulator */
#define REASONING_CONFIDENCE_HISTORY_SIZE 64

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Contributor role in the convergent architecture
 *
 * WHAT: Categorizes how a module participates in reasoning
 * WHY:  Different roles produce different outputs (steps, modulations, context)
 */
typedef enum {
    REASONING_ROLE_EVIDENCE_PRODUCER = 0,  /**< Produces reasoning steps */
    REASONING_ROLE_CONFIDENCE_MODULATOR,    /**< Adjusts confidence delta */
    REASONING_ROLE_CONTEXT_PROVIDER         /**< Supplies context, no output */
} reasoning_contributor_role_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Per-contributor output (thread-local, no contention)
 *
 * WHAT: Working state for one contributor during a convergent session
 * WHY:  Each contributor writes to its own struct — zero contention
 * HOW:  Populated by contributor function, consumed by accumulator merge
 */
typedef struct {
    const char* module_name;              /**< Contributor identifier */
    reasoning_contributor_role_t role;     /**< Evidence/modulator/context */
    uint32_t wave;                        /**< 0=instant, 1=parallel, 2=dependent */
    reasoning_engine_t* engine;           /**< Shared engine (read-only) */
    const char* query;                    /**< Query string (read-only) */
    const char* query_type;               /**< Classified query type (read-only) */
    uint32_t domain;                      /**< Knowledge domain restriction */
    reasoning_chain_t local_chain;        /**< Tier 1: accumulated steps */
    float result_confidence;              /**< Tier 1: output confidence */
    float confidence_delta;               /**< Tier 2: modulation delta */
    bool context_available;               /**< Tier 3: context loaded flag */
    bool completed;                       /**< Task finished */
    bool skipped;                         /**< Module unavailable, skipped */
    uint64_t duration_us;                 /**< Execution time */
} convergent_contribution_t;

/**
 * @brief Static registry entry for a reasoning contributor
 *
 * WHAT: Describes one brain module's contribution to reasoning
 * WHY:  Decouple module enumeration from orchestration logic
 * HOW:  Static array, iterated at session start to build active list
 */
typedef struct {
    const char* name;                          /**< Human-readable module name */
    reasoning_contributor_role_t role;          /**< Evidence/modulator/context */
    void (*fn)(void* arg);                     /**< nimcp_task_fn compatible */
    bool (*is_available)(reasoning_engine_t* engine);  /**< NULL-safety check */
    uint32_t wave;                             /**< 0=instant, 1=parallel, 2=dependent */
} reasoning_contributor_entry_t;

/**
 * @brief Evidence accumulator (mutex-protected shared state)
 *
 * WHAT: Aggregates evidence from all contributors with convergence detection
 * WHY:  Replace fixed pipeline with dynamic convergence — reasoning completes
 *       when confidence stabilizes rather than when all phases finish
 * HOW:  Running average confidence + EMA of confidence deltas
 *
 * CONVERGENCE ALGORITHM:
 *   On each evidence submission:
 *     weight = 1 / confidence_count
 *     current_confidence = (1 - weight) * current + weight * new
 *     delta = |current - previous|
 *     ema_delta = alpha * delta + (1 - alpha) * ema_delta
 *     converged = (count >= 3) AND (ema_delta < threshold)
 */
typedef struct {
    nimcp_mutex_t* mutex;                      /**< Protects all mutable fields */

    /* Confidence tracking */
    float confidence_history[REASONING_CONFIDENCE_HISTORY_SIZE];
    uint32_t confidence_count;                 /**< Number of submissions */
    float current_confidence;                  /**< Running average confidence */

    /* Convergence detection */
    float ema_delta;                           /**< EMA of confidence deltas */
    float ema_alpha;                           /**< Smoothing factor (default 0.3) */
    float convergence_threshold;               /**< Delta below this = converged */
    bool converged;                            /**< True when converged */

    /* Contributor tracking */
    nimcp_atomic_uint32_t completed_count;     /**< Atomic: tasks done */
    uint32_t total_contributors;               /**< Total active contributors */

    /* Modulation accumulation */
    float total_positive_modulation;           /**< Sum of positive deltas */
    float total_negative_modulation;           /**< Sum of negative deltas */
    uint32_t modulator_count;                  /**< Number of modulators applied */
} evidence_accumulator_t;

/**
 * @brief Session state for one convergent reasoning call
 *
 * WHAT: Complete state for a single convergent reasoning invocation
 * WHY:  Encapsulate all per-query state in one struct
 */
typedef struct {
    convergent_contribution_t contributions[REASONING_MAX_CONTRIBUTORS];
    uint32_t num_contributions;
    evidence_accumulator_t accumulator;
} convergent_session_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Execute convergent reasoning pipeline
 *
 * WHAT: Full convergent orchestrator — replaces wave pipeline when enabled
 * WHY:  Parallel evidence accumulation with convergence detection
 * HOW:  Wave 0 context → Wave 1 parallel → merge → convergence check →
 *       Wave 2 sequential (if needed) → modulation → synthesis
 *
 * @param engine Connected reasoning engine
 * @param query Natural language query string
 * @param domain Knowledge domain restriction (0 = general)
 * @param chain Output reasoning chain
 * @return 0 on success, -1 on error
 */
int reasoning_engine_reason_convergent(reasoning_engine_t* engine,
                                        const char* query,
                                        uint32_t domain,
                                        reasoning_chain_t* chain);

/**
 * @brief Get the static contributor registry
 *
 * WHAT: Returns pointer to the static array of all registered contributors
 * WHY:  Allow tests and introspection to examine the registry
 *
 * @param count_out Output: number of entries in the registry
 * @return Pointer to static registry array (do NOT free)
 */
const reasoning_contributor_entry_t* reasoning_convergent_get_registry(
    uint32_t* count_out);

/**
 * @brief Initialize evidence accumulator
 *
 * @param acc Accumulator to initialize
 * @param total_contributors Expected number of contributors
 * @param alpha EMA smoothing factor (0 = use default 0.3)
 * @param threshold Convergence threshold (0 = use default 0.005)
 * @return 0 on success, -1 on error
 */
int reasoning_accumulator_init(evidence_accumulator_t* acc,
                                uint32_t total_contributors,
                                float alpha, float threshold);

/**
 * @brief Destroy evidence accumulator
 *
 * @param acc Accumulator to destroy (NULL safe)
 */
void reasoning_accumulator_destroy(evidence_accumulator_t* acc);

/**
 * @brief Submit evidence from a Tier 1 producer to the accumulator
 *
 * WHAT: Merge local chain steps into main chain, update running confidence
 * WHY:  Thread-safe evidence aggregation with convergence detection
 *
 * @param acc Evidence accumulator (mutex-protected)
 * @param chain Main reasoning chain (merged under lock)
 * @param contrib Contribution with local_chain and result_confidence
 * @return 0 on success, -1 on error
 */
int reasoning_accumulator_submit_evidence(evidence_accumulator_t* acc,
                                           reasoning_chain_t* chain,
                                           const convergent_contribution_t* contrib);

/**
 * @brief Submit modulation from a Tier 2 modulator
 *
 * @param acc Evidence accumulator
 * @param delta Confidence delta (positive or negative)
 * @return 0 on success, -1 on error
 */
int reasoning_accumulator_submit_modulation(evidence_accumulator_t* acc,
                                             float delta);

/**
 * @brief Apply accumulated modulation to final confidence
 *
 * WHAT: Clamp net modulation to [-0.3, +0.3] and apply
 * WHY:  Modulators should influence but not dominate
 *
 * @param acc Evidence accumulator
 * @return Net modulation applied (clamped)
 */
float reasoning_accumulator_apply_modulation(evidence_accumulator_t* acc);

/**
 * @brief Check if accumulator has converged
 *
 * @param acc Evidence accumulator
 * @return true if converged
 */
bool reasoning_accumulator_is_converged(evidence_accumulator_t* acc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_CONVERGENT_H */
