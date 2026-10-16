/**
 * @file nimcp_genius_profiles.h
 * @brief Main genius profiles API with full system integration
 *
 * WHAT: Configurable genius brain profiles based on cognitive neuroscience
 * WHY:  Model domain-specific cognitive excellence (mathematical, artistic, etc.)
 * HOW:  Integrates with mesh, immune, SNN, training, cognitive layers
 *
 * Integration Points:
 *   - Mesh system: Distributed consensus on profile activation
 *   - Immune system: BBB validation, cytokine modulation, health agents
 *   - Exception handling: Presented to immune as antigens
 *   - SNN/Plasticity: STDP configuration, learning rate modulation
 *   - Bio-async: Full message routing and neuromodulator channels
 *   - Training: Gradient computation, loss tracking
 *   - Cognitive: RCOG/CCOG integration
 *   - KG Wiring: Entity registration and relationship tracking
 *   - Memory systems: Eidetic enhancement for all 11 memory subsystems
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#ifndef NIMCP_GENIUS_PROFILES_H
#define NIMCP_GENIUS_PROFILES_H

#include "nimcp_genius_types.h"
#include "nimcp_genius_traits.h"

/* Core dependencies */
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"  /* For brain_immune_system_t typedef */

/* System integrations (forward declarations to reduce header dependencies) */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * Note: Types like bbb_system_t, bio_router_t are already defined in
 * utils/bridge/nimcp_bridge_base.h which is included above. Only declare
 * types not already available from those headers.
 * ============================================================================ */

/* Immune system - matches cognitive/immune/nimcp_brain_immune.h */
/* brain_immune_system_t is typedef'd in nimcp_brain_immune.h or nimcp_exception_immune.h */
/* We don't re-typedef to avoid conflicts */
struct brain_immune_system;  /* Forward declare struct only */

/* health_agent already typedef'd via bridge_base.h */
struct health_agent;  /* Forward declare struct only */

/* Mesh system - forward declarations (full definitions in mesh/ headers) */
struct mesh_coordinator;
struct mesh_transaction;

/* Training */
#ifndef NIMCP_TRAINING_MODULE_T_DEFINED
#define NIMCP_TRAINING_MODULE_T_DEFINED
typedef struct nimcp_training_module_struct* nimcp_training_module_t;
#endif

/* Cognitive */
#ifndef RCOG_ENGINE_T_DEFINED
#define RCOG_ENGINE_T_DEFINED
typedef struct rcog_engine_struct* rcog_engine_t;
#endif

/* SNN/Plasticity - forward declarations (full definitions in plasticity/ headers) */
struct snn_config;
struct stdp_params;

/* Memory systems - forward declarations (full definitions in cognitive/ headers) */
struct working_memory;
struct autobiographical_memory_system;
struct semantic_memory_system;
struct nimcp_hippocampus;
struct hopfield_memory;

/* Brain regions */
#ifndef PARIETAL_ADAPTER_T_DEFINED
#define PARIETAL_ADAPTER_T_DEFINED
typedef struct parietal_adapter_struct* parietal_adapter_t;
#endif

#ifndef OCCIPITAL_ADAPTER_T_DEFINED
#define OCCIPITAL_ADAPTER_T_DEFINED
typedef struct occipital_adapter_struct* occipital_adapter_t;
#endif

#ifndef TEMPORAL_ADAPTER_T_DEFINED
#define TEMPORAL_ADAPTER_T_DEFINED
typedef struct temporal_adapter_struct* temporal_adapter_t;
#endif

#ifndef PREFRONTAL_ADAPTER_T_DEFINED
#define PREFRONTAL_ADAPTER_T_DEFINED
typedef struct prefrontal_adapter_struct* prefrontal_adapter_t;
#endif

#ifndef CEREBELLUM_ADAPTER_T_DEFINED
#define CEREBELLUM_ADAPTER_T_DEFINED
typedef struct cerebellum_adapter_struct* cerebellum_adapter_t;
#endif

#ifndef MOTOR_ADAPTER_T_DEFINED
#define MOTOR_ADAPTER_T_DEFINED
typedef struct motor_adapter_struct* motor_adapter_t;
#endif

/* Brain */
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct nimcp_brain_struct* nimcp_brain_t;
#endif

/* Forward declare hemispheric_brain_t - actual definition in nimcp_hemispheric_brain.h */
#ifndef HEMISPHERIC_BRAIN_T_DEFINED
#define HEMISPHERIC_BRAIN_T_DEFINED
typedef struct hemispheric_brain_struct hemispheric_brain_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator_struct* brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * GENIUS PROFILE STRUCTURE
 * ============================================================================ */

/**
 * @brief Complete genius profile definition
 *
 * Contains all parameters needed to configure a brain for a specific
 * type of cognitive excellence.
 */
struct genius_profile_t {
    /* === Identity === */
    genius_type_t type;                     /**< Profile type */
    char name[64];                          /**< Human-readable name */
    char description[256];                  /**< Profile description */
    char exemplars[128];                    /**< Famous exemplars */

    /* === Cognitive Traits === */
    genius_traits_t traits;                 /**< Cognitive trait parameters */

    /* === Brain Region Configurations === */
    genius_region_config_t parietal;        /**< Parietal cortex config */
    genius_region_config_t occipital;       /**< Occipital cortex config */
    genius_region_config_t temporal;        /**< Temporal cortex config */
    genius_region_config_t prefrontal;      /**< Prefrontal cortex config */
    genius_region_config_t cerebellum;      /**< Cerebellum config */
    genius_region_config_t hippocampus;     /**< Hippocampus config */
    genius_region_config_t motor;           /**< Motor cortex config */
    genius_region_config_t amygdala;        /**< Amygdala config */
    genius_region_config_t basal_ganglia;   /**< Basal ganglia config */

    /* === Connectivity === */
    genius_connectivity_t connectivity;     /**< Inter-region connectivity */

    /* === Lateralization === */
    genius_lateralization_t lateralization; /**< Hemispheric dominance */

    /* === Eidetic Memory === */
    eidetic_memory_config_t eidetic;        /**< Eidetic memory configuration */

    /* === Neuromodulator Profile === */
    float dopamine_baseline;                /**< DA baseline (1.0=normal) */
    float serotonin_baseline;               /**< 5-HT baseline */
    float norepinephrine_baseline;          /**< NE baseline */
    float acetylcholine_baseline;           /**< ACh baseline */

    /* === Immune Sensitivity === */
    float immune_sensitivity;               /**< Sensitivity to cytokines (0.5-2.0) */
    float inflammation_resistance;          /**< Resistance to inflammation (0.5-2.0) */

    /* === Flow State Parameters === */
    float flow_entry_threshold;             /**< Challenge/skill balance threshold */
    float flow_maintenance_factor;          /**< Flow state stability */
    float flow_exit_resistance;             /**< Resistance to interruption */

    /* === Metadata === */
    uint32_t version;                       /**< Profile version */
    uint64_t created_timestamp;             /**< Creation time */
    bool is_builtin;                        /**< True for predefined profiles */
};

/* ============================================================================
 * GENIUS PROFILES BRIDGE
 * ============================================================================ */

/**
 * @brief Genius profiles bridge configuration
 */
typedef struct {
    /* Bio-async */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    uint32_t inbox_capacity;                /**< Message inbox size (0=default) */

    /* Mesh */
    bool enable_mesh_coordination;          /**< Enable mesh consensus */
    uint32_t mesh_timeout_ms;               /**< Mesh consensus timeout */

    /* Immune */
    bool enable_immune_modulation;          /**< Allow immune effects */
    bool enable_bbb_validation;             /**< Enable BBB input validation */
    bool enable_health_agent;               /**< Enable heartbeat/resilience */
    uint32_t health_heartbeat_ms;           /**< Heartbeat interval */

    /* Training */
    bool enable_training_integration;       /**< Enable training layer */
    float base_learning_rate;               /**< Base learning rate */

    /* SNN/Plasticity */
    bool enable_snn_integration;            /**< Enable SNN configuration */
    bool enable_stdp;                       /**< Enable STDP learning */
    bool enable_metaplasticity;             /**< Enable metaplasticity */

    /* Cognitive */
    bool enable_rcog_integration;           /**< Enable RCOG engine */
    bool enable_ccog_integration;           /**< Enable CCOG */

    /* KG Wiring */
    bool enable_kg_wiring;                  /**< Enable KG entity registration */

    /* Exception handling */
    bool enable_exception_immune_presentation; /**< Present exceptions to immune */

    /* Quantum */
    bool enable_quantum_optimization;       /**< Enable quantum algorithms */

    /* Test Mode - Fast Brain Creation */
    bool test_mode;                         /**< Enable fast brain creation for tests.
                                                 When true, enables lazy initialization
                                                 of heavy subsystems (5-10x faster).
                                                 Skips: ethics, ToM, perception, glial,
                                                 neuromod, cortical columns, etc. */
} genius_profiles_config_t;

/**
 * @brief Genius profiles bridge instance
 *
 * Manages genius profile lifecycle and integrates with all NIMCP systems.
 * Bridge base MUST be first member for macro compatibility.
 */
struct genius_profiles_bridge_t {
    bridge_base_t base;                     /**< MUST BE FIRST - bridge base */

    /* === Configuration === */
    genius_profiles_config_t config;        /**< Bridge configuration */

    /* === Active Profiles === */
    genius_profile_t* active_profiles[GENIUS_MAX_ACTIVE_PROFILES];
    float blend_weights[GENIUS_MAX_ACTIVE_PROFILES];
    uint32_t active_count;                  /**< Number of active profiles */
    genius_activation_state_t state;        /**< Current activation state */

    /* === System References === */
    brain_immune_system_t* immune_system;   /**< Immune system reference */
    struct mesh_coordinator* mesh_coordinator; /**< Mesh coordinator reference */
    nimcp_training_module_t* training;      /**< Training module reference */
    rcog_engine_t* rcog_engine;             /**< RCOG engine reference */

    /* === Memory System References === */
    struct working_memory* working_memory;
    struct autobiographical_memory_system* autobio_memory;
    struct semantic_memory_system* semantic_memory;
    struct nimcp_hippocampus* hippocampus;
    struct hopfield_memory* hopfield_memory;

    /* === Brain Region References === */
    parietal_adapter_t* parietal;
    occipital_adapter_t* occipital;
    temporal_adapter_t* temporal;
    prefrontal_adapter_t* prefrontal;
    cerebellum_adapter_t* cerebellum;
    motor_adapter_t* motor;

    /* === State Tracking === */
    uint64_t activation_time_ms;            /**< When profile was activated */
    uint64_t last_update_ms;                /**< Last state update */
    float fatigue_level;                    /**< Current cognitive fatigue (0-1) */
    float flow_state_depth;                 /**< Current flow state depth (0-1) */

    /* === Immune Modulation State === */
    float current_cytokine_effect;          /**< Current cytokine modulation */
    float inflammation_level;               /**< Current inflammation level */
    bool is_degraded;                       /**< Degraded by immune response */

    /* === Statistics === */
    uint64_t total_activations;             /**< Total profile activations */
    uint64_t total_blends;                  /**< Total polymath blends */
    uint64_t total_exceptions;              /**< Total exceptions raised */
    uint64_t immune_presentations;          /**< Exceptions presented to immune */
    uint64_t mesh_consensus_count;          /**< Mesh consensus operations */

    /* === Health Agent State === */
    uint64_t last_heartbeat_ms;             /**< Last heartbeat time */
    bool health_agent_active;               /**< Health agent running */

    /* === Exception Epitope === */
    uint8_t current_epitope[GENIUS_EPITOPE_SIZE]; /**< Current exception fingerprint */

    /* === KG Wiring State === */
    uint32_t kg_root_node;                  /**< KG root node ID for genius profiles */

    /* === Mesh Integration === */
    uint64_t mesh_participant_id;           /**< Mesh network participant ID */
    uint64_t pending_mesh_tx_sequence;      /**< Pending mesh transaction sequence */
    uint64_t pending_mesh_tx_timestamp;     /**< Pending mesh transaction timestamp */
};

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 * @param config Output configuration (filled with defaults)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_config_default(genius_profiles_config_t* config);

/**
 * @brief Create genius profiles bridge
 *
 * Initializes bridge with all system integrations based on configuration.
 * Uses BRIDGE_CREATE_BEGIN macro for consistent initialization.
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
genius_profiles_bridge_t* genius_profiles_bridge_create(
    const genius_profiles_config_t* config
);

/**
 * @brief Destroy genius profiles bridge
 *
 * Cleans up all resources and disconnects from systems.
 * Uses BRIDGE_DESTROY macro for consistent cleanup.
 *
 * @param bridge Bridge to destroy
 */
void genius_profiles_bridge_destroy(genius_profiles_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge to reset
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_bridge_reset(genius_profiles_bridge_t* bridge);

/* ============================================================================
 * SYSTEM CONNECTION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Connect to immune system
 * @param bridge Bridge instance
 * @param immune Immune system
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_immune(
    genius_profiles_bridge_t* bridge,
    brain_immune_system_t* immune
);

/**
 * @brief Connect to mesh coordinator
 * @param bridge Bridge instance
 * @param mesh Mesh coordinator
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_mesh(
    genius_profiles_bridge_t* bridge,
    struct mesh_coordinator* mesh
);

/**
 * @brief Connect to training module
 * @param bridge Bridge instance
 * @param training Training module
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_training(
    genius_profiles_bridge_t* bridge,
    nimcp_training_module_t* training
);

/**
 * @brief Connect to RCOG engine
 * @param bridge Bridge instance
 * @param rcog RCOG engine
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_rcog(
    genius_profiles_bridge_t* bridge,
    rcog_engine_t* rcog
);

/**
 * @brief Connect to memory systems
 * @param bridge Bridge instance
 * @param wm Working memory
 * @param autobio Autobiographical memory
 * @param semantic Semantic memory
 * @param hippo Hippocampus
 * @param hopfield Hopfield memory
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_memory_systems(
    genius_profiles_bridge_t* bridge,
    struct working_memory* wm,
    struct autobiographical_memory_system* autobio,
    struct semantic_memory_system* semantic,
    struct nimcp_hippocampus* hippo,
    struct hopfield_memory* hopfield
);

/**
 * @brief Connect to brain regions
 * @param bridge Bridge instance
 * @param parietal Parietal adapter
 * @param occipital Occipital adapter
 * @param temporal Temporal adapter
 * @param prefrontal Prefrontal adapter
 * @param cerebellum Cerebellum adapter
 * @param motor Motor adapter
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_regions(
    genius_profiles_bridge_t* bridge,
    parietal_adapter_t* parietal,
    occipital_adapter_t* occipital,
    temporal_adapter_t* temporal,
    prefrontal_adapter_t* prefrontal,
    cerebellum_adapter_t* cerebellum,
    motor_adapter_t* motor
);

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_connect_bio_async(genius_profiles_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_disconnect_bio_async(genius_profiles_bridge_t* bridge);

/**
 * @brief Register KG wiring entities
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_register_kg_wiring(genius_profiles_bridge_t* bridge);

/* ============================================================================
 * PROFILE RETRIEVAL FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get predefined genius profile by type
 * @param type Genius type
 * @return Static profile pointer or NULL if invalid type
 */
const genius_profile_t* genius_profile_get(genius_type_t type);

/**
 * @brief Get profile type name
 * @param type Genius type
 * @return Static string name
 */
const char* genius_profile_type_name(genius_type_t type);

/**
 * @brief Get profile type description
 * @param type Genius type
 * @return Static string description
 */
const char* genius_profile_type_description(genius_type_t type);

/**
 * @brief Get profile type exemplars
 * @param type Genius type
 * @return Static string of exemplar names
 */
const char* genius_profile_type_exemplars(genius_type_t type);

/* ============================================================================
 * PROFILE ACTIVATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Activate a genius profile
 *
 * Activates the specified profile type with full system integration.
 * If mesh coordination is enabled, requires consensus.
 * Validates through BBB if enabled.
 *
 * @param bridge Bridge instance
 * @param type Genius type to activate
 * @param strength Activation strength (0.0-1.0)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_activate(
    genius_profiles_bridge_t* bridge,
    genius_type_t type,
    float strength
);

/**
 * @brief Deactivate current profile
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_deactivate(genius_profiles_bridge_t* bridge);

/**
 * @brief Blend multiple profiles (polymath mode)
 *
 * Combines multiple genius profiles with specified weights.
 * Total weights should sum to 1.0.
 *
 * @param bridge Bridge instance
 * @param types Array of genius types to blend
 * @param weights Blend weights for each type
 * @param count Number of profiles to blend
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_blend(
    genius_profiles_bridge_t* bridge,
    const genius_type_t* types,
    const float* weights,
    uint32_t count
);

/**
 * @brief Create polymath profile from primary and secondary
 *
 * Convenience function for blending two profiles.
 *
 * @param bridge Bridge instance
 * @param primary Primary genius type
 * @param secondary Secondary genius type
 * @param blend_factor Secondary weight (0.0-0.5)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_create_polymath(
    genius_profiles_bridge_t* bridge,
    genius_type_t primary,
    genius_type_t secondary,
    float blend_factor
);

/* ============================================================================
 * STATE QUERY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get current activation state
 * @param bridge Bridge instance
 * @return Current activation state
 */
genius_activation_state_t genius_profiles_get_state(
    const genius_profiles_bridge_t* bridge
);

/**
 * @brief Get currently active profile
 * @param bridge Bridge instance
 * @return Active profile or NULL if none active
 */
const genius_profile_t* genius_profiles_get_active(
    const genius_profiles_bridge_t* bridge
);

/**
 * @brief Get current fatigue level
 * @param bridge Bridge instance
 * @return Fatigue level (0.0-1.0)
 */
float genius_profiles_get_fatigue(const genius_profiles_bridge_t* bridge);

/**
 * @brief Get current flow state depth
 * @param bridge Bridge instance
 * @return Flow state depth (0.0-1.0)
 */
float genius_profiles_get_flow_depth(const genius_profiles_bridge_t* bridge);

/**
 * @brief Check if bridge is connected and ready
 * @param bridge Bridge instance
 * @return true if ready, false otherwise
 */
bool genius_profiles_is_ready(const genius_profiles_bridge_t* bridge);

/* ============================================================================
 * MODULATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Apply immune system modulation
 *
 * Called when cytokine levels change. Adjusts profile parameters
 * based on inflammation and immune state.
 *
 * @param bridge Bridge instance
 * @param cytokine_level Current cytokine level (0.0-1.0)
 * @param inflammation Current inflammation level (0.0-1.0)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_apply_immune_modulation(
    genius_profiles_bridge_t* bridge,
    float cytokine_level,
    float inflammation
);

/**
 * @brief Update cognitive fatigue
 *
 * Called periodically to update fatigue based on activity.
 * Triggers recovery if threshold exceeded.
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update
 * @param activity_level Current activity level (0.0-1.0)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_update_fatigue(
    genius_profiles_bridge_t* bridge,
    uint64_t delta_ms,
    float activity_level
);

/**
 * @brief Attempt to enter flow state
 *
 * Checks conditions and transitions to flow state if appropriate.
 *
 * @param bridge Bridge instance
 * @param challenge_level Current task challenge (0.0-1.0)
 * @param skill_level Current skill level (0.0-1.0)
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_enter_flow(
    genius_profiles_bridge_t* bridge,
    float challenge_level,
    float skill_level
);

/**
 * @brief Exit flow state
 * @param bridge Bridge instance
 * @param reason Reason for exit
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_exit_flow(
    genius_profiles_bridge_t* bridge,
    const char* reason
);

/* ============================================================================
 * TRAINING INTEGRATION
 * ============================================================================ */

/**
 * @brief Perform training step on active profile
 *
 * Computes gradients and updates profile parameters based on
 * performance feedback.
 *
 * @param bridge Bridge instance
 * @param loss Current loss value
 * @param gradients Gradient array
 * @param gradient_count Number of gradients
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_training_step(
    genius_profiles_bridge_t* bridge,
    float loss,
    const float* gradients,
    uint32_t gradient_count
);

/**
 * @brief Apply STDP learning to profile
 *
 * Updates profile based on spike-timing dependent plasticity.
 *
 * @param bridge Bridge instance
 * @param pre_spike_time Pre-synaptic spike time
 * @param post_spike_time Post-synaptic spike time
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_apply_stdp(
    genius_profiles_bridge_t* bridge,
    uint64_t pre_spike_time,
    uint64_t post_spike_time
);

/* ============================================================================
 * EIDETIC MEMORY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Apply eidetic memory configuration to all memory systems
 *
 * Configures all connected memory systems with eidetic enhancements
 * based on the active profile's eidetic configuration.
 *
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_apply_eidetic(genius_profiles_bridge_t* bridge);

/**
 * @brief Get eidetic configuration for current profile
 * @param bridge Bridge instance
 * @return Eidetic configuration or NULL if no active profile
 */
const eidetic_memory_config_t* genius_profiles_get_eidetic_config(
    const genius_profiles_bridge_t* bridge
);

/* ============================================================================
 * EXCEPTION HANDLING
 * ============================================================================ */

/**
 * @brief Raise exception and present to immune system
 *
 * Creates exception with genius-specific epitope and optionally
 * presents to immune system for response.
 *
 * @param bridge Bridge instance
 * @param error Error code
 * @param message Error message
 * @return Created exception (caller must destroy)
 */
nimcp_exception_t* genius_profiles_raise_exception(
    genius_profiles_bridge_t* bridge,
    genius_error_t error,
    const char* message
);

/* ============================================================================
 * BIO-ASYNC MESSAGE HANDLING
 * ============================================================================ */

/**
 * @brief Handle incoming bio-async message
 *
 * Internal handler registered with bio-async router.
 * Processes all genius profile message types.
 *
 * @param bridge Bridge instance
 * @param message Incoming message
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_handle_message(
    genius_profiles_bridge_t* bridge,
    const void* message,
    size_t message_size
);

/**
 * @brief Send bio-async message
 *
 * Sends message through bio-async router to target module.
 *
 * @param bridge Bridge instance
 * @param msg_type Message type
 * @param target_module Target module ID
 * @param data Message data
 * @param data_len Data length
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_send_message(
    genius_profiles_bridge_t* bridge,
    genius_bio_message_t msg_type,
    uint32_t target_module,
    const void* data,
    size_t data_len
);

/* ============================================================================
 * HEALTH AGENT INTEGRATION
 * ============================================================================ */

/**
 * @brief Send heartbeat to health agent
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_bridge_heartbeat(genius_profiles_bridge_t* bridge);

/**
 * @brief Start health agent monitoring
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_start_health_agent(genius_profiles_bridge_t* bridge);

/**
 * @brief Stop health agent monitoring
 * @param bridge Bridge instance
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_stop_health_agent(genius_profiles_bridge_t* bridge);

/* ============================================================================
 * MESH COORDINATION
 * ============================================================================ */

/**
 * @brief Propose profile change through mesh consensus
 *
 * Initiates consensus protocol for profile activation/change.
 *
 * @param bridge Bridge instance
 * @param type Proposed genius type
 * @param strength Proposed activation strength
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_mesh_propose(
    genius_profiles_bridge_t* bridge,
    genius_type_t type,
    float strength
);

/**
 * @brief Endorse pending mesh proposal
 * @param bridge Bridge instance
 * @param tx Transaction to endorse
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_mesh_endorse(
    genius_profiles_bridge_t* bridge,
    struct mesh_transaction* tx
);

/* ============================================================================
 * QUANTUM OPTIMIZATION
 * ============================================================================ */

/**
 * @brief Optimize profile parameters using quantum annealing
 *
 * Uses quantum algorithms to find optimal parameter configuration.
 *
 * @param bridge Bridge instance
 * @param objective Objective function values
 * @param objective_len Objective array length
 * @return GENIUS_ERROR_SUCCESS or error code
 */
genius_error_t genius_profiles_quantum_optimize(
    genius_profiles_bridge_t* bridge,
    const float* objective,
    size_t objective_len
);

/* ============================================================================
 * BRAIN CREATION HELPERS
 * ============================================================================ */

/**
 * @brief Create brain with genius profile
 *
 * Creates a new brain instance configured with the specified genius profile.
 *
 * @param type Genius type
 * @return Brain instance or NULL on failure
 */
brain_t genius_brain_create(genius_type_t type);

/**
 * @brief Create brain with genius profile (extended version)
 *
 * Creates a new brain instance configured with the specified genius profile.
 * Supports test_mode for faster brain creation via lazy initialization.
 *
 * @param type Genius type
 * @param test_mode When true, enables lazy initialization of heavy subsystems
 *                  (5-10x faster creation). Skips: ethics, ToM, perception,
 *                  glial networks, neuromodulators, cortical columns, etc.
 * @return Brain instance or NULL on failure
 */
brain_t genius_brain_create_ex(genius_type_t type, bool test_mode);

/**
 * @brief Create hemispheric brain with genius profile
 *
 * Creates a two-hemisphere brain with lateralization configured
 * according to the genius profile.
 *
 * @param type Genius type
 * @return Hemispheric brain instance or NULL on failure
 */
hemispheric_brain_t* genius_hemispheric_brain_create(genius_type_t type);

/**
 * @brief Create hemispheric brain with genius profile (extended version)
 *
 * Creates a two-hemisphere brain with lateralization configured
 * according to the genius profile. Supports test_mode for faster creation.
 *
 * @param type Genius type
 * @param test_mode When true, uses smaller brain size for faster creation.
 *                  Note: Full lazy initialization is not yet supported for
 *                  hemispheric brains. Use genius_brain_create_ex() for full
 *                  lazy init support with standard brains.
 * @return Hemispheric brain instance or NULL on failure
 */
hemispheric_brain_t* genius_hemispheric_brain_create_ex(genius_type_t type, bool test_mode);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Validate genius type
 * @param type Type to validate
 * @return true if valid, false otherwise
 */
static inline bool genius_type_is_valid(genius_type_t type) {
    return type >= 0 && type < GENIUS_TYPE_COUNT;
}

/**
 * @brief Get timestamp in milliseconds
 * @return Current timestamp
 */
uint64_t genius_profiles_get_timestamp_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_PROFILES_H */
