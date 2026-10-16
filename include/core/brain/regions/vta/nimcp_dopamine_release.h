/**
 * @file nimcp_dopamine_release.h
 * @brief Dopamine release dynamics for VTA
 * @date 2026-01-11
 *
 * Models DA release, reuptake (DAT), and degradation (MAO, COMT).
 * Implements vesicular release, tonic/phasic dynamics, and receptor kinetics.
 */

#ifndef NIMCP_DOPAMINE_RELEASE_H
#define NIMCP_DOPAMINE_RELEASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define DA_VESICLE_POOL_SIZE       10000    /* Number of DA vesicles */
#define DA_QUANTA_PER_VESICLE      5000     /* DA molecules per vesicle */
#define DA_RELEASE_PROBABILITY     0.3f     /* P(release|spike) */
#define DA_REUPTAKE_KM             0.15f    /* DAT Km (uM) */
#define DA_REUPTAKE_VMAX           4.0f     /* DAT Vmax (uM/s) */
#define DA_MAO_RATE                0.01f    /* MAO degradation (1/s) */
#define DA_COMT_RATE               0.005f   /* COMT degradation (1/s) */
#define DA_DIFFUSION_COEFF         0.2f     /* Diffusion coefficient */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief DA compartments
 */
typedef enum {
    DA_COMPARTMENT_VESICULAR = 0, /**< In vesicles */
    DA_COMPARTMENT_CYTOSOLIC,     /**< In cytoplasm */
    DA_COMPARTMENT_SYNAPTIC,      /**< In synaptic cleft */
    DA_COMPARTMENT_EXTRASYNAPTIC, /**< Volume transmission */
    DA_COMPARTMENT_COUNT
} nimcp_da_compartment_t;

/**
 * @brief DA receptor types (also in main header, repeated for completeness)
 */
typedef enum {
    DA_RECEPTOR_TYPE_D1 = 0,      /**< D1-like (excitatory) */
    DA_RECEPTOR_TYPE_D2,          /**< D2-like (inhibitory) */
    DA_RECEPTOR_TYPE_D3,          /**< D3 */
    DA_RECEPTOR_TYPE_D4,          /**< D4 */
    DA_RECEPTOR_TYPE_D5,          /**< D5 */
    DA_RECEPTOR_TYPE_COUNT
} nimcp_da_receptor_type_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief DA vesicle pool state
 */
typedef struct {
    uint32_t readily_releasable;  /**< Ready for immediate release */
    uint32_t recycling;           /**< Being recycled */
    uint32_t reserve;             /**< Reserve pool */
    float refill_rate;            /**< Refill rate (vesicles/ms) */
    float recycling_time;         /**< Average recycling time (ms) */
} nimcp_da_vesicle_pool_t;

/**
 * @brief DA concentration by compartment
 */
typedef struct {
    float vesicular;              /**< nM in vesicles */
    float cytosolic;              /**< nM in cytoplasm */
    float synaptic;               /**< nM in cleft */
    float extrasynaptic;          /**< nM volume */
} nimcp_da_concentrations_t;

/**
 * @brief DA receptor state
 */
typedef struct {
    float density;                /**< Receptors per um^2 */
    float activation;             /**< Fraction bound [0-1] */
    float kd;                     /**< Dissociation constant (nM) */
    float hill_coeff;             /**< Hill coefficient */
    float desensitization;        /**< Desensitization level [0-1] */
    float internalized;           /**< Fraction internalized [0-1] */
} nimcp_da_receptor_state_t;

/**
 * @brief DA transporter (DAT) state
 */
typedef struct {
    float vmax;                   /**< Max uptake rate */
    float km;                     /**< Michaelis constant */
    float inhibition;             /**< Drug-induced inhibition [0-1] */
    float expression;             /**< Surface expression [0-1] */
    bool enabled;
} nimcp_da_transporter_t;

/**
 * @brief DA release system configuration
 */
typedef struct {
    float release_probability;    /**< P(release|spike) */
    float quanta_per_vesicle;     /**< DA per vesicle */
    float dat_vmax;               /**< DAT Vmax */
    float dat_km;                 /**< DAT Km */
    float mao_rate;               /**< MAO degradation rate */
    float comt_rate;              /**< COMT degradation rate */
    bool enable_autoreceptors;    /**< D2 autoreceptor feedback */
    float autoreceptor_ic50;      /**< IC50 for autoreceptor */
} nimcp_da_release_config_t;

/**
 * @brief DA release system
 */
typedef struct {
    bool initialized;

    /* Vesicle pool */
    nimcp_da_vesicle_pool_t vesicles;

    /* Concentrations */
    nimcp_da_concentrations_t concentrations;

    /* Receptors */
    nimcp_da_receptor_state_t receptors[DA_RECEPTOR_TYPE_COUNT];

    /* Transporter */
    nimcp_da_transporter_t transporter;

    /* Configuration */
    nimcp_da_release_config_t config;

    /* State */
    float autoreceptor_feedback;  /**< D2 autoreceptor inhibition */
    float release_efficacy;       /**< Current release efficiency */
    float total_released;         /**< Total DA released (nM) */

    /* Metrics */
    uint32_t release_events;
    uint32_t spikes_processed;
} nimcp_da_release_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Initialize DA release system
 */
int nimcp_da_release_init(
    nimcp_da_release_system_t* system,
    const nimcp_da_release_config_t* config
);

/**
 * @brief Shutdown DA release system
 */
int nimcp_da_release_shutdown(nimcp_da_release_system_t* system);

/**
 * @brief Reset to initial state
 */
int nimcp_da_release_reset(nimcp_da_release_system_t* system);

/**
 * @brief Get default configuration
 */
nimcp_da_release_config_t nimcp_da_release_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update DA release dynamics
 * @param system DA release system
 * @param dt Time step (ms)
 * @param firing_rate Current firing rate (Hz)
 */
int nimcp_da_release_update(
    nimcp_da_release_system_t* system,
    float dt,
    float firing_rate
);

/**
 * @brief Trigger DA release for given spikes
 */
int nimcp_da_release_trigger(
    nimcp_da_release_system_t* system,
    uint32_t spikes
);

/**
 * @brief Trigger burst release (multiple spikes, enhanced release)
 */
int nimcp_da_release_burst(
    nimcp_da_release_system_t* system,
    uint32_t spikes,
    float enhancement
);

/*=============================================================================
 * Concentration API
 *===========================================================================*/

/**
 * @brief Get DA concentration in compartment
 */
int nimcp_da_get_concentration(
    nimcp_da_release_system_t* system,
    nimcp_da_compartment_t compartment,
    float* concentration
);

/**
 * @brief Set DA concentration (for testing/manipulation)
 */
int nimcp_da_set_concentration(
    nimcp_da_release_system_t* system,
    nimcp_da_compartment_t compartment,
    float concentration
);

/**
 * @brief Get total synaptic + extrasynaptic DA
 */
int nimcp_da_get_total_extracellular(
    nimcp_da_release_system_t* system,
    float* total
);

/*=============================================================================
 * Receptor API
 *===========================================================================*/

/**
 * @brief Get receptor activation level
 */
int nimcp_da_get_receptor_activation(
    nimcp_da_release_system_t* system,
    nimcp_da_receptor_type_t type,
    float* activation
);

/**
 * @brief Get D1/D2 balance
 */
int nimcp_da_get_d1_d2_balance(
    nimcp_da_release_system_t* system,
    float* balance
);

/**
 * @brief Apply receptor agonist/antagonist
 */
int nimcp_da_apply_drug(
    nimcp_da_release_system_t* system,
    nimcp_da_receptor_type_t receptor,
    float efficacy,
    float affinity
);

/*=============================================================================
 * Transporter API
 *===========================================================================*/

/**
 * @brief Set DAT inhibition level (e.g., cocaine effect)
 */
int nimcp_da_set_dat_inhibition(
    nimcp_da_release_system_t* system,
    float inhibition
);

/**
 * @brief Get current uptake rate
 */
int nimcp_da_get_uptake_rate(
    nimcp_da_release_system_t* system,
    float* rate
);

/*=============================================================================
 * Autoreceptor API
 *===========================================================================*/

/**
 * @brief Get autoreceptor feedback level
 */
int nimcp_da_get_autoreceptor_feedback(
    nimcp_da_release_system_t* system,
    float* feedback
);

/*=============================================================================
 * Vesicle API
 *===========================================================================*/

/**
 * @brief Get vesicle pool status
 */
int nimcp_da_get_vesicle_pool(
    nimcp_da_release_system_t* system,
    nimcp_da_vesicle_pool_t* pool
);

/**
 * @brief Get release efficacy (affected by vesicle depletion)
 */
int nimcp_da_get_release_efficacy(
    nimcp_da_release_system_t* system,
    float* efficacy
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DOPAMINE_RELEASE_H */
