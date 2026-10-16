//=============================================================================
// nimcp_portia_swarm_bridge.h - Portia Spider-Swarm Intelligence Integration
//=============================================================================
/**
 * @file nimcp_portia_swarm_bridge.h
 * @brief Bidirectional integration between Portia adaptive intelligence and Swarm collective systems
 *
 * WHAT: Bridge connecting Portia's individual resource-adaptive intelligence with
 *       Swarm's collective decision-making and emergent behavior systems
 * WHY:  Enable coordinated adaptive behavior where individual resource constraints
 *       inform collective decisions, and swarm consensus guides individual adaptation
 * HOW:  Bidirectional message passing via bio-async with state synchronization
 *
 * INTEGRATION SCENARIOS:
 * 1. Portia → Swarm (Resource Broadcasting):
 *    - Power state changes → swarm energy gossip
 *    - Thermal throttling → collective load balancing
 *    - Tier switches → swarm reconfiguration
 *    - Degradation events → swarm capability adaptation
 *
 * 2. Swarm → Portia (Collective Guidance):
 *    - Swarm consensus → tier recommendations
 *    - Emergence detection → planning hints
 *    - Collective energy state → power strategy
 *    - Quorum decisions → degradation policy
 *
 * BIOLOGICAL BASIS:
 * - Portia spider: Individual resource optimization (600K neurons)
 * - Swarm intelligence: Collective decision-making (ant colonies, bee swarms)
 * - Bridge: How individual constraints propagate to/from collective behavior
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_SWARM_BRIDGE_H
#define NIMCP_PORTIA_SWARM_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_context_t portia_context_t;
typedef struct nimcp_swarm_brain swarm_brain_t;
typedef struct swarm_consensus_context* swarm_consensus_t;
typedef struct swarm_emergence_t swarm_emergence_t;
typedef struct swarm_energy_gossip_t swarm_energy_gossip_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of swarm agents to coordinate with */
#define PORTIA_SWARM_MAX_AGENTS 256

/** Maximum tier recommendation history */
#define PORTIA_SWARM_RECOMMENDATION_HISTORY 16

/** Default sync interval in milliseconds */
#define PORTIA_SWARM_SYNC_INTERVAL_MS 100

/** Energy gossip broadcast interval */
#define PORTIA_SWARM_ENERGY_GOSSIP_INTERVAL_MS 500

/** Consensus polling timeout */
#define PORTIA_SWARM_CONSENSUS_TIMEOUT_MS 1000

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Integration mode for Portia-Swarm bridge
 */
typedef enum {
    PORTIA_SWARM_MODE_DISABLED = 0,     /**< Integration disabled */
    PORTIA_SWARM_MODE_PASSIVE = 1,      /**< Receive only, no broadcast */
    PORTIA_SWARM_MODE_BROADCAST = 2,    /**< Broadcast only, no consensus */
    PORTIA_SWARM_MODE_BIDIRECTIONAL = 3 /**< Full bidirectional integration */
} portia_swarm_mode_t;

/**
 * @brief Swarm influence level on Portia decisions
 */
typedef enum {
    PORTIA_SWARM_INFLUENCE_NONE = 0,    /**< Swarm has no influence */
    PORTIA_SWARM_INFLUENCE_ADVISORY = 1,/**< Swarm provides recommendations */
    PORTIA_SWARM_INFLUENCE_MODERATE = 2,/**< Swarm influences 50% of decisions */
    PORTIA_SWARM_INFLUENCE_DOMINANT = 3 /**< Swarm overrides most decisions */
} portia_swarm_influence_t;

/**
 * @brief Message types for Portia-Swarm communication
 */
typedef enum {
    PORTIA_SWARM_MSG_POWER_STATE = 0,       /**< Power state update */
    PORTIA_SWARM_MSG_THERMAL_STATE = 1,     /**< Thermal state update */
    PORTIA_SWARM_MSG_TIER_CHANGE = 2,       /**< Platform tier change */
    PORTIA_SWARM_MSG_DEGRADATION = 3,       /**< Degradation event */
    PORTIA_SWARM_MSG_RESOURCE_REQUEST = 4,  /**< Request for resources */
    PORTIA_SWARM_MSG_CONSENSUS_QUERY = 5,   /**< Query swarm consensus */
    PORTIA_SWARM_MSG_EMERGENCE_ALERT = 6,   /**< Emergence pattern detected */
    PORTIA_SWARM_MSG_CAPABILITY_UPDATE = 7  /**< Capability change notification */
} portia_swarm_msg_type_t;

/**
 * @brief Tier recommendation source
 */
typedef enum {
    PORTIA_TIER_REC_LOCAL = 0,          /**< Local Portia decision */
    PORTIA_TIER_REC_SWARM_CONSENSUS = 1,/**< Swarm consensus recommendation */
    PORTIA_TIER_REC_SWARM_EMERGENCE = 2,/**< Emergence-based recommendation */
    PORTIA_TIER_REC_HYBRID = 3          /**< Combined local + swarm */
} portia_tier_rec_source_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Portia state summary for swarm broadcast
 */
typedef struct {
    uint32_t agent_id;              /**< Unique agent identifier */
    uint8_t power_state;            /**< Current power state (portia_power_state_t) */
    uint8_t thermal_state;          /**< Current thermal state (portia_thermal_state_t) */
    uint8_t platform_tier;          /**< Current platform tier */
    uint8_t degradation_level;      /**< Current degradation level */
    float cpu_usage;                /**< CPU utilization [0.0-1.0] */
    float memory_usage;             /**< Memory utilization [0.0-1.0] */
    float battery_level;            /**< Battery level [0.0-1.0] */
    float thermal_headroom;         /**< Thermal headroom in °C */
    uint64_t timestamp;             /**< State timestamp */
} portia_swarm_state_t;

/**
 * @brief Swarm recommendation for Portia
 */
typedef struct {
    uint8_t recommended_tier;       /**< Recommended platform tier */
    uint8_t recommended_degradation;/**< Recommended degradation level */
    float confidence;               /**< Recommendation confidence [0.0-1.0] */
    uint32_t consensus_count;       /**< Number of agents in consensus */
    portia_tier_rec_source_t source;/**< Source of recommendation */
    uint64_t timestamp;             /**< Recommendation timestamp */
} portia_swarm_recommendation_t;

/**
 * @brief Collective resource state from swarm
 */
typedef struct {
    uint32_t agent_count;           /**< Number of agents in swarm */
    float avg_power_level;          /**< Average power level */
    float avg_cpu_usage;            /**< Average CPU usage */
    float avg_memory_usage;         /**< Average memory usage */
    float avg_thermal_headroom;     /**< Average thermal headroom */
    uint32_t agents_critical;       /**< Agents in critical state */
    uint32_t agents_degraded;       /**< Agents in degraded state */
    uint32_t agents_healthy;        /**< Agents in healthy state */
    uint64_t last_update;           /**< Last collective update time */
} portia_swarm_collective_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    portia_swarm_mode_t mode;       /**< Integration mode */
    portia_swarm_influence_t influence;/**< Swarm influence level */
    uint32_t sync_interval_ms;      /**< Sync interval in milliseconds */
    uint32_t gossip_interval_ms;    /**< Energy gossip interval */
    uint32_t consensus_timeout_ms;  /**< Consensus query timeout */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_energy_gossip;      /**< Enable energy gossip participation */
    bool enable_emergence_alerts;   /**< Enable emergence alerts */
    float consensus_weight;         /**< Weight of swarm consensus [0.0-1.0] */
    float local_weight;             /**< Weight of local decisions [0.0-1.0] */
} portia_swarm_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_sent;         /**< Total messages sent to swarm */
    uint64_t messages_received;     /**< Total messages from swarm */
    uint64_t consensus_queries;     /**< Number of consensus queries */
    uint64_t consensus_successes;   /**< Successful consensus operations */
    uint64_t tier_recommendations;  /**< Tier recommendations received */
    uint64_t recommendations_applied;/**< Recommendations actually applied */
    uint64_t energy_gossips;        /**< Energy gossip messages */
    uint64_t emergence_alerts;      /**< Emergence alerts received */
    float avg_consensus_latency_ms; /**< Average consensus latency */
    float sync_efficiency;          /**< State sync efficiency [0.0-1.0] */
} portia_swarm_stats_t;

/**
 * @brief Main bridge context (opaque)
 */
typedef struct portia_swarm_bridge_t portia_swarm_bridge_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for swarm recommendation events
 * @param bridge Bridge context
 * @param recommendation The recommendation received
 * @param user_data User-provided data
 */
typedef void (*portia_swarm_recommendation_cb)(
    portia_swarm_bridge_t* bridge,
    const portia_swarm_recommendation_t* recommendation,
    void* user_data
);

/**
 * @brief Callback for emergence alerts
 * @param bridge Bridge context
 * @param emergence_type Type of emergence detected
 * @param magnitude Emergence magnitude [0.0-1.0]
 * @param user_data User-provided data
 */
typedef void (*portia_swarm_emergence_cb)(
    portia_swarm_bridge_t* bridge,
    uint32_t emergence_type,
    float magnitude,
    void* user_data
);

/**
 * @brief Callback for collective state updates
 * @param bridge Bridge context
 * @param collective_state Updated collective state
 * @param user_data User-provided data
 */
typedef void (*portia_swarm_collective_cb)(
    portia_swarm_bridge_t* bridge,
    const portia_swarm_collective_state_t* collective_state,
    void* user_data
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration structure to initialize
 */
void portia_swarm_default_config(portia_swarm_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Portia-Swarm bridge
 * @param config Bridge configuration
 * @param portia Portia context to integrate
 * @return Bridge handle or NULL on failure
 */
portia_swarm_bridge_t* portia_swarm_bridge_create(
    const portia_swarm_config_t* config,
    portia_context_t* portia
);

/**
 * @brief Destroy bridge and free resources
 * @param bridge Bridge to destroy
 */
void portia_swarm_bridge_destroy(portia_swarm_bridge_t* bridge);

/**
 * @brief Start bridge operation
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_bridge_start(portia_swarm_bridge_t* bridge);

/**
 * @brief Stop bridge operation
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_bridge_stop(portia_swarm_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to swarm brain for collective coordination
 * @param bridge Bridge context
 * @param swarm_brain Swarm brain to connect
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_connect_brain(
    portia_swarm_bridge_t* bridge,
    swarm_brain_t* swarm_brain
);

/**
 * @brief Connect to swarm consensus for collective decisions
 * @param bridge Bridge context
 * @param consensus Swarm consensus module
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_connect_consensus(
    portia_swarm_bridge_t* bridge,
    swarm_consensus_t consensus
);

/**
 * @brief Connect to swarm emergence for pattern detection
 * @param bridge Bridge context
 * @param emergence Swarm emergence module
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_connect_emergence(
    portia_swarm_bridge_t* bridge,
    swarm_emergence_t* emergence
);

/**
 * @brief Connect to energy gossip for distributed energy awareness
 * @param bridge Bridge context
 * @param energy_gossip Energy gossip module
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_connect_energy_gossip(
    portia_swarm_bridge_t* bridge,
    swarm_energy_gossip_t* energy_gossip
);

/**
 * @brief Connect to bio-async messaging system
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_connect_bio_async(portia_swarm_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_disconnect_bio_async(portia_swarm_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 * @param bridge Bridge context
 * @return true if connected, false otherwise
 */
bool portia_swarm_is_bio_async_connected(const portia_swarm_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state (call periodically)
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_update(portia_swarm_bridge_t* bridge);

/**
 * @brief Broadcast current Portia state to swarm
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_broadcast_state(portia_swarm_bridge_t* bridge);

/**
 * @brief Request tier recommendation from swarm
 * @param bridge Bridge context
 * @param recommendation Output recommendation
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_request_recommendation(
    portia_swarm_bridge_t* bridge,
    portia_swarm_recommendation_t* recommendation
);

/**
 * @brief Notify swarm of tier change
 * @param bridge Bridge context
 * @param old_tier Previous platform tier
 * @param new_tier New platform tier
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_notify_tier_change(
    portia_swarm_bridge_t* bridge,
    uint8_t old_tier,
    uint8_t new_tier
);

/**
 * @brief Notify swarm of degradation event
 * @param bridge Bridge context
 * @param degradation_level New degradation level
 * @param reason Reason code for degradation
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_notify_degradation(
    portia_swarm_bridge_t* bridge,
    uint8_t degradation_level,
    uint32_t reason
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current collective swarm state
 * @param bridge Bridge context
 * @param state Output collective state
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_get_collective_state(
    const portia_swarm_bridge_t* bridge,
    portia_swarm_collective_state_t* state
);

/**
 * @brief Get local Portia state for swarm
 * @param bridge Bridge context
 * @param state Output local state
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_get_local_state(
    const portia_swarm_bridge_t* bridge,
    portia_swarm_state_t* state
);

/**
 * @brief Get latest tier recommendation
 * @param bridge Bridge context
 * @param recommendation Output recommendation
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_get_recommendation(
    const portia_swarm_bridge_t* bridge,
    portia_swarm_recommendation_t* recommendation
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge context
 * @param stats Output statistics
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_get_stats(
    const portia_swarm_bridge_t* bridge,
    portia_swarm_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_reset_stats(portia_swarm_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register callback for swarm recommendations
 * @param bridge Bridge context
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_register_recommendation_cb(
    portia_swarm_bridge_t* bridge,
    portia_swarm_recommendation_cb callback,
    void* user_data
);

/**
 * @brief Register callback for emergence alerts
 * @param bridge Bridge context
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_register_emergence_cb(
    portia_swarm_bridge_t* bridge,
    portia_swarm_emergence_cb callback,
    void* user_data
);

/**
 * @brief Register callback for collective state updates
 * @param bridge Bridge context
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_register_collective_cb(
    portia_swarm_bridge_t* bridge,
    portia_swarm_collective_cb callback,
    void* user_data
);

//=============================================================================
// Decision API
//=============================================================================

/**
 * @brief Compute optimal tier using hybrid Portia-Swarm decision
 * @param bridge Bridge context
 * @param local_recommendation Portia's local tier recommendation
 * @param optimal_tier Output optimal tier
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_compute_optimal_tier(
    portia_swarm_bridge_t* bridge,
    uint8_t local_recommendation,
    uint8_t* optimal_tier
);

/**
 * @brief Check if swarm consensus supports tier change
 * @param bridge Bridge context
 * @param proposed_tier Proposed new tier
 * @return true if consensus supports, false otherwise
 */
bool portia_swarm_consensus_supports_tier(
    portia_swarm_bridge_t* bridge,
    uint8_t proposed_tier
);

/**
 * @brief Apply swarm recommendation to Portia
 * @param bridge Bridge context
 * @param recommendation Recommendation to apply
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_apply_recommendation(
    portia_swarm_bridge_t* bridge,
    const portia_swarm_recommendation_t* recommendation
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_SWARM_BRIDGE_H */
