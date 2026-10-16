/**
 * @file nimcp_neuromod_nucleus_adapter.h
 * @brief Neuromodulatory Nucleus Adapter for specific brain nuclei
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts specific neuromodulatory nuclei for layer integration
 * WHY:  Each nucleus has distinct firing patterns and projection targets
 * HOW:  Configurable adapter for LC, VTA, Raphe, or basal forebrain
 *
 * SUPPORTED NUCLEI:
 * - Locus Coeruleus (LC): Norepinephrine, arousal/vigilance
 * - Ventral Tegmental Area (VTA): Dopamine, reward/motivation
 * - Raphe Nuclei: Serotonin, mood/inhibition
 * - Basal Forebrain: Acetylcholine, attention/encoding
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_NUCLEUS_ADAPTER_H
#define NIMCP_NEUROMOD_NUCLEUS_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_nucleus_adapter_struct* nimcp_nucleus_adapter_t;

//=============================================================================
// Nucleus Types
//=============================================================================

typedef enum {
    NUCLEUS_LC = 0,                     /**< Locus Coeruleus (NE) */
    NUCLEUS_VTA,                        /**< Ventral Tegmental Area (DA) */
    NUCLEUS_RAPHE,                      /**< Raphe Nuclei (5-HT) */
    NUCLEUS_BASAL_FOREBRAIN,            /**< Basal Forebrain (ACh) */
    NUCLEUS_COUNT
} nucleus_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    nucleus_type_t nucleus_type;        /**< Which nucleus this adapter models */
    uint32_t num_neurons;               /**< Number of neurons in nucleus */
    float tonic_rate_hz;                /**< Tonic firing rate (Hz) */
    float phasic_burst_rate_hz;         /**< Phasic burst rate (Hz) */
    float refractory_ms;                /**< Refractory period (ms) */
    float axon_conduction_ms;           /**< Axonal conduction delay (ms) */
    bool enable_autoreceptors;          /**< Enable autoreceptor feedback */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_nucleus_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float current_firing_rate_hz;       /**< Current firing rate */
    float output_concentration;         /**< Output neuromodulator level */
    float autoreceptor_inhibition;      /**< Autoreceptor feedback level */
    uint32_t burst_count;               /**< Number of bursts this epoch */
    float mean_isi_ms;                  /**< Mean inter-spike interval */
    bool is_active;                     /**< Whether adapter is active */
    bool in_burst;                      /**< Currently in phasic burst */
} nimcp_nucleus_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t spikes_generated;          /**< Total spikes */
    uint64_t bursts_generated;          /**< Total phasic bursts */
    float total_output;                 /**< Cumulative neuromodulator release */
} nimcp_nucleus_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration for a nucleus type
 */
NIMCP_EXPORT nimcp_nucleus_adapter_config_t nimcp_nucleus_adapter_default_config(
    nucleus_type_t nucleus_type
);

/**
 * @brief Create nucleus adapter
 *
 * @param config Configuration (NULL uses LC defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_nucleus_adapter_t nimcp_nucleus_adapter_create(
    const nimcp_nucleus_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_nucleus_adapter_destroy(nimcp_nucleus_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_nucleus_adapter_get_interface(
    nimcp_nucleus_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Trigger phasic burst response
 *
 * @param adapter Adapter handle
 * @param intensity Burst intensity (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_trigger_burst(
    nimcp_nucleus_adapter_t adapter,
    float intensity
);

/**
 * @brief Set tonic firing rate
 *
 * @param adapter Adapter handle
 * @param rate_hz Firing rate in Hz
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_set_tonic_rate(
    nimcp_nucleus_adapter_t adapter,
    float rate_hz
);

/**
 * @brief Get output neuromodulator concentration
 *
 * @param adapter Adapter handle
 * @param concentration_out Output concentration
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_get_output(
    nimcp_nucleus_adapter_t adapter,
    float* concentration_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_get_state(
    nimcp_nucleus_adapter_t adapter,
    nimcp_nucleus_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_get_stats(
    nimcp_nucleus_adapter_t adapter,
    nimcp_nucleus_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nucleus_adapter_reset_stats(
    nimcp_nucleus_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_NUCLEUS_ADAPTER_H */
