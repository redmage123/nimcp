/**
 * @file nimcp_habenula_fep_bridge.h
 * @brief Habenula - Free Energy Principle Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Habenula (aversive center) and FEP inference
 * WHY:  Enable negative prediction error signaling via habenula-mediated circuits
 * HOW:  Habenula signals disappointment/punishment; aversive PEs drive habenula
 *
 * THEORETICAL FOUNDATIONS:
 * - Matsumoto & Hikosaka (2009): Habenula and negative reward signals
 * - Lawson et al. (2014): Aversive prediction errors
 * - Proulx et al. (2014): Habenula in decision making
 *
 * BIOLOGICAL BASIS:
 * - Lateral habenula (LHb) signals negative reward prediction errors
 * - Inhibits VTA/Raphe during disappointment
 * - Encodes worse-than-expected outcomes
 * - Critical for learning from aversive outcomes
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> FEP:
 *   1. LHb activation signals negative prediction errors
 *   2. Disappointment modulates belief updating
 *   3. Aversive precision via habenula output
 *   4. Punishment learning rate modulation
 *
 * FEP --> Habenula:
 *   1. Negative prediction errors drive LHb activation
 *   2. Expected value violations trigger habenula
 *   3. Worse-than-expected outcomes activate LHb
 *   4. Aversive surprise signals to habenula
 *
 * @see nimcp_habenula.h
 * @see nimcp_fep.h
 */

#ifndef NIMCP_HABENULA_FEP_BRIDGE_H
#define NIMCP_HABENULA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_struct;
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
struct nimcp_fep_engine;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default negative PE threshold */
#define HAB_FEP_NEGATIVE_PE_THRESHOLD   0.1f

/** @brief Default aversive precision */
#define HAB_FEP_AVERSIVE_PRECISION      1.5f

/** @brief Bio-async module ID */
#define BIO_MODULE_HAB_FEP_BRIDGE       0x0F10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Negative PE processing mode
 */
typedef enum {
    HAB_FEP_NPE_STANDARD = 0,        /**< Standard negative PE */
    HAB_FEP_NPE_AMPLIFIED,           /**< Amplified (depression-like) */
    HAB_FEP_NPE_ATTENUATED,          /**< Attenuated (resilience) */
    HAB_FEP_NPE_ADAPTIVE             /**< Context-adaptive */
} nimcp_hab_fep_npe_mode_t;

/**
 * @brief Disappointment level
 */
typedef enum {
    HAB_FEP_DISAPPOINT_NONE = 0,     /**< No disappointment */
    HAB_FEP_DISAPPOINT_MILD,         /**< Mild disappointment */
    HAB_FEP_DISAPPOINT_MODERATE,     /**< Moderate disappointment */
    HAB_FEP_DISAPPOINT_SEVERE        /**< Severe disappointment */
} nimcp_hab_fep_disappoint_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-FEP bridge configuration
 */
typedef struct {
    /* Negative PE processing */
    nimcp_hab_fep_npe_mode_t npe_mode;
    float npe_threshold;             /**< Threshold for NPE signaling */
    float npe_gain;                  /**< NPE magnitude gain */

    /* Aversive precision */
    float aversive_precision;        /**< Precision on aversive outcomes */
    float punishment_learning_rate;  /**< Punishment learning rate */

    /* Disappointment */
    float disappointment_threshold;  /**< Threshold for disappointment */
    float disappointment_decay_tau;  /**< Disappointment decay */

    /* VTA/Raphe inhibition */
    float vta_inhibition_gain;       /**< Effect on VTA inhibition */
    float raphe_inhibition_gain;     /**< Effect on Raphe inhibition */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_hab_fep_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Aversive precision output
 */
typedef struct {
    float aversive_precision;        /**< Precision on aversive outcomes */
    float punishment_learning;       /**< Punishment learning rate */
    float disappointment_signal;     /**< Current disappointment */
    float vta_inhibition;            /**< VTA inhibition signal */
    float raphe_inhibition;          /**< Raphe inhibition signal */
} nimcp_hab_fep_precision_t;

/**
 * @brief FEP signal to Habenula
 */
typedef struct {
    float negative_pe;               /**< Negative prediction error */
    float expected_value;            /**< Expected value */
    float actual_value;              /**< Actual outcome */
    float aversive_surprise;         /**< Aversive surprise */
    bool worse_than_expected;        /**< Outcome was worse */
} nimcp_hab_fep_signal_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_hab_fep_npe_mode_t current_mode;
    nimcp_hab_fep_disappoint_t disappointment;
    float current_npe;
    float accumulated_disappointment;
    float vta_inhibition_level;
    float raphe_inhibition_level;
} nimcp_hab_fep_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t npe_events;
    uint64_t severe_disappointments;
    float avg_npe_magnitude;
    float avg_disappointment;
    float total_vta_inhibition;
    float total_raphe_inhibition;
} nimcp_hab_fep_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_hab_fep_bridge nimcp_hab_fep_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_hab_fep_config_t nimcp_hab_fep_config_default(void);

nimcp_hab_fep_bridge_t* nimcp_hab_fep_create(
    const nimcp_hab_fep_config_t* config
);

void nimcp_hab_fep_destroy(nimcp_hab_fep_bridge_t* bridge);

int nimcp_hab_fep_reset(nimcp_hab_fep_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_hab_fep_connect_habenula(
    nimcp_hab_fep_bridge_t* bridge,
    nimcp_habenula_adapter_t hab_adapter
);

int nimcp_hab_fep_connect_fep(
    nimcp_hab_fep_bridge_t* bridge,
    struct nimcp_fep_engine* fep
);

/*=============================================================================
 * Habenula --> FEP API
 *===========================================================================*/

/**
 * @brief Compute aversive precision from habenula state
 */
int nimcp_hab_fep_compute_precision(
    nimcp_hab_fep_bridge_t* bridge,
    nimcp_hab_fep_precision_t* precision
);

/**
 * @brief Get VTA inhibition signal
 */
float nimcp_hab_fep_get_vta_inhibition(nimcp_hab_fep_bridge_t* bridge);

/**
 * @brief Get Raphe inhibition signal
 */
float nimcp_hab_fep_get_raphe_inhibition(nimcp_hab_fep_bridge_t* bridge);

/**
 * @brief Get punishment learning rate
 */
float nimcp_hab_fep_get_punishment_rate(nimcp_hab_fep_bridge_t* bridge);

/*=============================================================================
 * FEP --> Habenula API
 *===========================================================================*/

/**
 * @brief Receive FEP signal
 */
int nimcp_hab_fep_receive_signal(
    nimcp_hab_fep_bridge_t* bridge,
    const nimcp_hab_fep_signal_t* signal
);

/**
 * @brief Process negative prediction error
 */
int nimcp_hab_fep_process_npe(
    nimcp_hab_fep_bridge_t* bridge,
    float npe_magnitude
);

/**
 * @brief Get habenula response to FEP state
 */
float nimcp_hab_fep_get_hab_response(nimcp_hab_fep_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_hab_fep_update(nimcp_hab_fep_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_hab_fep_get_state(
    const nimcp_hab_fep_bridge_t* bridge,
    nimcp_hab_fep_bridge_state_t* state
);

int nimcp_hab_fep_get_stats(
    const nimcp_hab_fep_bridge_t* bridge,
    nimcp_hab_fep_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_FEP_BRIDGE_H */
