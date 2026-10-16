/**
 * @file nimcp_security_distributed_training_fep_bridge.h
 * @brief Free Energy Principle bridge for Distributed Training Security
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for distributed training security - Byzantine behavior as surprise
 * WHY:  Byzantine workers are high-surprise observations in the FEP framework; normal
 *       cooperative behavior is expected (low free energy), while malicious or faulty
 *       behavior deviates from generative model predictions (high free energy)
 * HOW:  Map Byzantine detection scores to free energy, gradient divergence to prediction
 *       error, worker trust to precision weighting, and quarantine decisions to high-
 *       surprise responses
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * DISTRIBUTED COGNITION AS PREDICTIVE PROCESSING:
 * The brain operates as a distributed system where neural populations collaborate to
 * minimize collective prediction error. Byzantine workers in distributed training
 * parallel dysfunctional neural circuits that produce aberrant signals:
 *
 * - Healthy neurons = cooperative workers (low free energy)
 * - Epileptic foci = Byzantine workers (high free energy spikes)
 * - Immune response = quarantine mechanisms
 * - Synaptic pruning = worker removal
 *
 * FEP-BYZANTINE MAPPING:
 * ```
 * Byzantine Detection              Free Energy Framework
 * ─────────────────────────────────────────────────────────────────
 * Worker gradient                  → Observation o
 * Expected gradient (consensus)    → Prediction g(μ)
 * Gradient divergence              → Prediction error ε = o - g(μ)
 * Byzantine anomaly score          → Free energy F
 * Worker trust level               → Precision Π (inverse variance)
 * Quarantine decision              → High-surprise response
 * Model recovery                   → Belief update μ' = μ - lr*∂F/∂μ
 * ```
 *
 * PRECISION-WEIGHTED TRUST:
 * High-trust workers have high precision (their signals matter more).
 * Low-trust workers have low precision (their signals are discounted):
 *
 *   Weighted contribution = Π_worker * gradient_worker
 *   Aggregation = Σ(Π_i * g_i) / Σ(Π_i)
 *
 * ACTIVE INFERENCE FOR DEFENSE:
 * The system performs active inference to minimize expected free energy:
 * - Policies = {quarantine, monitor, trust, remove}
 * - Action selection minimizes G(π) = Risk + Ambiguity
 * - Risk = divergence from secure (low-Byzantine) state
 * - Ambiguity = uncertainty about worker behavior
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║     SECURITY DISTRIBUTED TRAINING - FEP BRIDGE (Byzantine Detection)      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                       ║
 * ║   │  FEP System      │────────▶│  Distributed     │                       ║
 * ║   │                  │         │  Training        │                       ║
 * ║   │ • Free Energy    │         │  Security        │                       ║
 * ║   │ • Surprise       │         │                  │                       ║
 * ║   │ • Precision      │         │ • Byzantine Det  │                       ║
 * ║   └──────────────────┘         │ • Trust Scoring  │                       ║
 * ║           ↓                    │ • Quarantine     │                       ║
 * ║   ┌──────────────────────────────────────────────────────────────┐        ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │        ║
 * ║   │                                                              │        ║
 * ║   │  FEP → Security:                                             │        ║
 * ║   │    • Free energy → Byzantine detection threshold             │        ║
 * ║   │    • Surprise → Quarantine trigger                           │        ║
 * ║   │    • Precision → Detection sensitivity                       │        ║
 * ║   │    • EFE → Defense policy selection                          │        ║
 * ║   │                                                              │        ║
 * ║   │  Security → FEP:                                             │        ║
 * ║   │    • Byzantine scores → High-surprise observations           │        ║
 * ║   │    • Gradient divergence → Prediction errors                 │        ║
 * ║   │    • Worker trust → Precision weights                        │        ║
 * ║   │    • Quarantine events → Belief updates (prune)              │        ║
 * ║   └──────────────────────────────────────────────────────────────┘        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DETECTION THRESHOLDS (Free Energy Based):
 * ```
 * Free Energy Range    | Byzantine Status     | Action
 * ---------------------|----------------------|---------------------------
 * < 2.0               | Normal (cooperative)  | Full trust, high precision
 * 2.0 - 5.0           | Suspicious            | Monitor, reduce precision
 * 5.0 - 10.0          | Likely Byzantine      | Flag for review
 * 10.0 - 20.0         | Byzantine detected    | Quarantine recommended
 * > 20.0              | Critical threat       | Immediate quarantine
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_DISTRIBUTED_TRAINING_FEP_BRIDGE_H
#define NIMCP_SECURITY_DISTRIBUTED_TRAINING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/distributed/nimcp_security_distributed_training_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module identification */
#define SECURITY_DISTRIBUTED_FEP_MODULE_NAME     "security_distributed_training_fep"
#define SECURITY_DISTRIBUTED_FEP_MODULE_VERSION  "1.0.0"

/** Free energy thresholds for Byzantine detection */
#define SECURITY_DIST_FEP_NORMAL_THRESHOLD       2.0f   /**< Below = cooperative */
#define SECURITY_DIST_FEP_SUSPICIOUS_THRESHOLD   5.0f   /**< Monitor mode */
#define SECURITY_DIST_FEP_BYZANTINE_THRESHOLD    10.0f  /**< Byzantine detected */
#define SECURITY_DIST_FEP_CRITICAL_THRESHOLD     20.0f  /**< Critical threat */

/** Precision bounds for detection sensitivity */
#define SECURITY_DIST_FEP_MIN_PRECISION          0.05f  /**< Minimum precision */
#define SECURITY_DIST_FEP_MAX_PRECISION          20.0f  /**< Maximum precision */
#define SECURITY_DIST_FEP_DEFAULT_PRECISION      1.0f   /**< Default precision */

/** Trust-precision mapping */
#define SECURITY_DIST_FEP_QUARANTINED_PRECISION  0.0f   /**< Zero influence */
#define SECURITY_DIST_FEP_UNTRUSTED_PRECISION    0.1f   /**< Minimal influence */
#define SECURITY_DIST_FEP_PROBATION_PRECISION    0.3f   /**< Reduced influence */
#define SECURITY_DIST_FEP_VERIFIED_PRECISION     0.7f   /**< Normal influence */
#define SECURITY_DIST_FEP_TRUSTED_PRECISION      1.0f   /**< Full influence */

/** Learning rates */
#define SECURITY_DIST_FEP_DEFAULT_BELIEF_LR      0.1f   /**< Belief update rate */
#define SECURITY_DIST_FEP_DEFAULT_PRECISION_LR   0.05f  /**< Precision adaptation rate */
#define SECURITY_DIST_FEP_DEFAULT_TRUST_LR       0.02f  /**< Trust-precision coupling */

/** Bio-async */
#define SECURITY_DIST_FEP_BIO_INBOX_CAPACITY     64

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Security distributed training FEP configuration
 *
 * WHAT: Configuration for FEP-security integration
 * WHY:  Customize Byzantine detection via free energy framework
 * HOW:  Thresholds, learning rates, and feature enables
 */
typedef struct {
    /* FEP parameters */
    float byzantine_fe_threshold;         /**< Free energy threshold for Byzantine */
    float surprise_threshold;             /**< Surprise threshold for quarantine */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_scoring;                 /**< Use FEP for Byzantine scoring */
    bool enable_precision_modulation;     /**< Adapt precision from trust levels */
    float normal_fe_threshold;            /**< FE threshold for normal (cooperative) */
    float critical_fe_threshold;          /**< FE threshold for critical threat */

    /* Trust-precision coupling */
    bool enable_trust_precision_coupling; /**< Map trust levels to precision */
    float trust_precision_scale;          /**< Scaling factor for trust→precision */

    /* Learning */
    bool enable_online_learning;          /**< Update FEP from detections */
    float belief_learning_rate;           /**< Belief update rate */
    bool learn_from_quarantines;          /**< Update beliefs on quarantine */

    /* Active inference for defense */
    bool enable_active_defense;           /**< Use EFE for policy selection */
    float action_temperature;             /**< Softmax temperature for actions */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;          /**< Message queue size */
} security_dist_fep_config_t;

/**
 * @brief FEP effects on security (FEP → Security)
 *
 * WHAT: FEP-derived modulation of security parameters
 * WHY:  Free energy informs detection thresholds and sensitivity
 * HOW:  Computed from current FEP state each update
 */
typedef struct {
    /* Detection modulation */
    float detection_threshold_scale;      /**< Scale factor for detection thresholds */
    float detection_sensitivity;          /**< Precision-based sensitivity [0-1] */
    float quarantine_urgency;             /**< Urgency for quarantine [0-1] */

    /* Per-worker precision */
    float* worker_precisions;             /**< Precision weight per worker */
    uint32_t num_worker_precisions;       /**< Number of workers */

    /* Free energy metrics */
    float current_free_energy;            /**< Current system free energy */
    float surprise_level;                 /**< Current surprise level */
    float prediction_error_magnitude;     /**< Aggregate prediction error */

    /* Active defense */
    float defense_policy_scores[4];       /**< EFE for {monitor, reduce_trust, quarantine, remove} */
    uint32_t recommended_policy;          /**< Index of recommended policy */

    /* Confidence */
    float detection_confidence;           /**< Confidence in Byzantine detection */

    /* Metadata */
    uint64_t last_update_ms;              /**< When effects were computed */
    bool valid;                           /**< Whether effects are current */
} security_dist_fep_effects_t;

/**
 * @brief Security effects on FEP (Security → FEP)
 *
 * WHAT: Security-derived updates to FEP system
 * WHY:  Detection outcomes inform belief updates and precision
 * HOW:  Converted from security events each cycle
 */
typedef struct {
    /* Detection statistics */
    uint64_t byzantine_detections;        /**< Total Byzantine detections */
    uint64_t quarantine_events;           /**< Total quarantine events */
    uint64_t normal_observations;         /**< Normal behavior observations */
    uint64_t false_positive_corrections;  /**< Corrected false positives */

    /* Aggregate metrics */
    float avg_byzantine_score;            /**< Average Byzantine score */
    float avg_gradient_divergence;        /**< Average gradient divergence */
    float current_threat_level;           /**< Overall threat level [0-1] */
    float estimated_byzantine_ratio;      /**< Estimated Byzantine worker ratio */

    /* Trust distribution */
    uint32_t quarantined_count;           /**< Workers quarantined */
    uint32_t trusted_count;               /**< Workers trusted */
    float avg_trust_score;                /**< Average trust across workers */

    /* Prediction error source */
    float max_gradient_divergence;        /**< Max divergence this cycle */
    uint32_t max_divergence_worker_idx;   /**< Worker with max divergence */

    /* Metadata */
    uint64_t timestamp_ms;                /**< When captured */
    bool valid;                           /**< Whether valid */
} fep_security_dist_effects_t;

/**
 * @brief FEP bridge internal state
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_cycles;            /**< Detection cycles processed */

    /* Precision tracking */
    float system_precision;               /**< Current system precision */
    float avg_worker_precision;           /**< Average per-worker precision */

    /* Surprise tracking */
    float avg_surprise;                   /**< Running average surprise */
    float max_surprise_seen;              /**< Maximum surprise observed */

    /* Belief state */
    float* expected_gradients;            /**< Expected gradient pattern */
    uint32_t gradient_dim;                /**< Gradient dimensionality */
} security_dist_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;               /**< Total update calls */
    uint64_t fep_updates;                 /**< FEP state updates */
    uint64_t security_updates;            /**< Security state updates */

    /* Detection statistics */
    uint64_t detections_processed;        /**< Total detections analyzed */
    uint64_t fep_triggered_quarantines;   /**< Quarantines from FEP threshold */
    uint64_t precision_adaptations;       /**< Precision update count */

    /* Free energy metrics */
    float avg_free_energy;                /**< Average free energy */
    float max_free_energy;                /**< Maximum free energy seen */
    float avg_surprise;                   /**< Average surprise */
    float current_precision;              /**< Current system precision */

    /* Byzantine metrics */
    float avg_byzantine_score;            /**< Average Byzantine score */
    uint64_t byzantine_events;            /**< Byzantine events detected */

    /* Performance */
    float avg_update_time_us;             /**< Average update time */
    uint64_t bio_async_messages_sent;     /**< Bio-async messages sent */
    uint64_t bio_async_messages_received; /**< Bio-async messages received */

    /* Connection status */
    bool fep_connected;                   /**< FEP system connected */
    bool security_connected;              /**< Security bridge connected */
    bool bio_async_connected;             /**< Bio-async connected */
} security_dist_fep_stats_t;

/**
 * @brief Security distributed training FEP bridge
 *
 * WHAT: Complete FEP-security integration bridge
 * WHY:  Enable free energy-based Byzantine detection
 * HOW:  Bidirectional effects, precision modulation, active defense
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge */

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    security_distributed_training_bridge_t* security_bridge; /**< Security bridge */

    /* Configuration */
    security_dist_fep_config_t config;

    /* Bidirectional effects */
    security_dist_fep_effects_t fep_effects;    /**< FEP → Security */
    fep_security_dist_effects_t security_effects; /**< Security → FEP */

    /* Internal state */
    security_dist_fep_state_t state;

    /* Statistics */
    security_dist_fep_stats_t stats;
} security_dist_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for FEP-security integration
 * WHY:  Simplify initialization with secure, biologically-plausible defaults
 * HOW:  Return struct with conservative thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int security_dist_fep_default_config(security_dist_fep_config_t* config);

/**
 * @brief Create security distributed training FEP bridge
 *
 * WHAT: Initialize FEP integration for distributed training security
 * WHY:  Enable free energy-based Byzantine detection
 * HOW:  Allocate bridge, connect systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @param fep_system FEP system handle
 * @param security_bridge Security bridge handle
 * @return Bridge handle or NULL on failure
 */
security_dist_fep_bridge_t* security_dist_fep_create(
    const security_dist_fep_config_t* config,
    fep_system_t* fep_system,
    security_distributed_training_bridge_t* security_bridge
);

/**
 * @brief Destroy security distributed training FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect bio-async, free allocations, destroy mutex
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_dist_fep_destroy(security_dist_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Allow reuse after training session
 * HOW:  Zero state/stats, keep config and connections
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_dist_fep_reset(security_dist_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int security_dist_fep_get_config(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Adjust behavior at runtime
 * HOW:  Copy config, validate, apply
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, error code on failure
 */
int security_dist_fep_set_config(
    security_dist_fep_bridge_t* bridge,
    const security_dist_fep_config_t* config
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security
 *
 * WHAT: Compute FEP-derived modulation of security parameters
 * WHY:  Free energy informs detection thresholds and precision
 * HOW:  Process current FEP state, update effects structure
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_dist_fep_compute_effects(security_dist_fep_bridge_t* bridge);

/**
 * @brief Update FEP from security detection
 *
 * WHAT: Feed security detection results to FEP
 * WHY:  Detection outcomes are observations for belief updates
 * HOW:  Convert detection to FEP observation, compute free energy
 *
 * @param bridge Bridge handle
 * @param worker_id Worker that was analyzed
 * @param byzantine_score Byzantine score for worker [0-1]
 * @param gradient_divergence Gradient divergence from consensus
 * @return 0 on success, error code on failure
 */
int security_dist_fep_update_from_detection(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    float byzantine_score,
    float gradient_divergence
);

/**
 * @brief Update worker precision from trust level
 *
 * WHAT: Map worker trust to FEP precision weight
 * WHY:  High-trust workers should have more influence
 * HOW:  Convert trust level to precision, update worker weights
 *
 * @param bridge Bridge handle
 * @param worker_id Worker identifier
 * @param trust_level Worker's current trust level
 * @return 0 on success, error code on failure
 */
int security_dist_fep_update_worker_precision(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    security_worker_trust_t trust_level
);

/**
 * @brief Report quarantine event to FEP
 *
 * WHAT: Inform FEP that a worker was quarantined
 * WHY:  Quarantine is a high-surprise response that updates beliefs
 * HOW:  Mark as confirmed Byzantine, update generative model
 *
 * @param bridge Bridge handle
 * @param worker_id Quarantined worker
 * @param byzantine_type Type of Byzantine behavior
 * @return 0 on success, error code on failure
 */
int security_dist_fep_report_quarantine(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    security_byzantine_type_t byzantine_type
);

/**
 * @brief Report false positive to FEP
 *
 * WHAT: Update FEP on corrected false positive
 * WHY:  Reduce precision to prevent similar false detections
 * HOW:  Lower precision for observation type, update beliefs
 *
 * @param bridge Bridge handle
 * @param worker_id Falsely flagged worker
 * @return 0 on success, error code on failure
 */
int security_dist_fep_report_false_positive(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int security_dist_fep_get_fep_effects(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int security_dist_fep_get_security_effects(
    const security_dist_fep_bridge_t* bridge,
    fep_security_dist_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int security_dist_fep_get_stats(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float security_dist_fep_get_free_energy(const security_dist_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0f on error
 */
float security_dist_fep_get_surprise(const security_dist_fep_bridge_t* bridge);

/**
 * @brief Get worker precision
 *
 * @param bridge Bridge handle
 * @param worker_id Worker identifier
 * @return Worker's precision weight or -1.0f on error
 */
float security_dist_fep_get_worker_precision(
    const security_dist_fep_bridge_t* bridge,
    const char* worker_id
);

/**
 * @brief Get Byzantine score from FEP
 *
 * WHAT: Compute FEP-based Byzantine score for current state
 * WHY:  Provides unified anomaly metric based on free energy
 * HOW:  Normalize free energy to [0-1] Byzantine score
 *
 * @param bridge Bridge handle
 * @return Byzantine score [0-1] or -1.0f on error
 */
float security_dist_fep_get_byzantine_score(const security_dist_fep_bridge_t* bridge);

/**
 * @brief Check if quarantine is recommended
 *
 * WHAT: Determine if current free energy warrants quarantine
 * WHY:  High surprise should trigger protective action
 * HOW:  Compare free energy to threshold
 *
 * @param bridge Bridge handle
 * @return true if quarantine recommended
 */
bool security_dist_fep_should_quarantine(const security_dist_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module Byzantine alert notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_dist_fep_connect_bio_async(security_dist_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_dist_fep_disconnect_bio_async(security_dist_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool security_dist_fep_is_bio_async_connected(const security_dist_fep_bridge_t* bridge);

/* ============================================================================
 * Debug API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Display human-readable bridge state
 * WHY:  Debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void security_dist_fep_print_summary(const security_dist_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_DISTRIBUTED_TRAINING_FEP_BRIDGE_H */
