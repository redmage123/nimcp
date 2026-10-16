/**
 * @file nimcp_norepinephrine_release.h
 * @brief Norepinephrine Release Dynamics for Locus Coeruleus
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Detailed NE release, reuptake, and degradation kinetics
 * WHY:  NE dynamics determine temporal profile of arousal modulation
 * HOW:  Multi-compartment model with release, diffusion, uptake, and degradation
 *
 * BIOLOGICAL BASIS:
 * - NE released from varicosities (volume transmission)
 * - Norepinephrine transporter (NET) mediates reuptake
 * - MAO and COMT enzymes degrade NE
 * - Alpha-2 autoreceptors provide negative feedback
 * - Different receptors (alpha-1, alpha-2, beta) have different affinities
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NOREPINEPHRINE_RELEASE_H
#define NIMCP_NOREPINEPHRINE_RELEASE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** NE vesicle pool size (number of vesicles) */
#define NE_VESICLE_POOL_SIZE            1000

/** NE per vesicle (molecules) */
#define NE_PER_VESICLE                  5000.0f

/** NET Km (nM) - transporter affinity */
#define NE_NET_KM                       250.0f

/** NET Vmax (nM/ms) - maximum uptake rate */
#define NE_NET_VMAX                     0.5f

/** MAO degradation rate (1/ms) */
#define NE_MAO_RATE                     0.001f

/** Alpha-2 autoreceptor EC50 (nM) */
#define NE_ALPHA2_EC50                  100.0f

/** Diffusion coefficient (um^2/ms) */
#define NE_DIFFUSION_COEFF              0.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief NE receptor types
 */
typedef enum {
    NE_RECEPTOR_ALPHA1 = 0,             /**< Alpha-1 (excitatory, postsynaptic) */
    NE_RECEPTOR_ALPHA2,                 /**< Alpha-2 (inhibitory, pre/postsynaptic) */
    NE_RECEPTOR_BETA1,                  /**< Beta-1 (excitatory) */
    NE_RECEPTOR_BETA2,                  /**< Beta-2 (excitatory) */
    NE_RECEPTOR_COUNT
} nimcp_ne_receptor_t;

/**
 * @brief Compartment types for NE dynamics
 */
typedef enum {
    NE_COMPARTMENT_VESICLE = 0,         /**< Vesicular storage */
    NE_COMPARTMENT_CYTOSOL,             /**< Intracellular */
    NE_COMPARTMENT_SYNAPTIC,            /**< Synaptic cleft */
    NE_COMPARTMENT_EXTRASYNAPTIC,       /**< Volume transmission */
    NE_COMPARTMENT_COUNT
} nimcp_ne_compartment_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief NE vesicle pool
 */
typedef struct {
    uint32_t total_vesicles;            /**< Total vesicles */
    uint32_t ready_pool;                /**< Ready for release */
    uint32_t reserve_pool;              /**< Reserve pool */
    uint32_t depleted;                  /**< Empty vesicles */
    float refill_rate;                  /**< Vesicle refill rate (1/ms) */
    float release_probability;          /**< Per-spike release prob */
} nimcp_ne_vesicle_pool_t;

/**
 * @brief NE receptor state
 */
typedef struct {
    nimcp_ne_receptor_t type;           /**< Receptor type */
    float density;                      /**< Receptor density (normalized) */
    float affinity_nm;                  /**< Binding affinity Kd (nM) */
    float occupancy;                    /**< Current occupancy (0-1) */
    float response;                     /**< Downstream response (0-1) */
    float desensitization;              /**< Receptor desensitization */
} nimcp_ne_receptor_state_t;

/**
 * @brief NE transporter state
 */
typedef struct {
    float km;                           /**< Michaelis constant (nM) */
    float vmax;                         /**< Maximum velocity (nM/ms) */
    float current_rate;                 /**< Current uptake rate */
    float inhibition;                   /**< Transporter inhibition (0-1) */
    bool enabled;
} nimcp_ne_transporter_t;

/**
 * @brief NE compartment concentrations
 */
typedef struct {
    float vesicular;                    /**< Vesicular [NE] */
    float cytosolic;                    /**< Cytosolic [NE] */
    float synaptic;                     /**< Synaptic cleft [NE] */
    float extrasynaptic;                /**< Extrasynaptic [NE] */
} nimcp_ne_concentrations_t;

/**
 * @brief Complete NE release system
 */
typedef struct {
    /* Concentrations */
    nimcp_ne_concentrations_t concentrations;

    /* Vesicle pool */
    nimcp_ne_vesicle_pool_t vesicles;

    /* Receptors */
    nimcp_ne_receptor_state_t receptors[NE_RECEPTOR_COUNT];

    /* Transporter */
    nimcp_ne_transporter_t transporter;

    /* Degradation */
    float mao_activity;                 /**< MAO enzyme activity */
    float comt_activity;                /**< COMT enzyme activity */

    /* Autoreceptor feedback */
    float autoreceptor_activation;      /**< Alpha-2 activation (0-1) */
    float release_inhibition;           /**< Inhibition from autoreceptors */

    /* Metabolism */
    float synthesis_rate;               /**< NE synthesis rate (nM/ms) */
    float degradation_rate;             /**< Current degradation rate */
    float total_released;               /**< Cumulative release */
    float total_cleared;                /**< Cumulative clearance */

    /* State */
    bool initialized;
    float current_time;
} nimcp_ne_release_system_t;

/**
 * @brief NE release configuration
 */
typedef struct {
    /* Vesicle parameters */
    uint32_t initial_vesicles;
    float ready_pool_fraction;
    float refill_rate;

    /* Release parameters */
    float release_probability;
    float ne_per_vesicle;

    /* Clearance parameters */
    float net_km;
    float net_vmax;
    float mao_rate;
    float comt_rate;

    /* Receptor parameters */
    float alpha1_density;
    float alpha2_density;
    float beta1_density;
    float beta2_density;

    /* Autoreceptor parameters */
    float autoreceptor_gain;
    float autoreceptor_ec50;

    /* Synthesis */
    float basal_synthesis_rate;
} nimcp_ne_release_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default NE release configuration
 */
NIMCP_EXPORT nimcp_ne_release_config_t nimcp_ne_release_default_config(void);

/**
 * @brief Initialize NE release system
 */
NIMCP_EXPORT int nimcp_ne_release_init(
    nimcp_ne_release_system_t* system,
    const nimcp_ne_release_config_t* config
);

/**
 * @brief Shutdown NE release system
 */
NIMCP_EXPORT int nimcp_ne_release_shutdown(nimcp_ne_release_system_t* system);

/**
 * @brief Reset to baseline
 */
NIMCP_EXPORT int nimcp_ne_release_reset(nimcp_ne_release_system_t* system);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Update NE dynamics
 * @param system NE release system
 * @param dt Time delta (ms)
 * @param firing_rate Current firing rate (Hz)
 */
NIMCP_EXPORT int nimcp_ne_release_update(
    nimcp_ne_release_system_t* system,
    float dt,
    float firing_rate
);

/**
 * @brief Trigger release event (spike-triggered)
 * @param system NE release system
 * @param num_spikes Number of spikes
 */
NIMCP_EXPORT int nimcp_ne_release_trigger(
    nimcp_ne_release_system_t* system,
    uint32_t num_spikes
);

/**
 * @brief Get receptor activation
 * @param system NE release system
 * @param receptor Receptor type
 * @param[out] activation Activation level (0-1)
 */
NIMCP_EXPORT int nimcp_ne_get_receptor_activation(
    const nimcp_ne_release_system_t* system,
    nimcp_ne_receptor_t receptor,
    float* activation
);

/**
 * @brief Get NE concentration in compartment
 * @param system NE release system
 * @param compartment Compartment
 * @param[out] concentration NE concentration (nM)
 */
NIMCP_EXPORT int nimcp_ne_get_concentration(
    const nimcp_ne_release_system_t* system,
    nimcp_ne_compartment_t compartment,
    float* concentration
);

/**
 * @brief Apply transporter inhibition (e.g., from drug)
 * @param system NE release system
 * @param inhibition Inhibition level (0-1)
 */
NIMCP_EXPORT int nimcp_ne_apply_net_inhibition(
    nimcp_ne_release_system_t* system,
    float inhibition
);

/**
 * @brief Get autoreceptor feedback
 * @param system NE release system
 * @param[out] feedback Feedback level (0-1)
 */
NIMCP_EXPORT int nimcp_ne_get_autoreceptor_feedback(
    const nimcp_ne_release_system_t* system,
    float* feedback
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NOREPINEPHRINE_RELEASE_H */
