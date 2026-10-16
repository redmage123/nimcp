/**
 * @file nimcp_mesh_integration.h
 * @brief Central Mesh Network Integration Manager
 *
 * WHAT: Unified manager for integrating all NIMCP components into mesh network
 * WHY:  Single point of control for mesh network lifecycle and component registration
 * HOW:  Manages channels, adapters, endorsement policies, and component wiring
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                      MESH INTEGRATION MANAGER                                │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                        CHANNEL LAYER                                │    │
 * │  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐           │    │
 * │  │  │  SYSTEM   │ │   LEFT    │ │   RIGHT   │ │SUBCORTICAL│ │  GPU  │ │    │
 * │  │  │ Channel 0 │ │ Channel 1 │ │ Channel 2 │ │ Channel 3 │ │Chan 4 │ │    │
 * │  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └───────┘ │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                       ADAPTER REGISTRY                              │    │
 * │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │    │
 * │  │  │Amygdala │ │Hippocam │ │Thalamus │ │ Visual  │ │  Motor  │ ...   │    │
 * │  │  │ Adapter │ │ Adapter │ │ Adapter │ │ Adapter │ │ Adapter │       │    │
 * │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘       │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                    ENDORSEMENT POLICIES                             │    │
 * │  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                │    │
 * │  │  │ motor_command│ │ memory_store │ │  emergency   │ ...            │    │
 * │  │  │ motor_cortex │ │ hippocampus  │ │  amygdala    │                │    │
 * │  │  │ + cerebellum │ │ + PFC        │ │  (VETO)      │                │    │
 * │  │  └──────────────┘ └──────────────┘ └──────────────┘                │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                    ORDERING SERVICE (Raft)                          │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                    MSP (BBB + Immune Integration)                   │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_INTEGRATION_H
#define NIMCP_MESH_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "mesh/nimcp_mesh_cross_channel.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_integration mesh_integration_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum adapters per integration */
#define MESH_MAX_ADAPTERS               256

/** @brief Number of standard channels */
#define MESH_NUM_STANDARD_CHANNELS      5

/* ============================================================================
 * Standard Policy Names
 * ============================================================================ */

#define MESH_POLICY_COGNITIVE           "cognitive"
#define MESH_POLICY_MOTOR_COMMAND       "motor_command"
#define MESH_POLICY_MEMORY_STORE        "memory_store"
#define MESH_POLICY_SENSORY_FUSION      "sensory_fusion"
#define MESH_POLICY_EMERGENCY           "emergency"
#define MESH_POLICY_CROSS_HEMISPHERE    "cross_hemisphere"
#define MESH_POLICY_SECURITY            "security"
#define MESH_POLICY_GPU_BATCH           "gpu_batch"
#define MESH_POLICY_PLASTICITY          "plasticity"
#define MESH_POLICY_HOMEOSTASIS         "homeostasis"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Mesh integration configuration
 */
typedef struct mesh_integration_config {
    /* Registry configuration */
    size_t max_participants;              /**< Max participants across all channels */
    
    /* Channel configuration */
    size_t world_state_capacity;          /**< World state capacity per channel */
    uint32_t gossip_rounds;               /**< Gossip rounds per update */
    float convergence_threshold;          /**< FEP convergence threshold */
    
    /* Ordering configuration */
    size_t ordering_batch_size;           /**< Ordering batch size */
    uint64_t ordering_batch_timeout_ms;   /**< Batch timeout */
    
    /* Coordinator pool configuration */
    size_t coordinators_per_channel;      /**< Coordinators per channel */
    float election_timeout_ms;            /**< Leader election timeout */
    
    /* MSP configuration */
    bool enable_bbb_integration;          /**< Integrate with BBB */
    bool enable_immune_integration;       /**< Integrate with immune system */
    
    /* Timing configuration */
    float base_interval_ms;               /**< Base timing interval */
    float jitter_amplitude_ms;            /**< Pink noise jitter */
    
    /* Logging */
    bool enable_logging;                  /**< Enable integration logging */
    
} mesh_integration_config_t;

/**
 * @brief Mesh integration statistics
 */
typedef struct mesh_integration_stats {
    size_t total_adapters;                /**< Total registered adapters */
    size_t adapters_by_channel[MESH_NUM_STANDARD_CHANNELS];
    
    uint64_t transactions_total;          /**< Total transactions */
    uint64_t transactions_committed;      /**< Committed transactions */
    uint64_t transactions_failed;         /**< Failed transactions */
    
    uint64_t beliefs_propagated;          /**< Total beliefs propagated */
    uint64_t endorsements_collected;      /**< Total endorsements */
    
    float avg_commit_latency_ms;          /**< Average commit latency */
    float system_free_energy;             /**< System-wide free energy */
    
} mesh_integration_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default integration configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_default_config(mesh_integration_config_t* config);

/**
 * @brief Create mesh integration manager
 *
 * WHAT: Allocate and initialize complete mesh network infrastructure
 * WHY:  Single call to set up all mesh components
 *
 * Creates:
 * - Participant registry
 * - All standard channels (SYSTEM, LEFT, RIGHT, SUBCORTICAL, GPU)
 * - Coordinator pools for each channel
 * - Ordering service
 * - MSP with BBB/immune integration
 * - Endorsement collector with standard policies
 * - Cross-channel router
 *
 * @param config Configuration (NULL for defaults)
 * @return Integration manager or NULL on failure
 */
mesh_integration_t* mesh_integration_create(
    const mesh_integration_config_t* config
);

/**
 * @brief Destroy mesh integration manager
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_integration_destroy(mesh_integration_t* integration);

/* ============================================================================
 * Component Access API
 * ============================================================================ */

/**
 * @brief Get participant registry
 */
mesh_participant_registry_t* mesh_integration_get_registry(
    mesh_integration_t* integration
);

/**
 * @brief Get channel by ID
 */
mesh_channel_t* mesh_integration_get_channel(
    mesh_integration_t* integration,
    mesh_channel_id_t channel_id
);

/**
 * @brief Get ordering service
 */
mesh_ordering_service_t* mesh_integration_get_ordering(
    mesh_integration_t* integration
);

/**
 * @brief Get MSP
 */
mesh_msp_t* mesh_integration_get_msp(
    mesh_integration_t* integration
);

/**
 * @brief Get endorsement collector
 */
mesh_endorsement_collector_t* mesh_integration_get_endorsement_collector(
    mesh_integration_t* integration
);

/**
 * @brief Get cross-channel router
 */
mesh_cross_router_t mesh_integration_get_router(
    mesh_integration_t* integration
);

/**
 * @brief Get coordinator pool for channel
 */
mesh_coordinator_pool_t* mesh_integration_get_coordinator_pool(
    mesh_integration_t* integration,
    mesh_channel_id_t channel_id
);

/* ============================================================================
 * Adapter Registration API
 * ============================================================================ */

/**
 * @brief Register adapter with integration
 *
 * WHAT: Add adapter to integration and wire to appropriate channel/policies
 * WHY:  Simplified registration that handles all wiring automatically
 *
 * Automatically:
 * - Registers with participant registry
 * - Joins appropriate channel(s) based on category
 * - Adds to endorsement policies based on role
 *
 * @param integration Integration manager
 * @param adapter_base Adapter base (from MESH_ADAPTER_DEFINE)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_register_adapter(
    mesh_integration_t* integration,
    mesh_adapter_base_t* adapter_base
);

/**
 * @brief Unregister adapter from integration
 *
 * @param integration Integration manager
 * @param participant_id Adapter's participant ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_unregister_adapter(
    mesh_integration_t* integration,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get adapter by participant ID
 *
 * @param integration Integration manager
 * @param participant_id Participant ID
 * @return Adapter base or NULL if not found
 */
mesh_adapter_base_t* mesh_integration_get_adapter(
    mesh_integration_t* integration,
    mesh_participant_id_t participant_id
);

/* ============================================================================
 * Convenience Registration Functions
 * ============================================================================ */

/**
 * @brief Register a cognitive module
 *
 * @param integration Integration manager
 * @param module Module instance
 * @param name Module name
 * @param callbacks Module callbacks (optional)
 * @return Participant ID or 0 on failure
 */
mesh_participant_id_t mesh_integration_register_cognitive(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register a subcortical module
 */
mesh_participant_id_t mesh_integration_register_subcortical(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register a perception module
 */
mesh_participant_id_t mesh_integration_register_perception(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register a security module
 */
mesh_participant_id_t mesh_integration_register_security(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register a GPU module
 */
mesh_participant_id_t mesh_integration_register_gpu(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register a swarm module
 */
mesh_participant_id_t mesh_integration_register_swarm(
    mesh_integration_t* integration,
    void* module,
    const char* name,
    const mesh_adapter_callbacks_t* callbacks
);

/* ============================================================================
 * Special Component Registration
 * ============================================================================ */

/**
 * @brief Register amygdala with VETO role for emergency policy
 */
mesh_participant_id_t mesh_integration_register_amygdala(
    mesh_integration_t* integration,
    void* amygdala
);

/**
 * @brief Register hippocampus as REQUIRED for memory_store policy
 */
mesh_participant_id_t mesh_integration_register_hippocampus(
    mesh_integration_t* integration,
    void* hippocampus
);

/**
 * @brief Register thalamus as GATEWAY for cross-channel routing
 */
mesh_participant_id_t mesh_integration_register_thalamus(
    mesh_integration_t* integration,
    void* thalamus
);

/**
 * @brief Register motor cortex as REQUIRED for motor_command policy
 */
mesh_participant_id_t mesh_integration_register_motor_cortex(
    mesh_integration_t* integration,
    void* motor_cortex
);

/**
 * @brief Register cerebellum for GPU channel
 */
mesh_participant_id_t mesh_integration_register_cerebellum(
    mesh_integration_t* integration,
    void* cerebellum
);

/**
 * @brief Register PFC as COORDINATOR
 */
mesh_participant_id_t mesh_integration_register_prefrontal_cortex(
    mesh_integration_t* integration,
    void* pfc,
    bool is_left_hemisphere
);

/**
 * @brief Register basal ganglia for motor selection
 */
mesh_participant_id_t mesh_integration_register_basal_ganglia(
    mesh_integration_t* integration,
    void* basal_ganglia
);

/* ============================================================================
 * Update and Processing API
 * ============================================================================ */

/**
 * @brief Update all mesh components
 *
 * WHAT: Periodic update of all channels, gossip, ordering
 * WHY:  Drive mesh network processing forward
 *
 * @param integration Integration manager
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_update(
    mesh_integration_t* integration,
    uint64_t delta_ms
);

/**
 * @brief Process pending transactions
 *
 * @param integration Integration manager
 * @return Number of transactions processed
 */
size_t mesh_integration_process_transactions(
    mesh_integration_t* integration
);

/**
 * @brief Check if system has converged (low free energy across channels)
 *
 * @param integration Integration manager
 * @return true if all channels converged
 */
bool mesh_integration_has_converged(
    const mesh_integration_t* integration
);

/**
 * @brief Get system-wide free energy
 *
 * @param integration Integration manager
 * @return Aggregate free energy across channels
 */
float mesh_integration_get_free_energy(
    const mesh_integration_t* integration
);

/* ============================================================================
 * Transaction Submission API
 * ============================================================================ */

/**
 * @brief Submit transaction through integration
 *
 * WHAT: Submit transaction for endorsement, ordering, and commit
 * WHY:  Simplified transaction submission with automatic policy selection
 *
 * @param integration Integration manager
 * @param tx Transaction to submit
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_submit_transaction(
    mesh_integration_t* integration,
    mesh_transaction_t* tx
);

/**
 * @brief Create and submit belief update transaction
 *
 * @param integration Integration manager
 * @param proposer Proposing participant
 * @param belief Belief to propagate
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_submit_belief(
    mesh_integration_t* integration,
    mesh_participant_id_t proposer,
    const mesh_belief_t* belief
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get integration statistics
 *
 * @param integration Integration manager
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_integration_get_stats(
    const mesh_integration_t* integration,
    mesh_integration_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param integration Integration manager
 */
void mesh_integration_reset_stats(mesh_integration_t* integration);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_INTEGRATION_H */
