/**
 * @file nimcp_anomaly_detector_fep_bridge.h
 * @brief Free Energy Principle bridge for Anomaly Detector
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: FEP integration for anomaly detection - surprise as anomaly metric
 * WHY:  Anomalies are high-surprise observations in FEP framework
 * HOW:  Map anomaly scores to free energy, use prediction errors for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ANOMALY DETECTION AS SURPRISE DETECTION:
 * - Normal behavior = low free energy (expected observations)
 * - Anomalies = high free energy (surprising/unexpected patterns)
 * - Bayesian network priors = generative model p(o,s)
 * - Anomaly threshold = surprise threshold
 *
 * FEP INTEGRATION:
 * ```
 * Input Observation (o) → Feature Extraction
 *         ↓
 * Expected Pattern μ (learned generative model)
 *         ↓
 * Prediction Error: ε = o - g(μ)
 *         ↓
 * Free Energy F = Complexity + Inaccuracy
 *         ↓
 * Surprise = -ln p(o) ≤ F
 *         ↓
 * Anomaly Score = F / F_threshold
 * ```
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0) → Normal behavior
 * - Medium FE (2-5) → Suspicious (flag for review)
 * - High FE (5-10) → Anomalous (alert)
 * - Very high FE (>10) → Critical anomaly (quarantine)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           ANOMALY DETECTOR - FEP BRIDGE (Surprise Detection)              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  FEP System      │────────▶│  Anomaly         │                      ║
 * ║   │                  │         │  Detector        │                      ║
 * ║   │ • Free Energy    │         │                  │                      ║
 * ║   │ • Surprise       │         │ • Bayesian Net   │                      ║
 * ║   │ • Precision      │         │ • Feature Extract│                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   ┌──────────────────────────────────────────────────────────────┐       ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │       ║
 * ║   │                                                              │       ║
 * ║   │  FEP → Anomaly:                                              │       ║
 * ║   │    • Free energy → Anomaly score                             │       ║
 * ║   │    • Surprise → Anomaly threshold                            │       ║
 * ║   │    • Precision → Detection sensitivity                       │       ║
 * ║   │                                                              │       ║
 * ║   │  Anomaly → FEP:                                              │       ║
 * ║   │    • Detected anomalies → High-surprise observations         │       ║
 * ║   │    • Normal samples → Update generative model                │       ║
 * ║   │    • False positives → Reduce precision                      │       ║
 * ║   └──────────────────────────────────────────────────────────────┘       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ANOMALY_DETECTOR_FEP_BRIDGE_H
#define NIMCP_ANOMALY_DETECTOR_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "security/nimcp_anomaly_detector.h"
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

/** Anomaly score thresholds based on free energy */
#define ANOMALY_FEP_NORMAL_THRESHOLD      2.0f
#define ANOMALY_FEP_SUSPICIOUS_THRESHOLD  5.0f
#define ANOMALY_FEP_ANOMALOUS_THRESHOLD   10.0f
#define ANOMALY_FEP_CRITICAL_THRESHOLD    20.0f

/** Precision bounds for detection sensitivity */
#define ANOMALY_FEP_MIN_PRECISION         0.1f
#define ANOMALY_FEP_MAX_PRECISION         10.0f
#define ANOMALY_FEP_DEFAULT_PRECISION     1.0f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Anomaly detector FEP configuration
 */
typedef struct {
    /** FEP parameters */
    float anomaly_fe_threshold;          /**< Free energy threshold for anomaly */
    float surprise_threshold;            /**< Surprise threshold */
    float precision_learning_rate;       /**< Precision adaptation rate */

    /** Detection parameters */
    bool use_fep_scoring;                /**< Use FEP for anomaly scoring */
    bool enable_precision_modulation;    /**< Adapt precision based on detections */
    float normal_fe_threshold;           /**< FE threshold for normal */
    float critical_fe_threshold;         /**< FE threshold for critical */

    /** Learning */
    bool enable_online_learning;         /**< Update FEP from detections */
    float learning_rate;                 /**< Belief update rate */
    bool learn_from_false_positives;     /**< Update on FP feedback */
} anomaly_fep_config_t;

/**
 * @brief FEP effects on anomaly detector (FEP → Anomaly)
 */
typedef struct {
    float fep_anomaly_score;             /**< Anomaly score from FEP */
    float surprise_score;                /**< Surprise-based score */
    float detection_sensitivity;         /**< Precision-based sensitivity */
    float confidence;                    /**< Detection confidence */
} anomaly_fep_effects_t;

/**
 * @brief Anomaly detector effects on FEP (Anomaly → FEP)
 */
typedef struct {
    uint64_t anomalies_detected;         /**< Total anomalies */
    uint64_t normal_samples;             /**< Normal samples */
    uint64_t false_positives;            /**< Known false positives */
    float avg_anomaly_score;             /**< Average anomaly score */
} fep_anomaly_effects_t;

/**
 * @brief Anomaly FEP state
 */
typedef struct {
    bool active;                         /**< Whether bridge is active */
    uint64_t update_count;               /**< Number of updates */
    uint64_t detection_count;            /**< Detections processed */
    float current_precision;             /**< Current precision level */
    float avg_surprise;                  /**< Running average surprise */
} anomaly_fep_state_t;

/**
 * @brief Anomaly FEP statistics
 */
typedef struct {
    uint64_t total_detections;           /**< Total detections run */
    uint64_t fep_based_detections;       /**< Detections using FEP */
    uint64_t anomalies_found;            /**< Anomalies found */
    uint64_t precision_adaptations;      /**< Precision updates */
    float avg_free_energy;               /**< Average free energy */
    float avg_surprise;                  /**< Average surprise */
    float current_precision;             /**< Current precision */
} anomaly_fep_stats_t;

/**
 * @brief Anomaly detector FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    anomaly_fep_config_t config;         /**< Configuration */
    fep_system_t* fep_system;            /**< FEP system */
    nimcp_anomaly_detector_t detector;   /**< Anomaly detector */

    anomaly_fep_effects_t fep_effects;   /**< FEP → Anomaly effects */
    fep_anomaly_effects_t anomaly_effects; /**< Anomaly → FEP effects */

    anomaly_fep_state_t state;           /**< Current state */
    anomaly_fep_stats_t stats;           /**< Statistics */} anomaly_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default anomaly FEP configuration
 *
 * WHAT: Provide sensible defaults for anomaly-FEP integration
 * WHY:  Simplify initialization
 * HOW:  Return biologically-plausible defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int anomaly_fep_default_config(anomaly_fep_config_t* config);

/**
 * @brief Create anomaly FEP bridge
 *
 * WHAT: Initialize FEP integration for anomaly detection
 * WHY:  Enable surprise-based anomaly detection
 * HOW:  Connect FEP system to detector, allocate structures
 *
 * @param config Configuration
 * @param detector Anomaly detector handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
anomaly_fep_bridge_t* anomaly_fep_create(
    const anomaly_fep_config_t* config,
    nimcp_anomaly_detector_t detector,
    fep_system_t* fep_system
);

/**
 * @brief Destroy anomaly FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void anomaly_fep_destroy(anomaly_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on anomaly detector
 *
 * WHAT: Compute FEP-derived anomaly scores
 * WHY:  Use free energy for anomaly detection
 * HOW:  Process current FEP state, update effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int anomaly_fep_update(anomaly_fep_bridge_t* bridge);

/**
 * @brief Detect anomaly using FEP-enhanced detection
 *
 * WHAT: Analyze input using both Bayesian and FEP methods
 * WHY:  Combine complementary approaches for better detection
 * HOW:  Run both detectors, fuse scores
 *
 * @param bridge Bridge handle
 * @param input Input data to analyze
 * @param input_len Length of input
 * @param result Output detection result
 * @return 0 on success, error code on failure
 */
int anomaly_fep_detect(
    anomaly_fep_bridge_t* bridge,
    const void* input,
    size_t input_len,
    nimcp_anomaly_result_t* result
);

/**
 * @brief Apply FEP modulation to detector
 *
 * WHAT: Adjust detector parameters based on FEP state
 * WHY:  Adapt sensitivity to current surprise levels
 * HOW:  Modulate precision, update thresholds
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int anomaly_fep_apply_modulation(anomaly_fep_bridge_t* bridge);

/**
 * @brief Report detection result to FEP
 *
 * WHAT: Feed detection back to FEP for learning
 * WHY:  Update generative model from detections
 * HOW:  Convert detection to FEP observation, process
 *
 * @param bridge Bridge handle
 * @param is_anomaly Whether anomaly was detected
 * @param confidence Detection confidence
 * @return 0 on success, error code on failure
 */
int anomaly_fep_report_detection(
    anomaly_fep_bridge_t* bridge,
    bool is_anomaly,
    float confidence
);

/**
 * @brief Report false positive to FEP
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for this observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int anomaly_fep_report_false_positive(anomaly_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on anomaly detector
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int anomaly_fep_get_effects(
    const anomaly_fep_bridge_t* bridge,
    anomaly_fep_effects_t* effects
);

/**
 * @brief Get anomaly effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int anomaly_fep_get_anomaly_effects(
    const anomaly_fep_bridge_t* bridge,
    fep_anomaly_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int anomaly_fep_get_stats(
    const anomaly_fep_bridge_t* bridge,
    anomaly_fep_stats_t* stats
);

/**
 * @brief Get current anomaly score
 *
 * @param bridge Bridge handle
 * @return Current anomaly score [0, 1] or -1 on error
 */
float anomaly_fep_get_anomaly_score(const anomaly_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module anomaly notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int anomaly_fep_connect_bio_async(anomaly_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int anomaly_fep_disconnect_bio_async(anomaly_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool anomaly_fep_is_bio_async_connected(const anomaly_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANOMALY_DETECTOR_FEP_BRIDGE_H */
