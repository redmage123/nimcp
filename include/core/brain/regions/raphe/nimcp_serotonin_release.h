/**
 * @file nimcp_serotonin_release.h
 * @brief 5-HT (Serotonin) release dynamics for Raphe Nuclei
 * @date 2026-01-11
 *
 * Models the complex dynamics of serotonin:
 * - Synthesis from tryptophan via TPH2
 * - Vesicular packaging via VMAT2
 * - Release via action potentials
 * - Reuptake via SERT
 * - Metabolism via MAO
 * - Autoreceptor feedback (5-HT1A/1B)
 *
 * Key 5-HT characteristics:
 * - Slower dynamics than DA/NE (modulatory)
 * - Volume transmission (extrasynaptic signaling)
 * - Multiple receptor subtypes with different effects
 */

#ifndef NIMCP_SEROTONIN_RELEASE_H
#define NIMCP_SEROTONIN_RELEASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define HT_DEFAULT_BASELINE       20.0f    /* nM - baseline 5-HT */
#define HT_DEFAULT_RELEASE_PROB   0.3f     /* Release probability per spike */
#define HT_DEFAULT_SERT_VMAX      5.0f     /* SERT max velocity (nM/ms) */
#define HT_DEFAULT_SERT_KM        80.0f    /* SERT Km (nM) */
#define HT_DEFAULT_MAO_RATE       0.001f   /* MAO degradation rate */
#define HT_AUTORECEPTOR_KD        5.0f     /* 5-HT1A Kd (nM) */
#define HT_MAX_CONCENTRATION      200.0f   /* Max safe 5-HT level */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief 5-HT compartments
 */
typedef enum {
    HT_COMPARTMENT_VESICULAR = 0,   /**< Stored in vesicles */
    HT_COMPARTMENT_CYTOSOLIC,       /**< Cytoplasmic pool */
    HT_COMPARTMENT_SYNAPTIC,        /**< Synaptic cleft */
    HT_COMPARTMENT_EXTRASYNAPTIC,   /**< Volume transmission */
    HT_COMPARTMENT_COUNT
} nimcp_ht_compartment_t;

/**
 * @brief 5-HT receptor types
 */
typedef enum {
    HT_RECEPTOR_TYPE_1A = 0,        /**< 5-HT1A (Gi, anxiolytic) */
    HT_RECEPTOR_TYPE_1B,            /**< 5-HT1B (Gi, autoreceptor) */
    HT_RECEPTOR_TYPE_2A,            /**< 5-HT2A (Gq, mood/hallucinations) */
    HT_RECEPTOR_TYPE_2C,            /**< 5-HT2C (Gq, appetite/anxiety) */
    HT_RECEPTOR_TYPE_COUNT
} nimcp_ht_receptor_type_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief 5-HT concentrations by compartment
 */
typedef struct {
    float vesicular;
    float cytosolic;
    float synaptic;
    float extrasynaptic;
} nimcp_ht_concentrations_t;

/**
 * @brief SERT (serotonin transporter) state
 */
typedef struct {
    float vmax;                      /**< Max reuptake velocity */
    float km;                        /**< Michaelis constant */
    float inhibition;                /**< SSRI inhibition [0-1] */
    float activity;                  /**< Current activity level */
} nimcp_sert_state_t;

/**
 * @brief Autoreceptor (5-HT1A/1B) state
 */
typedef struct {
    float activation_1a;             /**< 5-HT1A activation [0-1] */
    float activation_1b;             /**< 5-HT1B activation [0-1] */
    float feedback_strength;         /**< Inhibitory feedback [0-1] */
    float desensitization;           /**< Receptor desensitization [0-1] */
} nimcp_ht_autoreceptor_t;

/**
 * @brief 5-HT vesicle pool
 */
typedef struct {
    uint32_t readily_releasable;     /**< Ready for release */
    uint32_t reserve;                /**< Reserve pool */
    uint32_t recycling;              /**< Being recycled */
    float mobilization_rate;         /**< Reserve -> RRP rate */
} nimcp_ht_vesicle_pool_t;

/**
 * @brief 5-HT release configuration
 */
typedef struct {
    float release_probability;       /**< Per-spike release prob */
    float sert_vmax;                 /**< SERT max velocity */
    float sert_km;                   /**< SERT affinity */
    float mao_rate;                  /**< MAO degradation rate */
    float autoreceptor_gain;         /**< Autoreceptor feedback gain */
    float synthesis_rate;            /**< Tryptophan -> 5-HT rate */
    bool enable_volume_transmission; /**< Extrasynaptic release */
} nimcp_ht_release_config_t;

/**
 * @brief 5-HT release system state
 */
typedef struct {
    bool initialized;

    /* Concentrations */
    nimcp_ht_concentrations_t concentrations;

    /* Transporter */
    nimcp_sert_state_t transporter;

    /* Autoreceptors */
    nimcp_ht_autoreceptor_t autoreceptor;

    /* Vesicle pools */
    nimcp_ht_vesicle_pool_t vesicles;

    /* Receptor activations */
    float receptor_activation[HT_RECEPTOR_TYPE_COUNT];

    /* Dynamics */
    float release_efficacy;          /**< Current release efficiency */
    float synthesis_rate;            /**< Current synthesis rate */

    /* Configuration */
    nimcp_ht_release_config_t config;

} nimcp_ht_release_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

int nimcp_ht_release_init(nimcp_ht_release_system_t* system, const nimcp_ht_release_config_t* config);
int nimcp_ht_release_shutdown(nimcp_ht_release_system_t* system);
int nimcp_ht_release_reset(nimcp_ht_release_system_t* system);
nimcp_ht_release_config_t nimcp_ht_release_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update 5-HT dynamics
 * @param system Release system
 * @param firing_rate Current firing rate (Hz)
 * @param dt Time step (ms)
 */
int nimcp_ht_release_update(nimcp_ht_release_system_t* system, float firing_rate, float dt);

/*=============================================================================
 * Concentration API
 *===========================================================================*/

int nimcp_ht_get_concentration(
    nimcp_ht_release_system_t* system,
    nimcp_ht_compartment_t compartment,
    float* concentration
);

int nimcp_ht_set_concentration(
    nimcp_ht_release_system_t* system,
    nimcp_ht_compartment_t compartment,
    float concentration
);

int nimcp_ht_get_total_extracellular(
    nimcp_ht_release_system_t* system,
    float* total
);

/*=============================================================================
 * Receptor API
 *===========================================================================*/

int nimcp_ht_get_receptor_activation(
    nimcp_ht_release_system_t* system,
    nimcp_ht_receptor_type_t receptor,
    float* activation
);

int nimcp_ht_get_1a_2a_balance(
    nimcp_ht_release_system_t* system,
    float* balance
);

/*=============================================================================
 * Transporter API
 *===========================================================================*/

int nimcp_ht_set_sert_inhibition(nimcp_ht_release_system_t* system, float inhibition);
int nimcp_ht_get_uptake_rate(nimcp_ht_release_system_t* system, float* rate);

/*=============================================================================
 * Autoreceptor API
 *===========================================================================*/

int nimcp_ht_get_autoreceptor_feedback(nimcp_ht_release_system_t* system, float* feedback);

/*=============================================================================
 * Vesicle API
 *===========================================================================*/

int nimcp_ht_get_vesicle_pool(nimcp_ht_release_system_t* system, nimcp_ht_vesicle_pool_t* pool);
int nimcp_ht_get_release_efficacy(nimcp_ht_release_system_t* system, float* efficacy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEROTONIN_RELEASE_H */
