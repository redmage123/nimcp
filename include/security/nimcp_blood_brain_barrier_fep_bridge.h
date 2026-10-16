/**
 * @file nimcp_blood_brain_barrier_fep_bridge.h
 * @brief Free Energy Principle bridge for Blood-Brain Barrier security
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: FEP integration for BBB security - threat detection as surprise
 * WHY:  BBB enforces selective permeability (precision filtering) under FEP
 * HOW:  Map threats to prediction errors, validation to precision weighting
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * BBB AS PRECISION FILTER:
 * - BBB tight junctions = high precision (low variance) expectations
 * - Valid inputs = low prediction error (expected observations)
 * - Threats = high prediction error (surprising/unexpected patterns)
 * - Quarantine = precision-weighted error exceeds threshold
 *
 * FEP INTEGRATION:
 * ```
 * Input Observation (o) → BBB Validation
 *         ↓
 * Expected Input μ (learned from normal traffic)
 *         ↓
 * Prediction Error: ε = o - g(μ)
 *         ↓
 * Precision-Weighted: Π * ε
 *         ↓
 * Free Energy F → Threat Score
 *         ↓
 * Action Selection: ALLOW / BLOCK / QUARANTINE
 * ```
 *
 * THREAT MAPPING:
 * - Injection attacks → High prediction error (unusual patterns)
 * - Buffer overflow → High complexity (deviation from priors)
 * - Valid inputs → Low free energy (match expectations)
 * - Rate limiting → Temporal prediction errors
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║               BBB-FEP BRIDGE (Precision Filtering)                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  FEP System      │────────▶│  BBB System      │                      ║
 * ║   │                  │         │                  │                      ║
 * ║   │ • Free Energy    │         │ • Validation     │                      ║
 * ║   │ • Prediction Err │         │ • Quarantine     │                      ║
 * ║   │ • Precision      │         │ • Threat Reports │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   ┌──────────────────────────────────────────────────────────────┐       ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │       ║
 * ║   │                                                              │       ║
 * ║   │  FEP → BBB:                                                  │       ║
 * ║   │    • Free energy → Threat severity                           │       ║
 * ║   │    • Prediction error → Anomaly score                        │       ║
 * ║   │    • Precision → Validation strictness                       │       ║
 * ║   │                                                              │       ║
 * ║   │  BBB → FEP:                                                  │       ║
 * ║   │    • Threat detections → Observations (high surprise)        │       ║
 * ║   │    • Valid inputs → Update expected patterns                 │       ║
 * ║   │    • Quarantine → Increase precision on similar patterns     │       ║
 * ║   └──────────────────────────────────────────────────────────────┘       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BLOOD_BRAIN_BARRIER_FEP_BRIDGE_H
#define NIMCP_BLOOD_BRAIN_BARRIER_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "security/nimcp_blood_brain_barrier.h"
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

/** Maximum feature dimensions for FEP observation */
#define BBB_FEP_MAX_FEATURES           32

/** Threat score thresholds */
#define BBB_FEP_THREAT_LOW_THRESHOLD   0.3f
#define BBB_FEP_THREAT_MED_THRESHOLD   0.6f
#define BBB_FEP_THREAT_HIGH_THRESHOLD  0.85f

/** Precision modulation bounds */
#define BBB_FEP_MIN_PRECISION          0.1f
#define BBB_FEP_MAX_PRECISION          10.0f
#define BBB_FEP_DEFAULT_PRECISION      1.0f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief BBB FEP configuration
 */
typedef struct {
    /** FEP parameters */
    float threat_free_energy_threshold;  /**< Free energy threshold for threats */
    float precision_learning_rate;       /**< Rate of precision adaptation */
    bool enable_precision_modulation;    /**< Adapt precision based on threats */

    /** Feature extraction */
    uint32_t input_feature_dim;          /**< Dimensionality of input features */
    bool extract_timing_features;        /**< Include timing in features */
    bool extract_content_features;       /**< Include content analysis */

    /** Action selection */
    float allow_fe_threshold;            /**< FE threshold for ALLOW */
    float block_fe_threshold;            /**< FE threshold for BLOCK */
    float quarantine_fe_threshold;       /**< FE threshold for QUARANTINE */

    /** Learning */
    bool enable_online_learning;         /**< Update model from valid inputs */
    float learning_rate;                 /**< Belief update rate */
} bbb_fep_config_t;

/**
 * @brief FEP effects on BBB (FEP → BBB)
 */
typedef struct {
    float threat_score;                  /**< Threat score from free energy */
    float anomaly_score;                 /**< Anomaly from prediction error */
    float validation_strictness;         /**< Precision-based strictness */
    bbb_action_t recommended_action;     /**< FEP-based action */
} bbb_fep_effects_t;

/**
 * @brief BBB effects on FEP (BBB → FEP)
 */
typedef struct {
    uint64_t threats_detected;           /**< Threat count for surprise */
    uint64_t valid_inputs;               /**< Valid inputs for learning */
    uint64_t quarantined;                /**< Quarantine events */
    float avg_threat_severity;           /**< Average severity */
} fep_bbb_effects_t;

/**
 * @brief BBB FEP state
 */
typedef struct {
    bool active;                         /**< Whether bridge is active */
    uint64_t update_count;               /**< Number of updates */
    uint64_t observation_count;          /**< Observations processed */
    float current_precision;             /**< Current precision level */
    float avg_free_energy;               /**< Running average FE */
} bbb_fep_state_t;

/**
 * @brief BBB FEP statistics
 */
typedef struct {
    uint64_t total_inputs_processed;     /**< Total inputs analyzed */
    uint64_t threats_via_fep;            /**< Threats detected via FEP */
    uint64_t false_positives;            /**< Known false positives */
    uint64_t precision_adaptations;      /**< Precision update count */
    float avg_prediction_error;          /**< Average PE magnitude */
    float avg_free_energy;               /**< Average free energy */
    float current_precision;             /**< Current precision */
} bbb_fep_stats_t;

/**
 * @brief BBB FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    bbb_fep_config_t config;             /**< Configuration */
    fep_system_t* fep_system;            /**< FEP system */
    bbb_system_t bbb_system;             /**< BBB system */

    bbb_fep_effects_t fep_effects;       /**< FEP → BBB effects */
    fep_bbb_effects_t bbb_effects;       /**< BBB → FEP effects */

    bbb_fep_state_t state;               /**< Current state */
    bbb_fep_stats_t stats;               /**< Statistics */} bbb_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default BBB FEP configuration
 *
 * WHAT: Provide sensible defaults for BBB-FEP integration
 * WHY:  Simplify initialization
 * HOW:  Return biologically-plausible defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int bbb_fep_default_config(bbb_fep_config_t* config);

/**
 * @brief Create BBB FEP bridge
 *
 * WHAT: Initialize FEP integration for BBB
 * WHY:  Enable precision-weighted threat detection
 * HOW:  Connect FEP system to BBB, allocate structures
 *
 * @param config Configuration
 * @param bbb_system BBB system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
bbb_fep_bridge_t* bbb_fep_create(
    const bbb_fep_config_t* config,
    bbb_system_t bbb_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy BBB FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void bbb_fep_destroy(bbb_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on BBB
 *
 * WHAT: Compute FEP-derived threat scores and actions
 * WHY:  Use free energy to guide BBB decisions
 * HOW:  Process current FEP state, update effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int bbb_fep_update(bbb_fep_bridge_t* bridge);

/**
 * @brief Process input through FEP-enhanced BBB
 *
 * WHAT: Validate input using FEP predictions
 * WHY:  Detect threats as high prediction error
 * HOW:  Extract features, compute FE, recommend action
 *
 * @param bridge Bridge handle
 * @param data Input data
 * @param size Data size
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int bbb_fep_process_input(
    bbb_fep_bridge_t* bridge,
    const void* data,
    size_t size,
    bbb_validation_result_t* result
);

/**
 * @brief Apply FEP modulation to BBB
 *
 * WHAT: Adjust BBB parameters based on FEP state
 * WHY:  Adapt validation strictness to threat landscape
 * HOW:  Modulate precision, update thresholds
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int bbb_fep_apply_modulation(bbb_fep_bridge_t* bridge);

/**
 * @brief Report BBB threat to FEP
 *
 * WHAT: Feed BBB threat detection back to FEP
 * WHY:  High surprise observations update beliefs
 * HOW:  Convert threat to FEP observation, process
 *
 * @param bridge Bridge handle
 * @param threat_type Threat type detected
 * @param severity Severity level
 * @return 0 on success, error code on failure
 */
int bbb_fep_report_threat(
    bbb_fep_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on BBB
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int bbb_fep_get_effects(
    const bbb_fep_bridge_t* bridge,
    bbb_fep_effects_t* effects
);

/**
 * @brief Get BBB effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int bbb_fep_get_bbb_effects(
    const bbb_fep_bridge_t* bridge,
    fep_bbb_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int bbb_fep_get_stats(
    const bbb_fep_bridge_t* bridge,
    bbb_fep_stats_t* stats
);

/**
 * @brief Get current threat score
 *
 * @param bridge Bridge handle
 * @return Current threat score [0, 1] or -1 on error
 */
float bbb_fep_get_threat_score(const bbb_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module threat notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int bbb_fep_connect_bio_async(bbb_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int bbb_fep_disconnect_bio_async(bbb_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool bbb_fep_is_bio_async_connected(const bbb_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BLOOD_BRAIN_BARRIER_FEP_BRIDGE_H */
