/**
 * @file nimcp_energy_consistency_fep_bridge.h
 * @brief Bridge between Energy Consistency and FEP Systems
 *
 * Connects the energy-based consistency checker to the Free Energy Principle
 * system, mapping logical consistency energy to FEP prediction error.
 *
 * Integration Pattern:
 * - Consistency violations → FEP prediction errors
 * - FEP precision weighting → Violation importance
 * - Active inference → Consistency repair actions
 *
 * Biological Basis:
 * - Anterior cingulate cortex (ACC) conflict monitoring
 * - Prediction error signals in prefrontal cortex
 * - Error-driven learning and adaptation
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_ENERGY_CONSISTENCY_FEP_BRIDGE_H
#define NIMCP_ENERGY_CONSISTENCY_FEP_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_ENERGY_CONSISTENCY_FEP_BRIDGE    0x0396

/* ============================================================================
 * Bridge Effects
 * ============================================================================ */

/**
 * @brief Effects produced by the energy-consistency-FEP bridge
 */
typedef struct energy_fep_bridge_effects {
    float prediction_error;          /**< Consistency energy as prediction error */
    float precision;                 /**< FEP precision for consistency checks */
    float belief_update_rate;        /**< How fast to update beliefs */
    float action_urgency;            /**< Urgency of repair actions */
    float surprise;                  /**< Information-theoretic surprise */
} energy_fep_bridge_effects_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for energy-consistency-FEP bridge
 */
typedef struct energy_fep_bridge_config {
    /* Base config */
    bool enable_modulation;          /**< Enable bidirectional modulation */
    float sensitivity;               /**< General sensitivity [0.5-2.0] */

    /* Energy mapping */
    float energy_to_error_scale;     /**< Scale factor for energy → error */
    float error_threshold;           /**< Threshold for triggering actions */

    /* Precision settings */
    float base_precision;            /**< Base precision level */
    float precision_learning_rate;   /**< Learning rate for precision */

    /* Integration */
    bool enable_active_inference;    /**< Use active inference for repair */
    bool enable_bio_async;           /**< Enable async messaging */
} energy_fep_bridge_config_t;

/* ============================================================================
 * Bridge Structure (inherits from bridge_base_t)
 * ============================================================================ */

/**
 * @brief Energy-Consistency-FEP Bridge
 *
 * Inherits from bridge_base_t for standard lifecycle management.
 */
typedef struct energy_fep_bridge {
    bridge_base_t base;              /**< MUST be first member */

    /* Connected systems (typed for clarity) */
    /* base.system_a = energy_consistency_checker_t* */
    /* base.system_b = fep_system_t* */

    /* Configuration */
    energy_fep_bridge_config_t config;

    /* Current effects */
    energy_fep_bridge_effects_t effects;

    /* State */
    float current_energy;            /**< Current consistency energy */
    float smoothed_energy;           /**< Exponentially smoothed energy */
    float energy_derivative;         /**< Rate of change of energy */

    /* Precision tracking */
    float estimated_precision;       /**< Current estimated precision */
    float precision_history[64];     /**< History for adaptation */
    uint32_t precision_history_idx;  /**< Circular buffer index */

    /* Statistics */
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t error_signals_sent;     /**< Error signals sent to FEP */
    uint64_t actions_triggered;      /**< Repair actions triggered */
    float avg_prediction_error;      /**< Running average error */
} energy_fep_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create energy-consistency-FEP bridge
 *
 * @return Bridge handle or NULL on failure
 */
NIMCP_API energy_fep_bridge_t* energy_fep_bridge_create(void);

/**
 * @brief Create with configuration
 *
 * @param config Configuration
 * @return Bridge handle or NULL on failure
 */
NIMCP_API energy_fep_bridge_t* energy_fep_bridge_create_with_config(
    const energy_fep_bridge_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
NIMCP_API void energy_fep_bridge_destroy(energy_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge The bridge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_reset(energy_fep_bridge_t* bridge);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_get_default_config(
    energy_fep_bridge_config_t* config);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect energy consistency checker
 *
 * @param bridge The bridge
 * @param checker Energy consistency checker
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_connect_checker(
    energy_fep_bridge_t* bridge,
    energy_consistency_checker_t* checker);

/**
 * @brief Connect FEP system
 *
 * @param bridge The bridge
 * @param fep FEP system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_connect_fep(
    energy_fep_bridge_t* bridge,
    void* fep);

/**
 * @brief Disconnect energy consistency checker
 *
 * @param bridge The bridge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_disconnect_checker(
    energy_fep_bridge_t* bridge);

/**
 * @brief Disconnect FEP system
 *
 * @param bridge The bridge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_disconnect_fep(
    energy_fep_bridge_t* bridge);

/* ============================================================================
 * Update Functions
 * ============================================================================ */

/**
 * @brief Update bridge with new consistency energy
 *
 * Maps consistency energy to FEP prediction error and updates
 * precision estimates.
 *
 * @param bridge The bridge
 * @param consistency_energy Current consistency energy
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_update(
    energy_fep_bridge_t* bridge,
    float consistency_energy);

/**
 * @brief Update from consistency result
 *
 * @param bridge The bridge
 * @param result Consistency check result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_update_from_result(
    energy_fep_bridge_t* bridge,
    const energy_consistency_result_t* result);

/**
 * @brief Get precision-weighted energy for a violation
 *
 * Uses FEP precision weighting to modulate violation importance.
 *
 * @param bridge The bridge
 * @param violation The violation
 * @return Precision-weighted energy
 */
NIMCP_API float energy_fep_bridge_get_precision_weighted_energy(
    const energy_fep_bridge_t* bridge,
    const consistency_violation_t* violation);

/* ============================================================================
 * Effects Query
 * ============================================================================ */

/**
 * @brief Get current bridge effects
 *
 * @param bridge The bridge
 * @param effects Output effects
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_get_effects(
    const energy_fep_bridge_t* bridge,
    energy_fep_bridge_effects_t* effects);

/**
 * @brief Get current prediction error
 *
 * @param bridge The bridge
 * @return Current prediction error
 */
NIMCP_API float energy_fep_bridge_get_prediction_error(
    const energy_fep_bridge_t* bridge);

/**
 * @brief Get current precision estimate
 *
 * @param bridge The bridge
 * @return Current precision
 */
NIMCP_API float energy_fep_bridge_get_precision(
    const energy_fep_bridge_t* bridge);

/* ============================================================================
 * Active Inference
 * ============================================================================ */

/**
 * @brief Get recommended repair action based on active inference
 *
 * Uses FEP to select action that minimizes expected free energy.
 *
 * @param bridge The bridge
 * @param violation Current violation
 * @param action Output recommended action
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_get_repair_action(
    const energy_fep_bridge_t* bridge,
    const consistency_violation_t* violation,
    int* action);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param bridge The bridge
 * @param router Bio-async router
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_register_bio_async(
    energy_fep_bridge_t* bridge);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge The bridge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t energy_fep_bridge_unregister_bio_async(
    energy_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENERGY_CONSISTENCY_FEP_BRIDGE_H */
