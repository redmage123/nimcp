/**
 * @file nimcp_security_kg_fep_bridge.h
 * @brief Free Energy Principle Bridge for Security Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for knowledge graph security operations
 * WHY:  Malicious queries represent high-surprise deviations from expected patterns
 * HOW:  Map injection detection to free energy, use precision for sensitivity tuning
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * QUERY INJECTION AS SURPRISE:
 * ----------------------------
 * In the FEP framework, the brain maintains a generative model of expected inputs.
 * Malicious queries are high-surprise observations that deviate from learned patterns:
 * - Normal queries = low free energy (expected, predictable patterns)
 * - Injection attempts = high free energy (surprising, anomalous patterns)
 * - Schema violations = extreme surprise (impossible under generative model)
 *
 * The security system acts as a predictive filter, learning the statistical
 * regularities of legitimate queries and flagging high-surprise deviations.
 *
 * FEP INTEGRATION MAPPINGS:
 * -------------------------
 * ```
 * Security Metric          → FEP Metric
 * ─────────────────────────────────────────────────────────────
 * Injection anomaly score  → Free energy F
 * Query pattern deviation  → Prediction error ε = o - g(μ)
 * Schema violation severity→ Surprise -ln p(o)
 * Query sanitization       → Active inference (action to reduce FE)
 * Detection confidence     → Precision Π
 * Traversal depth limit    → Exploration bound (EFE planning horizon)
 * ```
 *
 * PRECISION-WEIGHTED DETECTION:
 * -----------------------------
 * Detection sensitivity is modulated by precision:
 * - High precision = high sensitivity (more false positives)
 * - Low precision = low sensitivity (more false negatives)
 *
 * Precision adapts based on detection feedback:
 * - Confirmed attacks → increase precision (heightened alertness)
 * - False positives → decrease precision (reduce over-sensitivity)
 * - Normal operations → gradual precision decay (relaxation)
 *
 * ACTIVE INFERENCE FOR SECURITY:
 * ------------------------------
 * The system uses active inference to:
 * 1. PREDICT expected query patterns
 * 2. DETECT deviations (prediction errors)
 * 3. ACT to minimize free energy (sanitize, block, alert)
 * 4. UPDATE generative model from confirmed attacks/normal traffic
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║        SECURITY KG - FEP BRIDGE (Surprise-Based Threat Detection)         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                       ║
 * ║   │  FEP System      │────────▶│  Security KG     │                       ║
 * ║   │                  │         │  Bridge          │                       ║
 * ║   │ • Free Energy    │         │                  │                       ║
 * ║   │ • Surprise       │         │ • Injection Det  │                       ║
 * ║   │ • Precision      │         │ • Traversal Ctrl │                       ║
 * ║   │ • Active Inf     │         │ • Integrity Ver  │                       ║
 * ║   └──────────────────┘         └──────────────────┘                       ║
 * ║           ↓                              ↓                                 ║
 * ║   ┌──────────────────────────────────────────────────────────────┐        ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │        ║
 * ║   │                                                              │        ║
 * ║   │  FEP → Security:                                             │        ║
 * ║   │    • Free energy → Threat level                              │        ║
 * ║   │    • Surprise → Detection threshold                          │        ║
 * ║   │    • Precision → Detection sensitivity                       │        ║
 * ║   │    • EFE → Traversal depth limit                             │        ║
 * ║   │                                                              │        ║
 * ║   │  Security → FEP:                                             │        ║
 * ║   │    • Injection detected → High-surprise observation          │        ║
 * ║   │    • Normal queries → Update generative model                │        ║
 * ║   │    • False positives → Reduce precision                      │        ║
 * ║   │    • Schema violations → Maximum surprise signal             │        ║
 * ║   └──────────────────────────────────────────────────────────────┘        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Bogacz, R. (2017) "A tutorial on the free-energy framework"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_KG_FEP_BRIDGE_H
#define NIMCP_SECURITY_KG_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security modules */
#include "security/knowledge/nimcp_security_knowledge_graph_bridge.h"

/* FEP module */
#include "cognitive/free_energy/nimcp_free_energy.h"

/* Bio-async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/* Utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for threat levels */
#define SEC_KG_FEP_NORMAL_FE_THRESHOLD     2.0f   /**< Normal query pattern */
#define SEC_KG_FEP_SUSPICIOUS_FE_THRESHOLD 5.0f   /**< Suspicious deviation */
#define SEC_KG_FEP_THREAT_FE_THRESHOLD     10.0f  /**< Active threat */
#define SEC_KG_FEP_CRITICAL_FE_THRESHOLD   20.0f  /**< Critical security event */

/** Precision bounds for detection sensitivity */
#define SEC_KG_FEP_MIN_PRECISION           0.1f   /**< Minimum sensitivity */
#define SEC_KG_FEP_MAX_PRECISION           10.0f  /**< Maximum sensitivity */
#define SEC_KG_FEP_DEFAULT_PRECISION       1.0f   /**< Baseline sensitivity */

/** Surprise thresholds for schema violations */
#define SEC_KG_FEP_LOW_SURPRISE            2.0f   /**< Minor deviation */
#define SEC_KG_FEP_MEDIUM_SURPRISE         5.0f   /**< Moderate deviation */
#define SEC_KG_FEP_HIGH_SURPRISE           10.0f  /**< Severe deviation */
#define SEC_KG_FEP_EXTREME_SURPRISE        20.0f  /**< Complete violation */

/** Bio-async module identification */
#define BIO_MODULE_SEC_KG_FEP              0x4021 /**< Security KG FEP bridge */
#define SEC_KG_FEP_MODULE_NAME             "security_kg_fep_bridge"

/** Default learning rates */
#define SEC_KG_FEP_DEFAULT_BELIEF_LR       0.05f  /**< Belief update rate */
#define SEC_KG_FEP_DEFAULT_PRECISION_LR    0.02f  /**< Precision adaptation rate */
#define SEC_KG_FEP_DEFAULT_THREAT_DECAY    0.95f  /**< Threat level decay rate */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief FEP-based threat classification
 *
 * WHAT: Threat levels derived from free energy analysis
 * WHY:  Categorical threat assessment for response selection
 */
typedef enum {
    SEC_KG_FEP_THREAT_NONE = 0,        /**< No threat detected (low FE) */
    SEC_KG_FEP_THREAT_LOW,             /**< Minor anomaly (slightly elevated FE) */
    SEC_KG_FEP_THREAT_MEDIUM,          /**< Suspicious pattern (moderate FE) */
    SEC_KG_FEP_THREAT_HIGH,            /**< Active threat (high FE) */
    SEC_KG_FEP_THREAT_CRITICAL         /**< Critical attack (extreme FE) */
} sec_kg_fep_threat_level_t;

/**
 * @brief Security response actions (active inference)
 *
 * WHAT: Actions to reduce free energy in security context
 * WHY:  Active inference requires action selection to minimize EFE
 */
typedef enum {
    SEC_KG_FEP_ACTION_NONE = 0,        /**< No action needed */
    SEC_KG_FEP_ACTION_LOG,             /**< Log for review */
    SEC_KG_FEP_ACTION_SANITIZE,        /**< Sanitize query */
    SEC_KG_FEP_ACTION_THROTTLE,        /**< Rate limit source */
    SEC_KG_FEP_ACTION_BLOCK,           /**< Block query */
    SEC_KG_FEP_ACTION_LOCKDOWN         /**< Full security lockdown */
} sec_kg_fep_action_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security KG FEP bridge configuration
 *
 * WHAT: Configuration parameters for FEP-security integration
 * WHY:  Tune sensitivity, learning rates, and response thresholds
 * HOW:  Set thresholds and enable/disable features
 */
typedef struct {
    /* FEP parameters */
    float injection_fe_threshold;      /**< FE threshold for injection detection */
    float traversal_fe_threshold;      /**< FE threshold for traversal anomaly */
    float schema_surprise_threshold;   /**< Surprise threshold for schema violation */
    float precision_learning_rate;     /**< Rate for precision adaptation */
    float belief_learning_rate;        /**< Rate for generative model update */

    /* Detection sensitivity */
    float initial_precision;           /**< Starting precision level */
    bool enable_precision_modulation;  /**< Adapt precision from feedback */
    bool enable_fep_scoring;           /**< Use FEP for threat scoring */

    /* Active inference settings */
    bool enable_active_inference;      /**< Enable action selection */
    float action_threshold;            /**< EFE threshold for action */
    float threat_decay_rate;           /**< Decay rate for threat levels */

    /* Learning settings */
    bool enable_online_learning;       /**< Update model from detections */
    bool learn_from_false_positives;   /**< Reduce precision on FP */
    bool learn_from_confirmed_attacks; /**< Increase precision on true positives */

    /* Integration settings */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    bool enable_detailed_logging;      /**< Verbose logging */
} sec_kg_fep_config_t;

/* ============================================================================
 * Effects Structures (Bidirectional Flow)
 * ============================================================================ */

/**
 * @brief FEP to Security effects
 *
 * WHAT: Effects flowing from FEP system to security operations
 * WHY:  FEP metrics modulate security sensitivity and thresholds
 * HOW:  Map free energy, surprise, precision to security parameters
 */
typedef struct {
    /* Threat assessment */
    float free_energy_score;           /**< Current free energy [0-∞) */
    float surprise_score;              /**< Surprise level [0-∞) */
    float normalized_threat;           /**< Normalized threat [0-1] */
    sec_kg_fep_threat_level_t threat_level; /**< Categorical threat level */

    /* Detection modulation */
    float detection_sensitivity;       /**< Precision-based sensitivity [0-1] */
    float injection_threshold_adj;     /**< Threshold adjustment factor */
    float traversal_threshold_adj;     /**< Traversal threshold adjustment */

    /* Recommended actions */
    sec_kg_fep_action_t recommended_action; /**< Suggested response action */
    float action_urgency;              /**< How urgent is action [0-1] */

    /* Confidence metrics */
    float detection_confidence;        /**< Confidence in current assessment */
    float model_certainty;             /**< Certainty of generative model */
} fep_to_sec_kg_effects_t;

/**
 * @brief Security to FEP effects
 *
 * WHAT: Effects flowing from security to FEP system
 * WHY:  Security detections become observations for FEP
 * HOW:  Convert security events to prediction errors and surprise
 */
typedef struct {
    /* Detection feedback */
    uint64_t queries_analyzed;         /**< Total queries processed */
    uint64_t injections_detected;      /**< Injection attempts found */
    uint64_t schema_violations;        /**< Schema violations found */
    uint64_t traversal_violations;     /**< Traversal limit violations */

    /* Score aggregates */
    float avg_injection_score;         /**< Average injection score */
    float avg_traversal_score;         /**< Average traversal anomaly */
    float avg_schema_deviation;        /**< Average schema deviation */

    /* False positive tracking */
    uint64_t false_positives;          /**< Known false positives */
    float estimated_precision;         /**< Estimated detection precision */

    /* Query pattern statistics */
    float query_complexity_avg;        /**< Average query complexity */
    float traversal_depth_avg;         /**< Average traversal depth */
    bool anomaly_trend_rising;         /**< Are anomalies increasing? */
} sec_kg_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge operational state
 *
 * WHAT: Current state of FEP-security bridge
 * WHY:  Track active processing and connection status
 */
typedef struct {
    bool active;                       /**< Bridge is operational */
    bool fep_connected;                /**< FEP system connected */
    bool sec_kg_connected;             /**< Security KG bridge connected */

    /* Current FEP state */
    float current_free_energy;         /**< Current free energy level */
    float current_surprise;            /**< Current surprise level */
    float current_precision;           /**< Current precision level */

    /* Threat tracking */
    sec_kg_fep_threat_level_t current_threat; /**< Current threat level */
    uint64_t threat_start_time;        /**< When current threat began */
    float threat_peak;                 /**< Peak threat level seen */

    /* Update tracking */
    uint64_t last_update_time_us;      /**< Last update timestamp */
    uint64_t update_count;             /**< Total updates performed */
} sec_kg_fep_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Cumulative statistics for bridge operation
 * WHY:  Monitor effectiveness and tune parameters
 */
typedef struct {
    /* Processing counts */
    uint64_t total_updates;            /**< Total update cycles */
    uint64_t fep_computations;         /**< FEP computations performed */
    uint64_t action_selections;        /**< Active inference selections */

    /* Detection statistics */
    uint64_t threats_detected;         /**< Total threats via FEP */
    uint64_t threats_by_level[5];      /**< Threats per level */
    uint64_t actions_taken[6];         /**< Actions taken by type */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float avg_surprise;                /**< Average surprise */
    float avg_precision;               /**< Average precision */
    float max_free_energy;             /**< Maximum FE observed */
    float max_surprise;                /**< Maximum surprise observed */

    /* Learning statistics */
    uint64_t precision_adaptations;    /**< Precision adjustments made */
    uint64_t belief_updates;           /**< Generative model updates */
    uint64_t false_positive_corrections; /**< FP-based corrections */

    /* Performance */
    float avg_update_time_us;          /**< Average update time */
    float max_update_time_us;          /**< Maximum update time */
} sec_kg_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Knowledge Graph FEP Bridge
 *
 * WHAT: FEP integration for knowledge graph security
 * WHY:  Use predictive processing for threat detection
 * HOW:  Connect FEP system to security KG bridge
 */
typedef struct {
    /* Base bridge - MUST be first member */
    bridge_base_t base;

    /* Connected systems */
    fep_system_t* fep_system;          /**< Connected FEP system */
    security_kg_bridge_t* sec_kg;      /**< Connected security KG bridge */

    /* Configuration */
    sec_kg_fep_config_t config;        /**< Bridge configuration */

    /* Bidirectional effects */
    fep_to_sec_kg_effects_t fep_effects;   /**< FEP→Security effects */
    sec_kg_to_fep_effects_t sec_effects;   /**< Security→FEP effects */

    /* State and statistics */
    sec_kg_fep_state_t state;          /**< Current state */
    sec_kg_fep_stats_t stats;          /**< Cumulative statistics */
} sec_kg_fep_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for FEP-security integration
 * WHY:  Easy initialization with balanced sensitivity
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_default_config(sec_kg_fep_config_t* config);

/**
 * @brief Get configuration
 *
 * WHAT: Retrieve current bridge configuration
 * WHY:  Allow inspection of active settings
 * HOW:  Copy current config to output
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_config(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Apply new config, validate values
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_set_config(
    sec_kg_fep_bridge_t* bridge,
    const sec_kg_fep_config_t* config
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create security KG FEP bridge
 *
 * WHAT: Allocate and initialize FEP-security bridge
 * WHY:  Enable surprise-based threat detection for KG
 * HOW:  Allocate structure, initialize base, apply config
 *
 * @param config Configuration (NULL for defaults)
 * @param sec_kg Security KG bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sec_kg_fep_bridge_t* sec_kg_fep_create(
    const sec_kg_fep_config_t* config,
    security_kg_bridge_t* sec_kg,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security KG FEP bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect bio-async, cleanup base, free memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_kg_fep_destroy(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Clear accumulated state for fresh start
 * HOW:  Zero state, reset precision to default
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_reset(sec_kg_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security
 *
 * WHAT: Calculate FEP-derived security modulation
 * WHY:  Map free energy to threat assessment
 * HOW:  Get FEP state, compute threat level, set sensitivity
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_compute_effects(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Update from security detection
 *
 * WHAT: Process security detection as FEP observation
 * WHY:  Feed security events back to FEP system
 * HOW:  Convert detection to prediction error, update beliefs
 *
 * @param bridge Bridge handle
 * @param query_result Result from query validation
 * @param injection_score Injection anomaly score [0-1]
 * @param schema_deviation Schema deviation severity [0-1]
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_update_from_detection(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_query_result_t query_result,
    float injection_score,
    float schema_deviation
);

/**
 * @brief Update from traversal event
 *
 * WHAT: Process traversal access check as FEP observation
 * WHY:  Traversal patterns inform threat model
 * HOW:  Convert traversal metrics to prediction error
 *
 * @param bridge Bridge handle
 * @param traversal_result Result from traversal check
 * @param depth_reached Traversal depth reached
 * @param anomaly_score Traversal anomaly score [0-1]
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_update_from_traversal(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_traversal_result_t traversal_result,
    uint32_t depth_reached,
    float anomaly_score
);

/**
 * @brief Full bidirectional update cycle
 *
 * WHAT: Perform complete update in both directions
 * WHY:  Convenience for regular update cycles
 * HOW:  Compute effects, apply modulation, update model
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_update(sec_kg_fep_bridge_t* bridge);

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

/**
 * @brief Select security action via active inference
 *
 * WHAT: Choose action to minimize expected free energy
 * WHY:  Active inference for security response
 * HOW:  Evaluate EFE for each action, select minimum
 *
 * @param bridge Bridge handle
 * @param action Output: selected action
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_select_action(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_action_t* action
);

/**
 * @brief Get expected free energy for action
 *
 * WHAT: Compute EFE for specific action
 * WHY:  Evaluate action before selection
 * HOW:  Project future states, compute expected surprise
 *
 * @param bridge Bridge handle
 * @param action Action to evaluate
 * @param efe Output: expected free energy
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_action_efe(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_action_t action,
    float* efe
);

/* ============================================================================
 * Precision Modulation API
 * ============================================================================ */

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adjust detection sensitivity via precision
 * WHY:  Adapt to current threat environment
 * HOW:  Scale thresholds based on precision level
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_apply_precision_modulation(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Report false positive
 *
 * WHAT: Report detection as false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Decrease precision proportionally
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_report_false_positive(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Report confirmed attack
 *
 * WHAT: Report detection as confirmed true positive
 * WHY:  Increase precision for heightened alertness
 * HOW:  Increase precision, update generative model
 *
 * @param bridge Bridge handle
 * @param severity Attack severity [0-1]
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_report_confirmed_attack(
    sec_kg_fep_bridge_t* bridge,
    float severity
);

/**
 * @brief Set precision level directly
 *
 * WHAT: Override precision to specific value
 * WHY:  Manual sensitivity tuning
 * HOW:  Clamp to valid range and apply
 *
 * @param bridge Bridge handle
 * @param precision New precision value [MIN, MAX]
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_set_precision(
    sec_kg_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Get current precision
 *
 * @param bridge Bridge handle
 * @return Current precision or -1.0 on error
 */
float sec_kg_fep_get_precision(const sec_kg_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP to security effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_fep_effects(
    const sec_kg_fep_bridge_t* bridge,
    fep_to_sec_kg_effects_t* effects
);

/**
 * @brief Get security to FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_sec_effects(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_to_fep_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_state(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_get_stats(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_stats_t* stats
);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat level or -1 on error
 */
sec_kg_fep_threat_level_t sec_kg_fep_get_threat_level(
    const sec_kg_fep_bridge_t* bridge
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0 on error
 */
float sec_kg_fep_get_free_energy(const sec_kg_fep_bridge_t* bridge);

/**
 * @brief Get current surprise
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0 on error
 */
float sec_kg_fep_get_surprise(const sec_kg_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_connect_bio_async(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_kg_fep_disconnect_bio_async(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_kg_fep_is_bio_async_connected(const sec_kg_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle async security notifications
 * HOW:  Drain inbox, process each message
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int sec_kg_fep_process_messages(sec_kg_fep_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Print bridge summary
 *
 * WHAT: Output human-readable bridge summary
 * WHY:  Debugging and monitoring
 * HOW:  Format state and stats to logging
 *
 * @param bridge Bridge handle
 */
void sec_kg_fep_print_summary(const sec_kg_fep_bridge_t* bridge);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear cumulative statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero out stats structure
 *
 * @param bridge Bridge handle
 */
void sec_kg_fep_reset_stats(sec_kg_fep_bridge_t* bridge);

/**
 * @brief Get threat level name
 *
 * @param level Threat level
 * @return Human-readable name string
 */
const char* sec_kg_fep_threat_level_name(sec_kg_fep_threat_level_t level);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return Human-readable name string
 */
const char* sec_kg_fep_action_name(sec_kg_fep_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_KG_FEP_BRIDGE_H */
