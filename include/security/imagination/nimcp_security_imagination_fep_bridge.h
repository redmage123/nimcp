/**
 * @file nimcp_security_imagination_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Imagination Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for imagination security - confabulation as free energy
 * WHY:  Confabulated content represents high prediction error / surprise in FEP
 * HOW:  Map confabulation detection to free energy, use precision for sensitivity
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMAGINATION-FEP SECURITY FRAMEWORK:
 * -----------------------------------
 * The prefrontal cortex uses predictive processing to distinguish reality from
 * imagination. Confabulation occurs when the generative model produces content
 * that deviates significantly from prior expectations about reality.
 *
 * Key mappings:
 * - Confabulation score -> Free energy (high confab = high FE = surprising)
 * - Reality divergence -> Prediction error (deviation from expected reality)
 * - Fantasy-reality boundary violation -> Surprise level
 * - Reality anchoring -> Active inference response (grounding to facts)
 *
 * FEP INTEGRATION:
 * ```
 * Imagination Content -> Feature Extraction
 *         |
 *         v
 * Reality Model mu (expected factual patterns)
 *         |
 *         v
 * Prediction Error: epsilon = content - g(mu)
 *         |
 *         v
 * Free Energy F = Complexity + Inaccuracy
 *         |
 *         v
 * Confabulation Score = F / F_threshold
 *         |
 *         v
 * Detection Result + Active Inference Response
 * ```
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0) -> Grounded imagination (safe)
 * - Medium FE (2-5) -> Minor reality deviation (monitor)
 * - High FE (5-10) -> Significant confabulation (flag)
 * - Very high FE (>10) -> Critical confabulation (block/quarantine)
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |         SECURITY IMAGINATION - FEP BRIDGE (Confabulation Detection)       |
 * +===========================================================================+
 * |                                                                           |
 * |   +---------------------------+     +---------------------------+        |
 * |   |     FEP System            |<--->|  Security Imagination     |        |
 * |   |                           |     |  Bridge                   |        |
 * |   | * Free Energy             |     |                           |        |
 * |   | * Surprise                |     | * Confabulation Detection |        |
 * |   | * Precision               |     | * Reality Grounding       |        |
 * |   | * Prediction Error        |     | * Sandbox Management      |        |
 * |   +---------------------------+     +---------------------------+        |
 * |              |                               |                           |
 * |              v                               v                           |
 * |   +-------------------------------------------------------------+       |
 * |   |                BIDIRECTIONAL EFFECTS                         |       |
 * |   |                                                              |       |
 * |   |  FEP -> Security:                                            |       |
 * |   |    * Free energy -> Confabulation score                      |       |
 * |   |    * Prediction error -> Reality divergence                  |       |
 * |   |    * Surprise -> Boundary violation severity                 |       |
 * |   |    * Precision -> Detection sensitivity                      |       |
 * |   |                                                              |       |
 * |   |  Security -> FEP:                                            |       |
 * |   |    * Confabulations detected -> High-surprise observations   |       |
 * |   |    * Grounded content -> Update reality model                |       |
 * |   |    * False positives -> Reduce precision                     |       |
 * |   |    * Reality anchors -> Strengthen priors                    |       |
 * |   +-------------------------------------------------------------+       |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - bridge_base_t as first member
 * - FEP bridges return 0 for success, -1 for errors
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_IMAGINATION_FEP_BRIDGE_H
#define NIMCP_SECURITY_IMAGINATION_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/imagination/nimcp_security_imagination_bridge.h"
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

/** @brief Magic number for bridge validation */
#define SECURITY_IMAGINATION_FEP_BRIDGE_MAGIC 0x53494645  /* 'SIFE' */

/** @brief Bridge version */
#define SECURITY_IMAGINATION_FEP_BRIDGE_VERSION 0x0100

/** @brief Bio-async module ID */
#define BIO_MODULE_SECURITY_IMAGINATION_FEP 0x1601

/** @brief Free energy thresholds for confabulation mapping */
#define CONFAB_FEP_GROUNDED_THRESHOLD     2.0f   /**< Below = grounded/safe */
#define CONFAB_FEP_MINOR_THRESHOLD        5.0f   /**< Below = minor deviation */
#define CONFAB_FEP_SIGNIFICANT_THRESHOLD  10.0f  /**< Below = significant confab */
#define CONFAB_FEP_CRITICAL_THRESHOLD     20.0f  /**< Above = critical confab */

/** @brief Precision bounds for detection sensitivity */
#define CONFAB_FEP_MIN_PRECISION          0.1f
#define CONFAB_FEP_MAX_PRECISION          10.0f
#define CONFAB_FEP_DEFAULT_PRECISION      1.0f

/** @brief Reality grounding precision weight */
#define CONFAB_FEP_GROUNDING_WEIGHT       2.0f

/** @brief Active inference response rate */
#define CONFAB_FEP_INFERENCE_RATE         0.1f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Confabulation severity level from FEP mapping
 */
typedef enum {
    CONFAB_FEP_LEVEL_GROUNDED = 0,   /**< Grounded imagination (low FE) */
    CONFAB_FEP_LEVEL_MINOR,          /**< Minor reality deviation */
    CONFAB_FEP_LEVEL_SIGNIFICANT,    /**< Significant confabulation */
    CONFAB_FEP_LEVEL_CRITICAL        /**< Critical confabulation */
} confab_fep_severity_t;

/**
 * @brief Active inference response type
 */
typedef enum {
    ACTIVE_INFERENCE_NONE = 0,       /**< No action needed */
    ACTIVE_INFERENCE_MONITOR,        /**< Monitor content */
    ACTIVE_INFERENCE_GROUND,         /**< Apply reality grounding */
    ACTIVE_INFERENCE_RESTRICT,       /**< Restrict imagination */
    ACTIVE_INFERENCE_QUARANTINE      /**< Quarantine content */
} active_inference_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security imagination FEP bridge configuration
 *
 * WHAT: Configuration for FEP-based confabulation detection
 * WHY:  Control how FEP maps to security decisions
 * HOW:  Thresholds, precision bounds, learning rates
 */
typedef struct {
    /** FEP parameters */
    float confabulation_fe_threshold;     /**< Free energy threshold for confab */
    float divergence_fe_threshold;        /**< FE threshold for reality divergence */
    float surprise_threshold;             /**< Surprise level threshold */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /** Detection parameters */
    bool use_fep_detection;               /**< Use FEP for detection scoring */
    bool enable_precision_modulation;     /**< Adapt precision based on detections */
    float grounded_fe_threshold;          /**< FE threshold for grounded content */
    float critical_fe_threshold;          /**< FE threshold for critical confab */

    /** Active inference settings */
    bool enable_active_inference;         /**< Enable active inference responses */
    float inference_learning_rate;        /**< Active inference learning rate */
    bool auto_ground_on_detection;        /**< Auto-apply grounding on detection */

    /** Learning settings */
    bool enable_online_learning;          /**< Update FEP from detections */
    float reality_model_learning_rate;    /**< Reality model update rate */
    bool learn_from_false_positives;      /**< Update on FP feedback */
} security_imagination_fep_config_t;

/* ============================================================================
 * FEP Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on security imagination (FEP -> Security)
 *
 * WHAT: How FEP state modulates imagination security
 * WHY:  Use predictive processing for confabulation detection
 * HOW:  Map free energy components to security metrics
 */
typedef struct {
    float fep_confabulation_score;        /**< Confabulation score from FEP [0-1] */
    float fep_divergence_score;           /**< Reality divergence from FEP [0-1] */
    float fep_surprise_level;             /**< Surprise-based severity */
    float detection_sensitivity;          /**< Precision-based sensitivity */
    float confidence;                     /**< Detection confidence [0-1] */
    confab_fep_severity_t severity;       /**< Mapped severity level */
    active_inference_response_t response; /**< Recommended response */
} fep_to_security_effects_t;

/**
 * @brief Security imagination effects on FEP (Security -> FEP)
 *
 * WHAT: How security detections update the FEP model
 * WHY:  Security feedback improves reality model
 * HOW:  Report detections, update precision, strengthen priors
 */
typedef struct {
    uint64_t confabulations_detected;     /**< Total confabulations */
    uint64_t grounded_content;            /**< Grounded content samples */
    uint64_t false_positives;             /**< Known false positives */
    uint64_t quarantined_content;         /**< Quarantined content count */
    float avg_confabulation_score;        /**< Average confab score */
    float avg_divergence_score;           /**< Average divergence score */
    float peak_surprise;                  /**< Peak surprise observed */
} security_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Security imagination FEP bridge state
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    bool security_connected;              /**< Security bridge connected */
    bool fep_connected;                   /**< FEP system connected */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_count;             /**< Detections processed */
    float current_precision;              /**< Current precision level */
    float avg_free_energy;                /**< Running average free energy */
    float avg_surprise;                   /**< Running average surprise */
} security_imagination_fep_state_t;

/**
 * @brief Security imagination FEP bridge statistics
 */
typedef struct {
    /* Detection statistics */
    uint64_t total_checks;                /**< Total checks performed */
    uint64_t fep_based_detections;        /**< Detections using FEP */
    uint64_t confabulations_found;        /**< Confabulations detected */
    uint64_t grounding_operations;        /**< Reality grounding ops */
    uint64_t quarantine_operations;       /**< Quarantine operations */

    /* FEP metrics */
    float avg_free_energy;                /**< Average free energy */
    float avg_prediction_error;           /**< Average prediction error */
    float avg_surprise;                   /**< Average surprise */
    float peak_free_energy;               /**< Peak free energy observed */
    float current_precision;              /**< Current precision */

    /* Precision adaptation */
    uint64_t precision_adaptations;       /**< Precision updates */
    uint64_t false_positive_adjustments;  /**< FP-based adjustments */

    /* Active inference */
    uint64_t inference_responses;         /**< Active inference responses */
    uint64_t reality_model_updates;       /**< Reality model updates */
} security_imagination_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security imagination FEP bridge main structure
 *
 * WHAT: FEP integration for imagination security
 * WHY:  Use free energy principle for confabulation detection
 * HOW:  Map security metrics to FEP, bidirectional modulation
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge */

    /* Configuration */
    security_imagination_fep_config_t config;  /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    security_imagination_bridge_t* security_bridge;  /**< Security bridge */

    /* Bidirectional effects */
    fep_to_security_effects_t fep_effects;     /**< FEP -> Security effects */
    security_to_fep_effects_t security_effects; /**< Security -> FEP effects */

    /* State and statistics */
    security_imagination_fep_state_t state;    /**< Current state */
    security_imagination_fep_stats_t stats;    /**< Statistics */

    /* Internal working state */
    float last_free_energy;               /**< Last computed free energy */
    float last_prediction_error;          /**< Last prediction error */
    float last_surprise;                  /**< Last surprise level */
} security_imagination_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security imagination FEP configuration
 *
 * WHAT: Provide sensible defaults for imagination-FEP security
 * WHY:  Simplify initialization with biologically-plausible values
 * HOW:  Return defaults tuned for confabulation detection
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_default_config(
    security_imagination_fep_config_t* config
);

/**
 * @brief Create security imagination FEP bridge
 *
 * WHAT: Initialize FEP integration for imagination security
 * WHY:  Enable free-energy-based confabulation detection
 * HOW:  Connect FEP system to security bridge, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param security_bridge Security imagination bridge
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
security_imagination_fep_bridge_t* security_imagination_fep_create(
    const security_imagination_fep_config_t* config,
    security_imagination_bridge_t* security_bridge,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security imagination FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_imagination_fep_destroy(
    security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full reinitialization
 * HOW:  Clear stats/state, preserve connections and config
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_reset(
    security_imagination_fep_bridge_t* bridge
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_get_config(
    const security_imagination_fep_bridge_t* bridge,
    security_imagination_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of detection parameters
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_set_config(
    security_imagination_fep_bridge_t* bridge,
    const security_imagination_fep_config_t* config
);

/* ============================================================================
 * Core Processing API
 * ============================================================================ */

/**
 * @brief Compute FEP effects from current state
 *
 * WHAT: Process FEP system state into security effects
 * WHY:  Map free energy to confabulation metrics
 * HOW:  Read FEP state, compute scores, update effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_compute_effects(
    security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Update bridge from security detection
 *
 * WHAT: Feed security detection back to FEP
 * WHY:  Improve reality model from security feedback
 * HOW:  Convert detection to FEP observation, update model
 *
 * @param bridge Bridge handle
 * @param confab_result Confabulation detection result
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_update_from_detection(
    security_imagination_fep_bridge_t* bridge,
    const security_imagination_confab_result_t* confab_result
);

/**
 * @brief Detect confabulation using FEP-enhanced detection
 *
 * WHAT: Analyze content using FEP and security methods
 * WHY:  Combine complementary approaches for better detection
 * HOW:  Run both detectors, fuse scores, determine response
 *
 * @param bridge Bridge handle
 * @param content Content to analyze
 * @param content_size Content size
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_detect(
    security_imagination_fep_bridge_t* bridge,
    const void* content,
    size_t content_size,
    security_imagination_confab_result_t* result
);

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adjust detection precision based on FEP state
 * WHY:  Adapt sensitivity to current surprise levels
 * HOW:  Modulate precision, update thresholds
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_apply_precision(
    security_imagination_fep_bridge_t* bridge
);

/* ============================================================================
 * Reality Grounding API
 * ============================================================================ */

/**
 * @brief Ground imagination to reality using FEP
 *
 * WHAT: Apply active inference to ground imagination
 * WHY:  Reduce divergence from reality model
 * HOW:  Strengthen reality priors, reduce prediction error
 *
 * @param bridge Bridge handle
 * @param sandbox_id Sandbox to ground
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_ground(
    security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id
);

/**
 * @brief Get grounding strength from FEP
 *
 * WHAT: Compute how well grounded imagination is
 * WHY:  Quantify connection to reality model
 * HOW:  Inverse of free energy / prediction error
 *
 * @param bridge Bridge handle
 * @param sandbox_id Sandbox to check
 * @return Grounding strength [0-1], negative on error
 */
float security_imagination_fep_get_grounding(
    const security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id
);

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

/**
 * @brief Compute active inference response
 *
 * WHAT: Determine appropriate response to confabulation
 * WHY:  Active inference minimizes future free energy
 * HOW:  Evaluate response options, select min EFE
 *
 * @param bridge Bridge handle
 * @param severity Detected severity level
 * @return Recommended response action
 */
active_inference_response_t security_imagination_fep_compute_response(
    security_imagination_fep_bridge_t* bridge,
    confab_fep_severity_t severity
);

/**
 * @brief Execute active inference response
 *
 * WHAT: Apply the computed response
 * WHY:  Actually reduce free energy through action
 * HOW:  Dispatch to appropriate security action
 *
 * @param bridge Bridge handle
 * @param sandbox_id Target sandbox
 * @param response Response to execute
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_execute_response(
    security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id,
    active_inference_response_t response
);

/* ============================================================================
 * Feedback API
 * ============================================================================ */

/**
 * @brief Report false positive to update model
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for this observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_report_false_positive(
    security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Report confirmed confabulation
 *
 * WHAT: Confirm detection was correct
 * WHY:  Strengthen model for similar patterns
 * HOW:  Increase precision for this observation type
 *
 * @param bridge Bridge handle
 * @param confab_type Type of confabulation confirmed
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_confirm_confabulation(
    security_imagination_fep_bridge_t* bridge,
    confabulation_type_t confab_type
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_get_fep_effects(
    const security_imagination_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_get_security_effects(
    const security_imagination_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_get_stats(
    const security_imagination_fep_bridge_t* bridge,
    security_imagination_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy, negative on error
 */
float security_imagination_fep_get_free_energy(
    const security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Get current confabulation score from FEP
 *
 * @param bridge Bridge handle
 * @return Confabulation score [0-1], negative on error
 */
float security_imagination_fep_get_confab_score(
    const security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Get current severity level
 *
 * @param bridge Bridge handle
 * @return Current severity level
 */
confab_fep_severity_t security_imagination_fep_get_severity(
    const security_imagination_fep_bridge_t* bridge
);

/* ============================================================================
 * Debug/Diagnostic API
 * ============================================================================ */

/**
 * @brief Print bridge summary
 *
 * WHAT: Print human-readable bridge state summary
 * WHY:  Debugging and monitoring
 * HOW:  Format and output key metrics
 *
 * @param bridge Bridge handle
 */
void security_imagination_fep_print_summary(
    const security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Get severity level as string
 *
 * @param severity Severity level
 * @return Human-readable string
 */
const char* confab_fep_severity_to_string(confab_fep_severity_t severity);

/**
 * @brief Get response type as string
 *
 * @param response Response type
 * @return Human-readable string
 */
const char* active_inference_response_to_string(
    active_inference_response_t response
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_connect_bio_async(
    security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_imagination_fep_disconnect_bio_async(
    security_imagination_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool security_imagination_fep_is_bio_async_connected(
    const security_imagination_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_IMAGINATION_FEP_BRIDGE_H */
