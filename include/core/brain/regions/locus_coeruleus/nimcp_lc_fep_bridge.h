/**
 * @file nimcp_lc_fep_bridge.h
 * @brief Locus Coeruleus - Free Energy Principle Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between LC (norepinephrine) and FEP inference
 * WHY:  Enable precision-weighted prediction error via NE-mediated gain control
 * HOW:  NE modulates prediction precision; surprise drives phasic LC response
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free Energy Principle and active inference
 * - Parr & Friston (2017): Precision and neuromodulation
 * - Sales et al. (2019): LC-NE and expected precision
 *
 * BIOLOGICAL BASIS:
 * - NE controls the gain on prediction errors (precision weighting)
 * - High NE = high precision = attend to prediction errors
 * - Low NE = low precision = rely on priors
 * - Surprise/novelty triggers phasic NE release
 *
 * INTEGRATION FLOWS:
 *
 * LC --> FEP:
 *   1. NE level sets precision on sensory prediction errors
 *   2. Phasic bursts signal high-precision events
 *   3. Arousal state modulates overall inference precision
 *   4. Exploration drive affects model update rate
 *
 * FEP --> LC:
 *   1. Free energy (surprise) drives phasic responses
 *   2. Expected precision modulates tonic NE baseline
 *   3. Model uncertainty triggers exploration mode
 *   4. Prediction errors gate learning via NE
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_fep.h
 */

#ifndef NIMCP_LC_FEP_BRIDGE_H
#define NIMCP_LC_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_lc_adapter_struct;
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
struct nimcp_fep_engine;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default precision baseline */
#define LC_FEP_PRECISION_BASELINE       1.0f

/** @brief Maximum precision multiplier */
#define LC_FEP_PRECISION_MAX            10.0f

/** @brief Minimum precision multiplier */
#define LC_FEP_PRECISION_MIN            0.1f

/** @brief Default surprise threshold for phasic response */
#define LC_FEP_SURPRISE_THRESHOLD       2.0f

/** @brief Bio-async module ID for LC-FEP bridge */
#define BIO_MODULE_LC_FEP_BRIDGE        0x0C10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Precision modulation mode
 */
typedef enum {
    LC_FEP_PRECISION_LINEAR = 0,     /**< Linear NE-precision mapping */
    LC_FEP_PRECISION_SIGMOID,        /**< Sigmoidal mapping */
    LC_FEP_PRECISION_EXPONENTIAL,    /**< Exponential mapping */
    LC_FEP_PRECISION_ADAPTIVE        /**< Context-adaptive mapping */
} nimcp_lc_fep_precision_mode_t;

/**
 * @brief Inference state for LC integration
 */
typedef enum {
    LC_FEP_STATE_STABLE = 0,         /**< Low free energy, stable model */
    LC_FEP_STATE_UPDATING,           /**< Model being updated */
    LC_FEP_STATE_SURPRISED,          /**< High prediction error */
    LC_FEP_STATE_EXPLORING           /**< Active exploration mode */
} nimcp_lc_fep_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-FEP bridge configuration
 */
typedef struct {
    /* Precision mapping */
    nimcp_lc_fep_precision_mode_t precision_mode;
    float precision_baseline;        /**< Baseline precision */
    float precision_gain;            /**< NE-to-precision gain */
    float precision_decay_tau_ms;    /**< Precision decay time constant */

    /* Surprise response */
    float surprise_threshold;        /**< Threshold for phasic trigger */
    float surprise_gain;             /**< Surprise-to-NE gain */
    float surprise_decay_tau_ms;     /**< Surprise decay constant */

    /* Free energy integration */
    float fe_coupling_strength;      /**< Coupling to free energy */
    float model_uncertainty_gain;    /**< Uncertainty effect on NE */
    bool enable_active_inference;    /**< Enable action-perception loop */

    /* Exploration/exploitation */
    float exploration_threshold;     /**< Uncertainty threshold for explore */
    float exploitation_precision;    /**< Precision during exploitation */

    /* Update parameters */
    float update_interval_ms;        /**< Bridge update interval */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_lc_fep_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Precision weighting output
 */
typedef struct {
    float sensory_precision;         /**< Precision on sensory errors */
    float prior_precision;           /**< Precision on prior predictions */
    float action_precision;          /**< Precision on action selection */
    float learning_rate;             /**< NE-modulated learning rate */
} nimcp_lc_fep_precision_t;

/**
 * @brief Free energy signal for LC
 */
typedef struct {
    float free_energy;               /**< Current free energy */
    float prediction_error;          /**< Sensory prediction error */
    float model_uncertainty;         /**< Epistemic uncertainty */
    float expected_precision;        /**< Expected sensory precision */
    bool surprise_event;             /**< High surprise detected */
} nimcp_lc_fep_signal_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_fep_state_t state;      /**< Current inference state */
    float current_precision;         /**< Current precision setting */
    float accumulated_surprise;      /**< Running surprise accumulator */
    float exploration_drive;         /**< Current exploration tendency */
    float ne_contribution;           /**< NE effect on inference */
    bool phasic_triggered;           /**< Phasic response active */
    float time_since_surprise;       /**< Time since last surprise */
} nimcp_lc_fep_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total update calls */
    uint64_t surprise_events;        /**< High surprise events */
    uint64_t phasic_triggers;        /**< Phasic responses triggered */
    float avg_precision;             /**< Average precision */
    float avg_free_energy;           /**< Average free energy */
    float avg_prediction_error;      /**< Average prediction error */
    float total_ne_contribution;     /**< Total NE effect */
} nimcp_lc_fep_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_fep_bridge nimcp_lc_fep_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_lc_fep_config_t nimcp_lc_fep_config_default(void);

/**
 * @brief Create LC-FEP bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
nimcp_lc_fep_bridge_t* nimcp_lc_fep_create(const nimcp_lc_fep_config_t* config);

/**
 * @brief Destroy LC-FEP bridge
 */
void nimcp_lc_fep_destroy(nimcp_lc_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_lc_fep_reset(nimcp_lc_fep_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to LC adapter
 */
int nimcp_lc_fep_connect_lc(
    nimcp_lc_fep_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

/**
 * @brief Connect to FEP engine
 */
int nimcp_lc_fep_connect_fep(
    nimcp_lc_fep_bridge_t* bridge,
    struct nimcp_fep_engine* fep
);

/*=============================================================================
 * LC --> FEP API
 *===========================================================================*/

/**
 * @brief Compute precision weights from NE state
 * @param bridge Bridge instance
 * @param precision Output precision values
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_fep_compute_precision(
    nimcp_lc_fep_bridge_t* bridge,
    nimcp_lc_fep_precision_t* precision
);

/**
 * @brief Get NE-modulated learning rate
 * @param bridge Bridge instance
 * @return Learning rate multiplier
 */
float nimcp_lc_fep_get_learning_rate(nimcp_lc_fep_bridge_t* bridge);

/**
 * @brief Check if exploration mode recommended
 * @param bridge Bridge instance
 * @return true if exploration recommended
 */
bool nimcp_lc_fep_should_explore(nimcp_lc_fep_bridge_t* bridge);

/*=============================================================================
 * FEP --> LC API
 *===========================================================================*/

/**
 * @brief Receive free energy signal
 * @param bridge Bridge instance
 * @param signal FEP signal data
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_fep_receive_signal(
    nimcp_lc_fep_bridge_t* bridge,
    const nimcp_lc_fep_signal_t* signal
);

/**
 * @brief Process surprise event
 * @param bridge Bridge instance
 * @param surprise_magnitude Magnitude of surprise
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_fep_process_surprise(
    nimcp_lc_fep_bridge_t* bridge,
    float surprise_magnitude
);

/**
 * @brief Update with prediction error
 * @param bridge Bridge instance
 * @param prediction_error Error magnitude
 * @return NE response magnitude
 */
float nimcp_lc_fep_update_prediction_error(
    nimcp_lc_fep_bridge_t* bridge,
    float prediction_error
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update bridge state
 * @param bridge Bridge instance
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_fep_update(nimcp_lc_fep_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_lc_fep_get_state(
    const nimcp_lc_fep_bridge_t* bridge,
    nimcp_lc_fep_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_lc_fep_get_stats(
    const nimcp_lc_fep_bridge_t* bridge,
    nimcp_lc_fep_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_FEP_BRIDGE_H */
