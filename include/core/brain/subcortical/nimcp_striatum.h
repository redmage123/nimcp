//=============================================================================
// nimcp_striatum.h - Striatum (D1/D2 Medium Spiny Neurons)
//=============================================================================
/**
 * @file nimcp_striatum.h
 * @brief Striatum implementation with D1 and D2 medium spiny neurons
 *
 * WHAT: Striatum model with distinct D1 (direct) and D2 (indirect) pathways
 * WHY:  Striatum is the primary input nucleus of the basal ganglia
 * HOW:  D1 MSNs express dopamine D1 receptors (GO), D2 MSNs express D2 receptors (NO-GO)
 *
 * BIOLOGICAL BASIS:
 * - The striatum receives glutamatergic input from cortex and thalamus
 * - ~95% of striatal neurons are GABAergic medium spiny neurons (MSNs)
 * - D1 MSNs project to GPi/SNr (direct pathway - facilitates movement)
 * - D2 MSNs project to GPe (indirect pathway - inhibits movement)
 * - Dopamine from SNc modulates both populations differently:
 *   - D1: dopamine increases excitability (enhances GO signal)
 *   - D2: dopamine decreases excitability (reduces NO-GO signal)
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_STRIATUM_H
#define NIMCP_STRIATUM_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define STRIATUM_MAX_NEURONS 1024       /**< Maximum MSN neurons per pathway */
#define STRIATUM_DEFAULT_NEURONS 256    /**< Default neurons per pathway */
#define STRIATUM_D1_DOPAMINE_GAIN 1.5f  /**< D1 receptor dopamine sensitivity */
#define STRIATUM_D2_DOPAMINE_GAIN 1.2f  /**< D2 receptor dopamine sensitivity */
#define STRIATUM_BASELINE_FIRING 0.1f   /**< Baseline firing rate */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief MSN type (D1 or D2 receptor expressing)
 */
typedef enum {
    MSN_TYPE_D1 = 0,    /**< D1 receptor (direct pathway) */
    MSN_TYPE_D2         /**< D2 receptor (indirect pathway) */
} msn_type_t;

/**
 * @brief Striatum region
 */
typedef enum {
    STRIATUM_REGION_DORSAL = 0,     /**< Dorsal striatum (sensorimotor) */
    STRIATUM_REGION_VENTRAL,        /**< Ventral striatum (limbic/reward) */
    STRIATUM_REGION_ANTERIOR,       /**< Anterior (associative) */
    STRIATUM_REGION_COUNT
} striatum_region_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Medium spiny neuron (MSN)
 */
typedef struct {
    uint32_t id;                    /**< Neuron ID */
    msn_type_t type;                /**< D1 or D2 type */
    float membrane_potential;       /**< Membrane potential */
    float firing_rate;              /**< Current firing rate */
    float dopamine_sensitivity;     /**< Dopamine modulation sensitivity */
    float cortical_weight;          /**< Weight from cortical input */
    uint32_t action_id;             /**< Associated action */
} msn_neuron_t;

/**
 * @brief Striatal pathway (D1 or D2)
 */
typedef struct {
    msn_type_t type;                /**< Pathway type */
    msn_neuron_t* neurons;          /**< MSN neurons */
    uint32_t num_neurons;           /**< Number of neurons */
    float* activations;             /**< Per-action activations */
    uint32_t num_actions;           /**< Number of actions */
    float dopamine_gain;            /**< Dopamine sensitivity gain */
    float baseline_firing;          /**< Baseline firing rate */
} striatum_pathway_t;

/**
 * @brief Striatum configuration
 */
typedef struct {
    uint32_t neurons_per_pathway;   /**< Neurons per pathway */
    uint32_t num_actions;           /**< Number of possible actions */
    float d1_dopamine_gain;         /**< D1 dopamine sensitivity */
    float d2_dopamine_gain;         /**< D2 dopamine sensitivity */
    float baseline_firing;          /**< Baseline firing rate */
    bool enable_lateral_inhibition; /**< Enable lateral inhibition between MSNs */
    float lateral_inhibition_strength; /**< Lateral inhibition weight */
} striatum_config_t;

/**
 * @brief Striatum statistics
 */
typedef struct {
    float avg_d1_firing;            /**< Average D1 pathway firing */
    float avg_d2_firing;            /**< Average D2 pathway firing */
    float d1_d2_ratio;              /**< Ratio of D1 to D2 activity */
    uint32_t active_d1_neurons;     /**< Number of active D1 neurons */
    uint32_t active_d2_neurons;     /**< Number of active D2 neurons */
} striatum_stats_t;

/**
 * @brief Main striatum structure
 */
typedef struct {
    /* Pathways */
    striatum_pathway_t direct;      /**< Direct (D1) pathway */
    striatum_pathway_t indirect;    /**< Indirect (D2) pathway */

    /* Configuration */
    striatum_config_t config;       /**< Configuration */

    /* Dopamine state */
    float dopamine_level;           /**< Current dopamine level */

    /* Lateral inhibition */
    float* inhibition_matrix;       /**< Lateral inhibition weights */
    bool lateral_inhibition_enabled; /**< Lateral inhibition active */

    /* Statistics */
    striatum_stats_t stats;         /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Mutex for thread safety */
} striatum_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default striatum configuration
 * @param config Configuration to initialize
 */
void striatum_default_config(striatum_config_t* config);

/**
 * @brief Create striatum
 * @param config Configuration (NULL for defaults)
 * @return New striatum instance or NULL on failure
 */
striatum_t* striatum_create(const striatum_config_t* config);

/**
 * @brief Destroy striatum
 * @param striatum Instance to destroy
 */
void striatum_destroy(striatum_t* striatum);

/**
 * @brief Reset striatum to initial state
 * @param striatum Instance to reset
 * @return 0 on success, negative on error
 */
int striatum_reset(striatum_t* striatum);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process cortical input through striatum
 * @param striatum Striatum instance
 * @param cortical_input Cortical activations (size num_actions)
 * @param dopamine Current dopamine level [0-1]
 * @return 0 on success, negative on error
 */
int striatum_process_input(
    striatum_t* striatum,
    const float* cortical_input,
    float dopamine
);

/**
 * @brief Get D1 (direct pathway) output
 * @param striatum Striatum instance
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int striatum_get_d1_output(const striatum_t* striatum, float* output);

/**
 * @brief Get D2 (indirect pathway) output
 * @param striatum Striatum instance
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int striatum_get_d2_output(const striatum_t* striatum, float* output);

/**
 * @brief Get D1 activation for specific action
 * @param striatum Striatum instance
 * @param action_id Action to query
 * @return D1 activation [0-1], or -1 on error
 */
float striatum_get_d1_activation(const striatum_t* striatum, uint32_t action_id);

/**
 * @brief Get D2 activation for specific action
 * @param striatum Striatum instance
 * @param action_id Action to query
 * @return D2 activation [0-1], or -1 on error
 */
float striatum_get_d2_activation(const striatum_t* striatum, uint32_t action_id);

/**
 * @brief Set dopamine level
 * @param striatum Striatum instance
 * @param level Dopamine level [0-1]
 * @return 0 on success, negative on error
 */
int striatum_set_dopamine(striatum_t* striatum, float level);

/**
 * @brief Step striatum simulation
 * @param striatum Striatum instance
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int striatum_step(striatum_t* striatum, float dt);

/**
 * @brief Update synaptic weights (for learning)
 * @param striatum Striatum instance
 * @param action_id Action to update
 * @param delta_d1 Weight change for D1 pathway
 * @param delta_d2 Weight change for D2 pathway
 * @return 0 on success, negative on error
 */
int striatum_update_weights(
    striatum_t* striatum,
    uint32_t action_id,
    float delta_d1,
    float delta_d2
);

/**
 * @brief Get striatum statistics
 * @param striatum Striatum instance
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int striatum_get_stats(const striatum_t* striatum, striatum_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STRIATUM_H */
