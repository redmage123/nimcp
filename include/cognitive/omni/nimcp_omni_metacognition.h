/**
 * @file nimcp_omni_metacognition.h
 * @brief Phase 10: Self-Organizing Inference (Metacognition)
 * @version 1.0.0
 * @date 2026-01-04
 *
 * WHAT: Metacognitive control for omnidirectional inference
 * WHY:  Enable the system to monitor, evaluate, and optimize its own inference
 * HOW:  Self-model + resource allocation + mode selection + coherence monitoring
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * METACOGNITION IN AI SYSTEMS:
 * ----------------------------
 * Metacognition is "thinking about thinking" - the ability to:
 *   1. MONITOR: Track own processing and performance
 *   2. EVALUATE: Assess quality and confidence of inferences
 *   3. CONTROL: Adjust processing strategies adaptively
 *   4. LEARN: Improve metacognitive policies over time
 *
 * SELF-ORGANIZING SYSTEMS (Friston, 2019):
 * ----------------------------------------
 * Self-organization emerges from minimizing free energy at multiple scales:
 *
 *   F_total = F_inference + F_meta
 *
 * Where:
 *   F_inference = Free energy of current inference
 *   F_meta = Free energy of metacognitive state
 *
 * The system must balance:
 *   - Accuracy: Correct inferences
 *   - Efficiency: Computational cost
 *   - Flexibility: Adaptation to novel situations
 *
 * RESOURCE RATIONALITY (Lieder & Griffiths, 2020):
 * ------------------------------------------------
 * Optimal decision-making under computational constraints:
 *
 *   π* = argmax_π [U(π) - λ * Cost(π)]
 *
 * Where:
 *   U(π) = Expected utility of policy π
 *   Cost(π) = Computational cost of executing π
 *   λ = Resource-accuracy tradeoff parameter
 *
 * COHERENCE THEORY (Thagard, 2000):
 * ---------------------------------
 * Beliefs/inferences should be mutually supportive:
 *
 *   Coherence(S) = Σ_i,j w_ij * support(b_i, b_j)
 *
 * Where:
 *   S = Set of beliefs/inferences
 *   w_ij = Weight of coherence constraint
 *   support(b_i, b_j) = Degree of mutual support
 *
 * ARCHITECTURE:
 * ```
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                    PHASE 10: SELF-ORGANIZING INFERENCE                    ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                           ║
 * ║   ┌─────────────────────────────────────────────────────────────────┐    ║
 * ║   │                        SELF-MODEL                                │    ║
 * ║   │   ┌───────────────┐  ┌───────────────┐  ┌───────────────┐       │    ║
 * ║   │   │  Capabilities │  │  Resources    │  │  History      │       │    ║
 * ║   │   │  - Inference  │  │  - Compute    │  │  - Successes  │       │    ║
 * ║   │   │  - Modalities │  │  - Memory     │  │  - Failures   │       │    ║
 * ║   │   │  - Precision  │  │  - Time       │  │  - Patterns   │       │    ║
 * ║   │   └───────────────┘  └───────────────┘  └───────────────┘       │    ║
 * ║   └─────────────────────────────────────────────────────────────────┘    ║
 * ║                                   │                                       ║
 * ║                                   ▼                                       ║
 * ║   ┌─────────────────────────────────────────────────────────────────┐    ║
 * ║   │                    METACOGNITIVE CONTROLLER                      │    ║
 * ║   │                                                                  │    ║
 * ║   │   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │    ║
 * ║   │   │   MONITOR    │───▶│   EVALUATE   │───▶│   CONTROL    │      │    ║
 * ║   │   │              │    │              │    │              │      │    ║
 * ║   │   │ - Track      │    │ - Confidence │    │ - Mode       │      │    ║
 * ║   │   │ - Measure    │    │ - Coherence  │    │ - Resources  │      │    ║
 * ║   │   │ - Log        │    │ - Progress   │    │ - Strategy   │      │    ║
 * ║   │   └──────────────┘    └──────────────┘    └──────────────┘      │    ║
 * ║   │                              ▲                    │              │    ║
 * ║   │                              └────────────────────┘              │    ║
 * ║   │                           (Feedback Loop)                        │    ║
 * ║   └─────────────────────────────────────────────────────────────────┘    ║
 * ║                                   │                                       ║
 * ║                                   ▼                                       ║
 * ║   ┌─────────────────────────────────────────────────────────────────┐    ║
 * ║   │                    INFERENCE ORCHESTRATION                       │    ║
 * ║   │                                                                  │    ║
 * ║   │   World Model ◀────▶ Active Inference ◀────▶ Precision          │    ║
 * ║   │       (P9)              (P7)                  (P6)               │    ║
 * ║   │                                                                  │    ║
 * ║   └─────────────────────────────────────────────────────────────────┘    ║
 * ║                                                                           ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * INTEGRATION WITH PREVIOUS PHASES:
 * ---------------------------------
 * - Phase 6 (Precision): Provides precision weights for metacognitive decisions
 * - Phase 7 (Active Inference): Policy evaluation under metacognitive control
 * - Phase 9 (World Model): Self-model is a special case of world model
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_METACOGNITION_H
#define NIMCP_OMNI_METACOGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

typedef struct omni_metacog_ctx omni_metacog_ctx_t;
typedef struct omni_self_model omni_self_model_t;
typedef struct omni_metacog_controller omni_metacog_controller_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Maximum inference modes supported */
#define OMNI_METACOG_MAX_MODES 8

/** Maximum modalities tracked */
#define OMNI_METACOG_MAX_MODALITIES 16

/** Maximum history entries for learning */
#define OMNI_METACOG_MAX_HISTORY 1024

/** Default coherence threshold */
#define OMNI_METACOG_DEFAULT_COHERENCE_THRESHOLD 0.7f

/** Default resource budget (normalized 0-1) */
#define OMNI_METACOG_DEFAULT_RESOURCE_BUDGET 0.8f

/** Minimum confidence for action */
#define OMNI_METACOG_MIN_CONFIDENCE 0.1f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Inference modes available for selection
 */
typedef enum {
    OMNI_METACOG_MODE_FORWARD = 0,      /**< Forward inference (prediction) */
    OMNI_METACOG_MODE_BACKWARD,          /**< Backward inference (explanation) */
    OMNI_METACOG_MODE_LATERAL,           /**< Cross-modal inference */
    OMNI_METACOG_MODE_HIERARCHICAL,      /**< Multi-scale inference */
    OMNI_METACOG_MODE_EXPLORATORY,       /**< Low-precision exploration */
    OMNI_METACOG_MODE_EXPLOITATIVE,      /**< High-precision exploitation */
    OMNI_METACOG_MODE_DREAMING,          /**< Imagination/counterfactual */
    OMNI_METACOG_MODE_CONSOLIDATING,     /**< Memory consolidation */
    OMNI_METACOG_MODE_COUNT
} omni_metacog_mode_t;

/**
 * @brief Resource types for allocation
 */
typedef enum {
    OMNI_RESOURCE_COMPUTE = 0,           /**< Computational cycles */
    OMNI_RESOURCE_MEMORY,                /**< Working memory capacity */
    OMNI_RESOURCE_TIME,                  /**< Processing time budget */
    OMNI_RESOURCE_ATTENTION,             /**< Attentional resources */
    OMNI_RESOURCE_PRECISION,             /**< Precision budget */
    OMNI_RESOURCE_COUNT
} omni_resource_type_t;

/**
 * @brief Metacognitive states
 */
typedef enum {
    OMNI_METACOG_STATE_IDLE = 0,         /**< Not actively monitoring */
    OMNI_METACOG_STATE_MONITORING,       /**< Passively observing */
    OMNI_METACOG_STATE_EVALUATING,       /**< Assessing performance */
    OMNI_METACOG_STATE_INTERVENING,      /**< Actively adjusting */
    OMNI_METACOG_STATE_LEARNING,         /**< Updating metacognitive model */
    OMNI_METACOG_STATE_COUNT
} omni_metacog_state_t;

/**
 * @brief Coherence violation types
 */
typedef enum {
    OMNI_COHERENCE_OK = 0,               /**< No violation */
    OMNI_COHERENCE_CONTRADICTION,        /**< Logical contradiction */
    OMNI_COHERENCE_IMPLAUSIBILITY,       /**< Implausible combination */
    OMNI_COHERENCE_TEMPORAL,             /**< Temporal inconsistency */
    OMNI_COHERENCE_MODAL,                /**< Cross-modal mismatch */
    OMNI_COHERENCE_COUNT
} omni_coherence_status_t;

/* ============================================================================
 * CORE STRUCTURES
 * ============================================================================ */

/**
 * @brief Capability profile for a specific inference type
 */
typedef struct {
    omni_metacog_mode_t mode;            /**< Inference mode */
    float proficiency;                   /**< Learned proficiency [0,1] */
    float typical_cost;                  /**< Typical resource cost */
    float typical_accuracy;              /**< Typical accuracy achieved */
    float typical_latency;               /**< Typical processing time */
    uint32_t usage_count;                /**< Times this mode was used */
    uint32_t success_count;              /**< Successful uses */
    double last_used;                    /**< Timestamp of last use */
} omni_capability_t;

/**
 * @brief Resource state tracking
 */
typedef struct {
    float available[OMNI_RESOURCE_COUNT]; /**< Currently available [0,1] */
    float allocated[OMNI_RESOURCE_COUNT]; /**< Currently allocated [0,1] */
    float budget[OMNI_RESOURCE_COUNT];    /**< Maximum allowed [0,1] */
    float consumption_rate[OMNI_RESOURCE_COUNT]; /**< Rate of consumption */
    float replenishment_rate[OMNI_RESOURCE_COUNT]; /**< Rate of recovery */
} omni_resource_state_t;

/**
 * @brief History entry for learning
 */
typedef struct {
    omni_metacog_mode_t mode;            /**< Mode used */
    float resource_used;                 /**< Total resources consumed */
    float accuracy;                      /**< Accuracy achieved */
    float latency;                       /**< Time taken */
    float coherence;                     /**< Coherence of result */
    bool success;                        /**< Whether inference succeeded */
    double timestamp;                    /**< When this occurred */
    uint32_t context_hash;               /**< Hash of context for similarity */
} omni_metacog_history_t;

/**
 * @brief Self-model: System's representation of itself
 *
 * WHAT: Internal model of own capabilities and limitations
 * WHY:  Enable metacognitive reasoning about own performance
 */
struct omni_self_model {
    /** Capability profiles for each mode */
    omni_capability_t capabilities[OMNI_METACOG_MODE_COUNT];

    /** Modality proficiency (vision, audio, etc.) */
    float modality_proficiency[OMNI_METACOG_MAX_MODALITIES];
    uint32_t num_modalities;

    /** Resource state */
    omni_resource_state_t resources;

    /** Historical performance */
    omni_metacog_history_t history[OMNI_METACOG_MAX_HISTORY];
    uint32_t history_count;
    uint32_t history_head;               /**< Ring buffer head */

    /** Aggregate statistics */
    float overall_accuracy;              /**< Running average */
    float overall_efficiency;            /**< Resource efficiency */
    float overall_coherence;             /**< Average coherence */

    /** Model confidence */
    float self_model_confidence;         /**< Confidence in self-model */

    /** Learning parameters */
    float learning_rate;                 /**< For updating self-model */
    float discount_factor;               /**< For temporal weighting */
};

/**
 * @brief Coherence assessment result
 */
typedef struct {
    omni_coherence_status_t status;      /**< Overall status */
    float coherence_score;               /**< [0,1] overall coherence */
    float contradiction_level;           /**< Level of contradictions */
    float plausibility_score;            /**< How plausible the inference */
    float temporal_consistency;          /**< Temporal coherence */
    float modal_agreement;               /**< Cross-modal agreement */
    uint32_t num_violations;             /**< Number of violations found */
    char violation_description[256];     /**< Human-readable description */
} omni_coherence_result_t;

/**
 * @brief Monitoring snapshot
 */
typedef struct {
    double timestamp;                    /**< When snapshot was taken */
    omni_metacog_mode_t current_mode;    /**< Current inference mode */
    omni_metacog_state_t meta_state;     /**< Metacognitive state */

    /** Performance metrics */
    float current_accuracy;              /**< Estimated current accuracy */
    float current_confidence;            /**< Confidence in current inference */
    float current_progress;              /**< Progress toward goal [0,1] */

    /** Resource usage */
    float resource_usage[OMNI_RESOURCE_COUNT];

    /** Coherence */
    omni_coherence_result_t coherence;

    /** Anomaly detection */
    bool anomaly_detected;               /**< Whether anomaly was found */
    float anomaly_score;                 /**< Severity of anomaly */
    char anomaly_description[256];       /**< Description of anomaly */
} omni_monitoring_snapshot_t;

/**
 * @brief Mode selection recommendation
 */
typedef struct {
    omni_metacog_mode_t recommended_mode; /**< Recommended mode */
    float confidence;                     /**< Confidence in recommendation */
    float expected_accuracy;              /**< Expected accuracy */
    float expected_cost;                  /**< Expected resource cost */
    float expected_latency;               /**< Expected time */
    char rationale[256];                  /**< Why this mode was chosen */

    /** Alternative modes (ranked) */
    omni_metacog_mode_t alternatives[3];
    float alternative_scores[3];
} omni_mode_recommendation_t;

/**
 * @brief Resource allocation plan
 */
typedef struct {
    float allocations[OMNI_RESOURCE_COUNT]; /**< Recommended allocations */
    float total_budget;                   /**< Total budget used */
    float efficiency_score;               /**< Efficiency of this plan */
    bool within_budget;                   /**< Whether plan is feasible */
    char rationale[256];                  /**< Why this allocation */
} omni_resource_plan_t;

/**
 * @brief Metacognitive intervention
 */
typedef struct {
    bool should_intervene;               /**< Whether to intervene */
    omni_metacog_mode_t new_mode;        /**< Mode to switch to (if any) */
    omni_resource_plan_t resource_plan;  /**< New resource allocation */
    float precision_adjustment;          /**< Precision change (-1 to +1) */
    bool should_abort;                   /**< Whether to abort current inference */
    bool should_retry;                   /**< Whether to retry */
    char intervention_reason[256];       /**< Why intervening */
} omni_intervention_t;

/**
 * @brief Metacognitive controller configuration
 */
typedef struct {
    /** Monitoring parameters */
    float monitoring_frequency;          /**< How often to monitor (Hz) */
    float coherence_threshold;           /**< Threshold for coherence alerts */
    float anomaly_threshold;             /**< Threshold for anomaly detection */

    /** Control parameters */
    float intervention_threshold;        /**< When to intervene */
    float mode_switch_cost;              /**< Cost of switching modes */
    float resource_tradeoff_lambda;      /**< Accuracy-cost tradeoff */

    /** Learning parameters */
    float meta_learning_rate;            /**< Learning rate for metacognition */
    float exploration_rate;              /**< Rate of exploring new strategies */
    bool enable_online_learning;         /**< Learn during operation */

    /** Safety constraints */
    float min_coherence;                 /**< Minimum acceptable coherence */
    float max_resource_usage;            /**< Maximum resource cap */
    uint32_t max_retries;                /**< Maximum retry attempts */
} omni_metacog_config_t;

/**
 * @brief Main metacognition context
 */
struct omni_metacog_ctx {
    /** Self-model */
    omni_self_model_t* self_model;

    /** Configuration */
    omni_metacog_config_t config;

    /** Current state */
    omni_metacog_state_t state;
    omni_metacog_mode_t current_mode;

    /** Latest monitoring snapshot */
    omni_monitoring_snapshot_t latest_snapshot;

    /** Statistics */
    uint64_t total_inferences;
    uint64_t successful_inferences;
    uint64_t interventions_made;
    uint64_t mode_switches;

    /** Timing */
    double creation_time;
    double last_update_time;

    /** Thread safety */
    void* mutex;

    /** Integration hooks */
    void* world_model;                   /**< Phase 9 world model */
    void* active_inference;              /**< Phase 7 active inference */
    void* precision_system;              /**< Phase 6 precision weighting */
};

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create metacognition context with default configuration
 * @return New context or NULL on failure
 */
omni_metacog_ctx_t* omni_metacog_create(void);

/**
 * @brief Create metacognition context with custom configuration
 * @param config Configuration parameters
 * @return New context or NULL on failure
 */
omni_metacog_ctx_t* omni_metacog_create_with_config(
    const omni_metacog_config_t* config);

/**
 * @brief Destroy metacognition context
 * @param ctx Context to destroy
 */
void omni_metacog_destroy(omni_metacog_ctx_t* ctx);

/**
 * @brief Reset metacognition to initial state
 * @param ctx Context to reset
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_reset(omni_metacog_ctx_t* ctx);

/**
 * @brief Get default configuration
 * @param config Output configuration
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_get_default_config(omni_metacog_config_t* config);

/* ============================================================================
 * SELF-MODEL API
 * ============================================================================ */

/**
 * @brief Initialize self-model
 * @param ctx Metacognition context
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_init_self_model(omni_metacog_ctx_t* ctx);

/**
 * @brief Update self-model with new experience
 * @param ctx Metacognition context
 * @param mode Mode that was used
 * @param accuracy Accuracy achieved
 * @param cost Resource cost
 * @param latency Time taken
 * @param success Whether successful
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_update_self_model(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    float accuracy,
    float cost,
    float latency,
    bool success);

/**
 * @brief Get capability profile for a mode
 * @param ctx Metacognition context
 * @param mode Mode to query
 * @param capability Output capability profile
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_get_capability(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    omni_capability_t* capability);

/**
 * @brief Get resource state
 * @param ctx Metacognition context
 * @param resources Output resource state
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_get_resources(
    omni_metacog_ctx_t* ctx,
    omni_resource_state_t* resources);

/**
 * @brief Set resource budget
 * @param ctx Metacognition context
 * @param resource Resource type
 * @param budget New budget [0,1]
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_set_resource_budget(
    omni_metacog_ctx_t* ctx,
    omni_resource_type_t resource,
    float budget);

/* ============================================================================
 * MONITORING API
 * ============================================================================ */

/**
 * @brief Take a monitoring snapshot
 * @param ctx Metacognition context
 * @param snapshot Output snapshot
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_monitor(
    omni_metacog_ctx_t* ctx,
    omni_monitoring_snapshot_t* snapshot);

/**
 * @brief Check coherence of current inferences
 * @param ctx Metacognition context
 * @param inferences Array of inference results to check
 * @param num_inferences Number of inferences
 * @param result Output coherence result
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_check_coherence(
    omni_metacog_ctx_t* ctx,
    const void* inferences,
    uint32_t num_inferences,
    omni_coherence_result_t* result);

/**
 * @brief Detect anomalies in inference process
 * @param ctx Metacognition context
 * @param snapshot Current monitoring snapshot
 * @param anomaly_score Output anomaly score [0,1]
 * @param description Output description (256 chars)
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_detect_anomaly(
    omni_metacog_ctx_t* ctx,
    const omni_monitoring_snapshot_t* snapshot,
    float* anomaly_score,
    char* description);

/* ============================================================================
 * EVALUATION API
 * ============================================================================ */

/**
 * @brief Evaluate performance of current mode
 * @param ctx Metacognition context
 * @param accuracy Output accuracy estimate
 * @param confidence Output confidence in estimate
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_evaluate_performance(
    omni_metacog_ctx_t* ctx,
    float* accuracy,
    float* confidence);

/**
 * @brief Estimate expected performance for a mode
 * @param ctx Metacognition context
 * @param mode Mode to evaluate
 * @param context_hash Hash representing current context
 * @param expected_accuracy Output expected accuracy
 * @param expected_cost Output expected cost
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_predict_performance(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    uint32_t context_hash,
    float* expected_accuracy,
    float* expected_cost);

/**
 * @brief Get confidence in current inference
 * @param ctx Metacognition context
 * @return Confidence [0,1], or -1 on error
 */
float omni_metacog_get_confidence(omni_metacog_ctx_t* ctx);

/* ============================================================================
 * CONTROL API
 * ============================================================================ */

/**
 * @brief Select best inference mode for current situation
 * @param ctx Metacognition context
 * @param context_hash Hash representing current context
 * @param recommendation Output mode recommendation
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_select_mode(
    omni_metacog_ctx_t* ctx,
    uint32_t context_hash,
    omni_mode_recommendation_t* recommendation);

/**
 * @brief Plan resource allocation
 * @param ctx Metacognition context
 * @param mode Target mode
 * @param accuracy_target Target accuracy
 * @param plan Output resource plan
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_plan_resources(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    float accuracy_target,
    omni_resource_plan_t* plan);

/**
 * @brief Decide whether to intervene
 * @param ctx Metacognition context
 * @param snapshot Current monitoring snapshot
 * @param intervention Output intervention decision
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_decide_intervention(
    omni_metacog_ctx_t* ctx,
    const omni_monitoring_snapshot_t* snapshot,
    omni_intervention_t* intervention);

/**
 * @brief Execute an intervention
 * @param ctx Metacognition context
 * @param intervention Intervention to execute
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_execute_intervention(
    omni_metacog_ctx_t* ctx,
    const omni_intervention_t* intervention);

/**
 * @brief Switch inference mode
 * @param ctx Metacognition context
 * @param new_mode Mode to switch to
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_switch_mode(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t new_mode);

/**
 * @brief Adjust precision level
 * @param ctx Metacognition context
 * @param adjustment Precision adjustment (-1 to +1)
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_adjust_precision(
    omni_metacog_ctx_t* ctx,
    float adjustment);

/* ============================================================================
 * META-LEARNING API
 * ============================================================================ */

/**
 * @brief Learn from recent history
 * @param ctx Metacognition context
 * @param num_entries Number of history entries to learn from
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_learn(
    omni_metacog_ctx_t* ctx,
    uint32_t num_entries);

/**
 * @brief Update metacognitive policy
 * @param ctx Metacognition context
 * @param reward Reward signal
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_update_policy(
    omni_metacog_ctx_t* ctx,
    float reward);

/**
 * @brief Get meta-learning statistics
 * @param ctx Metacognition context
 * @param improvement Output improvement rate
 * @param convergence Output convergence measure
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_get_learning_stats(
    omni_metacog_ctx_t* ctx,
    float* improvement,
    float* convergence);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Connect to Phase 9 world model
 * @param ctx Metacognition context
 * @param world_model World model context
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_connect_world_model(
    omni_metacog_ctx_t* ctx,
    void* world_model);

/**
 * @brief Connect to Phase 7 active inference
 * @param ctx Metacognition context
 * @param active_inference Active inference context
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_connect_active_inference(
    omni_metacog_ctx_t* ctx,
    void* active_inference);

/**
 * @brief Connect to Phase 6 precision system
 * @param ctx Metacognition context
 * @param precision_system Precision weighting context
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_connect_precision(
    omni_metacog_ctx_t* ctx,
    void* precision_system);

/**
 * @brief Run one metacognitive cycle
 * @param ctx Metacognition context
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_step(omni_metacog_ctx_t* ctx);

/* ============================================================================
 * STATISTICS AND DEBUGGING
 * ============================================================================ */

/**
 * @brief Get overall statistics
 * @param ctx Metacognition context
 * @param total_inferences Output total inferences
 * @param success_rate Output success rate
 * @param avg_efficiency Output average efficiency
 * @return NIMCP_OK on success
 */
nimcp_error_t omni_metacog_get_statistics(
    omni_metacog_ctx_t* ctx,
    uint64_t* total_inferences,
    float* success_rate,
    float* avg_efficiency);

/**
 * @brief Convert mode to string
 * @param mode Mode to convert
 * @return Static string representation
 */
const char* omni_metacog_mode_to_string(omni_metacog_mode_t mode);

/**
 * @brief Convert state to string
 * @param state State to convert
 * @return Static string representation
 */
const char* omni_metacog_state_to_string(omni_metacog_state_t state);

/**
 * @brief Convert resource type to string
 * @param resource Resource type to convert
 * @return Static string representation
 */
const char* omni_resource_type_to_string(omni_resource_type_t resource);

/**
 * @brief Convert coherence status to string
 * @param status Coherence status to convert
 * @return Static string representation
 */
const char* omni_coherence_status_to_string(omni_coherence_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_METACOGNITION_H */
