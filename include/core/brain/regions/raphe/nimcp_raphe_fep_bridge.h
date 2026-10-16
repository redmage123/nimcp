/**
 * @file nimcp_raphe_fep_bridge.h
 * @brief Raphe Nuclei - Free Energy Principle Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and FEP inference
 * WHY:  Enable 5-HT-mediated temporal discounting and aversive prediction
 * HOW:  5-HT modulates temporal precision; punishment predictions drive 5-HT
 *
 * THEORETICAL FOUNDATIONS:
 * - Dayan & Huys (2009): Serotonin and Bayesian prediction
 * - Cools et al. (2011): 5-HT and aversive processing
 * - Boureau & Dayan (2011): Opponency in reward/punishment
 *
 * BIOLOGICAL BASIS:
 * - 5-HT signals aversive prediction errors
 * - Temporal discounting modulated by 5-HT
 * - Patience and waiting behavior via 5-HT
 * - 5-HT affects precision on future outcomes
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> FEP:
 *   1. 5-HT level sets temporal precision/discount
 *   2. Aversive signals modulate negative predictions
 *   3. Patience state affects future outcome weighting
 *   4. Impulse control gates belief updating rate
 *
 * FEP --> Raphe:
 *   1. Aversive prediction errors drive 5-HT
 *   2. Temporal uncertainty affects 5-HT tone
 *   3. Punishment expectations modulate 5-HT release
 *   4. Long-term cost predictions shape 5-HT
 *
 * @see nimcp_raphe.h
 * @see nimcp_fep.h
 */

#ifndef NIMCP_RAPHE_FEP_BRIDGE_H
#define NIMCP_RAPHE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter_struct;
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
struct nimcp_fep_engine;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default temporal discount factor */
#define RAPHE_FEP_DISCOUNT_BASELINE     0.95f

/** @brief Default aversive precision */
#define RAPHE_FEP_AVERSIVE_PRECISION    1.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_FEP_BRIDGE     0x0E10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Temporal precision mode
 */
typedef enum {
    RAPHE_FEP_TEMPORAL_SHORT = 0,    /**< Short-term focus */
    RAPHE_FEP_TEMPORAL_BALANCED,     /**< Balanced temporal view */
    RAPHE_FEP_TEMPORAL_LONG,         /**< Long-term planning */
    RAPHE_FEP_TEMPORAL_ADAPTIVE      /**< Context-adaptive */
} nimcp_raphe_fep_temporal_t;

/**
 * @brief Aversive processing mode
 */
typedef enum {
    RAPHE_FEP_AVERSIVE_ATTENUATE = 0, /**< Attenuate aversive signals */
    RAPHE_FEP_AVERSIVE_NORMAL,        /**< Normal processing */
    RAPHE_FEP_AVERSIVE_AMPLIFY        /**< Amplify aversive signals */
} nimcp_raphe_fep_aversive_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-FEP bridge configuration
 */
typedef struct {
    /* Temporal discounting */
    nimcp_raphe_fep_temporal_t temporal_mode;
    float discount_baseline;         /**< Baseline discount factor */
    float ht5_discount_gain;         /**< 5-HT effect on discounting */

    /* Aversive processing */
    nimcp_raphe_fep_aversive_t aversive_mode;
    float aversive_precision;        /**< Precision on aversive pred */
    float aversive_gain;             /**< Aversive signal gain */

    /* Patience/waiting */
    float patience_threshold;        /**< Threshold for patience mode */
    float impulse_control_gain;      /**< Impulse control strength */

    /* Prediction error */
    float ape_threshold;             /**< Aversive PE threshold */
    float ape_gain;                  /**< APE-to-5HT gain */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_raphe_fep_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Temporal precision output
 */
typedef struct {
    float temporal_discount;         /**< Current discount factor */
    float future_precision;          /**< Precision on future outcomes */
    float aversive_precision;        /**< Precision on aversive preds */
    float patience_level;            /**< Current patience [0-1] */
} nimcp_raphe_fep_precision_t;

/**
 * @brief FEP signal to Raphe
 */
typedef struct {
    float aversive_prediction_error; /**< Aversive prediction error */
    float temporal_uncertainty;      /**< Uncertainty about timing */
    float punishment_expectation;    /**< Expected punishment */
    float waiting_cost;              /**< Cost of waiting */
    bool aversive_surprise;          /**< Unexpected aversive event */
} nimcp_raphe_fep_signal_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_fep_temporal_t temporal_mode;
    float current_discount;
    float current_patience;
    float accumulated_ape;
    bool in_patience_mode;
} nimcp_raphe_fep_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t aversive_events;
    uint64_t patience_activations;
    float avg_temporal_discount;
    float avg_aversive_pe;
    float total_waiting_time;
} nimcp_raphe_fep_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_fep_bridge nimcp_raphe_fep_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_raphe_fep_config_t nimcp_raphe_fep_config_default(void);

nimcp_raphe_fep_bridge_t* nimcp_raphe_fep_create(
    const nimcp_raphe_fep_config_t* config
);

void nimcp_raphe_fep_destroy(nimcp_raphe_fep_bridge_t* bridge);

int nimcp_raphe_fep_reset(nimcp_raphe_fep_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_raphe_fep_connect_raphe(
    nimcp_raphe_fep_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

int nimcp_raphe_fep_connect_fep(
    nimcp_raphe_fep_bridge_t* bridge,
    struct nimcp_fep_engine* fep
);

/*=============================================================================
 * Raphe --> FEP API
 *===========================================================================*/

/**
 * @brief Compute temporal precision from 5-HT state
 */
int nimcp_raphe_fep_compute_precision(
    nimcp_raphe_fep_bridge_t* bridge,
    nimcp_raphe_fep_precision_t* precision
);

/**
 * @brief Get 5-HT-modulated discount factor
 */
float nimcp_raphe_fep_get_discount(nimcp_raphe_fep_bridge_t* bridge);

/**
 * @brief Get patience level for waiting
 */
float nimcp_raphe_fep_get_patience(nimcp_raphe_fep_bridge_t* bridge);

/*=============================================================================
 * FEP --> Raphe API
 *===========================================================================*/

/**
 * @brief Receive FEP signal
 */
int nimcp_raphe_fep_receive_signal(
    nimcp_raphe_fep_bridge_t* bridge,
    const nimcp_raphe_fep_signal_t* signal
);

/**
 * @brief Process aversive prediction error
 */
int nimcp_raphe_fep_process_ape(
    nimcp_raphe_fep_bridge_t* bridge,
    float ape_magnitude
);

/**
 * @brief Get 5-HT response to FEP state
 */
float nimcp_raphe_fep_get_ht5_response(nimcp_raphe_fep_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_raphe_fep_update(nimcp_raphe_fep_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_raphe_fep_get_state(
    const nimcp_raphe_fep_bridge_t* bridge,
    nimcp_raphe_fep_bridge_state_t* state
);

int nimcp_raphe_fep_get_stats(
    const nimcp_raphe_fep_bridge_t* bridge,
    nimcp_raphe_fep_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_FEP_BRIDGE_H */
