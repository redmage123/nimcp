//=============================================================================
// nimcp_subthalamic.h - Subthalamic Nucleus (STN)
//=============================================================================
/**
 * @file nimcp_subthalamic.h
 * @brief Subthalamic nucleus implementation for action suppression
 *
 * WHAT: Subthalamic nucleus model for global action suppression
 * WHY:  STN provides "hold your horses" signal to prevent premature action
 * HOW:  Receives cortical input (hyperdirect) and GPe input (indirect pathway)
 *
 * BIOLOGICAL BASIS:
 * - STN is a small glutamatergic (excitatory) nucleus
 * - Key role in stopping ongoing actions and preventing premature responses
 * - Two main pathways:
 *   1. Hyperdirect: Cortex → STN → GPi/SNr (fast global inhibition)
 *   2. Indirect: GPe → STN → GPi/SNr (slower, selective)
 * - STN dysfunction leads to hemiballismus (violent involuntary movements)
 * - Deep brain stimulation of STN used to treat Parkinson's disease
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_SUBTHALAMIC_H
#define NIMCP_SUBTHALAMIC_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define STN_MAX_NEURONS 256             /**< Maximum STN neurons */
#define STN_DEFAULT_NEURONS 64          /**< Default number of neurons */
#define STN_TONIC_FIRING 25.0f          /**< Baseline tonic firing (Hz) */
#define STN_MAX_FIRING 300.0f           /**< Maximum burst firing (Hz) */
#define STN_HYPERDIRECT_DELAY 3.0f      /**< Hyperdirect pathway delay (ms) */
#define STN_INDIRECT_DELAY 10.0f        /**< Indirect pathway delay (ms) */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief STN activation mode
 */
typedef enum {
    STN_MODE_BASELINE = 0,      /**< Normal baseline activity */
    STN_MODE_HYPERDIRECT,       /**< Activated via hyperdirect pathway */
    STN_MODE_INDIRECT,          /**< Disinhibited via indirect pathway */
    STN_MODE_SUPPRESSION        /**< Maximum suppression mode */
} stn_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief STN neuron
 */
typedef struct {
    uint32_t id;                /**< Neuron ID */
    float firing_rate;          /**< Current firing rate (Hz) */
    float membrane_potential;   /**< Membrane potential */
    float cortical_input;       /**< Hyperdirect cortical input */
    float gpe_inhibition;       /**< GPe inhibition level */
} stn_neuron_t;

/**
 * @brief Subthalamic nucleus configuration
 */
typedef struct {
    uint32_t num_neurons;           /**< Number of neurons */
    uint32_t num_actions;           /**< Number of actions to modulate */
    float tonic_firing_rate;        /**< Baseline tonic firing (Hz) */
    float max_firing_rate;          /**< Maximum firing rate (Hz) */
    float hyperdirect_gain;         /**< Gain for hyperdirect pathway */
    float indirect_gain;            /**< Gain for indirect pathway (GPe input) */
    float hyperdirect_delay_ms;     /**< Hyperdirect pathway delay */
    float indirect_delay_ms;        /**< Indirect pathway delay */
    float urgency_threshold;        /**< Threshold for urgent stop signal */
} subthalamic_config_t;

/**
 * @brief STN statistics
 */
typedef struct {
    float avg_firing_rate;          /**< Average firing rate */
    float max_firing_rate;          /**< Maximum firing rate */
    float hyperdirect_activations;  /**< Hyperdirect pathway activations */
    float indirect_activations;     /**< Indirect pathway activations */
    uint32_t suppression_events;    /**< Number of suppression events */
} stn_stats_t;

/**
 * @brief Subthalamic nucleus structure
 */
typedef struct {
    /* Neurons */
    stn_neuron_t* neurons;          /**< STN neurons */
    uint32_t num_neurons;           /**< Number of neurons */

    /* Output */
    float* output;                  /**< Output to GPi/SNr (size num_actions) */
    uint32_t num_actions;           /**< Number of actions */
    float global_output;            /**< Global suppression signal */

    /* Mode */
    stn_mode_t mode;                /**< Current activation mode */

    /* Input buffers */
    float* cortical_input;          /**< Hyperdirect cortical input */
    float* gpe_input;               /**< GPe input (indirect pathway) */

    /* Delay buffers (for realistic timing) */
    float* hyperdirect_buffer;      /**< Buffer for hyperdirect delay */
    float* indirect_buffer;         /**< Buffer for indirect delay */
    uint32_t buffer_index;          /**< Current buffer position */
    uint32_t hyperdirect_delay_samples; /**< Delay in samples */
    uint32_t indirect_delay_samples;    /**< Delay in samples */

    /* Configuration */
    subthalamic_config_t config;    /**< Configuration */

    /* Statistics */
    stn_stats_t stats;              /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Mutex for thread safety */
} subthalamic_nucleus_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default configuration
 * @param config Configuration to initialize
 */
void subthalamic_default_config(subthalamic_config_t* config);

/**
 * @brief Create subthalamic nucleus
 * @param config Configuration
 * @return New STN instance or NULL on failure
 */
subthalamic_nucleus_t* subthalamic_create(const subthalamic_config_t* config);

/**
 * @brief Destroy subthalamic nucleus
 * @param stn Instance to destroy
 */
void subthalamic_destroy(subthalamic_nucleus_t* stn);

/**
 * @brief Reset to initial state
 * @param stn Instance to reset
 * @return 0 on success, negative on error
 */
int subthalamic_reset(subthalamic_nucleus_t* stn);

//=============================================================================
// Input Functions
//=============================================================================

/**
 * @brief Set hyperdirect cortical input
 *
 * This is the fast pathway for emergency stopping
 *
 * @param stn STN instance
 * @param input Cortical input (global or per-action)
 * @param global True for global stop signal, false for per-action
 * @return 0 on success, negative on error
 */
int subthalamic_set_cortical_input(
    subthalamic_nucleus_t* stn,
    const float* input,
    bool global
);

/**
 * @brief Set GPe input (indirect pathway)
 * @param stn STN instance
 * @param input GPe output levels (size num_actions)
 * @return 0 on success, negative on error
 */
int subthalamic_set_gpe_input(
    subthalamic_nucleus_t* stn,
    const float* input
);

/**
 * @brief Trigger emergency stop (maximum hyperdirect activation)
 * @param stn STN instance
 * @param urgency Urgency level [0-1]
 * @return 0 on success, negative on error
 */
int subthalamic_emergency_stop(
    subthalamic_nucleus_t* stn,
    float urgency
);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process inputs and update output
 * @param stn STN instance
 * @return 0 on success, negative on error
 */
int subthalamic_process(subthalamic_nucleus_t* stn);

/**
 * @brief Get output to GPi/SNr
 * @param stn STN instance
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int subthalamic_get_output(const subthalamic_nucleus_t* stn, float* output);

/**
 * @brief Get global suppression signal
 * @param stn STN instance
 * @return Global suppression level [0-1]
 */
float subthalamic_get_global_output(const subthalamic_nucleus_t* stn);

/**
 * @brief Step simulation
 * @param stn STN instance
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int subthalamic_step(subthalamic_nucleus_t* stn, float dt);

/**
 * @brief Get current mode
 * @param stn STN instance
 * @return Current activation mode
 */
stn_mode_t subthalamic_get_mode(const subthalamic_nucleus_t* stn);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get statistics
 * @param stn STN instance
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int subthalamic_get_stats(const subthalamic_nucleus_t* stn, stn_stats_t* stats);

/**
 * @brief Get mode name
 * @param mode STN mode
 * @return Mode name string
 */
const char* subthalamic_mode_name(stn_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUBTHALAMIC_H */
