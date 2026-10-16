/**
 * @file nimcp_swarm_brain_local.h
 * @brief Swarm Brain Local Instantiation - Per-Agent Brain Management
 *
 * WHAT: Manages local brain instances for each swarm agent with distributed learning
 * WHY:  Enables each agent to have its own local brain while maintaining swarm coherence
 * HOW:  Per-agent brain allocation, weight synchronization, divergence tracking
 *
 * ARCHITECTURE:
 *
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │          Swarm Brain Manager                                 │
 *   │                                                              │
 *   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
 *   │  │  Agent 1     │  │  Agent 2     │  │  Agent N     │      │
 *   │  │  ┌────────┐  │  │  ┌────────┐  │  │  ┌────────┐  │      │
 *   │  │  │ Brain  │  │  │  │ Brain  │  │  │  │ Brain  │  │      │
 *   │  │  │Instance│  │  │  │Instance│  │  │  │Instance│  │      │
 *   │  │  └────────┘  │  │  └────────┘  │  │  └────────┘  │      │
 *   │  │  Weights     │  │  Weights     │  │  Weights     │      │
 *   │  │  Divergence  │  │  Divergence  │  │  Divergence  │      │
 *   │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
 *   │         │                 │                 │              │
 *   │         └─────────────────┴─────────────────┘              │
 *   │                           │                                │
 *   │                  ┌────────▼────────┐                       │
 *   │                  │ Weight Consensus │                      │
 *   │                  │    Aggregation   │                      │
 *   │                  └─────────────────┘                       │
 *   │                           │                                │
 *   │                  ┌────────▼────────┐                       │
 *   │                  │  Bio-Async      │                       │
 *   │                  │  Integration    │                       │
 *   │                  └─────────────────┘                       │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Per-agent brain allocation with configurable sizes
 * - Local learning and processing
 * - Weight synchronization across swarm
 * - Divergence detection and management
 * - Consensus weight aggregation
 * - Bio-async message passing
 *
 * USAGE EXAMPLE:
 * ```c
 * // Configure swarm brain manager
 * swarm_brain_config_t config = {
 *     .default_brain_size = 100,
 *     .max_local_neurons = 500,
 *     .sync_interval_ms = 1000,
 *     .divergence_threshold = 0.3,
 *     .enable_weight_sharing = true,
 *     .enable_bio_async = true
 * };
 *
 * // Create manager
 * swarm_brain_manager_t* mgr = swarm_brain_manager_create(&config);
 *
 * // Create brain for agent
 * swarm_brain_create_for_agent(mgr, agent_id, 100);
 *
 * // Local learning
 * swarm_brain_local_learn(mgr, agent_id, input, 10, target, 5);
 *
 * // Synchronize with swarm
 * swarm_brain_local_sync_weights(mgr, agent_id);
 *
 * // Check divergence
 * float divergence = swarm_brain_get_divergence(mgr, agent_id);
 *
 * // Cleanup
 * swarm_brain_manager_destroy(mgr);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_BRAIN_LOCAL_H
#define NIMCP_SWARM_BRAIN_LOCAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "nimcp.h"
#include "core/brain/nimcp_brain.h"  // For brain_size_t

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/** Forward declare brain_t type */
typedef struct brain_struct* brain_t;

/** Opaque swarm brain manager handle */
typedef struct swarm_brain_manager swarm_brain_manager_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of agents that can be managed */
#define SWARM_BRAIN_MAX_AGENTS 256

/** Default brain size (neurons) */
#define SWARM_BRAIN_DEFAULT_SIZE 100

/** Default sync interval (ms) */
#define SWARM_BRAIN_DEFAULT_SYNC_INTERVAL 1000

/** Default divergence threshold */
#define SWARM_BRAIN_DEFAULT_DIVERGENCE_THRESHOLD 0.3f

/** Maximum neurons per agent brain */
#define SWARM_BRAIN_MAX_NEURONS 1000

//=============================================================================
// Drone Role Types (Asymmetric Brain Instantiation)
//=============================================================================

/**
 * @brief Drone role types for asymmetric brain instantiation
 *
 * WHAT: Defines specialized roles for swarm agents
 * WHY:  Memory optimization (89% savings) via role-specific brain templates
 * HOW:  Each role has tailored brain features and neuron counts
 *
 * MEMORY SAVINGS (100-drone swarm example):
 * - Uniform (all MEDIUM): 100 × 10MB = 1000MB
 * - Asymmetric: (10×10MB) + (60×1MB) + (10×3MB) + (15×0.5MB) + (5×5MB) = ~200MB
 *
 * ROLE DISTRIBUTION RECOMMENDATIONS:
 * - Scout:       10% - Navigation and exploration
 * - Worker:      60% - Basic task execution
 * - Coordinator:  5% - Swarm coordination
 * - Sensor:      10% - Environmental perception
 * - Guardian:    10% - Security and defense
 * - Relay:        5% - Communication relay
 */
typedef enum {
    DRONE_ROLE_SCOUT,       /**< Navigation + spatial memory (SMALL brain, 500 neurons) */
    DRONE_ROLE_WORKER,      /**< Basic motor control (MICRO brain, 25 neurons) */
    DRONE_ROLE_COORDINATOR, /**< Swarm coordination (MEDIUM brain, 1000 neurons) */
    DRONE_ROLE_SENSOR,      /**< Environmental perception (TINY brain, 100 neurons) */
    DRONE_ROLE_GUARDIAN,    /**< Security + threat detection (SMALL brain, 500 neurons) */
    DRONE_ROLE_RELAY,       /**< Communication relay (MICRO brain, 25 neurons) */
    DRONE_ROLE_CUSTOM,      /**< Custom configuration */
    DRONE_ROLE_COUNT        /**< Number of predefined roles */
} drone_role_t;

/**
 * @brief Brain template for drone roles
 *
 * Specifies which brain features are enabled for each role.
 * This allows per-role memory optimization.
 */
typedef struct {
    drone_role_t role;              /**< Role type */
    brain_size_t brain_size;        /**< Brain size preset */
    uint32_t neuron_override;       /**< Custom neuron count (0 = use preset) */

    // Feature enable flags (matched to brain_config_t)
    bool enable_visual_cortex;      /**< Visual processing */
    bool enable_audio_cortex;       /**< Audio processing */
    bool enable_speech_cortex;      /**< Speech processing */
    bool enable_working_memory;     /**< Short-term memory buffer */
    bool enable_global_workspace;   /**< Conscious awareness */
    bool enable_theory_of_mind;     /**< Social cognition */
    bool enable_ethics;             /**< Ethical reasoning */
    bool enable_curiosity;          /**< Exploration drive */
    bool enable_mirror_neurons;     /**< Imitation learning */
    bool enable_executive_control;  /**< Task switching */
    bool enable_consolidation;      /**< Memory consolidation */
    bool enable_glial;              /**< Glial integration */
    bool enable_cortical_columns;   /**< Cortical architecture */
    bool enable_predictive;         /**< Predictive processing */
    bool enable_bio_async;          /**< Bio-async messaging */
    bool minimal_mode;              /**< Minimal mode flag */
    bool lazy_init_mode;            /**< Lazy initialization */

    // Performance hints
    float max_inference_time_ms;    /**< Target inference latency */
    uint32_t max_memory_kb;         /**< Target memory budget */
} drone_brain_template_t;

//=============================================================================
// Bio-Async Message Types
//=============================================================================

/** Bio-async message types for swarm brain */
typedef enum {
    BIO_MSG_BRAIN_SYNCED = 0x0C00,          /**< Brain weights synchronized */
    BIO_MSG_BRAIN_DIVERGENCE = 0x0C01,      /**< Brain divergence detected */
    BIO_MSG_BRAIN_WEIGHTS_REQUEST = 0x0C02, /**< Request consensus weights */
    BIO_MSG_BRAIN_WEIGHTS_UPDATE = 0x0C03,  /**< Consensus weights update */
    BIO_MSG_BRAIN_AGENT_JOINED = 0x0C04,    /**< New agent joined swarm */
    BIO_MSG_BRAIN_AGENT_LEFT = 0x0C05       /**< Agent left swarm */
} swarm_brain_bio_msg_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Per-agent brain instance
 *
 * Tracks individual agent's brain, weights, and synchronization state
 */
typedef struct {
    uint32_t agent_id;              /**< Unique agent identifier */
    nimcp_brain_t brain;            /**< Local brain instance */
    uint32_t brain_size;            /**< Number of neurons */
    float* local_weights;           /**< Agent-specific weights */
    uint32_t num_weights;           /**< Number of weights */
    uint64_t last_sync_ms;          /**< Last sync timestamp */
    float divergence_score;         /**< Divergence from consensus (0-1) */
    bool active;                    /**< Agent is active */
    drone_role_t role;              /**< Agent's drone role (for asymmetric training) */
} agent_brain_t;

/**
 * @brief Swarm brain manager configuration
 */
typedef struct {
    uint32_t default_brain_size;    /**< Default neurons per brain */
    uint32_t max_local_neurons;     /**< Maximum neurons per agent */
    float sync_interval_ms;         /**< Sync interval (default: 1000ms) */
    float divergence_threshold;     /**< Divergence alert threshold (0-1) */
    bool enable_weight_sharing;     /**< Enable weight sharing between agents */
    bool enable_bio_async;          /**< Enable bio-async integration */
    bool test_mode;                 /**< Skip actual brain creation (for fast unit tests) */
} swarm_brain_config_t;

/**
 * @brief Swarm brain statistics
 */
typedef struct {
    uint32_t num_agents;            /**< Number of active agents */
    uint32_t total_neurons;         /**< Total neurons across all agents */
    uint64_t sync_count;            /**< Total synchronizations performed */
    float avg_divergence;           /**< Average divergence across agents */
    float max_divergence;           /**< Maximum divergence */
    uint32_t divergent_agents;      /**< Number of divergent agents */
    uint64_t uptime_ms;             /**< Manager uptime */
} swarm_brain_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create swarm brain manager
 *
 * WHAT: Initializes manager for coordinating per-agent brains
 * WHY:  Central coordination point for distributed brain instances
 * HOW:  Allocates state, sets up bio-async, initializes consensus tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Manager handle or NULL on failure
 */
swarm_brain_manager_t* swarm_brain_manager_create(const swarm_brain_config_t* config);

/**
 * @brief Destroy swarm brain manager
 *
 * WHAT: Cleans up all agent brains and manager state
 * WHY:  Release all resources and disconnect from bio-async
 * HOW:  Destroys all agent brains, frees memory, unregisters from bio-async
 *
 * @param mgr Manager handle (NULL safe)
 */
void swarm_brain_manager_destroy(swarm_brain_manager_t* mgr);

//=============================================================================
// Agent Brain Management
//=============================================================================

/**
 * @brief Create brain for specific agent
 *
 * WHAT: Allocates and initializes brain instance for agent
 * WHY:  Each agent needs its own local brain for processing
 * HOW:  Creates brain, allocates weights, registers with manager
 *
 * @param mgr Manager handle
 * @param agent_id Unique agent identifier
 * @param brain_size Number of neurons (0 = use default)
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_create_for_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    uint32_t brain_size
);

/**
 * @brief Create brain for agent with role-based template
 *
 * WHAT: Creates agent brain optimized for specific drone role
 * WHY:  Memory optimization via role-specific feature sets (up to 89% savings)
 * HOW:  Applies role template to configure brain features
 *
 * EXAMPLE:
 * ```c
 * // Create a scout drone with navigation-optimized brain
 * swarm_brain_create_for_agent_with_role(mgr, agent_id, DRONE_ROLE_SCOUT);
 *
 * // Create a worker drone with minimal brain
 * swarm_brain_create_for_agent_with_role(mgr, agent_id, DRONE_ROLE_WORKER);
 *
 * // Create a coordinator with full cognitive capabilities
 * swarm_brain_create_for_agent_with_role(mgr, agent_id, DRONE_ROLE_COORDINATOR);
 * ```
 *
 * @param mgr Manager handle
 * @param agent_id Unique agent identifier
 * @param role Drone role for brain template selection
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_create_for_agent_with_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role
);

/**
 * @brief Create brain for agent with custom template
 *
 * WHAT: Creates agent brain with custom feature configuration
 * WHY:  Full control over brain features for specialized use cases
 * HOW:  Applies custom template to configure brain
 *
 * @param mgr Manager handle
 * @param agent_id Unique agent identifier
 * @param template Custom brain template
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_create_for_agent_with_template(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const drone_brain_template_t* templ
);

/**
 * @brief Get default template for drone role
 *
 * WHAT: Returns optimized brain template for specified role
 * WHY:  Easy access to predefined role configurations
 * HOW:  Returns static template with role-specific settings
 *
 * @param role Drone role
 * @return Template for role (or default if invalid role)
 */
drone_brain_template_t swarm_brain_get_role_template(drone_role_t role);

/**
 * @brief Get role name as string
 *
 * @param role Drone role
 * @return Human-readable role name
 */
const char* swarm_brain_role_name(drone_role_t role);

/**
 * @brief Destroy brain for specific agent
 *
 * WHAT: Removes and cleans up agent's brain instance
 * WHY:  Agent leaving swarm or resource cleanup
 * HOW:  Destroys brain, frees weights, removes from manager
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_destroy_for_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

/**
 * @brief Get brain instance for agent
 *
 * WHAT: Retrieves brain handle for direct access
 * WHY:  Allow direct brain operations if needed
 * HOW:  Looks up agent, returns brain handle
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return Brain handle or NULL if not found
 */
nimcp_brain_t swarm_brain_get(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

//=============================================================================
// Local Learning and Processing
//=============================================================================

/**
 * @brief Perform local learning on agent's brain
 *
 * WHAT: Trains agent's brain on local data
 * WHY:  Each agent learns from its own experiences
 * HOW:  Forwards to brain learning API, tracks weight changes
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @param input Input data
 * @param input_size Input size
 * @param target Target output
 * @param target_size Target size
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_local_learn(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size
);

/**
 * @brief Process input through agent's brain
 *
 * WHAT: Runs inference on agent's local brain
 * WHY:  Local processing for agent decisions
 * HOW:  Forwards to brain inference, returns output
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @param input Input data
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output size (in/out)
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_local_process(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t* output_size
);

//=============================================================================
// Weight Synchronization
//=============================================================================

/**
 * @brief Synchronize agent's weights with swarm consensus
 *
 * WHAT: Updates agent's weights based on swarm consensus
 * WHY:  Maintain coherence across distributed brains
 * HOW:  Averages with consensus weights, updates divergence
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_local_sync_weights(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

/**
 * @brief Synchronize all agents with consensus
 *
 * WHAT: Performs full swarm synchronization
 * WHY:  Periodic coherence maintenance
 * HOW:  Syncs each active agent sequentially
 *
 * @param mgr Manager handle
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_sync_all(swarm_brain_manager_t* mgr);

/**
 * @brief Get consensus weights across all agents
 *
 * WHAT: Computes averaged weights from all active agents
 * WHY:  Calculate swarm consensus for synchronization
 * HOW:  Averages weights across all active brains
 *
 * @param mgr Manager handle
 * @param weights Output weights array (allocated by function)
 * @param num_weights Output weight count
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free weights array with nimcp_free()
 */
int swarm_brain_get_consensus_weights(
    swarm_brain_manager_t* mgr,
    float** weights,
    uint32_t* num_weights
);

//=============================================================================
// Divergence Detection
//=============================================================================

/**
 * @brief Get divergence score for specific agent
 *
 * WHAT: Returns how different agent's weights are from consensus
 * WHY:  Track individual agent coherence with swarm
 * HOW:  Computes distance between local and consensus weights
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return Divergence score (0-1), or -1 on error
 */
float swarm_brain_get_divergence(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

/**
 * @brief Get list of divergent agents
 *
 * WHAT: Returns agents exceeding divergence threshold
 * WHY:  Identify agents needing synchronization
 * HOW:  Scans all agents, collects IDs above threshold
 *
 * @param mgr Manager handle
 * @param agents Output array of agent IDs (allocated by function)
 * @param count Output count of divergent agents
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free agents array with nimcp_free()
 */
int swarm_brain_get_divergent_agents(
    swarm_brain_manager_t* mgr,
    uint32_t** agents,
    uint32_t* count
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get swarm brain statistics
 *
 * @param mgr Manager handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_local_get_stats(
    swarm_brain_manager_t* mgr,
    swarm_brain_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param mgr Manager handle
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_local_reset_stats(swarm_brain_manager_t* mgr);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default swarm brain configuration
 *
 * @return Default configuration structure
 */
swarm_brain_config_t swarm_brain_local_default_config(void);

/**
 * @brief Check if agent has active brain
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return true if agent has active brain, false otherwise
 */
bool swarm_brain_has_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

/**
 * @brief Get number of active agents
 *
 * @param mgr Manager handle
 * @return Number of active agents
 */
uint32_t swarm_brain_get_agent_count(swarm_brain_manager_t* mgr);

/**
 * @brief Get list of all active agent IDs
 *
 * @param mgr Manager handle
 * @param agents Output array of agent IDs (allocated by function)
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free agents array with nimcp_free()
 */
int swarm_brain_get_all_agents(
    swarm_brain_manager_t* mgr,
    uint32_t** agents,
    uint32_t* count
);

//=============================================================================
// Role-Based Training (Asymmetric Swarm Learning)
//=============================================================================

/**
 * @brief Role-based training configuration
 *
 * WHAT: Specifies training parameters specific to each drone role
 * WHY:  Different roles need different learning strategies
 * HOW:  Applied per-role during training operations
 */
typedef struct {
    drone_role_t role;              /**< Target role */
    float learning_rate;            /**< Role-specific learning rate */
    uint32_t batch_size;            /**< Training batch size */
    bool use_replay_buffer;         /**< Enable experience replay */
    uint32_t replay_buffer_size;    /**< Size of replay buffer */
    bool sync_within_role;          /**< Only sync with same role agents */
    float sync_strength;            /**< How much to weight consensus (0-1) */
    bool enable_transfer_learning;  /**< Allow learning from other roles */
    drone_role_t transfer_from;     /**< Role to transfer knowledge from */
    float transfer_weight;          /**< Weight for transferred knowledge */
} role_training_config_t;

/**
 * @brief Get default training config for role
 *
 * WHAT: Returns optimized training configuration for specified role
 * WHY:  Each role has different training requirements
 * HOW:  Returns predefined configs based on role characteristics
 *
 * ROLE-SPECIFIC DEFAULTS:
 * - Scout: Higher learning rate, exploration-focused
 * - Worker: Lower learning rate, stable task execution
 * - Coordinator: Moderate learning rate, balanced approach
 * - Sensor: Fast adaptation to environmental changes
 * - Guardian: Conservative learning, stability priority
 * - Relay: Minimal learning, mostly fixed behavior
 *
 * @param role Drone role
 * @return Training configuration for role
 */
role_training_config_t swarm_brain_get_role_training_config(drone_role_t role);

/**
 * @brief Train agent using role-specific configuration
 *
 * WHAT: Trains an agent's brain with role-appropriate parameters
 * WHY:  Role-specific training improves specialization
 * HOW:  Applies role training config to learning process
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @param role Agent's role
 * @param input Training input
 * @param input_size Input size
 * @param target Target output
 * @param target_size Target size
 * @param config Role training config (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_train_with_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size,
    const role_training_config_t* config
);

/**
 * @brief Synchronize weights within a specific role group
 *
 * WHAT: Syncs weights only among agents with the same role
 * WHY:  Role-specific weight sharing preserves specialization
 * HOW:  Computes consensus only within role group, applies to each agent
 *
 * EXAMPLE:
 * ```c
 * // Only sync scout drones together
 * swarm_brain_sync_role_group(mgr, DRONE_ROLE_SCOUT);
 * ```
 *
 * @param mgr Manager handle
 * @param role Role to synchronize
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_sync_role_group(
    swarm_brain_manager_t* mgr,
    drone_role_t role
);

/**
 * @brief Get agents by role
 *
 * WHAT: Returns list of agent IDs for specified role
 * WHY:  Enable role-specific operations
 * HOW:  Scans agents, filters by role
 *
 * @param mgr Manager handle
 * @param role Role to filter by
 * @param agents Output array of agent IDs (allocated by function)
 * @param count Output count
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free agents array with nimcp_free()
 */
int swarm_brain_get_agents_by_role(
    swarm_brain_manager_t* mgr,
    drone_role_t role,
    uint32_t** agents,
    uint32_t* count
);

/**
 * @brief Transfer knowledge between roles
 *
 * WHAT: Enables one role to learn from another's experiences
 * WHY:  Scouts may benefit from Guardian threat knowledge, etc.
 * HOW:  Weighted transfer of learned representations
 *
 * TRANSFER RECOMMENDATIONS:
 * - Scout → Worker: Navigation patterns
 * - Sensor → Guardian: Threat detection patterns
 * - Coordinator → All: Coordination strategies
 *
 * @param mgr Manager handle
 * @param to_agent Target agent ID
 * @param from_role Source role to transfer from
 * @param transfer_weight How much to weight transferred knowledge (0-1)
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_transfer_role_knowledge(
    swarm_brain_manager_t* mgr,
    uint32_t to_agent,
    drone_role_t from_role,
    float transfer_weight
);

/**
 * @brief Set agent role (for tracking)
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @param role New role for agent
 * @return NIMCP_SUCCESS or error code
 */
int swarm_brain_set_agent_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role
);

/**
 * @brief Get agent role
 *
 * @param mgr Manager handle
 * @param agent_id Agent identifier
 * @return Agent's role or DRONE_ROLE_CUSTOM if not found
 */
drone_role_t swarm_brain_get_agent_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_BRAIN_LOCAL_H */
