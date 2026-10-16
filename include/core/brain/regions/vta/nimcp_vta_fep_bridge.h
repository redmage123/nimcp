/**
 * @file nimcp_vta_fep_bridge.h
 * @brief Ventral Tegmental Area - Free Energy Principle Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and FEP inference
 * WHY:  Enable prediction error signaling via DA reward prediction errors
 * HOW:  DA signals precision-weighted value prediction errors for learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston et al. (2012): Dopamine and active inference
 * - Schwartenbeck et al. (2015): Expected free energy and DA
 * - FitzGerald et al. (2015): DA and model-based control
 *
 * BIOLOGICAL BASIS:
 * - DA encodes reward prediction errors (RPE)
 * - RPE = precision-weighted value prediction error
 * - DA modulates model updating for value-relevant features
 * - Expected free energy drives action selection via DA
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> FEP:
 *   1. RPE signals precision-weighted value errors
 *   2. DA level modulates value model updating
 *   3. Incentive salience affects expected free energy
 *   4. Motivational state shapes belief updating
 *
 * FEP --> VTA:
 *   1. Expected free energy drives DA release patterns
 *   2. Value prediction errors trigger phasic DA
 *   3. Epistemic value affects exploration via DA
 *   4. Model uncertainty modulates tonic DA
 *
 * @see nimcp_vta.h
 * @see nimcp_fep.h
 */

#ifndef NIMCP_VTA_FEP_BRIDGE_H
#define NIMCP_VTA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_vta_adapter_struct;
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
struct nimcp_fep_engine;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default value precision baseline */
#define VTA_FEP_VALUE_PRECISION_BASE    1.0f

/** @brief Maximum value precision */
#define VTA_FEP_VALUE_PRECISION_MAX     5.0f

/** @brief RPE threshold for significant update */
#define VTA_FEP_RPE_THRESHOLD           0.1f

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_FEP_BRIDGE       0x0D10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Value prediction mode
 */
typedef enum {
    VTA_FEP_VALUE_MODEL_FREE = 0,    /**< Model-free value prediction */
    VTA_FEP_VALUE_MODEL_BASED,       /**< Model-based value prediction */
    VTA_FEP_VALUE_HYBRID,            /**< Hybrid model-free/model-based */
    VTA_FEP_VALUE_EPISTEMIC          /**< Epistemic (information-seeking) */
} nimcp_vta_fep_value_mode_t;

/**
 * @brief Expected free energy component
 */
typedef enum {
    VTA_FEP_EFE_PRAGMATIC = 0,       /**< Goal-directed (reward) */
    VTA_FEP_EFE_EPISTEMIC,           /**< Information gain */
    VTA_FEP_EFE_COMBINED             /**< Combined pragmatic + epistemic */
} nimcp_vta_fep_efe_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-FEP bridge configuration
 */
typedef struct {
    /* Value precision */
    nimcp_vta_fep_value_mode_t value_mode;
    float value_precision_baseline;  /**< Baseline value precision */
    float da_precision_gain;         /**< DA-to-precision gain */

    /* RPE parameters */
    float rpe_threshold;             /**< Threshold for RPE signaling */
    float rpe_gain;                  /**< RPE magnitude scaling */
    float rpe_decay_tau_ms;          /**< RPE signal decay */

    /* Expected free energy */
    nimcp_vta_fep_efe_t efe_mode;    /**< EFE computation mode */
    float pragmatic_weight;          /**< Weight on pragmatic value */
    float epistemic_weight;          /**< Weight on epistemic value */

    /* Model updating */
    float model_update_rate;         /**< DA-modulated update rate */
    bool enable_active_inference;    /**< Enable action-perception loop */

    /* Update parameters */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_vta_fep_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Value precision output
 */
typedef struct {
    float value_precision;           /**< Precision on value predictions */
    float reward_precision;          /**< Precision on reward signals */
    float action_precision;          /**< Precision on action selection */
    float model_update_rate;         /**< DA-modulated learning rate */
} nimcp_vta_fep_precision_t;

/**
 * @brief Free energy signal to VTA
 */
typedef struct {
    float expected_free_energy;      /**< Expected free energy */
    float pragmatic_value;           /**< Goal-directed value */
    float epistemic_value;           /**< Information value */
    float value_prediction_error;    /**< Precision-weighted VPE */
    bool surprise_event;             /**< High value surprise */
} nimcp_vta_fep_signal_t;

/**
 * @brief RPE output for FEP
 */
typedef struct {
    float rpe_magnitude;             /**< RPE magnitude */
    float rpe_sign;                  /**< RPE sign (+/-) */
    float precision_weighted_rpe;    /**< Precision-weighted RPE */
    bool phasic_response;            /**< Phasic DA response occurred */
} nimcp_vta_fep_rpe_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_fep_value_mode_t current_mode;
    float current_value_precision;
    float accumulated_rpe;
    float expected_value;
    float actual_value;
    bool in_update;
} nimcp_vta_fep_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t rpe_events;
    uint64_t positive_rpes;
    uint64_t negative_rpes;
    float avg_value_precision;
    float avg_rpe_magnitude;
    float total_model_updates;
} nimcp_vta_fep_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_fep_bridge nimcp_vta_fep_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_fep_config_t nimcp_vta_fep_config_default(void);

nimcp_vta_fep_bridge_t* nimcp_vta_fep_create(
    const nimcp_vta_fep_config_t* config
);

void nimcp_vta_fep_destroy(nimcp_vta_fep_bridge_t* bridge);

int nimcp_vta_fep_reset(nimcp_vta_fep_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_fep_connect_vta(
    nimcp_vta_fep_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

int nimcp_vta_fep_connect_fep(
    nimcp_vta_fep_bridge_t* bridge,
    struct nimcp_fep_engine* fep
);

/*=============================================================================
 * VTA --> FEP API
 *===========================================================================*/

/**
 * @brief Compute value precision from DA state
 */
int nimcp_vta_fep_compute_precision(
    nimcp_vta_fep_bridge_t* bridge,
    nimcp_vta_fep_precision_t* precision
);

/**
 * @brief Get current RPE for FEP
 */
int nimcp_vta_fep_get_rpe(
    nimcp_vta_fep_bridge_t* bridge,
    nimcp_vta_fep_rpe_t* rpe
);

/**
 * @brief Get DA-modulated learning rate
 */
float nimcp_vta_fep_get_learning_rate(nimcp_vta_fep_bridge_t* bridge);

/*=============================================================================
 * FEP --> VTA API
 *===========================================================================*/

/**
 * @brief Receive expected free energy signal
 */
int nimcp_vta_fep_receive_signal(
    nimcp_vta_fep_bridge_t* bridge,
    const nimcp_vta_fep_signal_t* signal
);

/**
 * @brief Process value prediction error
 */
int nimcp_vta_fep_process_vpe(
    nimcp_vta_fep_bridge_t* bridge,
    float predicted_value,
    float actual_value
);

/**
 * @brief Get expected value from FEP model
 */
float nimcp_vta_fep_get_expected_value(nimcp_vta_fep_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_fep_update(nimcp_vta_fep_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_vta_fep_get_state(
    const nimcp_vta_fep_bridge_t* bridge,
    nimcp_vta_fep_bridge_state_t* state
);

int nimcp_vta_fep_get_stats(
    const nimcp_vta_fep_bridge_t* bridge,
    nimcp_vta_fep_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_FEP_BRIDGE_H */
