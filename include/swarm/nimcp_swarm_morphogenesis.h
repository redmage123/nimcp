/**
 * @file nimcp_swarm_morphogenesis.h
 * @brief Swarm Morphogenesis - Role Specialization System
 *
 * Biological inspiration: Cellular differentiation in development, slime mold specialization
 *
 * Implements dynamic role assignment and specialization for swarm drones:
 * - Role-based differentiation (scout, relay, sentinel, worker, medic, leader)
 * - Morphogen gradient-based role assignment
 * - Load balancing and automatic redistribution
 * - De-differentiation and role transitions
 * - Bio-async integration for coordination
 */

#ifndef NIMCP_SWARM_MORPHOGENESIS_H
#define NIMCP_SWARM_MORPHOGENESIS_H

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Role Type Definitions
 * ============================================================================ */

/**
 * @brief Drone role types (inspired by cellular differentiation)
 */
typedef enum {
    NIMCP_SWARM_ROLE_GENERALIST = 0,  /**< Default unspecialized state */
    NIMCP_SWARM_ROLE_SCOUT,           /**< Reconnaissance and exploration */
    NIMCP_SWARM_ROLE_RELAY,           /**< Communication relay node */
    NIMCP_SWARM_ROLE_SENTINEL,        /**< Perimeter defense and monitoring */
    NIMCP_SWARM_ROLE_WORKER,          /**< Task execution and processing */
    NIMCP_SWARM_ROLE_MEDIC,           /**< Support and repair other drones */
    NIMCP_SWARM_ROLE_LEADER,          /**< Coordination and decision making */
    NIMCP_SWARM_ROLE_COUNT            /**< Total number of role types */
} NimcpSwarmRole;

/**
 * @brief Role capabilities bitmap
 */
typedef enum {
    NIMCP_ROLE_CAP_EXPLORE    = (1 << 0),  /**< Can explore environment */
    NIMCP_ROLE_CAP_RELAY      = (1 << 1),  /**< Can relay messages */
    NIMCP_ROLE_CAP_DEFEND     = (1 << 2),  /**< Can defend perimeter */
    NIMCP_ROLE_CAP_WORK       = (1 << 3),  /**< Can execute tasks */
    NIMCP_ROLE_CAP_REPAIR     = (1 << 4),  /**< Can repair others */
    NIMCP_ROLE_CAP_COORDINATE = (1 << 5),  /**< Can coordinate swarm */
    NIMCP_ROLE_CAP_ADAPT      = (1 << 6),  /**< Can change roles easily */
    NIMCP_ROLE_CAP_SENSE      = (1 << 7)   /**< Enhanced sensing */
} NimcpRoleCapability;

/* ============================================================================
 * Differentiation Trigger Types
 * ============================================================================ */

/**
 * @brief Triggers that cause role differentiation
 */
typedef enum {
    NIMCP_DIFF_TRIGGER_POSITION,      /**< Position-based (morphogen gradient) */
    NIMCP_DIFF_TRIGGER_RESOURCE,      /**< Resource proximity/need */
    NIMCP_DIFF_TRIGGER_THREAT,        /**< Threat proximity/level */
    NIMCP_DIFF_TRIGGER_IMBALANCE,     /**< Role distribution imbalance */
    NIMCP_DIFF_TRIGGER_COMMAND,       /**< Direct command from leader */
    NIMCP_DIFF_TRIGGER_EMERGENCY,     /**< Emergency situation */
    NIMCP_DIFF_TRIGGER_COOLDOWN       /**< Cooldown expired */
} NimcpDifferentiationTrigger;

/* ============================================================================
 * Morphogen Gradient Types
 * ============================================================================ */

/**
 * @brief Morphogen types influencing differentiation
 */
typedef enum {
    NIMCP_MORPHOGEN_CENTER_DISTANCE,  /**< Distance from swarm center */
    NIMCP_MORPHOGEN_RESOURCE,         /**< Resource concentration */
    NIMCP_MORPHOGEN_THREAT,           /**< Threat level */
    NIMCP_MORPHOGEN_CONNECTIVITY,     /**< Network connectivity */
    NIMCP_MORPHOGEN_LEADER_SIGNAL,    /**< Leader coordination signal */
    NIMCP_MORPHOGEN_COUNT
} NimcpMorphogenType;

/* ============================================================================
 * Structure Definitions
 * ============================================================================ */

/**
 * @brief Role specification and capabilities
 */
typedef struct {
    NimcpSwarmRole role;              /**< Role type */
    const char* name;                 /**< Human-readable name */
    uint32_t capabilities;            /**< Capability bitmap */
    float energy_cost_multiplier;    /**< Energy consumption multiplier */
    float processing_power;           /**< Processing capability */
    float sensing_range;              /**< Sensing range multiplier */
    float communication_range;        /**< Communication range multiplier */
} NimcpRoleSpec;

/**
 * @brief Morphogen gradient state
 */
typedef struct {
    NimcpMorphogenType type;          /**< Morphogen type */
    float concentration;              /**< Current concentration (0.0-1.0) */
    float threshold_scout;            /**< Threshold for scout differentiation */
    float threshold_relay;            /**< Threshold for relay differentiation */
    float threshold_sentinel;         /**< Threshold for sentinel differentiation */
    float threshold_worker;           /**< Threshold for worker differentiation */
    float threshold_medic;            /**< Threshold for medic differentiation */
    float threshold_leader;           /**< Threshold for leader differentiation */
} NimcpMorphogenGradient;

/**
 * @brief Role transition history entry
 */
typedef struct {
    uint64_t timestamp;               /**< When transition occurred */
    NimcpSwarmRole from_role;         /**< Previous role */
    NimcpSwarmRole to_role;           /**< New role */
    NimcpDifferentiationTrigger trigger; /**< What caused the transition */
    float morphogen_values[NIMCP_MORPHOGEN_COUNT]; /**< Morphogen state at transition */
} NimcpRoleTransition;

/**
 * @brief Role distribution statistics
 */
typedef struct {
    uint32_t role_counts[NIMCP_SWARM_ROLE_COUNT]; /**< Count of drones in each role */
    float role_ratios[NIMCP_SWARM_ROLE_COUNT];    /**< Ratio of total for each role */
    uint32_t total_drones;                         /**< Total drones in swarm */
    float balance_score;                           /**< How balanced distribution is (0-1) */
    bool needs_rebalancing;                        /**< Whether rebalancing needed */
} NimcpRoleDistribution;

/**
 * @brief Individual drone role state
 */
typedef struct {
    NimcpSwarmRole current_role;      /**< Current active role */
    NimcpSwarmRole desired_role;      /**< Desired role (may differ during transition) */
    uint32_t drone_id;                /**< Unique drone identifier */

    float position[3];                /**< Drone position (x, y, z) */
    float swarm_center_distance;      /**< Distance from swarm center */

    NimcpMorphogenGradient gradients[NIMCP_MORPHOGEN_COUNT]; /**< Morphogen concentrations */

    uint64_t role_adoption_time;      /**< When current role was adopted */
    uint64_t last_transition_time;    /**< Last role transition time */
    uint32_t transition_count;        /**< Total number of role transitions */

    float cooldown_remaining;         /**< Cooldown time before next transition (seconds) */
    bool in_transition;               /**< Currently transitioning roles */

    NimcpRoleTransition* transition_history; /**< History of role transitions */
    uint32_t history_size;            /**< Size of history buffer */
    uint32_t history_count;           /**< Number of entries in history */

    float fitness_score;              /**< How well suited for current role */
    float adaptation_rate;            /**< How quickly can adapt to new roles */
} NimcpDroneRoleState;

/**
 * @brief Swarm morphogenesis system
 */
typedef struct NimcpSwarmMorphogenesis {
    /* Configuration */
    uint32_t max_drones;              /**< Maximum drones in swarm */
    uint32_t active_drones;           /**< Currently active drones */

    float transition_cooldown;        /**< Cooldown between transitions (seconds) */
    float morphogen_update_rate;      /**< How often to update morphogens (Hz) */
    float rebalance_threshold;        /**< Imbalance threshold triggering rebalance */

    /* Role specifications */
    NimcpRoleSpec role_specs[NIMCP_SWARM_ROLE_COUNT]; /**< Specs for each role */

    /* Drone states */
    NimcpDroneRoleState* drone_states; /**< Array of drone role states */

    /* Swarm-level state */
    float swarm_center[3];            /**< Current swarm center position */
    NimcpRoleDistribution distribution; /**< Current role distribution */

    /* Morphogen field */
    NimcpMorphogenGradient* global_gradients; /**< Global morphogen field */
    uint32_t gradient_grid_size;      /**< Size of gradient grid */

    /* Statistics */
    uint64_t total_differentiations;  /**< Total differentiation events */
    uint64_t total_dedifferentiations; /**< Total de-differentiation events */
    uint64_t forced_transitions;      /**< Forced transitions (commands) */
    uint64_t automatic_transitions;   /**< Automatic transitions (gradients) */

    /* Threading and synchronization */
    nimcp_platform_mutex_t lock;      /**< Protects morphogenesis state */
    nimcp_atomic_int32_t update_in_progress; /**< Update operation in progress */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;     /**< Bio-async module context */
    bool bio_async_enabled;           /**< Whether bio-async is enabled */

    /* Memory management */
    bool initialized;                 /**< Whether system is initialized */
} NimcpSwarmMorphogenesis;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create and initialize a swarm morphogenesis system
 *
 * @param max_drones Maximum number of drones in swarm
 * @param transition_cooldown Cooldown between role transitions (seconds)
 * @param rebalance_threshold Imbalance threshold (0.0-1.0)
 * @return Initialized morphogenesis system, or NULL on failure
 */
NimcpSwarmMorphogenesis* nimcp_swarm_morphogenesis_create(
    uint32_t max_drones,
    float transition_cooldown,
    float rebalance_threshold
);

/**
 * @brief Destroy morphogenesis system and free resources
 *
 * @param morph Morphogenesis system to destroy
 */
void nimcp_swarm_morphogenesis_destroy(NimcpSwarmMorphogenesis* morph);

/**
 * @brief Initialize morphogenesis system with bio-async integration
 *
 * @param morph Morphogenesis system
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_init_bio_async(
    NimcpSwarmMorphogenesis* morph
);

/* ============================================================================
 * Drone Management Functions
 * ============================================================================ */

/**
 * @brief Register a new drone in the swarm
 *
 * @param morph Morphogenesis system
 * @param drone_id Unique drone identifier
 * @param position Initial position [x, y, z]
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_register_drone(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    const float position[3]
);

/**
 * @brief Unregister a drone from the swarm
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier to unregister
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_unregister_drone(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id
);

/**
 * @brief Update drone position (affects morphogen gradients)
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param position New position [x, y, z]
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_update_position(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    const float position[3]
);

/* ============================================================================
 * Role Assignment and Differentiation
 * ============================================================================ */

/**
 * @brief Get current role for a drone
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param out_role Output for current role
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_get_role(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole* out_role
);

/**
 * @brief Force a drone to differentiate into a specific role
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param new_role Role to assign
 * @param force Whether to ignore cooldown
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_assign_role(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole new_role,
    bool force
);

/**
 * @brief De-differentiate a drone back to generalist
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_dedifferentiate(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id
);

/**
 * @brief Check if a drone can differentiate (cooldown expired)
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param out_can_differentiate Output boolean
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_can_differentiate(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    bool* out_can_differentiate
);

/* ============================================================================
 * Morphogen Gradient Functions
 * ============================================================================ */

/**
 * @brief Update morphogen gradients based on current swarm state
 *
 * @param morph Morphogenesis system
 * @param delta_time Time since last update (seconds)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_update_gradients(
    NimcpSwarmMorphogenesis* morph,
    float delta_time
);

/**
 * @brief Set morphogen concentration at a position
 *
 * @param morph Morphogenesis system
 * @param morphogen_type Type of morphogen
 * @param position Position [x, y, z]
 * @param concentration Concentration value (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_set_morphogen(
    NimcpSwarmMorphogenesis* morph,
    NimcpMorphogenType morphogen_type,
    const float position[3],
    float concentration
);

/**
 * @brief Get morphogen concentration at a position
 *
 * @param morph Morphogenesis system
 * @param morphogen_type Type of morphogen
 * @param position Position [x, y, z]
 * @param out_concentration Output concentration
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_get_morphogen(
    const NimcpSwarmMorphogenesis* morph,
    NimcpMorphogenType morphogen_type,
    const float position[3],
    float* out_concentration
);

/**
 * @brief Evaluate differentiation based on morphogen gradients
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param out_suggested_role Output suggested role based on gradients
 * @param out_confidence Confidence in suggestion (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_evaluate_differentiation(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole* out_suggested_role,
    float* out_confidence
);

/* ============================================================================
 * Load Balancing Functions
 * ============================================================================ */

/**
 * @brief Get current role distribution across swarm
 *
 * @param morph Morphogenesis system
 * @param out_distribution Output distribution statistics
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_get_distribution(
    const NimcpSwarmMorphogenesis* morph,
    NimcpRoleDistribution* out_distribution
);

/**
 * @brief Check if role distribution is balanced
 *
 * @param morph Morphogenesis system
 * @param out_balanced Output whether balanced
 * @param out_balance_score Output balance score (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_is_balanced(
    const NimcpSwarmMorphogenesis* morph,
    bool* out_balanced,
    float* out_balance_score
);

/**
 * @brief Rebalance role distribution across swarm
 *
 * Automatically reassigns roles to achieve better distribution
 *
 * @param morph Morphogenesis system
 * @param target_distribution Desired distribution ratios (NULL for automatic)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_rebalance(
    NimcpSwarmMorphogenesis* morph,
    const float* target_distribution
);

/* ============================================================================
 * Role Capability Functions
 * ============================================================================ */

/**
 * @brief Get capabilities for a specific role
 *
 * @param role Role type
 * @return Capability bitmap
 */
uint32_t nimcp_swarm_morphogenesis_get_role_capabilities(NimcpSwarmRole role);

/**
 * @brief Check if a role has a specific capability
 *
 * @param role Role type
 * @param capability Capability to check
 * @return true if role has capability, false otherwise
 */
bool nimcp_swarm_morphogenesis_role_has_capability(
    NimcpSwarmRole role,
    NimcpRoleCapability capability
);

/**
 * @brief Get role name as string
 *
 * @param role Role type
 * @return Role name (static string)
 */
const char* nimcp_swarm_morphogenesis_role_name(NimcpSwarmRole role);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get transition history for a drone
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone identifier
 * @param out_history Output array (allocated by caller)
 * @param max_entries Maximum entries to retrieve
 * @param out_count Actual number of entries retrieved
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_get_transition_history(
    const NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpRoleTransition* out_history,
    uint32_t max_entries,
    uint32_t* out_count
);

/**
 * @brief Get morphogenesis statistics
 *
 * @param morph Morphogenesis system
 * @param out_total_diff Output total differentiations
 * @param out_total_dediff Output total de-differentiations
 * @param out_forced Output forced transitions
 * @param out_automatic Output automatic transitions
 */
void nimcp_swarm_morphogenesis_get_statistics(
    const NimcpSwarmMorphogenesis* morph,
    uint64_t* out_total_diff,
    uint64_t* out_total_dediff,
    uint64_t* out_forced,
    uint64_t* out_automatic
);

/**
 * @brief Print morphogenesis state for debugging
 *
 * @param morph Morphogenesis system
 * @param verbose Whether to print detailed information
 */
void nimcp_swarm_morphogenesis_print_state(
    const NimcpSwarmMorphogenesis* morph,
    bool verbose
);

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Process incoming bio-async messages
 *
 * Call periodically to handle role coordination messages
 *
 * @param morph Morphogenesis system
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_process_messages(
    NimcpSwarmMorphogenesis* morph
);

/**
 * @brief Broadcast role change to swarm
 *
 * @param morph Morphogenesis system
 * @param drone_id Drone that changed role
 * @param new_role New role assigned
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_morphogenesis_broadcast_role_change(
    NimcpSwarmMorphogenesis* morph,
    uint32_t drone_id,
    NimcpSwarmRole new_role
);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MORPHOGENESIS_H */
