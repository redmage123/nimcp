//=============================================================================
// nimcp_globus_pallidus.h - Globus Pallidus (GPe/GPi)
//=============================================================================
/**
 * @file nimcp_globus_pallidus.h
 * @brief Globus pallidus implementation (external and internal segments)
 *
 * WHAT: Globus pallidus model with GPe and GPi/SNr output
 * WHY:  GP is the output stage of basal ganglia, controlling thalamic disinhibition
 * HOW:  GPi/SNr tonically inhibit thalamus; disinhibition allows movement
 *
 * BIOLOGICAL BASIS:
 * - GPe (external): Part of indirect pathway, inhibited by D2 striatal neurons
 * - GPi (internal): Main output, projects to thalamus (VA/VL nuclei)
 * - GPi neurons have high tonic firing (~60-80 Hz), providing constant inhibition
 * - Direct pathway: Striatum D1 inhibits GPi → disinhibits thalamus (GO)
 * - Indirect pathway: Striatum D2 inhibits GPe → disinhibits STN → excites GPi (NO-GO)
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_GLOBUS_PALLIDUS_H
#define NIMCP_GLOBUS_PALLIDUS_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define GP_MAX_NEURONS 512              /**< Maximum GP neurons */
#define GP_DEFAULT_NEURONS 128          /**< Default number of neurons */
#define GP_TONIC_FIRING_RATE 70.0f      /**< Baseline tonic firing (Hz) */
#define GP_MAX_FIRING_RATE 200.0f       /**< Maximum firing rate (Hz) */
#define GP_INHIBITION_TIME_CONST 10.0f  /**< Inhibition time constant (ms) */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Globus pallidus segment type
 */
typedef enum {
    GP_SEGMENT_EXTERNAL = 0,    /**< GPe - external segment */
    GP_SEGMENT_INTERNAL         /**< GPi - internal segment */
} gp_segment_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Globus pallidus neuron
 */
typedef struct {
    uint32_t id;                /**< Neuron ID */
    float firing_rate;          /**< Current firing rate (Hz) */
    float membrane_potential;   /**< Membrane potential */
    float inhibition;           /**< Current inhibition level */
    uint32_t target_action;     /**< Action this neuron maps to */
} gp_neuron_t;

/**
 * @brief Globus pallidus configuration
 */
typedef struct {
    gp_segment_t segment;           /**< GPe or GPi */
    uint32_t num_neurons;           /**< Number of neurons */
    uint32_t num_actions;           /**< Number of actions to map */
    float tonic_firing_rate;        /**< Baseline tonic firing (Hz) */
    float max_firing_rate;          /**< Maximum firing rate (Hz) */
    float inhibition_time_const;    /**< Time constant for inhibition decay */
    float inhibition_gain;          /**< Gain for input inhibition */
} globus_pallidus_config_t;

/**
 * @brief Globus pallidus statistics
 */
typedef struct {
    float avg_firing_rate;          /**< Average firing rate */
    float min_firing_rate;          /**< Minimum firing rate */
    float max_firing_rate;          /**< Maximum firing rate */
    float avg_inhibition;           /**< Average inhibition level */
    uint32_t active_neurons;        /**< Number of active neurons */
} gp_stats_t;

/**
 * @brief Globus pallidus structure
 */
typedef struct {
    /* Properties */
    gp_segment_t segment;           /**< GPe or GPi */
    gp_neuron_t* neurons;           /**< GP neurons */
    uint32_t num_neurons;           /**< Number of neurons */

    /* Output per action */
    float* output;                  /**< Output activations (size num_actions) */
    uint32_t num_actions;           /**< Number of actions */

    /* Configuration */
    globus_pallidus_config_t config; /**< Configuration */

    /* Input buffers */
    float* striatal_input;          /**< Input from striatum */
    float* stn_input;               /**< Input from STN (GPi only) */
    float* gpe_input;               /**< Input from GPe (for GPi) */

    /* Statistics */
    gp_stats_t stats;               /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Mutex for thread safety */
} globus_pallidus_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default configuration
 * @param config Configuration to initialize
 * @param segment GPe or GPi segment
 */
void globus_pallidus_default_config(
    globus_pallidus_config_t* config,
    gp_segment_t segment
);

/**
 * @brief Create globus pallidus
 * @param config Configuration
 * @return New GP instance or NULL on failure
 */
globus_pallidus_t* globus_pallidus_create(const globus_pallidus_config_t* config);

/**
 * @brief Destroy globus pallidus
 * @param gp Instance to destroy
 */
void globus_pallidus_destroy(globus_pallidus_t* gp);

/**
 * @brief Reset to initial state
 * @param gp Instance to reset
 * @return 0 on success, negative on error
 */
int globus_pallidus_reset(globus_pallidus_t* gp);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Set striatal inhibition input
 * @param gp GP instance
 * @param inhibition Inhibition levels (size num_actions)
 * @return 0 on success, negative on error
 */
int globus_pallidus_set_striatal_input(
    globus_pallidus_t* gp,
    const float* inhibition
);

/**
 * @brief Set STN excitation input (for GPi)
 * @param gp GP instance
 * @param excitation Excitation levels (size num_actions)
 * @return 0 on success, negative on error
 */
int globus_pallidus_set_stn_input(
    globus_pallidus_t* gp,
    const float* excitation
);

/**
 * @brief Set GPe input (for GPi in indirect pathway)
 * @param gp GP instance (must be GPi)
 * @param input GPe output levels
 * @return 0 on success, negative on error
 */
int globus_pallidus_set_gpe_input(
    globus_pallidus_t* gp,
    const float* input
);

/**
 * @brief Process inputs and update output
 * @param gp GP instance
 * @return 0 on success, negative on error
 */
int globus_pallidus_process(globus_pallidus_t* gp);

/**
 * @brief Get output (thalamic inhibition level)
 * @param gp GP instance
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int globus_pallidus_get_output(const globus_pallidus_t* gp, float* output);

/**
 * @brief Get output for specific action
 * @param gp GP instance
 * @param action_id Action to query
 * @return Output level [0-1], or -1 on error
 */
float globus_pallidus_get_action_output(
    const globus_pallidus_t* gp,
    uint32_t action_id
);

/**
 * @brief Step simulation
 * @param gp GP instance
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int globus_pallidus_step(globus_pallidus_t* gp, float dt);

/**
 * @brief Get statistics
 * @param gp GP instance
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int globus_pallidus_get_stats(const globus_pallidus_t* gp, gp_stats_t* stats);

/**
 * @brief Get segment name
 * @param segment Segment type
 * @return Segment name string
 */
const char* globus_pallidus_segment_name(gp_segment_t segment);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLOBUS_PALLIDUS_H */
