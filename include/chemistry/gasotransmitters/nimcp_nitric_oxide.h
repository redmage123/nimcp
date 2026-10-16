/**
 * @file nimcp_nitric_oxide.h
 * @brief Nitric Oxide Signaling Module - Retrograde neurotransmitter and vasodilator
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Nitric oxide (NO) signaling for neural modulation
 * WHY:  NO is a gaseous retrograde messenger critical for LTP and blood flow
 * HOW:  Model NOS activity, NO diffusion, cGMP signaling, and presynaptic effects
 *
 * KEY CONCEPTS:
 * - NO Synthase (NOS): Produces NO from L-arginine
 * - Retrograde Signaling: NO diffuses from post- to presynaptic neuron
 * - cGMP Pathway: NO activates guanylyl cyclase, produces cGMP
 * - Vasodilation: NO relaxes blood vessels, increases blood flow
 * - Volume Transmission: NO diffuses in 3D, affects multiple synapses
 *
 * BIOLOGICAL BASIS:
 * - nNOS (neuronal) is Ca2+/calmodulin activated
 * - NO has ~1s half-life, diffuses ~100 µm
 * - cGMP activates PKG, modulates ion channels
 * - Critical for NMDA-dependent LTP
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NITRIC_OXIDE_H
#define NIMCP_NITRIC_OXIDE_H

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

/** Physiological NO concentration (nM) */
#define NO_BASAL_CONCENTRATION          1.0f

/** Peak NO concentration during activity (nM) */
#define NO_PEAK_CONCENTRATION           100.0f

/** NO half-life (seconds) */
#define NO_HALF_LIFE_SEC                1.0f

/** NO diffusion coefficient (µm²/s) */
#define NO_DIFFUSION_COEFF              3300.0f

/** NO effective diffusion radius (µm) */
#define NO_DIFFUSION_RADIUS             100.0f

/** cGMP basal concentration (µM) */
#define CGMP_BASAL_CONCENTRATION        0.1f

/** cGMP peak concentration (µM) */
#define CGMP_PEAK_CONCENTRATION         10.0f

/** Maximum NO sources per system */
#define NO_MAX_SOURCES                  256

/** Maximum target synapses per source */
#define NO_MAX_TARGETS_PER_SOURCE       64

/** NOS activation time constant (ms) */
#define NOS_ACTIVATION_TAU              100.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    NO_OK = 0,
    NO_ERR_NULL_PTR = -1,
    NO_ERR_INVALID_PARAM = -2,
    NO_ERR_NOT_INITIALIZED = -3,
    NO_ERR_ALREADY_INITIALIZED = -4,
    NO_ERR_NO_MEMORY = -5,
    NO_ERR_SOURCE_NOT_FOUND = -6,
    NO_ERR_CAPACITY_EXCEEDED = -7,
    NO_ERR_NOS_INACTIVE = -8,
    NO_ERR_SUBSTRATE_DEPLETED = -9
} nimcp_no_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief NOS isoforms
 */
typedef enum {
    NOS_TYPE_NNOS = 0,                  /**< Neuronal NOS (nNOS/NOS1) */
    NOS_TYPE_INOS,                       /**< Inducible NOS (iNOS/NOS2) */
    NOS_TYPE_ENOS,                       /**< Endothelial NOS (eNOS/NOS3) */
    NOS_TYPE_COUNT
} nimcp_nos_type_t;

/**
 * @brief NO signaling state
 */
typedef enum {
    NO_STATE_INACTIVE = 0,              /**< No NO production */
    NO_STATE_BASAL,                      /**< Baseline production */
    NO_STATE_ACTIVATED,                  /**< Activity-dependent burst */
    NO_STATE_SUSTAINED,                  /**< Prolonged elevation */
    NO_STATE_PATHOLOGICAL                /**< Excessive production */
} nimcp_no_state_t;

/**
 * @brief NO target types
 */
typedef enum {
    NO_TARGET_GUANYLYL_CYCLASE = 0,     /**< Primary target - cGMP production */
    NO_TARGET_ION_CHANNEL,               /**< Direct channel modulation */
    NO_TARGET_MITOCHONDRIA,              /**< Respiratory chain */
    NO_TARGET_S_NITROSYLATION,           /**< Protein modification */
    NO_TARGET_BLOOD_VESSEL               /**< Vasodilation */
} nimcp_no_target_t;

/**
 * @brief Retrograde signaling mode
 */
typedef enum {
    NO_RETROGRADE_NONE = 0,             /**< No retrograde effect */
    NO_RETROGRADE_PRESYNAPTIC_RELEASE,  /**< Enhance presynaptic release */
    NO_RETROGRADE_PRESYNAPTIC_INHIBIT,  /**< Inhibit presynaptic release */
    NO_RETROGRADE_BILATERAL              /**< Both directions */
} nimcp_no_retrograde_mode_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_no_source_s nimcp_no_source_t;
typedef struct nimcp_no_target_synapse_s nimcp_no_target_synapse_t;
typedef struct nimcp_no_system_s nimcp_no_system_t;
typedef struct nimcp_no_config_s nimcp_no_config_t;
typedef struct nimcp_no_metrics_s nimcp_no_metrics_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for NO release events
 */
typedef void (*nimcp_no_release_callback_t)(
    nimcp_no_source_t* source,
    float concentration,
    void* user_data
);

/**
 * @brief Callback for retrograde effects
 */
typedef void (*nimcp_no_retrograde_callback_t)(
    nimcp_no_target_synapse_t* target,
    float potentiation_factor,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief NO target synapse (presynaptic terminal affected by NO)
 */
struct nimcp_no_target_synapse_s {
    uint32_t synapse_id;                /**< Synapse identifier */
    float distance;                     /**< Distance from source (µm) */
    float no_concentration;             /**< Local NO concentration */
    float cgmp_concentration;           /**< Local cGMP level */
    float potentiation_factor;          /**< Release probability modifier */
    nimcp_no_retrograde_mode_t mode;    /**< Retrograde mode */
    bool active;                        /**< Target is active */
};

/**
 * @brief NO source (typically postsynaptic location with nNOS)
 */
struct nimcp_no_source_s {
    uint32_t id;                        /**< Source identifier */
    float position[3];                  /**< 3D position (µm) */

    /* NOS state */
    nimcp_nos_type_t nos_type;          /**< NOS isoform */
    float nos_activity;                 /**< Current NOS activity (0-1) */
    float nos_expression;               /**< NOS protein level */

    /* Activation requirements */
    float calcium_level;                /**< Ca2+ concentration (µM) */
    float calmodulin_bound;             /**< CaM binding fraction (0-1) */
    float nmda_activation;              /**< NMDA receptor activation */

    /* Substrate */
    float arginine_level;               /**< L-arginine substrate (µM) */
    float oxygen_level;                 /**< O2 availability */
    float bh4_level;                    /**< Tetrahydrobiopterin cofactor */

    /* Production */
    float no_production_rate;           /**< NO molecules/sec */
    float no_concentration;             /**< Local NO (nM) */

    /* Targets */
    nimcp_no_target_synapse_t targets[NO_MAX_TARGETS_PER_SOURCE];
    uint32_t num_targets;

    /* State */
    nimcp_no_state_t state;
    bool initialized;
};

/**
 * @brief NO system configuration
 */
struct nimcp_no_config_s {
    /* NOS parameters */
    nimcp_nos_type_t default_nos_type;
    float nos_km_arginine;              /**< Arginine Km (µM) */
    float nos_km_calcium;               /**< Ca2+ Km (µM) */
    float nos_vmax;                     /**< Maximum rate */

    /* Diffusion parameters */
    float diffusion_coefficient;        /**< µm²/s */
    float decay_rate;                   /**< 1/s */
    float effective_radius;             /**< µm */

    /* cGMP parameters */
    float gc_sensitivity;               /**< Guanylyl cyclase sensitivity */
    float cgmp_decay_rate;              /**< cGMP degradation rate */
    float pde_activity;                 /**< Phosphodiesterase activity */

    /* Retrograde parameters */
    float potentiation_max;             /**< Maximum potentiation */
    float potentiation_threshold;       /**< NO threshold for effect */

    /* Vasodilation parameters */
    float vasodilation_sensitivity;     /**< Blood flow response */

    /* Callbacks */
    nimcp_no_release_callback_t on_release;
    nimcp_no_retrograde_callback_t on_retrograde;
    void* callback_data;
};

/**
 * @brief NO system metrics
 */
struct nimcp_no_metrics_s {
    /* Production */
    float total_no_produced;            /**< Total NO molecules */
    float mean_no_concentration;        /**< Average concentration */
    float peak_no_concentration;        /**< Maximum observed */

    /* Effects */
    float mean_cgmp_level;
    float total_potentiation;
    float vasodilation_index;

    /* Sources */
    uint32_t active_sources;
    uint32_t total_sources;
    uint32_t activated_targets;

    /* Events */
    uint32_t release_events;
    uint32_t retrograde_events;

    /* Time */
    float total_simulation_time;
    uint64_t update_count;
};

/**
 * @brief Main NO signaling system
 */
struct nimcp_no_system_s {
    /* Sources */
    nimcp_no_source_t sources[NO_MAX_SOURCES];
    uint32_t num_sources;

    /* Configuration */
    nimcp_no_config_t config;

    /* Metrics */
    nimcp_no_metrics_t metrics;

    /* Global effects */
    float global_no_level;              /**< System-wide NO */
    float global_cgmp_level;            /**< System-wide cGMP */
    float vasodilation_factor;          /**< Blood flow modifier */
    float plasticity_modifier;          /**< LTP enhancement */

    /* State */
    bool initialized;
    uint64_t update_count;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize the NO signaling system
 * @param system NO system to initialize
 * @param config Configuration (NULL for defaults)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_init(
    nimcp_no_system_t* system,
    const nimcp_no_config_t* config
);

/**
 * @brief Shutdown the NO signaling system
 * @param system NO system to shutdown
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_shutdown(
    nimcp_no_system_t* system
);

/**
 * @brief Reset NO system to initial state
 * @param system NO system to reset
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_reset(
    nimcp_no_system_t* system
);

//=============================================================================
// Source Management API
//=============================================================================

/**
 * @brief Add an NO source
 * @param system NO system
 * @param position 3D position [x, y, z] in µm
 * @param nos_type NOS isoform
 * @param[out] source_id Assigned source ID
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_add_source(
    nimcp_no_system_t* system,
    const float position[3],
    nimcp_nos_type_t nos_type,
    uint32_t* source_id
);

/**
 * @brief Get source by ID
 * @param system NO system
 * @param source_id Source ID
 * @return Source pointer or NULL
 */
NIMCP_EXPORT nimcp_no_source_t* nimcp_no_get_source(
    nimcp_no_system_t* system,
    uint32_t source_id
);

/**
 * @brief Remove an NO source
 * @param system NO system
 * @param source_id Source ID to remove
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_remove_source(
    nimcp_no_system_t* system,
    uint32_t source_id
);

//=============================================================================
// Target Management API
//=============================================================================

/**
 * @brief Add a target synapse to a source
 * @param source NO source
 * @param synapse_id Target synapse identifier
 * @param distance Distance from source (µm)
 * @param mode Retrograde signaling mode
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_add_target(
    nimcp_no_source_t* source,
    uint32_t synapse_id,
    float distance,
    nimcp_no_retrograde_mode_t mode
);

/**
 * @brief Remove a target synapse
 * @param source NO source
 * @param synapse_id Target synapse to remove
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_remove_target(
    nimcp_no_source_t* source,
    uint32_t synapse_id
);

/**
 * @brief Get potentiation factor for a target
 * @param source NO source
 * @param synapse_id Target synapse
 * @param[out] potentiation Potentiation factor
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_target_potentiation(
    const nimcp_no_source_t* source,
    uint32_t synapse_id,
    float* potentiation
);

//=============================================================================
// NOS Activation API
//=============================================================================

/**
 * @brief Set calcium level for a source
 * @param source NO source
 * @param calcium_um Calcium concentration (µM)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_set_calcium(
    nimcp_no_source_t* source,
    float calcium_um
);

/**
 * @brief Set NMDA activation level
 * @param source NO source
 * @param activation NMDA activation (0-1)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_set_nmda_activation(
    nimcp_no_source_t* source,
    float activation
);

/**
 * @brief Set substrate levels
 * @param source NO source
 * @param arginine L-arginine (µM)
 * @param oxygen O2 level (0-1)
 * @param bh4 Tetrahydrobiopterin level (0-1)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_set_substrate(
    nimcp_no_source_t* source,
    float arginine,
    float oxygen,
    float bh4
);

/**
 * @brief Get current NOS activity
 * @param source NO source
 * @param[out] activity NOS activity (0-1)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_nos_activity(
    const nimcp_no_source_t* source,
    float* activity
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update NO system (single timestep)
 * @param system NO system
 * @param dt Time delta (ms)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_update(
    nimcp_no_system_t* system,
    float dt
);

/**
 * @brief Update single source
 * @param system NO system
 * @param source Source to update
 * @param dt Time delta (ms)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_update_source(
    nimcp_no_system_t* system,
    nimcp_no_source_t* source,
    float dt
);

/**
 * @brief Calculate NO diffusion to targets
 * @param system NO system
 * @param source Source
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_diffuse(
    nimcp_no_system_t* system,
    nimcp_no_source_t* source
);

//=============================================================================
// Effects API
//=============================================================================

/**
 * @brief Get cGMP concentration at source
 * @param system NO system
 * @param source_id Source ID
 * @param[out] cgmp cGMP concentration (µM)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_cgmp(
    const nimcp_no_system_t* system,
    uint32_t source_id,
    float* cgmp
);

/**
 * @brief Get vasodilation factor
 * @param system NO system
 * @param[out] factor Vasodilation factor (1.0 = normal)
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_vasodilation(
    const nimcp_no_system_t* system,
    float* factor
);

/**
 * @brief Get plasticity modifier
 * @param system NO system
 * @param[out] modifier LTP enhancement factor
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_plasticity_modifier(
    const nimcp_no_system_t* system,
    float* modifier
);

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Get NO system metrics
 * @param system NO system
 * @param[out] metrics Metrics output
 * @return NO_OK on success
 */
NIMCP_EXPORT nimcp_no_error_t nimcp_no_get_metrics(
    const nimcp_no_system_t* system,
    nimcp_no_metrics_t* metrics
);

/**
 * @brief Get NO state
 * @param source NO source
 * @return Current NO state
 */
NIMCP_EXPORT nimcp_no_state_t nimcp_no_get_state(
    const nimcp_no_source_t* source
);

/**
 * @brief Get error string
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* nimcp_no_error_string(nimcp_no_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NITRIC_OXIDE_H */
