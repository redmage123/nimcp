/**
 * @file nimcp_astrocytes_fep_bridge.h
 * @brief Free Energy Principle bridge for astrocyte glial cells
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between astrocytes and Free Energy Principle
 * WHY:  Astrocytes modulate synaptic precision via glutamate uptake/release
 * HOW:  FEP precision signals drive calcium dynamics; calcium modulates FEP predictions
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * - Astrocytes regulate synaptic transmission via gliotransmitter release
 * - Glutamate uptake precision: High precision synapses → more efficient uptake
 * - D-serine modulation: Required for NMDA-dependent plasticity (precision gating)
 * - Calcium waves: Prediction error signals propagate through astrocyte networks
 * - ATP release: Metabolic support based on predicted energy demand
 *
 * FEP INTEGRATION:
 * =================================================================================
 * - FEP → Astrocyte: Precision estimates modulate calcium release thresholds
 * - Astrocyte → FEP: Glutamate levels adjust synaptic precision weights
 * - Prediction error minimization: Calcium dynamics track surprise signals
 * - Active inference: Astrocytes predict synaptic demand and release support
 *
 * ARCHITECTURAL DECISIONS:
 * =================================================================================
 * - Non-invasive: Doesn't modify astrocyte_t or fep_system_t structures
 * - Bridge pattern: Coordinates between astrocyte and FEP modules
 * - Bidirectional: Both forward (FEP→Astro) and reverse (Astro→FEP) effects
 * - Bio-async: Integrates with bio-router for distributed coordination
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTES_FEP_BRIDGE_H
#define NIMCP_ASTROCYTES_FEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default precision sensitivity for calcium modulation */
#define ASTROCYTES_FEP_DEFAULT_PRECISION_SENSITIVITY 0.5f

/** Default glutamate-to-precision conversion gain */
#define ASTROCYTES_FEP_DEFAULT_GLUTAMATE_GAIN 0.3f

/** Calcium threshold for prediction error signaling (µM) */
#define ASTROCYTES_FEP_CALCIUM_PE_THRESHOLD 1.0f

/** Maximum calcium modulation factor */
#define ASTROCYTES_FEP_MAX_CALCIUM_MODULATION 2.0f

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Configuration for astrocytes-FEP bridge
 */
typedef struct {
    float precision_sensitivity;    /**< How much FEP precision affects calcium (0-1) */
    float glutamate_gain;            /**< Glutamate → precision conversion gain */
    float calcium_pe_threshold;      /**< Calcium threshold for prediction error (µM) */
    float d_serine_precision_factor; /**< D-serine release modulation by precision */
    bool enable_calcium_prediction;  /**< Use FEP to predict calcium dynamics */
    bool enable_glutamate_precision; /**< Use glutamate to modulate precision */
    bool enable_prediction_errors;   /**< Generate prediction error signals */
} astrocytes_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on astrocyte calcium and neurotransmitter release
 *
 * WHAT: Quantifies how FEP precision/prediction error affects astrocyte state
 * WHY:  FEP top-down signals modulate glial support
 * HOW:  Precision → calcium threshold adjustment, PE → calcium wave triggering
 */
typedef struct {
    float calcium_threshold_shift;   /**< Additive shift to calcium threshold (µM) */
    float glutamate_release_factor;  /**< Multiplicative factor for glutamate (0-2) */
    float d_serine_release_factor;   /**< Multiplicative factor for D-serine (0-2) */
    float atp_demand_prediction;     /**< Predicted ATP demand (0-1) */
} fep_astrocyte_effects_t;

/**
 * @brief Astrocyte effects on FEP precision and predictions
 *
 * WHAT: Quantifies how astrocyte state affects FEP computations
 * WHY:  Bottom-up glial signals inform precision-weighted inference
 * HOW:  Glutamate → precision weights, calcium → prediction error signals
 */
typedef struct {
    float synaptic_precision_modulation; /**< Precision adjustment for synapses (0-2) */
    float prediction_error_signal;       /**< Calcium-derived prediction error */
    float metabolic_uncertainty;         /**< ATP depletion → increased uncertainty */
} astrocyte_fep_effects_t;

/**
 * @brief Astrocyte-FEP bridge state tracking
 */
typedef struct {
    uint64_t last_update_time;       /**< Last update timestamp (µs) */
    float predicted_calcium;         /**< FEP prediction of calcium level (µM) */
    float calcium_prediction_error;  /**< Actual - predicted calcium */
    uint32_t num_covered_synapses;   /**< Number of synapses in domain */
} astrocytes_fep_state_t;

/**
 * @brief Astrocyte-FEP bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t calcium_predictions;
    uint64_t precision_modulations;
    float avg_prediction_error;
    float avg_glutamate_factor;
    float max_calcium_shift;
} astrocytes_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Astrocytes-FEP integration bridge
 *
 * WHAT: Coordinates astrocyte network with FEP system
 * WHY:  Enable bidirectional FEP-glial communication
 * HOW:  Track effects, update both systems, maintain state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    astrocytes_fep_config_t config;      /**< Bridge configuration */
    fep_system_t* fep_system;            /**< FEP system pointer */
    astrocyte_network_t* astrocyte_network; /**< Astrocyte network pointer */

    fep_astrocyte_effects_t fep_effects; /**< FEP → astrocyte effects */
    astrocyte_fep_effects_t astro_effects; /**< Astrocyte → FEP effects */
    astrocytes_fep_state_t state;        /**< Bridge state */
    astrocytes_fep_stats_t stats;        /**< Statistics */} astrocytes_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for astrocytes-FEP integration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Return pre-filled config structure
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int astrocytes_fep_default_config(astrocytes_fep_config_t* config);

/**
 * @brief Create astrocytes-FEP bridge
 *
 * WHAT: Initialize bidirectional astrocyte-FEP integration
 * WHY:  Enable precision-modulated glial support
 * HOW:  Allocate bridge, connect systems, initialize state
 *
 * @param config Bridge configuration
 * @param astrocyte_network Astrocyte network to integrate
 * @param fep_system FEP system to integrate
 * @return Bridge pointer or NULL on failure
 */
astrocytes_fep_bridge_t* astrocytes_fep_create(
    const astrocytes_fep_config_t* config,
    astrocyte_network_t* astrocyte_network,
    fep_system_t* fep_system
);

/**
 * @brief Destroy astrocytes-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void astrocytes_fep_destroy(astrocytes_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on astrocytes
 *
 * WHAT: Compute how FEP precision/prediction error affects astrocyte state
 * WHY:  Top-down modulation of glial support
 * HOW:  Read FEP system state, compute calcium/glutamate modulations
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int astrocytes_fep_update_fep_to_astrocyte(astrocytes_fep_bridge_t* bridge);

/**
 * @brief Update astrocyte effects on FEP
 *
 * WHAT: Compute how astrocyte state affects FEP precision/predictions
 * WHY:  Bottom-up glial signals inform inference
 * HOW:  Read astrocyte calcium/glutamate, compute precision modulations
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int astrocytes_fep_update_astrocyte_to_fep(astrocytes_fep_bridge_t* bridge);

/**
 * @brief Bidirectional update (both directions)
 *
 * WHAT: Update both FEP→Astrocyte and Astrocyte→FEP effects
 * WHY:  Maintain bidirectional coupling
 * HOW:  Call both update functions sequentially
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int astrocytes_fep_update(astrocytes_fep_bridge_t* bridge);

/**
 * @brief Apply FEP-derived modulations to astrocyte network
 *
 * WHAT: Actually modify astrocyte state based on FEP effects
 * WHY:  Enact top-down modulation
 * HOW:  Adjust calcium thresholds, glutamate release factors
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int astrocytes_fep_apply_modulation(astrocytes_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP-derived calcium threshold shift
 *
 * @param bridge Bridge instance
 * @return Calcium threshold shift (µM)
 */
float astrocytes_fep_get_calcium_shift(const astrocytes_fep_bridge_t* bridge);

/**
 * @brief Get astrocyte-derived synaptic precision modulation
 *
 * @param bridge Bridge instance
 * @return Precision modulation factor (0-2, default 1.0)
 */
float astrocytes_fep_get_precision_modulation(const astrocytes_fep_bridge_t* bridge);

/**
 * @brief Get calcium prediction error
 *
 * @param bridge Bridge instance
 * @return Prediction error magnitude
 */
float astrocytes_fep_get_prediction_error(const astrocytes_fep_bridge_t* bridge);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success
 */
int astrocytes_fep_get_stats(
    const astrocytes_fep_bridge_t* bridge,
    astrocytes_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async for inter-module messaging
 * WHY:  Enable distributed FEP-glial coordination
 * HOW:  Register module context, set up message handlers
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int astrocytes_fep_connect_bio_async(astrocytes_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int astrocytes_fep_disconnect_bio_async(astrocytes_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool astrocytes_fep_is_bio_async_connected(const astrocytes_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTES_FEP_BRIDGE_H */
