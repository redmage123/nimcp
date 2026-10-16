/**
 * @file nimcp_cochlea_substrate_bridge.h
 * @brief Cochlea-Neural Substrate integration bridge
 *
 * WHAT: Connect cochlear output to neural substrate (SNN/neuron models)
 * WHY:  Enable spike-based auditory processing
 * HOW:  Convert rate-coded cochlear output to spike trains
 *
 * NEURAL SUBSTRATE INTEGRATION:
 * - Map cochlear channels to substrate neuron populations
 * - Convert firing rates to Poisson spike trains
 * - Preserve temporal fine structure via phase locking
 * - Enable STDP-based auditory learning
 *
 * BIOLOGICAL FIDELITY:
 * - Auditory nerve fibers as substrate inputs
 * - Tonotopic mapping preserved
 * - Temporal coding for low frequencies
 * - Rate coding for high frequencies
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_SUBSTRATE_BRIDGE_H
#define NIMCP_COCHLEA_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct neural_substrate neural_substrate_t;
typedef struct neuron_population neuron_population_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_SUBSTRATE_DEFAULT_NEURONS_PER_CHANNEL   10
#define COCHLEA_SUBSTRATE_MAX_SPIKE_RATE_HZ             300.0f
#define COCHLEA_SUBSTRATE_PHASE_LOCK_CUTOFF_HZ          4000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding mode
 */
typedef enum {
    SPIKE_ENCODE_RATE,              /**< Pure rate coding */
    SPIKE_ENCODE_TEMPORAL,          /**< Phase-locked temporal */
    SPIKE_ENCODE_MIXED              /**< Rate + temporal */
} spike_encoding_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Spike output from cochlea-substrate
 */
typedef struct {
    uint32_t* spike_counts;         /**< Spikes per population */
    float* spike_times;             /**< Spike timestamps (flat array) */
    uint32_t* spike_neuron_ids;     /**< Which neuron spiked */
    uint32_t total_spikes;          /**< Total spikes this step */
    uint32_t num_populations;       /**< Number of neuron pops */

    float avg_rate;                 /**< Average firing rate */
    float max_rate;                 /**< Peak firing rate */
} substrate_spike_output_t;

/**
 * @brief Population mapping
 */
typedef struct {
    uint32_t channel_id;            /**< Cochlear channel */
    uint32_t population_id;         /**< Substrate population */
    uint32_t num_neurons;           /**< Neurons in population */
    float center_freq_hz;           /**< Best frequency */
    spike_encoding_mode_t encoding; /**< Encoding mode */
} population_mapping_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Population setup */
    uint32_t neurons_per_channel;   /**< Neurons per cochlear channel */
    spike_encoding_mode_t default_encoding;

    /* Rate coding */
    float max_rate_hz;              /**< Maximum spike rate */
    float spontaneous_rate_hz;      /**< Baseline firing */

    /* Temporal coding */
    float phase_lock_cutoff_hz;     /**< Above this: rate only */
    float phase_lock_strength;      /**< Phase locking precision */

    /* Noise */
    float jitter_ms;                /**< Spike timing jitter */
    bool add_spontaneous;           /**< Add spontaneous spikes */
} cochlea_substrate_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_substrate_bridge cochlea_substrate_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_substrate_config_t cochlea_substrate_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_substrate_bridge_t* cochlea_substrate_bridge_create(
    cochlea_t* cochlea,
    neural_substrate_t* substrate,
    const cochlea_substrate_config_t* config
);

void cochlea_substrate_bridge_destroy(cochlea_substrate_bridge_t* bridge);

nimcp_error_t cochlea_substrate_bridge_update(
    cochlea_substrate_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_substrate_bridge_reset(cochlea_substrate_bridge_t* bridge);

//=============================================================================
// Spike Access
//=============================================================================

nimcp_error_t cochlea_substrate_get_spikes(
    const cochlea_substrate_bridge_t* bridge,
    substrate_spike_output_t* output
);

nimcp_error_t cochlea_substrate_get_population_spikes(
    const cochlea_substrate_bridge_t* bridge,
    uint32_t population_id,
    uint32_t* spike_count,
    float** spike_times
);

//=============================================================================
// Population Mapping
//=============================================================================

nimcp_error_t cochlea_substrate_get_mapping(
    const cochlea_substrate_bridge_t* bridge,
    uint32_t channel,
    population_mapping_t* mapping
);

nimcp_error_t cochlea_substrate_set_encoding(
    cochlea_substrate_bridge_t* bridge,
    uint32_t channel,
    spike_encoding_mode_t encoding
);

//=============================================================================
// Statistics
//=============================================================================

float cochlea_substrate_get_avg_rate(const cochlea_substrate_bridge_t* bridge);
uint64_t cochlea_substrate_get_total_spikes(const cochlea_substrate_bridge_t* bridge);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_substrate_verify_bidirectional(const cochlea_substrate_bridge_t* bridge);
uint64_t cochlea_substrate_get_last_outbound(const cochlea_substrate_bridge_t* bridge);
uint64_t cochlea_substrate_get_last_inbound(const cochlea_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_SUBSTRATE_BRIDGE_H */
