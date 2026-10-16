//=============================================================================
// nimcp_hemispheric_injury.h - Brain Injury and Recovery Modeling
//=============================================================================
/**
 * @file nimcp_hemispheric_injury.h
 * @brief Hemispheric brain injury simulation and neuroplastic recovery
 *
 * WHAT: Model brain lesions, strokes, and recovery mechanisms
 * WHY:  Enables study of unilateral damage, functional reorganization,
 *       and rehabilitation strategies
 * HOW:  Region-based damage model, connectivity pruning, compensatory plasticity
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * TYPES OF BRAIN INJURY:
 * ----------------------
 * 1. Stroke (Ischemic/Hemorrhagic):
 *    - Focal damage to specific brain regions
 *    - Effects spread to connected areas (diaschisis)
 *    - Contralateral deficits (right brain → left body)
 *
 * 2. Traumatic Brain Injury (TBI):
 *    - Diffuse axonal injury (widespread disconnection)
 *    - Coup-contrecoup damage (impact + rebound)
 *    - Variable severity (mild to severe)
 *
 * 3. Degenerative (Alzheimer's, Parkinson's):
 *    - Progressive loss of function
 *    - Asymmetric onset (often one hemisphere first)
 *    - Spreading pattern through neural networks
 *
 * RECOVERY MECHANISMS:
 * -------------------
 * 1. Neuroplasticity:
 *    - Synaptic strengthening in intact circuits
 *    - Axonal sprouting (new connections)
 *    - Dendritic growth
 *
 * 2. Functional Reorganization:
 *    - Homologous regions take over functions
 *    - Adjacent cortex expands representation
 *    - Inter-hemispheric compensation
 *
 * 3. Behavioral Compensation:
 *    - Alternative strategies
 *    - Cognitive reserve utilization
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    HEMISPHERIC INJURY SYSTEM                            |
 * +=========================================================================+
 * |                                                                          |
 * |   INJURY LAYER                      RECOVERY LAYER                      |
 * |   ┌────────────────────┐            ┌────────────────────┐             |
 * |   │  Lesion Model      │            │  Plasticity Engine │             |
 * |   │  - Region damage   │            │  - Synaptic boost  │             |
 * |   │  - Axon severing   │            │  - Axon sprouting  │             |
 * |   │  - Diaschisis      │            │  - Map expansion   │             |
 * |   └────────────────────┘            └────────────────────┘             |
 * |            │                                  │                         |
 * |            ▼                                  ▼                         |
 * |   ┌────────────────────┐            ┌────────────────────┐             |
 * |   │  Damage Effects    │            │  Recovery Progress │             |
 * |   │  - % capacity lost │            │  - Function return │             |
 * |   │  - Functional map  │            │  - Time course     │             |
 * |   │  - Connectivity    │            │  - Plateau level   │             |
 * |   └────────────────────┘            └────────────────────┘             |
 * |                                                                          |
 * |   ┌──────────────────────────────────────────────────────────────┐     |
 * |   │              REHABILITATION INTERFACE                         │     |
 * |   │  - Targeted training    - Constraint-induced therapy         │     |
 * |   │  - Neuromodulation      - Environmental enrichment           │     |
 * |   └──────────────────────────────────────────────────────────────┘     |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_INJURY_H
#define NIMCP_HEMISPHERIC_INJURY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of concurrent lesions */
#define MAX_LESIONS                     16

/** Maximum number of affected regions per lesion */
#define MAX_AFFECTED_REGIONS            32

/** Default recovery time constant (days) */
#define DEFAULT_RECOVERY_TAU_DAYS       30.0f

/** Minimum function after complete lesion */
#define MINIMUM_RESIDUAL_FUNCTION       0.05f

/** Maximum compensation from intact hemisphere */
#define MAX_CONTRALATERAL_COMPENSATION  0.40f

/** Diaschisis propagation factor */
#define DIASCHISIS_FACTOR               0.30f

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Type of brain injury
 */
typedef enum {
    INJURY_TYPE_STROKE_ISCHEMIC,    /**< Blood flow blockage */
    INJURY_TYPE_STROKE_HEMORRHAGIC, /**< Bleeding in brain */
    INJURY_TYPE_TBI_FOCAL,          /**< Localized trauma */
    INJURY_TYPE_TBI_DIFFUSE,        /**< Widespread axonal damage */
    INJURY_TYPE_DEGENERATIVE,       /**< Progressive degeneration */
    INJURY_TYPE_SURGICAL,           /**< Planned surgical ablation */
    INJURY_TYPE_CUSTOM              /**< User-defined injury */
} injury_type_t;

/**
 * @brief Severity level of injury
 */
typedef enum {
    SEVERITY_MILD,                  /**< <25% function loss */
    SEVERITY_MODERATE,              /**< 25-50% function loss */
    SEVERITY_SEVERE,                /**< 50-75% function loss */
    SEVERITY_PROFOUND               /**< >75% function loss */
} injury_severity_t;

/**
 * @brief Brain region affected (for injury modeling)
 *
 * Note: Uses INJURY_ prefix to avoid conflict with injury_region_t struct
 */
typedef enum {
    INJURY_REGION_MOTOR_CORTEX,
    INJURY_REGION_SENSORY_CORTEX,
    INJURY_REGION_PREFRONTAL,
    INJURY_REGION_TEMPORAL,
    INJURY_REGION_PARIETAL,
    INJURY_REGION_OCCIPITAL,
    INJURY_REGION_BROCA,
    INJURY_REGION_WERNICKE,
    INJURY_REGION_HIPPOCAMPUS,
    INJURY_REGION_AMYGDALA,
    INJURY_REGION_BASAL_GANGLIA,
    INJURY_REGION_CEREBELLUM,
    INJURY_REGION_THALAMUS,
    INJURY_REGION_BRAINSTEM,
    INJURY_REGION_CORPUS_CALLOSUM,
    INJURY_REGION_COUNT
} injury_region_t;

/**
 * @brief Recovery phase
 */
typedef enum {
    RECOVERY_PHASE_ACUTE,           /**< Immediate post-injury (days) */
    RECOVERY_PHASE_SUBACUTE,        /**< Early recovery (weeks) */
    RECOVERY_PHASE_CHRONIC,         /**< Long-term adaptation (months) */
    RECOVERY_PHASE_PLATEAU          /**< Stable end-state */
} recovery_phase_t;

/**
 * @brief Individual lesion description
 */
typedef struct {
    uint32_t lesion_id;             /**< Unique lesion identifier */
    injury_type_t type;             /**< Type of injury */
    injury_severity_t severity;     /**< Severity level */
    hemisphere_id_t hemisphere;     /**< Affected hemisphere */

    // Spatial extent
    injury_region_t primary_region;  /**< Primary affected region */
    injury_region_t affected_regions[MAX_AFFECTED_REGIONS]; /**< All affected */
    uint32_t num_affected_regions;
    float damage_radius;            /**< Spread radius (abstract units) */

    // Damage levels (0.0 = intact, 1.0 = destroyed)
    float primary_damage;           /**< Damage to primary region */
    float secondary_damage;         /**< Diaschisis damage to connected areas */
    float axonal_damage;            /**< White matter disconnection */

    // Timing
    uint64_t onset_time;            /**< When injury occurred */
    bool is_progressive;            /**< True for degenerative conditions */
    float progression_rate;         /**< Rate of progression if degenerative */

    bool active;                    /**< Currently active lesion */
} lesion_t;

/**
 * @brief Region-specific damage state
 */
typedef struct {
    injury_region_t region;
    hemisphere_id_t hemisphere;
    float structural_damage;        /**< Physical damage (0-1) */
    float functional_impairment;    /**< Current function loss (0-1) */
    float connectivity_loss;        /**< Connection damage (0-1) */
    float recovery_progress;        /**< Recovery achieved (0-1) */
    float compensatory_function;    /**< Function from compensation (0-1) */
} region_damage_state_t;

/**
 * @brief Recovery parameters for plasticity
 */
typedef struct {
    float spontaneous_recovery_rate; /**< Natural recovery speed */
    float plasticity_potential;      /**< Remaining capacity for change */
    float contralateral_compensation; /**< Intact hemisphere takeover */
    float perilesional_expansion;    /**< Adjacent cortex expansion */
    float synaptic_strengthening;    /**< LTP enhancement factor */
    float axonal_sprouting_rate;     /**< New connection formation */
} recovery_params_t;

/**
 * @brief Rehabilitation intervention
 */
typedef struct {
    bool active;
    float intensity;                /**< Training intensity (0-1) */
    float frequency;                /**< Sessions per day */
    injury_region_t target_region;   /**< Region being trained */
    float efficacy;                 /**< Intervention effectiveness */
} rehabilitation_t;

/**
 * @brief Injury system configuration
 */
typedef struct {
    // Recovery parameters
    float recovery_tau_days;        /**< Time constant for recovery */
    float max_recovery_potential;   /**< Maximum recovery (0-1) */
    bool enable_spontaneous_recovery;
    bool enable_contralateral_compensation;
    bool enable_perilesional_plasticity;

    // Diaschisis
    bool enable_diaschisis;         /**< Remote effects of lesions */
    float diaschisis_decay_factor;  /**< How much diaschisis decreases over time */

    // Rehabilitation
    bool enable_rehabilitation;     /**< Training-based recovery */
    float rehabilitation_boost;     /**< Multiplier for rehab effect */

    // Bio-async
    bool enable_bio_async;          /**< Event-driven updates */
} hemispheric_injury_config_t;

/**
 * @brief Injury system statistics
 */
typedef struct {
    uint64_t injury_updates;        /**< Total update cycles */
    uint32_t total_lesions;         /**< Lesions created */
    uint32_t active_lesions;        /**< Currently active */
    float avg_damage_level;         /**< Average damage across regions */
    float avg_recovery_level;       /**< Average recovery progress */
    float total_function_loss;      /**< Overall function deficit */
    float total_compensated;        /**< Function recovered via compensation */
    uint64_t rehab_sessions;        /**< Rehabilitation sessions completed */
} hemispheric_injury_stats_t;

/**
 * @brief Hemispheric injury system
 */
typedef struct {
    // Connected brain
    hemispheric_brain_t* brain;

    // Configuration
    hemispheric_injury_config_t config;

    // Active lesions
    lesion_t lesions[MAX_LESIONS];
    uint32_t num_lesions;

    // Per-region damage state
    region_damage_state_t left_regions[INJURY_REGION_COUNT];
    region_damage_state_t right_regions[INJURY_REGION_COUNT];

    // Recovery state
    recovery_params_t left_recovery;
    recovery_params_t right_recovery;
    recovery_phase_t current_phase;
    float time_since_injury_days;

    // Rehabilitation
    rehabilitation_t left_rehab;
    rehabilitation_t right_rehab;

    // Statistics
    hemispheric_injury_stats_t stats;

    // Bio-async
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Thread safety
    nimcp_mutex_t* mutex;

    bool initialized;
} hemispheric_injury_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default injury configuration
 */
hemispheric_injury_config_t hemispheric_injury_default_config(void);

/**
 * @brief Create injury system
 *
 * @param config System configuration
 * @param brain Hemispheric brain to attach to
 * @return System instance or NULL on failure
 */
hemispheric_injury_system_t* hemispheric_injury_create(
    const hemispheric_injury_config_t* config,
    hemispheric_brain_t* brain
);

/**
 * @brief Destroy injury system
 */
void hemispheric_injury_destroy(hemispheric_injury_system_t* system);

//=============================================================================
// Lesion API
//=============================================================================

/**
 * @brief Induce lesion in brain
 *
 * WHAT: Create damage to specific brain region
 * WHY:  Simulate stroke, trauma, or surgical ablation
 *
 * @param system Injury system
 * @param type Type of injury
 * @param severity Severity level
 * @param hemisphere Affected hemisphere
 * @param region Primary affected region
 * @param damage Primary damage level (0-1)
 * @param lesion_id Output: assigned lesion ID
 * @return 0 on success, negative on error
 */
int hemispheric_injury_induce_lesion(
    hemispheric_injury_system_t* system,
    injury_type_t type,
    injury_severity_t severity,
    hemisphere_id_t hemisphere,
    injury_region_t region,
    float damage,
    uint32_t* lesion_id
);

/**
 * @brief Remove lesion (surgical repair, simulation reset)
 *
 * @param system Injury system
 * @param lesion_id Lesion to remove
 * @return 0 on success, negative on error
 */
int hemispheric_injury_remove_lesion(
    hemispheric_injury_system_t* system,
    uint32_t lesion_id
);

/**
 * @brief Get lesion information
 *
 * @param system Injury system
 * @param lesion_id Lesion ID
 * @return Lesion structure or NULL if not found
 */
const lesion_t* hemispheric_injury_get_lesion(
    const hemispheric_injury_system_t* system,
    uint32_t lesion_id
);

/**
 * @brief Expand lesion (simulate hemorrhage expansion or degeneration)
 *
 * @param system Injury system
 * @param lesion_id Lesion to expand
 * @param additional_damage Additional damage (0-1)
 * @return 0 on success, negative on error
 */
int hemispheric_injury_expand_lesion(
    hemispheric_injury_system_t* system,
    uint32_t lesion_id,
    float additional_damage
);

//=============================================================================
// Damage Query API
//=============================================================================

/**
 * @brief Get damage level for brain region
 *
 * @param system Injury system
 * @param hemisphere Hemisphere to query
 * @param region Brain region
 * @return Damage level (0 = intact, 1 = destroyed)
 */
float hemispheric_injury_get_damage(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
);

/**
 * @brief Get current functional capacity for region
 *
 * @param system Injury system
 * @param hemisphere Hemisphere to query
 * @param region Brain region
 * @return Functional capacity (0 = no function, 1 = full function)
 */
float hemispheric_injury_get_function(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
);

/**
 * @brief Get connectivity loss for region
 *
 * @param system Injury system
 * @param hemisphere Hemisphere to query
 * @param region Brain region
 * @return Connectivity loss (0 = intact, 1 = disconnected)
 */
float hemispheric_injury_get_connectivity_loss(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
);

/**
 * @brief Get full region damage state
 */
region_damage_state_t hemispheric_injury_get_region_state(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
);

//=============================================================================
// Recovery API
//=============================================================================

/**
 * @brief Update injury system (advance recovery)
 *
 * @param system Injury system
 * @param dt Time step in days
 * @return 0 on success, negative on error
 */
int hemispheric_injury_update(
    hemispheric_injury_system_t* system,
    float dt
);

/**
 * @brief Get recovery progress for region
 *
 * @param system Injury system
 * @param hemisphere Hemisphere to query
 * @param region Brain region
 * @return Recovery progress (0 = no recovery, 1 = full recovery)
 */
float hemispheric_injury_get_recovery(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
);

/**
 * @brief Get current recovery phase
 */
recovery_phase_t hemispheric_injury_get_phase(
    const hemispheric_injury_system_t* system
);

/**
 * @brief Set recovery parameters
 *
 * @param system Injury system
 * @param hemisphere Target hemisphere
 * @param params Recovery parameters
 * @return 0 on success, negative on error
 */
int hemispheric_injury_set_recovery_params(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    const recovery_params_t* params
);

/**
 * @brief Boost plasticity (simulate neuromodulation/enrichment)
 *
 * @param system Injury system
 * @param hemisphere Target hemisphere
 * @param boost Plasticity boost factor (>1.0)
 * @return 0 on success, negative on error
 */
int hemispheric_injury_boost_plasticity(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    float boost
);

//=============================================================================
// Rehabilitation API
//=============================================================================

/**
 * @brief Start rehabilitation intervention
 *
 * @param system Injury system
 * @param hemisphere Target hemisphere
 * @param target_region Region being trained
 * @param intensity Training intensity (0-1)
 * @param frequency Sessions per day
 * @return 0 on success, negative on error
 */
int hemispheric_injury_start_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t target_region,
    float intensity,
    float frequency
);

/**
 * @brief Stop rehabilitation intervention
 *
 * @param system Injury system
 * @param hemisphere Target hemisphere
 * @return 0 on success, negative on error
 */
int hemispheric_injury_stop_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
);

/**
 * @brief Apply rehabilitation session
 *
 * @param system Injury system
 * @param hemisphere Target hemisphere
 * @return 0 on success, negative on error
 */
int hemispheric_injury_apply_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get injury system statistics
 */
hemispheric_injury_stats_t hemispheric_injury_get_stats(
    const hemispheric_injury_system_t* system
);

/**
 * @brief Reset statistics
 */
void hemispheric_injury_reset_stats(hemispheric_injury_system_t* system);

/**
 * @brief Get total function loss across all regions
 *
 * @param system Injury system
 * @param hemisphere Hemisphere to query
 * @return Total function loss (0-1)
 */
float hemispheric_injury_get_total_deficit(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get string name for brain region
 */
const char* hemispheric_injury_region_name(injury_region_t region);

/**
 * @brief Get string name for injury type
 */
const char* hemispheric_injury_type_name(injury_type_t type);

/**
 * @brief Get string name for recovery phase
 */
const char* hemispheric_injury_phase_name(recovery_phase_t phase);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_injury_connect_bio_async(hemispheric_injury_system_t* system);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_injury_disconnect_bio_async(hemispheric_injury_system_t* system);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_INJURY_H
