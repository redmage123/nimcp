/**
 * @file nimcp_epigenetics.h
 * @brief Epigenetics Module - Long-term neural modifications
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Epigenetic modifications for long-term neural plasticity
 * WHY:  Enable experience-dependent, persistent changes to neural behavior
 * HOW:  Model methylation, histone modifications, and plasticity windows
 *
 * KEY CONCEPTS:
 * - Methylation: Persistent silencing/activation of synaptic pathways
 * - Histone Modifications: Control plasticity windows and learning rates
 * - Chromatin State: Neural architecture flexibility (open/closed regions)
 * - Environmental Imprinting: Experience-based neural modifications
 * - Epigenetic Memory: Long-term storage without weight changes
 *
 * BIOLOGICAL BASIS:
 * - DNA methylation affects gene transcription
 * - Histone acetylation opens chromatin for transcription
 * - These changes can persist across cell divisions
 * - In neural context: affects synapse strength and plasticity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EPIGENETICS_H
#define NIMCP_EPIGENETICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum methylation sites per neuron */
#define EPIGENETICS_MAX_SITES               256

/** Maximum histone modifications per region */
#define EPIGENETICS_MAX_HISTONES            64

/** Maximum chromatin regions */
#define EPIGENETICS_MAX_REGIONS             128

/** Maximum imprinting events */
#define EPIGENETICS_MAX_IMPRINTS            64

/** Methylation stability (1.0 = permanent) */
#define EPIGENETICS_DEFAULT_STABILITY       0.95f

/** Plasticity window default duration (ms) */
#define EPIGENETICS_WINDOW_DURATION_MS      1000.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    EPIGENETICS_OK = 0,
    EPIGENETICS_ERR_NULL_PTR = -1,
    EPIGENETICS_ERR_INVALID_PARAM = -2,
    EPIGENETICS_ERR_NOT_INITIALIZED = -3,
    EPIGENETICS_ERR_ALREADY_INITIALIZED = -4,
    EPIGENETICS_ERR_NO_MEMORY = -5,
    EPIGENETICS_ERR_SITE_NOT_FOUND = -6,
    EPIGENETICS_ERR_REGION_FULL = -7,
    EPIGENETICS_ERR_WINDOW_CLOSED = -8
} nimcp_epigenetics_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Methylation state
 */
typedef enum {
    METHYL_STATE_UNMETHYLATED = 0,      /**< No methylation - full activity */
    METHYL_STATE_HEMIMETHYLATED,         /**< Partial methylation - reduced activity */
    METHYL_STATE_METHYLATED,             /**< Full methylation - silenced */
    METHYL_STATE_HYPERMETHYLATED         /**< Excessive methylation - strongly silenced */
} nimcp_methylation_state_t;

/**
 * @brief Histone modification type
 */
typedef enum {
    HISTONE_MOD_ACETYLATION = 0,        /**< Increases plasticity */
    HISTONE_MOD_DEACETYLATION,          /**< Decreases plasticity */
    HISTONE_MOD_METHYLATION,            /**< Can activate or repress */
    HISTONE_MOD_PHOSPHORYLATION,        /**< Activity-dependent */
    HISTONE_MOD_UBIQUITINATION          /**< Marks for degradation */
} nimcp_histone_mod_type_t;

/**
 * @brief Chromatin state
 */
typedef enum {
    CHROMATIN_STATE_OPEN = 0,           /**< High plasticity, accessible */
    CHROMATIN_STATE_POISED,             /**< Ready for activation */
    CHROMATIN_STATE_CLOSED,             /**< Low plasticity, silenced */
    CHROMATIN_STATE_HETEROCHROMATIN     /**< Permanently silenced */
} nimcp_chromatin_state_t;

/**
 * @brief Imprint type
 */
typedef enum {
    IMPRINT_TYPE_POSITIVE = 0,          /**< Reinforces pathway */
    IMPRINT_TYPE_NEGATIVE,              /**< Suppresses pathway */
    IMPRINT_TYPE_CRITICAL_PERIOD,       /**< Time-limited plasticity */
    IMPRINT_TYPE_ENVIRONMENTAL          /**< External factor driven */
} nimcp_imprint_type_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_brain_struct* nimcp_brain_t;
typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;

//=============================================================================
// Opaque Handles
//=============================================================================

typedef struct nimcp_epigenetics_struct* nimcp_epigenetics_t;
typedef struct nimcp_methylation_struct* nimcp_methylation_t;
typedef struct nimcp_histone_struct* nimcp_histone_t;
typedef struct nimcp_chromatin_struct* nimcp_chromatin_t;
typedef struct nimcp_imprinting_struct* nimcp_imprinting_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Epigenetics system configuration
 */
typedef struct {
    uint32_t max_neurons;               /**< Maximum neurons to track */
    uint32_t max_sites_per_neuron;      /**< Methylation sites per neuron */
    float methylation_stability;        /**< How stable methylation is (0-1) */
    float histone_decay_rate;           /**< Histone modification decay */
    float plasticity_window_ms;         /**< Default plasticity window */
    bool enable_environmental;          /**< Enable environmental factors */
    bool enable_inheritance;            /**< Enable epigenetic inheritance */
    bool enable_logging;
    bool enable_metrics;
} nimcp_epigenetics_config_t;

/**
 * @brief Methylation site configuration
 */
typedef struct {
    uint32_t site_id;                   /**< Unique site identifier */
    uint32_t neuron_id;                 /**< Associated neuron */
    uint32_t synapse_id;                /**< Associated synapse (optional) */
    float initial_level;                /**< Initial methylation (0-1) */
    float stability;                    /**< Site-specific stability */
} nimcp_methylation_site_t;

/**
 * @brief Histone modification configuration
 */
typedef struct {
    nimcp_histone_mod_type_t type;      /**< Modification type */
    uint32_t region_id;                 /**< Target region */
    float magnitude;                    /**< Modification strength */
    float decay_rate;                   /**< How fast it decays */
    bool is_activating;                 /**< True if activates, false if represses */
} nimcp_histone_config_t;

/**
 * @brief Chromatin region configuration
 */
typedef struct {
    uint32_t region_id;                 /**< Region identifier */
    uint32_t start_neuron;              /**< First neuron in region */
    uint32_t end_neuron;                /**< Last neuron in region */
    nimcp_chromatin_state_t initial_state;
    float transition_threshold;         /**< Activity threshold for state change */
} nimcp_chromatin_config_t;

/**
 * @brief Imprinting event configuration
 */
typedef struct {
    nimcp_imprint_type_t type;          /**< Imprint type */
    uint32_t target_region;             /**< Target chromatin region */
    float strength;                     /**< Imprint strength */
    float duration_ms;                  /**< How long until fade (0 = permanent) */
    uint64_t trigger_time;              /**< When imprint occurred */
} nimcp_imprint_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Epigenetics system state
 */
typedef struct {
    uint32_t active_methylations;       /**< Number of active methylation sites */
    uint32_t active_histones;           /**< Number of histone modifications */
    uint32_t open_regions;              /**< Number of open chromatin regions */
    uint32_t active_imprints;           /**< Number of active imprints */
    float global_plasticity;            /**< Overall plasticity level (0-1) */
    float methylation_load;             /**< Total methylation burden */
    bool is_critical_period;            /**< In critical period? */
    uint64_t last_update_time;
} nimcp_epigenetics_state_t;

/**
 * @brief Epigenetics statistics
 */
typedef struct {
    uint64_t methylations_added;
    uint64_t methylations_removed;
    uint64_t histone_modifications;
    uint64_t chromatin_transitions;
    uint64_t imprinting_events;
    float avg_plasticity;
    float avg_methylation;
    float critical_period_time;
} nimcp_epigenetics_stats_t;

//=============================================================================
// Main Epigenetics API
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT nimcp_epigenetics_config_t nimcp_epigenetics_default_config(void);

/**
 * @brief Create epigenetics system
 */
NIMCP_EXPORT nimcp_epigenetics_t nimcp_epigenetics_create(
    const nimcp_epigenetics_config_t* config
);

/**
 * @brief Destroy epigenetics system
 */
NIMCP_EXPORT void nimcp_epigenetics_destroy(nimcp_epigenetics_t epi);

/**
 * @brief Initialize with brain connection
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_init(
    nimcp_epigenetics_t epi,
    nimcp_brain_t brain
);

/**
 * @brief Shutdown epigenetics system
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_shutdown(
    nimcp_epigenetics_t epi
);

/**
 * @brief Update epigenetics state
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_update(
    nimcp_epigenetics_t epi,
    float dt
);

/**
 * @brief Get plasticity modifier for neuron
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_plasticity(
    nimcp_epigenetics_t epi,
    uint32_t neuron_id,
    float* plasticity
);

/**
 * @brief Check if region is in critical period
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_is_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    bool* is_critical
);

/**
 * @brief Get current state
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_state(
    nimcp_epigenetics_t epi,
    nimcp_epigenetics_state_t* state
);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_stats(
    nimcp_epigenetics_t epi,
    nimcp_epigenetics_stats_t* stats
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_reset_stats(
    nimcp_epigenetics_t epi
);

//=============================================================================
// Methylation API
//=============================================================================

/**
 * @brief Add methylation site
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_add_methylation(
    nimcp_epigenetics_t epi,
    const nimcp_methylation_site_t* site
);

/**
 * @brief Remove methylation site
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_remove_methylation(
    nimcp_epigenetics_t epi,
    uint32_t site_id
);

/**
 * @brief Get methylation state for neuron
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_methylation(
    nimcp_epigenetics_t epi,
    uint32_t neuron_id,
    float* methylation_level,
    nimcp_methylation_state_t* state
);

/**
 * @brief Methylate synapse (reduce its plasticity)
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_methylate_synapse(
    nimcp_epigenetics_t epi,
    uint32_t synapse_id,
    float level
);

/**
 * @brief Demethylate synapse (restore plasticity)
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_demethylate_synapse(
    nimcp_epigenetics_t epi,
    uint32_t synapse_id
);

//=============================================================================
// Histone Modification API
//=============================================================================

/**
 * @brief Apply histone modification
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_modify_histone(
    nimcp_epigenetics_t epi,
    const nimcp_histone_config_t* config
);

/**
 * @brief Get histone state for region
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_histone_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    float* acetylation_level,
    float* methylation_level
);

/**
 * @brief Clear all histone modifications for region
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_clear_histones(
    nimcp_epigenetics_t epi,
    uint32_t region_id
);

//=============================================================================
// Chromatin State API
//=============================================================================

/**
 * @brief Configure chromatin region
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_configure_region(
    nimcp_epigenetics_t epi,
    const nimcp_chromatin_config_t* config
);

/**
 * @brief Get chromatin state
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_chromatin_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    nimcp_chromatin_state_t* state
);

/**
 * @brief Force chromatin state transition
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_set_chromatin_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    nimcp_chromatin_state_t state
);

/**
 * @brief Open chromatin region (increase plasticity)
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_open_region(
    nimcp_epigenetics_t epi,
    uint32_t region_id
);

/**
 * @brief Close chromatin region (decrease plasticity)
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_close_region(
    nimcp_epigenetics_t epi,
    uint32_t region_id
);

//=============================================================================
// Imprinting API
//=============================================================================

/**
 * @brief Create imprint
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_create_imprint(
    nimcp_epigenetics_t epi,
    const nimcp_imprint_config_t* config,
    uint32_t* imprint_id
);

/**
 * @brief Get imprint status
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_imprint(
    nimcp_epigenetics_t epi,
    uint32_t imprint_id,
    float* strength,
    float* remaining_duration
);

/**
 * @brief Remove imprint
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_remove_imprint(
    nimcp_epigenetics_t epi,
    uint32_t imprint_id
);

/**
 * @brief Start critical period for region
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_start_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    float duration_ms
);

/**
 * @brief End critical period for region
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_end_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id
);

//=============================================================================
// Environmental Factors API
//=============================================================================

/**
 * @brief Apply environmental factor
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_apply_environment(
    nimcp_epigenetics_t epi,
    float stress_level,
    float enrichment_level
);

/**
 * @brief Get environmental effect on plasticity
 */
NIMCP_EXPORT nimcp_epigenetics_error_t nimcp_epigenetics_get_environment_effect(
    nimcp_epigenetics_t epi,
    float* plasticity_modifier
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string
 */
NIMCP_EXPORT const char* nimcp_epigenetics_error_string(nimcp_epigenetics_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPIGENETICS_H */
