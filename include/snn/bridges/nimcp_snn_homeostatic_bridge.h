/**
 * @file nimcp_snn_homeostatic_bridge.h
 * @brief SNN-Homeostatic Plasticity Integration Bridge
 *
 * WHAT: Bidirectional integration between SNN and homeostatic plasticity module
 * WHY:  Maintain firing rate stability and prevent runaway excitation in SNNs
 * HOW:  Target firing rate maintenance, intrinsic excitability adjustment
 *
 * BIOLOGICAL BASIS:
 * - Homeostatic plasticity prevents runaway Hebbian learning (Turrigiano & Nelson, 2004)
 * - Intrinsic plasticity adjusts neuronal threshold to maintain target rate
 * - Operates on slower timescale than Hebbian learning (hours vs seconds)
 * - Essential for network stability and preventing silent/saturated neurons
 *
 * INTEGRATION:
 * - SNN provides spike-based firing rate estimates
 * - Homeostatic module provides target rates and threshold adjustments
 * - Bridge applies intrinsic plasticity to SNN neuron parameters
 * - Bio-async for rate monitoring and threshold updates
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_HOMEOSTATIC_BRIDGE_H
#define NIMCP_SNN_HOMEOSTATIC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN-Homeostatic bridge configuration
 */
typedef struct snn_homeostatic_bridge_config_s {
    /* Target rate control */
    float target_rate_hz;           /**< Target firing rate (Hz) - default 5.0 */
    float rate_tolerance;           /**< Acceptable deviation from target */
    float rate_window_ms;           /**< Time window for rate estimation */

    /* Threshold adaptation */
    bool enable_threshold_adaptation; /**< Enable intrinsic plasticity */
    float threshold_tau_ms;         /**< Time constant for threshold adaptation */
    float threshold_step_size;      /**< Step size for threshold adjustments */
    float min_threshold_mv;         /**< Minimum spike threshold (mV) */
    float max_threshold_mv;         /**< Maximum spike threshold (mV) */

    /* Update control */
    float update_interval_ms;       /**< How often to check and adjust */
    bool bidirectional_updates;     /**< SNN ↔ Homeostatic or SNN → only */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_homeostatic_bridge_config_t;

/**
 * @brief Homeostatic effects on SNN
 */
typedef struct snn_homeostatic_effects_s {
    float avg_firing_rate;          /**< Network average firing rate */
    float rate_deviation;           /**< Deviation from target */
    float avg_threshold_shift;      /**< Average threshold adjustment */
    uint32_t neurons_above_target;  /**< Count of over-firing neurons */
    uint32_t neurons_below_target;  /**< Count of under-firing neurons */
    bool network_stable;            /**< Within target range */
} snn_homeostatic_effects_t;

/**
 * @brief Per-neuron homeostatic state
 */
typedef struct neuron_homeostatic_state_s {
    uint32_t neuron_id;             /**< Neuron ID */
    float current_rate;             /**< Current firing rate (Hz) */
    float rate_deviation;           /**< Deviation from target */
    float threshold_adjustment;     /**< Cumulative threshold shift (mV) */
    intrinsic_plasticity_state_t ip_state; /**< Intrinsic plasticity state */
    bool is_stable;                 /**< Within target range */
} neuron_homeostatic_state_t;

/**
 * @brief SNN-Homeostatic bridge structure
 */
typedef struct snn_homeostatic_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;              /**< SNN network */
    homeostatic_controller_t controller; /**< Homeostatic controller */
    uint32_t n_neurons;                  /**< Number of neurons */

    snn_homeostatic_bridge_config_t config; /**< Configuration */
    snn_homeostatic_effects_t effects;   /**< Current effects */

    /* Per-neuron state */
    neuron_homeostatic_state_t* neuron_states; /**< State per neuron */

    /* Statistics */
    bool connected;
    float last_update_time_ms;
    uint32_t threshold_adjustments;
    uint32_t stability_checks;

    /* Bio-async */
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;

    void* mutex;
} snn_homeostatic_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_homeostatic_bridge_config_default(snn_homeostatic_bridge_config_t* config);

snn_homeostatic_bridge_t* snn_homeostatic_bridge_create(
    const snn_homeostatic_bridge_config_t* config,
    snn_network_t* network,
    homeostatic_controller_t controller,
    uint32_t n_neurons
);

void snn_homeostatic_bridge_destroy(snn_homeostatic_bridge_t* bridge);

int snn_homeostatic_bridge_connect_bio_async(snn_homeostatic_bridge_t* bridge);
int snn_homeostatic_bridge_disconnect_bio_async(snn_homeostatic_bridge_t* bridge);
bool snn_homeostatic_bridge_is_bio_async_connected(const snn_homeostatic_bridge_t* bridge);

int snn_homeostatic_bridge_update_rates(snn_homeostatic_bridge_t* bridge, float dt);
int snn_homeostatic_bridge_apply_plasticity(snn_homeostatic_bridge_t* bridge, float dt);
int snn_homeostatic_bridge_update(snn_homeostatic_bridge_t* bridge, float dt);

int snn_homeostatic_bridge_get_weight_changes(
    const snn_homeostatic_bridge_t* bridge,
    uint32_t* neuron_ids,
    float* threshold_adjustments,
    uint32_t max_changes,
    uint32_t* n_changes
);

int snn_homeostatic_bridge_get_effects(
    const snn_homeostatic_bridge_t* bridge,
    snn_homeostatic_effects_t* effects
);

float snn_homeostatic_bridge_get_avg_rate(const snn_homeostatic_bridge_t* bridge);
bool snn_homeostatic_bridge_is_stable(const snn_homeostatic_bridge_t* bridge);

int snn_homeostatic_bridge_get_stats(
    const snn_homeostatic_bridge_t* bridge,
    uint32_t* threshold_adjustments,
    uint32_t* stability_checks,
    uint32_t* updates
);

void snn_homeostatic_bridge_reset_stats(snn_homeostatic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_HOMEOSTATIC_BRIDGE_H */
