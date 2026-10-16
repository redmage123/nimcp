/**
 * @file nimcp_neurogenesis.h
 * @brief Neurogenesis Module - Generation of new neurons
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Neurogenesis for dynamic neural network growth
 * WHY:  Enable activity-dependent network expansion and adaptation
 * HOW:  Model stem cells, differentiation, integration, and pruning
 *
 * KEY CONCEPTS:
 * - Neural Stem Cells: Self-renewing progenitor cells
 * - Differentiation: Stem cell -> Neuron transformation
 * - Integration: New neuron connection to existing network
 * - Activity-Dependent Survival: Use it or lose it
 * - Neurogenic Niches: Brain regions supporting neurogenesis
 *
 * BIOLOGICAL BASIS:
 * - Adult neurogenesis occurs in hippocampus (DG) and SVZ
 * - New neurons integrate over 4-6 weeks
 * - Activity and environment regulate neurogenesis rate
 * - BDNF, exercise, and learning increase neurogenesis
 * - Stress and aging decrease neurogenesis
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROGENESIS_H
#define NIMCP_NEUROGENESIS_H

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

/** Maximum stem cells per niche */
#define NEUROGENESIS_MAX_STEM_CELLS         256

/** Maximum neurogenic niches */
#define NEUROGENESIS_MAX_NICHES             16

/** Maximum pending neurons */
#define NEUROGENESIS_MAX_PENDING            128

/** Default integration time (simulation steps) */
#define NEUROGENESIS_DEFAULT_INTEGRATION    1000

/** Survival threshold for activity */
#define NEUROGENESIS_SURVIVAL_THRESHOLD     0.1f

/** Default proliferation rate */
#define NEUROGENESIS_DEFAULT_PROLIF_RATE    0.01f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    NEUROGENESIS_OK = 0,
    NEUROGENESIS_ERR_NULL_PTR = -1,
    NEUROGENESIS_ERR_INVALID_PARAM = -2,
    NEUROGENESIS_ERR_NOT_INITIALIZED = -3,
    NEUROGENESIS_ERR_ALREADY_INITIALIZED = -4,
    NEUROGENESIS_ERR_NO_MEMORY = -5,
    NEUROGENESIS_ERR_NICHE_NOT_FOUND = -6,
    NEUROGENESIS_ERR_NICHE_FULL = -7,
    NEUROGENESIS_ERR_NEURON_NOT_FOUND = -8,
    NEUROGENESIS_ERR_NO_STEM_CELLS = -9
} nimcp_neurogenesis_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neural stem cell state
 */
typedef enum {
    STEM_STATE_QUIESCENT = 0,           /**< Dormant, not dividing */
    STEM_STATE_ACTIVATED,               /**< Ready to divide */
    STEM_STATE_PROLIFERATING,           /**< Actively dividing */
    STEM_STATE_DIFFERENTIATING,         /**< Becoming a neuron */
    STEM_STATE_DEPLETED                 /**< Exhausted, cannot divide */
} nimcp_stem_state_t;

/**
 * @brief New neuron development stage
 */
typedef enum {
    NEURON_STAGE_PROGENITOR = 0,        /**< Just differentiated */
    NEURON_STAGE_IMMATURE,              /**< Growing dendrites/axon */
    NEURON_STAGE_INTEGRATING,           /**< Forming connections */
    NEURON_STAGE_MATURE,                /**< Fully functional */
    NEURON_STAGE_APOPTOTIC              /**< Dying - insufficient activity */
} nimcp_neuron_stage_t;

/**
 * @brief Neurogenic niche type
 */
typedef enum {
    NICHE_TYPE_HIPPOCAMPUS = 0,         /**< Dentate gyrus - memory */
    NICHE_TYPE_SVZ,                     /**< Subventricular zone - olfactory */
    NICHE_TYPE_CORTICAL,                /**< Limited cortical neurogenesis */
    NICHE_TYPE_CUSTOM                   /**< User-defined niche */
} nimcp_niche_type_t;

/**
 * @brief Neuron type for new neurons
 */
typedef enum {
    NEW_NEURON_TYPE_GRANULE = 0,        /**< Granule cells (hippocampus) */
    NEW_NEURON_TYPE_INTERNEURON,        /**< Inhibitory interneurons */
    NEW_NEURON_TYPE_PYRAMIDAL,          /**< Excitatory pyramidal */
    NEW_NEURON_TYPE_GENERIC             /**< Generic type */
} nimcp_new_neuron_type_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_brain_struct* nimcp_brain_t;
typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;

//=============================================================================
// Opaque Handles
//=============================================================================

typedef struct nimcp_neurogenesis_struct* nimcp_neurogenesis_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Neurogenesis system configuration
 */
typedef struct {
    uint32_t max_niches;                /**< Maximum neurogenic niches */
    uint32_t max_stem_cells_per_niche;  /**< Stem cells per niche */
    uint32_t max_pending_neurons;       /**< Max neurons in development */
    float base_proliferation_rate;      /**< Base division rate (0-1) */
    float integration_duration;         /**< Steps for integration */
    float survival_threshold;           /**< Activity threshold for survival */
    float pruning_rate;                 /**< Rate of pruning inactive neurons */
    bool enable_activity_modulation;    /**< Activity affects neurogenesis */
    bool enable_environmental_factors;  /**< Environment affects rate */
    bool enable_logging;
    bool enable_metrics;
} nimcp_neurogenesis_config_t;

/**
 * @brief Neurogenic niche configuration
 */
typedef struct {
    uint32_t niche_id;                  /**< Unique niche identifier */
    nimcp_niche_type_t type;            /**< Type of niche */
    uint32_t region_start;              /**< Starting neuron index */
    uint32_t region_end;                /**< Ending neuron index */
    uint32_t initial_stem_cells;        /**< Starting stem cell count */
    float local_proliferation_rate;     /**< Niche-specific rate */
    nimcp_new_neuron_type_t default_type; /**< Default neuron type */
} nimcp_niche_config_t;

/**
 * @brief Stem cell configuration
 */
typedef struct {
    uint32_t niche_id;                  /**< Parent niche */
    float proliferation_capacity;       /**< Divisions remaining (0-1 -> depleted) */
    nimcp_stem_state_t initial_state;   /**< Starting state */
} nimcp_stem_cell_config_t;

/**
 * @brief New neuron specification
 */
typedef struct {
    uint32_t niche_id;                  /**< Source niche */
    uint32_t neuron_id;                 /**< Assigned ID (0 = auto-assign) */
    nimcp_new_neuron_type_t type;       /**< Neuron type */
    float position[3];                  /**< Position in space (optional) */
    uint32_t target_connections;        /**< Target # of connections */
} nimcp_new_neuron_spec_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Individual new neuron state
 */
typedef struct {
    uint32_t neuron_id;
    nimcp_neuron_stage_t stage;
    float maturity;                     /**< 0-1, 1 = fully mature */
    float activity_level;               /**< Recent activity */
    uint32_t connections_formed;
    uint32_t integration_steps_remaining;
    bool is_active;
} nimcp_new_neuron_state_t;

/**
 * @brief Neurogenesis system state
 */
typedef struct {
    uint32_t total_stem_cells;          /**< Total stem cells across niches */
    uint32_t active_stem_cells;         /**< Currently proliferating */
    uint32_t pending_neurons;           /**< Neurons in development */
    uint32_t mature_neurons_created;    /**< Total mature neurons created */
    uint32_t neurons_pruned;            /**< Neurons that died */
    float global_proliferation_rate;    /**< Current overall rate */
    float environmental_modifier;       /**< Environment effect on rate */
    uint64_t last_update_time;
} nimcp_neurogenesis_state_t;

/**
 * @brief Neurogenesis statistics
 */
typedef struct {
    uint64_t neurons_created;
    uint64_t neurons_integrated;
    uint64_t neurons_pruned;
    uint64_t stem_cell_divisions;
    uint64_t stem_cells_depleted;
    float avg_integration_time;
    float avg_survival_rate;
    float avg_proliferation_rate;
} nimcp_neurogenesis_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback when new neuron is created
 */
typedef void (*nimcp_neuron_created_cb)(
    uint32_t neuron_id,
    nimcp_new_neuron_type_t type,
    uint32_t niche_id,
    void* user_data
);

/**
 * @brief Callback when neuron matures
 */
typedef void (*nimcp_neuron_matured_cb)(
    uint32_t neuron_id,
    uint32_t connections,
    void* user_data
);

/**
 * @brief Callback when neuron is pruned
 */
typedef void (*nimcp_neuron_pruned_cb)(
    uint32_t neuron_id,
    float final_activity,
    void* user_data
);

//=============================================================================
// Main Neurogenesis API
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT nimcp_neurogenesis_config_t nimcp_neurogenesis_default_config(void);

/**
 * @brief Create neurogenesis system
 */
NIMCP_EXPORT nimcp_neurogenesis_t nimcp_neurogenesis_create(
    const nimcp_neurogenesis_config_t* config
);

/**
 * @brief Destroy neurogenesis system
 */
NIMCP_EXPORT void nimcp_neurogenesis_destroy(nimcp_neurogenesis_t ng);

/**
 * @brief Initialize with brain connection
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_init(
    nimcp_neurogenesis_t ng,
    nimcp_brain_t brain
);

/**
 * @brief Shutdown neurogenesis system
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_shutdown(
    nimcp_neurogenesis_t ng
);

/**
 * @brief Update neurogenesis (call each simulation step)
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_update(
    nimcp_neurogenesis_t ng,
    float dt
);

/**
 * @brief Set callbacks for neuron lifecycle events
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_set_callbacks(
    nimcp_neurogenesis_t ng,
    nimcp_neuron_created_cb on_created,
    nimcp_neuron_matured_cb on_matured,
    nimcp_neuron_pruned_cb on_pruned,
    void* user_data
);

/**
 * @brief Get current state
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_state(
    nimcp_neurogenesis_t ng,
    nimcp_neurogenesis_state_t* state
);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stats(
    nimcp_neurogenesis_t ng,
    nimcp_neurogenesis_stats_t* stats
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_reset_stats(
    nimcp_neurogenesis_t ng
);

//=============================================================================
// Niche API
//=============================================================================

/**
 * @brief Create neurogenic niche
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_create_niche(
    nimcp_neurogenesis_t ng,
    const nimcp_niche_config_t* config
);

/**
 * @brief Remove niche
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_remove_niche(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id
);

/**
 * @brief Get niche info
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_niche_info(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* stem_cells,
    uint32_t* pending_neurons,
    float* proliferation_rate
);

/**
 * @brief Set niche proliferation rate
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_set_niche_rate(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    float rate
);

//=============================================================================
// Stem Cell API
//=============================================================================

/**
 * @brief Add stem cells to niche
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_add_stem_cells(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t count
);

/**
 * @brief Get stem cell count in niche
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stem_count(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* count
);

/**
 * @brief Activate stem cells (start proliferation)
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_activate_stem_cells(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t count
);

/**
 * @brief Get stem cell state distribution
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stem_distribution(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* quiescent,
    uint32_t* activated,
    uint32_t* proliferating,
    uint32_t* differentiating
);

//=============================================================================
// Neuron Generation API
//=============================================================================

/**
 * @brief Trigger neuron creation in niche
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_create_neuron(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* new_neuron_id
);

/**
 * @brief Get new neuron state
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_neuron_state(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id,
    nimcp_new_neuron_state_t* state
);

/**
 * @brief Report activity for new neuron (affects survival)
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_report_activity(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id,
    float activity
);

/**
 * @brief Force neuron to mature (skip integration period)
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_force_mature(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id
);

/**
 * @brief Prune neuron (remove due to low activity)
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_prune_neuron(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id
);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Set environmental factors affecting neurogenesis
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_set_environment(
    nimcp_neurogenesis_t ng,
    float bdnf_level,           /**< Brain-derived neurotrophic factor (0-1) */
    float stress_level,         /**< Stress reduces neurogenesis (0-1) */
    float exercise_level,       /**< Exercise increases neurogenesis (0-1) */
    float enrichment_level      /**< Environmental enrichment (0-1) */
);

/**
 * @brief Get current environmental modulation
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_get_environment(
    nimcp_neurogenesis_t ng,
    float* modifier
);

/**
 * @brief Set global proliferation rate
 */
NIMCP_EXPORT nimcp_neurogenesis_error_t nimcp_neurogenesis_set_global_rate(
    nimcp_neurogenesis_t ng,
    float rate
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string
 */
NIMCP_EXPORT const char* nimcp_neurogenesis_error_string(nimcp_neurogenesis_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_H */
