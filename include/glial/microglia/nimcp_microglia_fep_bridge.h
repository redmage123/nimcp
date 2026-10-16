/**
 * @file nimcp_microglia_fep_bridge.h
 * @brief Free Energy Principle bridge for microglia glial cells
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between microglia and Free Energy Principle
 * WHY:  Microglia prune low-precision synapses via complement-mediated mechanisms
 * HOW:  FEP precision signals guide pruning; pruning updates FEP model structure
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * - Microglia perform activity-dependent synaptic pruning during development
 * - Complement cascade (C1q/C3) tags weak synapses for elimination
 * - Synaptic precision: Low-precision synapses tagged for pruning
 * - Prediction error signals: High PE triggers microglial surveillance
 * - Cytokine signaling: IL-1β, TNF-α modulate network precision
 *
 * FEP INTEGRATION:
 * =================================================================================
 * - FEP → Microglia: Precision estimates determine pruning thresholds
 * - Microglia → FEP: Pruning updates generative model connectivity
 * - Prediction error minimization: Remove low-precision connections
 * - Active inference: Microglia predict optimal synaptic density
 *
 * ARCHITECTURAL DECISIONS:
 * =================================================================================
 * - Non-invasive: Doesn't modify microglia_t or fep_system_t structures
 * - Bridge pattern: Coordinates between microglia and FEP modules
 * - Bidirectional: Both forward (FEP→Micro) and reverse (Micro→FEP) effects
 * - Bio-async: Integrates with bio-router for distributed coordination
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MICROGLIA_FEP_BRIDGE_H
#define NIMCP_MICROGLIA_FEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "glial/microglia/nimcp_microglia.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default precision threshold for synaptic pruning */
#define MICROGLIA_FEP_DEFAULT_PRECISION_THRESHOLD 0.3f

/** Default pruning rate modulation by prediction error */
#define MICROGLIA_FEP_DEFAULT_PE_PRUNING_GAIN 0.5f

/** Cytokine modulation of network precision */
#define MICROGLIA_FEP_CYTOKINE_PRECISION_IMPACT 0.2f

/** Maximum pruning rate adjustment */
#define MICROGLIA_FEP_MAX_PRUNING_MODULATION 3.0f

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Configuration for microglia-FEP bridge
 */
typedef struct {
    float precision_threshold;       /**< Minimum precision to avoid pruning */
    float pe_pruning_gain;           /**< Prediction error → pruning rate gain */
    float cytokine_precision_impact; /**< Cytokine effect on precision */
    float complement_precision_factor; /**< Precision → complement tag strength */
    bool enable_precision_pruning;   /**< Use FEP precision for pruning */
    bool enable_pe_modulation;       /**< Modulate pruning by prediction error */
    bool enable_cytokine_feedback;   /**< Cytokines affect FEP precision */
} microglia_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on microglia pruning and surveillance
 *
 * WHAT: Quantifies how FEP precision/prediction error affects pruning
 * WHY:  FEP top-down signals guide synaptic refinement
 * HOW:  Low precision → increased pruning, high PE → heightened surveillance
 */
typedef struct {
    float pruning_threshold_shift;   /**< Additive shift to pruning threshold */
    float complement_tag_strength;   /**< C1q/C3 tagging strength (0-1) */
    float surveillance_intensity;    /**< Microglial activity level (0-1) */
    float cytokine_release_factor;   /**< Pro-inflammatory cytokine release */
} fep_microglia_effects_t;

/**
 * @brief Microglia effects on FEP precision and model structure
 *
 * WHAT: Quantifies how pruning affects FEP computations
 * WHY:  Bottom-up structural changes update generative model
 * HOW:  Pruning → model connectivity, cytokines → precision weights
 */
typedef struct {
    float network_precision_shift;   /**< Global precision adjustment */
    uint32_t synapses_pruned;        /**< Number of synapses removed */
    float structural_uncertainty;    /**< Uncertainty from network changes */
    float cytokine_precision_modulation; /**< Cytokine-driven precision change */
} microglia_fep_effects_t;

/**
 * @brief Microglia-FEP bridge state tracking
 */
typedef struct {
    uint64_t last_update_time;       /**< Last update timestamp (µs) */
    float predicted_pruning_rate;    /**< FEP prediction of pruning (synapses/s) */
    float actual_pruning_rate;       /**< Measured pruning rate */
    float pruning_prediction_error;  /**< Prediction error for pruning */
    uint32_t total_synapses_monitored; /**< Synapses under surveillance */
} microglia_fep_state_t;

/**
 * @brief Microglia-FEP bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t precision_guided_prunings;
    uint64_t pe_triggered_surveillances;
    float avg_pruning_threshold;
    float avg_cytokine_modulation;
    uint32_t max_synapses_pruned_per_update;
} microglia_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Microglia-FEP integration bridge
 *
 * WHAT: Coordinates microglia network with FEP system
 * WHY:  Enable bidirectional FEP-microglial communication
 * HOW:  Track effects, update both systems, maintain state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    microglia_fep_config_t config;       /**< Bridge configuration */
    fep_system_t* fep_system;            /**< FEP system pointer */
    microglia_network_t* microglia_network; /**< Microglia network pointer */

    fep_microglia_effects_t fep_effects; /**< FEP → microglia effects */
    microglia_fep_effects_t micro_effects; /**< Microglia → FEP effects */
    microglia_fep_state_t state;         /**< Bridge state */
    microglia_fep_stats_t stats;         /**< Statistics */} microglia_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success
 */
int microglia_fep_default_config(microglia_fep_config_t* config);

/**
 * @brief Create microglia-FEP bridge
 *
 * @param config Bridge configuration
 * @param microglia_network Microglia network to integrate
 * @param fep_system FEP system to integrate
 * @return Bridge pointer or NULL on failure
 */
microglia_fep_bridge_t* microglia_fep_create(
    const microglia_fep_config_t* config,
    microglia_network_t* microglia_network,
    fep_system_t* fep_system
);

/**
 * @brief Destroy microglia-FEP bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void microglia_fep_destroy(microglia_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on microglia
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int microglia_fep_update_fep_to_microglia(microglia_fep_bridge_t* bridge);

/**
 * @brief Update microglia effects on FEP
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int microglia_fep_update_microglia_to_fep(microglia_fep_bridge_t* bridge);

/**
 * @brief Bidirectional update
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int microglia_fep_update(microglia_fep_bridge_t* bridge);

/**
 * @brief Apply FEP-derived modulations to microglia network
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int microglia_fep_apply_modulation(microglia_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

float microglia_fep_get_pruning_threshold(const microglia_fep_bridge_t* bridge);
float microglia_fep_get_precision_modulation(const microglia_fep_bridge_t* bridge);
uint32_t microglia_fep_get_synapses_pruned(const microglia_fep_bridge_t* bridge);
int microglia_fep_get_stats(
    const microglia_fep_bridge_t* bridge,
    microglia_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int microglia_fep_connect_bio_async(microglia_fep_bridge_t* bridge);
int microglia_fep_disconnect_bio_async(microglia_fep_bridge_t* bridge);
bool microglia_fep_is_bio_async_connected(const microglia_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MICROGLIA_FEP_BRIDGE_H */
