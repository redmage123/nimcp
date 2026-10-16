/**
 * @file nimcp_neuromod_pool_adapter.h
 * @brief Neuromodulator Pool Adapter for Neuromodulatory Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts the global neuromodulator pool for layer integration
 * WHY:  Provides unified access to all neuromodulator levels
 * HOW:  Implements nimcp_module_interface_t wrapping neuromodulator_pool_t
 *
 * NEUROMODULATORY SYSTEMS:
 * - Dopamine (DA): VTA/SNc - reward, motivation
 * - Serotonin (5-HT): Raphe - mood, inhibition
 * - Acetylcholine (ACh): Basal forebrain - attention
 * - Norepinephrine (NE): Locus coeruleus - arousal
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_POOL_ADAPTER_H
#define NIMCP_NEUROMOD_POOL_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_neuromod_pool_adapter_struct* nimcp_neuromod_pool_adapter_t;

//=============================================================================
// Neuromodulator Types (matches plasticity/neuromodulators)
//=============================================================================

typedef enum {
    NMOD_DOPAMINE = 0,
    NMOD_SEROTONIN,
    NMOD_ACETYLCHOLINE,
    NMOD_NOREPINEPHRINE,
    NMOD_COUNT
} nmod_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    float basal_levels[NMOD_COUNT];     /**< Baseline concentrations (0-1) */
    float decay_rates[NMOD_COUNT];      /**< Decay time constants (ms) */
    float max_levels[NMOD_COUNT];       /**< Maximum concentrations */
    bool enable_cross_modulation;       /**< Enable modulators affecting each other */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_neuromod_pool_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float levels[NMOD_COUNT];           /**< Current neuromodulator levels */
    float tonic_levels[NMOD_COUNT];     /**< Tonic (baseline) levels */
    float phasic_levels[NMOD_COUNT];    /**< Phasic (transient) levels */
    float global_arousal;               /**< Overall arousal state (0-1) */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_neuromod_pool_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t release_events[NMOD_COUNT];/**< Release events per modulator */
    float peak_levels[NMOD_COUNT];      /**< Peak levels reached */
} nimcp_neuromod_pool_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_neuromod_pool_config_t nimcp_neuromod_pool_adapter_default_config(void);

/**
 * @brief Create neuromodulator pool adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_neuromod_pool_adapter_t nimcp_neuromod_pool_adapter_create(
    const nimcp_neuromod_pool_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_neuromod_pool_adapter_destroy(nimcp_neuromod_pool_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_neuromod_pool_adapter_get_interface(
    nimcp_neuromod_pool_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Release neuromodulator (phasic burst)
 *
 * @param adapter Adapter handle
 * @param nmod_type Neuromodulator type
 * @param amount Release amount (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_release(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float amount
);

/**
 * @brief Set tonic (baseline) level
 *
 * @param adapter Adapter handle
 * @param nmod_type Neuromodulator type
 * @param level Tonic level (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_set_tonic(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float level
);

/**
 * @brief Get all neuromodulator levels
 *
 * @param adapter Adapter handle
 * @param levels_out Output array for levels [NMOD_COUNT]
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_levels(
    nimcp_neuromod_pool_adapter_t adapter,
    float* levels_out
);

/**
 * @brief Get specific neuromodulator level
 *
 * @param adapter Adapter handle
 * @param nmod_type Neuromodulator type
 * @param level_out Output level
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_level(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float* level_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_state(
    nimcp_neuromod_pool_adapter_t adapter,
    nimcp_neuromod_pool_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_stats(
    nimcp_neuromod_pool_adapter_t adapter,
    nimcp_neuromod_pool_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_pool_adapter_reset_stats(
    nimcp_neuromod_pool_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_POOL_ADAPTER_H */
