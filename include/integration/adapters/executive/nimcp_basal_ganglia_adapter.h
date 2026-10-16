/**
 * @file nimcp_basal_ganglia_adapter.h
 * @brief Basal Ganglia Adapter for Executive Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts basal ganglia for Executive layer action selection
 * WHY:  Basal ganglia mediates action selection and reinforcement learning
 * HOW:  Implements direct/indirect pathway competition, dopamine modulation
 *
 * BASAL GANGLIA MODEL:
 * - Striatum: D1 (direct) and D2 (indirect) pathways
 * - GPe/GPi: Tonic inhibition of thalamus
 * - STN: Hyperdirect pathway for response inhibition
 * - SNc: Dopamine modulation of striatal plasticity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BASAL_GANGLIA_ADAPTER_H
#define NIMCP_BASAL_GANGLIA_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_basal_ganglia_adapter_struct* nimcp_basal_ganglia_adapter_t;

/**
 * @brief Action representation for selection
 */
typedef struct {
    uint32_t action_id;
    float salience;             /**< Input salience [0,1] */
    float direct_activity;      /**< D1 pathway activity */
    float indirect_activity;    /**< D2 pathway activity */
    float selection_probability;
} bg_action_t;

typedef struct {
    uint32_t num_actions;               /**< Number of action channels */
    uint32_t striatum_size;             /**< Striatal neurons per channel */
    float d1_d2_balance;                /**< D1/D2 pathway balance [0,1] */
    float selection_threshold;          /**< Winner selection threshold */
    float lateral_inhibition;           /**< Competition strength */
    float dopamine_baseline;            /**< Tonic dopamine level */
    float learning_rate;                /**< Striatal plasticity rate */
    bool enable_stn;                    /**< Enable STN hyperdirect path */
    bool enable_logging;
} nimcp_basal_ganglia_config_t;

typedef struct {
    uint32_t selected_action;           /**< Currently selected action */
    float selection_confidence;         /**< Selection confidence */
    float gpi_output;                   /**< GPi inhibitory output */
    float stn_activity;                 /**< STN urgency signal */
    float dopamine_level;               /**< Current dopamine level */
    float conflict_level;               /**< Action conflict measure */
    bool is_active;
} nimcp_basal_ganglia_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t actions_selected;
    uint64_t conflicts_resolved;
    uint64_t inhibitions_triggered;
    uint64_t dopamine_bursts;
} nimcp_basal_ganglia_stats_t;

NIMCP_EXPORT nimcp_basal_ganglia_config_t nimcp_basal_ganglia_adapter_default_config(void);
NIMCP_EXPORT nimcp_basal_ganglia_adapter_t nimcp_basal_ganglia_adapter_create(const nimcp_basal_ganglia_config_t* config);
NIMCP_EXPORT void nimcp_basal_ganglia_adapter_destroy(nimcp_basal_ganglia_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_basal_ganglia_adapter_get_interface(nimcp_basal_ganglia_adapter_t adapter);

/**
 * @brief Set action salience for selection
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_set_action_salience(
    nimcp_basal_ganglia_adapter_t adapter,
    uint32_t action_id,
    float salience
);

/**
 * @brief Run action selection competition
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_select_action(
    nimcp_basal_ganglia_adapter_t adapter,
    uint32_t* selected_action_out,
    float* confidence_out
);

/**
 * @brief Apply dopamine reward signal
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_apply_dopamine(
    nimcp_basal_ganglia_adapter_t adapter,
    float dopamine_delta
);

/**
 * @brief Trigger response inhibition via STN
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_inhibit_response(
    nimcp_basal_ganglia_adapter_t adapter,
    float inhibition_strength
);

NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_get_state(nimcp_basal_ganglia_adapter_t adapter, nimcp_basal_ganglia_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_get_stats(nimcp_basal_ganglia_adapter_t adapter, nimcp_basal_ganglia_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_basal_ganglia_adapter_reset_stats(nimcp_basal_ganglia_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_ADAPTER_H */
