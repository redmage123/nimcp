/**
 * @file nimcp_mirror_neurons.c
 * @brief Mirror Neuron System Implementation
 * @version 1.0.0
 * @date 2025-01-09
 *
 * Implementation follows NIMCP coding standards:
 * - Proper WHAT/WHY/HOW documentation
 * - Error validation on all inputs
 * - Memory safety and leak prevention
 * - Thread-safety where applicable
 * - Performance optimization
 */

#include "cognitive/nimcp_mirror_neurons.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: mirror activation events */
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/mirror_neurons/nimcp_mirror_substrate.h"  // Substrate integration (Phase 10.11.2)
#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"       // STDP learning (Phase 10.11.4)
#include "cognitive/mirror_neurons/nimcp_mirror_resonance.h"  // Motor resonance (Phase 10.11.5)
#include "cognitive/mirror_neurons/nimcp_mirror_hierarchy.h"  // Hierarchical goals (Phase 10.11.6)
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "core/brain/nimcp_brain.h"  // Brain reference
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"  // For error codes

// SNN and Plasticity bridge integration
#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "mirror_neurons"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(mirror_neurons, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Logging macros
#define MIRROR_LOG_ERROR NIMCP_LOGGING_ERROR
#define MIRROR_LOG_WARN NIMCP_LOGGING_WARN
#define MIRROR_LOG_INFO NIMCP_LOGGING_INFO

/* P3-COG-06: Named constant for maximum action features per neuron */
#define MIRROR_MAX_FEATURES 32

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Single mirror neuron unit
 *
 * WHAT: Represents one mirror neuron with dual observation/execution pathways
 * WHY:  Enable shared representation for observed and executed actions
 * HOW:  Combines cognitive state with optional biological substrate backing
 */
typedef struct {
    uint32_t neuron_id;                /**< Unique neuron identifier */
    uint32_t action_id;                /**< Action this neuron represents */

    // Dual pathways
    float observation_activation;      /**< Current observation activation */
    float execution_activation;        /**< Current execution activation */

    // Learning state
    float association_weight;          /**< Obs-exec association strength */
    uint32_t observation_count;        /**< Times activated by observation */
    uint32_t execution_count;          /**< Times activated by execution */

    // Feature representation
    /* P3-COG-06: Named constant for max features */
    float action_features[MIRROR_MAX_FEATURES];  /**< Learned action features */
    uint32_t num_features;             /**< Number of features */

    // Temporal state
    uint64_t last_observation_time;    /**< Last observation activation */
    uint64_t last_execution_time;      /**< Last execution activation */

    // Phase 10.11.2: Substrate integration
    mirror_substrate_backing_t* substrate; /**< Biological substrate backing (NULL = abstract mode) */
    bool has_substrate;                /**< True if substrate backing is active */

} mirror_neuron_unit_t;

/**
 * @brief Action-to-neuron mapping entry
 *
 * WHAT: Maps action IDs to corresponding mirror neuron populations
 * WHY:  Enable fast lookup of neurons for a given action
 */
typedef struct {
    uint32_t action_id;
    char action_name[NIMCP_ID_BUFFER_SIZE];
    uint32_t* neuron_indices;          /**< Array of neuron indices */
    uint32_t num_neurons;              /**< Number of neurons for this action */
    uint32_t capacity;                 /**< Allocated capacity */

    // Statistics
    uint32_t total_observations;
    uint32_t total_executions;
    float avg_similarity;              /**< Average obs-exec similarity */

} action_mapping_t;

/**
 * @brief Observed agent tracking
 *
 * WHAT: Track information about observed agents
 * WHY:  Enable multi-agent learning and agent-specific adaptation
 */
typedef struct {
    uint32_t agent_id;
    uint32_t observation_count;
    uint64_t last_observation_time;
    float trust_score;                 /**< Agent reliability (0.0-1.0) */
} agent_info_t;

/**
 * @brief Main mirror neuron system structure
 *
 * WHAT: Complete mirror neuron system state
 * WHY:  Encapsulate all data for observation-based learning
 * HOW:  Combines cognitive infrastructure with biological substrate integration
 */
struct mirror_neurons_system {
    // Configuration
    mirror_neuron_config_t config;

    // Neuron population
    mirror_neuron_unit_t* neurons;
    uint32_t num_neurons;

    // Action mappings
    action_mapping_t* actions;
    uint32_t num_actions;
    uint32_t actions_capacity;

    // Agent tracking
    agent_info_t* agents;
    uint32_t num_agents;
    uint32_t agents_capacity;

    // Statistics (atomic for thread-safety)
    mirror_neuron_stats_t stats;

    // Integration handles
    void* working_memory;              /**< Working memory system */
    void* theory_of_mind;              /**< Theory of mind system */
    void* predictive_network;          /**< Predictive network */
    void* glial_integration;           /**< Glial cell integration (Phase 10.11.1) */
    brain_t brain;                     /**< Brain reference for neuromodulation */

    // Temporal state
    uint64_t creation_time;
    uint64_t last_update_time;

    // Memory management
    bool initialized;

    // P2-COG-08: Thread safety mutex
    nimcp_mutex_t* mutex;

    // Phase 10.11.2: Substrate integration
    mirror_substrate_config_t substrate_config;   /**< Substrate configuration */
    mirror_substrate_pool_t* substrate_pool;      /**< Memory pool for substrate backings */
    bool substrate_enabled;                       /**< True if substrate mode active */

    // Phase 10.11.2: Network integration handles
    void* axon_network;                /**< Axon network for propagation delays */
    void* dendrite_network;            /**< Dendrite network for spine plasticity */
    void* myelin_network;              /**< Myelin sheath network for myelination */

    // Phase 10.11.4-6: Enhancement systems
    void* stdp_system;                 /**< STDP learning system */
    void* resonance_system;            /**< Motor resonance system */
    void* hierarchy_system;            /**< Goal-motor hierarchy system */
    bool stdp_enabled;                 /**< True if STDP enabled */
    bool resonance_enabled;            /**< True if resonance enabled */
    bool hierarchy_enabled;            /**< True if hierarchy enabled */

    // Bio-async integration
    bio_module_context_t bio_ctx;      /**< Bio-async module context */
    bool bio_async_enabled;            /**< Bio-async registration status */

    // SNN and Plasticity bridge integration
    mirror_snn_bridge_t* snn_bridge;           /**< SNN bridge for spike-based computation */
    mirror_plasticity_bridge_t* plasticity_bridge; /**< Plasticity bridge for learning rules */
    bool bridges_enabled;                      /**< True if bridges are active */
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float compute_feature_similarity(const float* f1, const float* f2, uint32_t n);
static uint32_t find_or_create_action(mirror_neurons_t mirror, const action_t* action);
static uint32_t find_or_create_agent(mirror_neurons_t mirror, uint32_t agent_id);
static void activate_neurons_for_action(mirror_neurons_t mirror, uint32_t action_idx,
                                       float strength, bool is_observation);
static void update_action_statistics(mirror_neurons_t mirror, uint32_t action_idx);


// Forward declarations for static functions (SRP split)
static float get_mirror_ach_modulation(mirror_neurons_t mirror);
static nimcp_error_t handle_mirror_activation( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static void bio_broadcast_mirror_fire(mirror_neurons_t mirror, uint32_t action_id, float activation);
static int mirror_neurons_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data );

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_mirror_neurons_part_accessors.c"  // 18 functions: accessors
#include "nimcp_mirror_neurons_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_mirror_neurons_part_helpers.c"  // 6 functions: helpers
#include "nimcp_mirror_neurons_part_processing.c"  // 6 functions: processing
#include "nimcp_mirror_neurons_part_core.c"  // 23 functions: core
#include "nimcp_mirror_neurons_part_io.c"  // 2 functions: io
