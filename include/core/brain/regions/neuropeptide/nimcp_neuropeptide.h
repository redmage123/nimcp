/**
 * @file nimcp_neuropeptide.h
 * @brief Neuropeptide System - Peptidergic Neuromodulation
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Models 8 key neuropeptides and their neuromodulatory effects
 * WHY:  Neuropeptides provide slow, sustained modulation of neural circuits,
 *        complementing fast neurotransmitter signaling (DA, 5-HT, NE, ACh)
 * HOW:  Kinetic model of synthesis, vesicular release, receptor binding,
 *        degradation, and downstream signaling for each peptide
 *
 * KEY CONCEPTS:
 * - Volume Transmission: Neuropeptides diffuse widely through extracellular space
 * - Slow Kinetics: Minutes-to-hours timescale vs ms for classical transmitters
 * - High Potency: Active at nanomolar concentrations (vs micromolar for GABA/glutamate)
 * - Receptor Occupancy: Sigmoid binding curve models receptor saturation
 *
 * BIOLOGICAL BASIS:
 * - Oxytocin: Social bonding, trust, empathy (paraventricular nucleus)
 * - Vasopressin: Stress response, social recognition, aggression
 * - NPY: Appetite, anxiety reduction, energy homeostasis
 * - Substance P: Pain signaling, neurogenic inflammation
 * - Orexin/Hypocretin: Wakefulness, feeding behavior, reward seeking
 * - CRH: Stress axis activation, anxiety, HPA axis regulation
 * - Endorphin: Pain relief, euphoria, reward (arcuate nucleus)
 * - CCK: Satiety signaling, anxiety modulation, memory
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROPEPTIDE_H
#define NIMCP_NEUROPEPTIDE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Magic number for validation: 0x4E455552 ('NEUR') */
#define NPT_MAGIC                   0x4E455552U

/** Maximum peptide concentration (nM equivalent) */
#define NPT_MAX_CONCENTRATION       100.0f

/** Default receptor Kd (sigmoid midpoint, nM) */
#define NPT_DEFAULT_KD              10.0f

/** Default downstream gain */
#define NPT_DEFAULT_GAIN            1.0f

/** Number of neuropeptide types */
#define NPT_TYPE_COUNT              8

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Neuropeptide types
 */
typedef enum {
    NPT_OXYTOCIN = 0,      /**< Social bonding, trust */
    NPT_VASOPRESSIN,        /**< Stress, social recognition */
    NPT_NPY,               /**< Appetite, anxiolytic */
    NPT_SUBSTANCE_P,       /**< Pain, inflammation */
    NPT_OREXIN,            /**< Wakefulness, feeding */
    NPT_CRH,               /**< Stress axis (HPA) */
    NPT_ENDORPHIN,         /**< Analgesia, euphoria */
    NPT_CCK,               /**< Satiety, anxiety */
    NPT_COUNT              /**< Sentinel */
} neuropeptide_type_t;

/**
 * @brief Neuropeptide system error codes
 */
typedef enum {
    NPT_OK = 0,
    NPT_ERR_NULL_PTR       = -1,
    NPT_ERR_INVALID_PARAM  = -2,
    NPT_ERR_NOT_INIT       = -3,
    NPT_ERR_ALREADY_INIT   = -4,
    NPT_ERR_NO_MEMORY      = -5,
    NPT_ERR_INVALID_TYPE   = -6,
    NPT_ERR_INVALID_STATE  = -7
} npt_error_t;

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct npt_config_s npt_config_t;
typedef struct neuropeptide_state_s neuropeptide_state_t;
typedef struct neuropeptide_system neuropeptide_system_t;

/* Mutex type from thread layer */
#include "utils/thread/nimcp_thread.h"

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Per-peptide kinetic state
 */
struct neuropeptide_state_s {
    float concentration;         /**< Current concentration 0.0-100.0 nM equivalent */
    float synthesis_rate;        /**< Synthesis rate (nM/s) */
    float degradation_rate;      /**< Degradation rate constant (1/s) */
    float release_threshold;     /**< Firing rate threshold for vesicle release */
    float receptor_occupancy;    /**< Fractional receptor occupancy 0.0-1.0 */
    float downstream_effect;     /**< Computed effect from occupancy * gain */
    float firing_rate;           /**< Current afferent firing rate (Hz) */
    float kd;                    /**< Receptor dissociation constant (nM) */
    float gain;                  /**< Downstream effect gain multiplier */
};

/**
 * @brief Neuropeptide system configuration
 */
struct npt_config_s {
    float base_synthesis_rates[NPT_COUNT];   /**< Baseline synthesis rates (nM/s) */
    float degradation_rates[NPT_COUNT];      /**< Degradation rate constants (1/s) */
    float release_thresholds[NPT_COUNT];     /**< Firing rate thresholds (Hz) */
    float kd_values[NPT_COUNT];              /**< Receptor Kd values (nM) */
    float gains[NPT_COUNT];                  /**< Downstream effect gains */
};

/**
 * @brief Main neuropeptide system
 */
struct neuropeptide_system {
    uint32_t magic;              /**< Validation magic (NPT_MAGIC) */
    npt_config_t config;
    neuropeptide_state_t peptides[NPT_COUNT];

    /* Derived behavioral drives */
    float social_drive;          /**< Oxytocin-mediated social motivation */
    float stress_level;          /**< CRH-mediated stress response */
    float wakefulness;           /**< Orexin-mediated arousal */
    float pain_level;            /**< Substance P mediated nociception */
    float satiety;               /**< CCK-mediated satiety */
    float euphoria;              /**< Endorphin-mediated reward */

    /* Timing */
    uint64_t last_update_us;     /**< Last update timestamp (microseconds) */
    uint64_t update_count;       /**< Total update calls */

    /* Thread safety */
    nimcp_mutex_t* lock;

    /* State */
    bool initialized;
};

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Get default neuropeptide configuration
 *
 * Returns biologically-motivated defaults for all 8 peptides:
 * - Synthesis rates: 0.1-1.0 nM/s (slow peptide production)
 * - Degradation: 0.01-0.1 /s (enzymatic breakdown)
 * - Thresholds: 5-20 Hz (firing rate for vesicular release)
 *
 * @return Default configuration
 */
NIMCP_EXPORT npt_config_t npt_default_config(void);

/**
 * @brief Create and initialize neuropeptide system
 * @param config Configuration (NULL for defaults)
 * @return Allocated system or NULL on failure
 */
NIMCP_EXPORT neuropeptide_system_t* npt_create(const npt_config_t* config);

/**
 * @brief Destroy neuropeptide system and free resources
 * @param system System to destroy (NULL-safe)
 */
NIMCP_EXPORT void npt_destroy(neuropeptide_system_t* system);

/*=============================================================================
 * Core Simulation API
 *===========================================================================*/

/**
 * @brief Update neuropeptide dynamics for one timestep
 *
 * For each peptide:
 * 1. If firing_rate >= release_threshold: concentration += synthesis_rate * dt
 * 2. Enzymatic degradation: concentration -= degradation_rate * concentration * dt
 * 3. Clamp concentration to [0, NPT_MAX_CONCENTRATION]
 * 4. Compute receptor occupancy: occupancy = conc / (conc + Kd)
 * 5. Compute downstream effect: effect = occupancy * gain
 *
 * Also updates derived behavioral drives (social_drive, stress_level, etc.)
 *
 * @param system Neuropeptide system
 * @param dt_s Time step in seconds
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_update(neuropeptide_system_t* system, float dt_s);

/**
 * @brief Get current concentration of a peptide
 * @param system Neuropeptide system
 * @param type Peptide type
 * @param[out] concentration Output concentration (nM)
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_get_concentration(
    const neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float* concentration
);

/**
 * @brief Get downstream effect of a peptide
 * @param system Neuropeptide system
 * @param type Peptide type
 * @param[out] effect Output effect value
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_get_downstream_effect(
    const neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float* effect
);

/**
 * @brief Stimulate release of a specific peptide (bolus addition)
 * @param system Neuropeptide system
 * @param type Peptide type
 * @param stimulus Amount of peptide to add (nM)
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_stimulate_release(
    neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float stimulus
);

/**
 * @brief Set afferent firing rate for a peptide's releasing neurons
 * @param system Neuropeptide system
 * @param type Peptide type
 * @param rate Firing rate (Hz)
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_set_firing_rate(
    neuropeptide_system_t* system,
    neuropeptide_type_t type,
    float rate
);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get all peptide states (snapshot)
 * @param system Neuropeptide system
 * @param[out] states Array of NPT_COUNT states to fill
 * @return NPT_OK on success
 */
NIMCP_EXPORT npt_error_t npt_get_all_states(
    const neuropeptide_system_t* system,
    neuropeptide_state_t states[NPT_COUNT]
);

/**
 * @brief Get name string for a peptide type
 * @param type Peptide type
 * @return Human-readable name or "UNKNOWN"
 */
NIMCP_EXPORT const char* npt_type_name(neuropeptide_type_t type);

/**
 * @brief Get error string
 * @param error Error code
 * @return Human-readable error description
 */
NIMCP_EXPORT const char* npt_error_string(npt_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROPEPTIDE_H */
