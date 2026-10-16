/**
 * @file nimcp_swarm_energy_gossip.h
 * @brief Energy-Aware Gossip Protocol for NIMCP Swarms
 *
 * Biological Inspiration: Metabolic efficiency in biological systems
 * - Energy conservation strategies
 * - Adaptive activity levels
 * - Coordinated rest periods
 * - Resource sharing and cooperation
 *
 * Features:
 * - Dynamic energy state management
 * - Adaptive message intervals
 * - Energy-aware relay selection
 * - Sleep scheduling coordination
 * - Harvest awareness
 * - Emergency reserve management
 * - Probabilistic gossip forwarding
 */

#ifndef NIMCP_SWARM_ENERGY_GOSSIP_H
#define NIMCP_SWARM_ENERGY_GOSSIP_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Energy State Definitions
 * ============================================================================ */

/**
 * @brief Energy states for swarm drones
 */
typedef enum {
    NIMCP_ENERGY_CRITICAL = 0,  /**< Below 10%, emergency mode */
    NIMCP_ENERGY_LOW,           /**< 10-25%, reduced messaging */
    NIMCP_ENERGY_NORMAL,        /**< 25-75%, standard operation */
    NIMCP_ENERGY_HIGH,          /**< 75-90%, can assist others */
    NIMCP_ENERGY_FULL,          /**< Above 90%, maximum capability */
    NIMCP_ENERGY_CHARGING       /**< Currently recharging */
} NimcpEnergyState;

/**
 * @brief Energy level thresholds (as percentages)
 */
#define NIMCP_ENERGY_CRITICAL_THRESHOLD 10.0f
#define NIMCP_ENERGY_LOW_THRESHOLD      25.0f
#define NIMCP_ENERGY_NORMAL_THRESHOLD   75.0f
#define NIMCP_ENERGY_HIGH_THRESHOLD     90.0f

/* ============================================================================
 * Energy Statistics and Metrics
 * ============================================================================ */

/**
 * @brief Energy statistics for a drone
 */
typedef struct {
    float current_level;        /**< Current energy level (0-100%) */
    float consumption_rate;     /**< Energy consumption per second */
    float harvest_rate;         /**< Energy harvest rate (if applicable) */
    float predicted_lifetime;   /**< Predicted operational time remaining */
    uint64_t total_consumed;    /**< Total energy consumed */
    uint64_t total_harvested;   /**< Total energy harvested */
    uint32_t charge_cycles;     /**< Number of charge cycles */
    time_t last_update;         /**< Last update timestamp */
} NimcpEnergyStats;

/**
 * @brief Harvest opportunity information
 */
typedef struct {
    bool available;             /**< Harvest opportunity available */
    float harvest_rate;         /**< Expected harvest rate */
    float position[3];          /**< Optimal position for harvesting */
    uint32_t congestion_level;  /**< Number of drones already harvesting */
    float quality_score;        /**< Quality of harvest location */
    time_t discovered_time;     /**< When opportunity was discovered */
} NimcpHarvestOpportunity;

/**
 * @brief Emergency reserve configuration
 */
typedef struct {
    float reserve_percentage;   /**< Energy reserved for return-to-base */
    float return_distance;      /**< Distance to base station */
    float return_energy_cost;   /**< Estimated energy cost to return */
    bool emergency_mode;        /**< Currently in emergency mode */
    bool auto_return_enabled;   /**< Automatic low-battery return */
    float emergency_threshold;  /**< Threshold for emergency return */
} NimcpEmergencyReserve;

/* ============================================================================
 * Gossip Protocol Structures
 * ============================================================================ */

/* Note: NimcpMessagePriority is defined in nimcp_common.h */

/**
 * @brief Gossip message header
 */
typedef struct {
    uint64_t message_id;        /**< Unique message identifier */
    uint32_t originator_id;     /**< ID of originating drone */
    uint32_t hop_count;         /**< Number of hops */
    uint32_t ttl;               /**< Time-to-live (remaining hops) */
    NimcpMessagePriority priority; /**< Message priority */
    time_t timestamp;           /**< Message creation time */
    uint32_t sequence_number;   /**< Sequence number from originator */
} NimcpGossipMessageHeader;

/**
 * @brief Gossip message
 */
typedef struct {
    NimcpGossipMessageHeader header;
    size_t payload_size;
    void* payload;
    bool forwarded;             /**< Whether message has been forwarded */
    uint32_t forward_count;     /**< Number of times forwarded */
} NimcpGossipMessage;

/**
 * @brief Relay node information
 */
typedef struct {
    uint32_t node_id;           /**< Node identifier */
    NimcpEnergyState energy_state; /**< Current energy state */
    float energy_level;         /**< Current energy level */
    float reliability_score;    /**< Reliability metric */
    float distance;             /**< Distance from source */
    uint32_t message_count;     /**< Messages relayed */
    time_t last_seen;           /**< Last communication time */
    bool is_available;          /**< Currently available for relay */
} NimcpRelayNode;

/**
 * @brief Sleep schedule entry
 */
typedef struct {
    time_t sleep_start;         /**< Sleep period start time */
    time_t sleep_end;           /**< Sleep period end time */
    uint32_t duration_seconds;  /**< Sleep duration */
    bool wake_on_emergency;     /**< Wake up for emergency messages */
    uint32_t wake_events;       /**< Events that can wake node */
    float energy_saved;         /**< Estimated energy saved */
} NimcpSleepSchedule;

/* ============================================================================
 * Energy-Aware Gossip Configuration
 * ============================================================================ */

/**
 * @brief Adaptive interval configuration
 */
typedef struct {
    uint32_t interval_critical_ms;  /**< Interval in critical state */
    uint32_t interval_low_ms;       /**< Interval in low state */
    uint32_t interval_normal_ms;    /**< Interval in normal state */
    uint32_t interval_high_ms;      /**< Interval in high state */
    uint32_t interval_full_ms;      /**< Interval in full state */
    uint32_t interval_charging_ms;  /**< Interval while charging */
} NimcpAdaptiveIntervals;

/**
 * @brief Gossip forwarding probabilities by energy state
 */
typedef struct {
    float prob_critical;        /**< Forward probability when critical */
    float prob_low;             /**< Forward probability when low */
    float prob_normal;          /**< Forward probability when normal */
    float prob_high;            /**< Forward probability when high */
    float prob_full;            /**< Forward probability when full */
    float prob_charging;        /**< Forward probability while charging */
} NimcpForwardingProbabilities;

/**
 * @brief Energy-aware gossip configuration
 */
typedef struct {
    NimcpAdaptiveIntervals intervals;
    NimcpForwardingProbabilities forwarding_probs;
    uint32_t max_relay_candidates;  /**< Max relay nodes to consider */
    float min_relay_energy;         /**< Minimum energy for relay */
    uint32_t sleep_check_interval_ms; /**< Sleep schedule check interval */
    bool enable_harvest_awareness;  /**< Enable harvest opportunity detection */
    bool enable_coordinated_sleep;  /**< Enable coordinated sleep */
    uint32_t gossip_fanout;         /**< Number of nodes to gossip to */
    uint32_t max_message_cache;     /**< Max messages to cache */
    float convergence_threshold;    /**< Gossip convergence threshold */
} NimcpEnergyGossipConfig;

/* ============================================================================
 * Main Energy-Aware Gossip Protocol Structure
 * ============================================================================ */

/**
 * @brief Energy-aware gossip protocol state
 */
typedef struct NimcpEnergyGossip {
    uint32_t node_id;                   /**< This node's identifier */
    NimcpEnergyState current_state;     /**< Current energy state */
    NimcpEnergyStats stats;             /**< Energy statistics */
    NimcpEmergencyReserve reserve;      /**< Emergency reserve config */
    NimcpEnergyGossipConfig config;     /**< Protocol configuration */

    /* Message management */
    NimcpGossipMessage** message_cache; /**< Cached messages */
    uint32_t message_cache_size;        /**< Current cache size */
    uint32_t message_cache_capacity;    /**< Cache capacity */
    uint64_t next_message_id;           /**< Next message ID to assign */
    uint32_t sequence_number;           /**< Sequence number for sent messages */

    /* Relay management */
    NimcpRelayNode* relay_nodes;        /**< Known relay nodes */
    uint32_t relay_node_count;          /**< Number of relay nodes */
    uint32_t relay_node_capacity;       /**< Relay node array capacity */

    /* Sleep scheduling */
    NimcpSleepSchedule* sleep_schedule; /**< Sleep schedule */
    uint32_t sleep_schedule_count;      /**< Number of schedule entries */
    bool is_sleeping;                   /**< Currently sleeping */
    time_t sleep_until;                 /**< Sleep until this time */

    /* Harvest awareness */
    NimcpHarvestOpportunity* opportunities; /**< Harvest opportunities */
    uint32_t opportunity_count;         /**< Number of opportunities */
    NimcpHarvestOpportunity* current_harvest; /**< Current harvest location */

    /* Bio-async integration */
    void* inbox;               /**< Bio-async inbox */
    bool bio_async_enabled;             /**< Bio-async enabled */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;         /**< Mutex for thread safety */

    /* Statistics */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_forwarded;        /**< Total messages forwarded */
    uint64_t messages_dropped;          /**< Total messages dropped */
    uint64_t energy_state_changes;      /**< Number of state changes */
    uint64_t sleep_cycles;              /**< Number of sleep cycles */

    bool is_initialized;                /**< Initialization flag */
} NimcpEnergyGossip;

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * @brief Create energy-aware gossip protocol
 * @param node_id Unique node identifier
 * @param config Protocol configuration (NULL for defaults)
 * @return New gossip protocol instance or NULL on failure
 */
NimcpEnergyGossip* nimcp_energy_gossip_create(
    uint32_t node_id,
    const NimcpEnergyGossipConfig* config
);

/**
 * @brief Destroy energy-aware gossip protocol
 * @param gossip Gossip protocol instance
 */
void nimcp_energy_gossip_destroy(NimcpEnergyGossip* gossip);

/**
 * @brief Initialize with bio-async support
 * @param gossip Gossip protocol instance
 * @param inbox Bio-async inbox for message handling
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_init_bio_async(
    NimcpEnergyGossip* gossip,
    void* inbox
);

/* ============================================================================
 * Energy Management Functions
 * ============================================================================ */

/**
 * @brief Update energy level
 * @param gossip Gossip protocol instance
 * @param energy_level Current energy level (0-100%)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_update_energy(
    NimcpEnergyGossip* gossip,
    float energy_level
);

/**
 * @brief Get current energy state
 * @param gossip Gossip protocol instance
 * @return Current energy state
 */
NimcpEnergyState nimcp_energy_gossip_get_state(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Set energy consumption rate
 * @param gossip Gossip protocol instance
 * @param rate Energy consumption rate (units per second)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_set_consumption_rate(
    NimcpEnergyGossip* gossip,
    float rate
);

/**
 * @brief Set energy harvest rate
 * @param gossip Gossip protocol instance
 * @param rate Energy harvest rate (units per second)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_set_harvest_rate(
    NimcpEnergyGossip* gossip,
    float rate
);

/**
 * @brief Get predicted operational lifetime
 * @param gossip Gossip protocol instance
 * @return Predicted lifetime in seconds
 */
float nimcp_energy_gossip_predict_lifetime(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Get energy statistics
 * @param gossip Gossip protocol instance
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_get_stats(
    const NimcpEnergyGossip* gossip,
    NimcpEnergyStats* stats
);

/* ============================================================================
 * Emergency Reserve Functions
 * ============================================================================ */

/**
 * @brief Configure emergency reserve
 * @param gossip Gossip protocol instance
 * @param reserve_percentage Percentage of energy to reserve
 * @param return_distance Distance to base station
 * @param return_energy_cost Energy cost to return
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_configure_reserve(
    NimcpEnergyGossip* gossip,
    float reserve_percentage,
    float return_distance,
    float return_energy_cost
);

/**
 * @brief Check if emergency return is needed
 * @param gossip Gossip protocol instance
 * @return true if emergency return needed
 */
bool nimcp_energy_gossip_needs_emergency_return(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Trigger emergency mode
 * @param gossip Gossip protocol instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_emergency_mode(
    NimcpEnergyGossip* gossip
);

/* ============================================================================
 * Gossip Protocol Functions
 * ============================================================================ */

/**
 * @brief Broadcast a message using gossip protocol
 * @param gossip Gossip protocol instance
 * @param payload Message payload
 * @param payload_size Payload size in bytes
 * @param priority Message priority
 * @param ttl Time-to-live (max hops)
 * @return Message ID or 0 on failure
 */
uint64_t nimcp_energy_gossip_broadcast(
    NimcpEnergyGossip* gossip,
    const void* payload,
    size_t payload_size,
    NimcpMessagePriority priority,
    uint32_t ttl
);

/**
 * @brief Receive and process a gossip message
 * @param gossip Gossip protocol instance
 * @param message Incoming message
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_receive(
    NimcpEnergyGossip* gossip,
    const NimcpGossipMessage* message
);

/**
 * @brief Forward a message based on energy-aware forwarding
 * @param gossip Gossip protocol instance
 * @param message Message to forward
 * @return NIMCP_OK on success, NIMCP_ERR_SKIP if not forwarded
 */
nimcp_result_t nimcp_energy_gossip_forward(
    NimcpEnergyGossip* gossip,
    NimcpGossipMessage* message
);

/**
 * @brief Get forwarding probability for current energy state
 * @param gossip Gossip protocol instance
 * @return Forwarding probability (0.0-1.0)
 */
float nimcp_energy_gossip_get_forward_probability(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Check if message should be forwarded (probabilistic)
 * @param gossip Gossip protocol instance
 * @param message Message to check
 * @return true if should forward
 */
bool nimcp_energy_gossip_should_forward(
    const NimcpEnergyGossip* gossip,
    const NimcpGossipMessage* message
);

/* ============================================================================
 * Relay Selection Functions
 * ============================================================================ */

/**
 * @brief Register a relay node
 * @param gossip Gossip protocol instance
 * @param node_id Node identifier
 * @param energy_level Node's energy level
 * @param distance Distance from this node
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_register_relay(
    NimcpEnergyGossip* gossip,
    uint32_t node_id,
    float energy_level,
    float distance
);

/**
 * @brief Update relay node energy state
 * @param gossip Gossip protocol instance
 * @param node_id Node identifier
 * @param energy_level Updated energy level
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_update_relay(
    NimcpEnergyGossip* gossip,
    uint32_t node_id,
    float energy_level
);

/**
 * @brief Select best relay nodes for message forwarding
 * @param gossip Gossip protocol instance
 * @param max_relays Maximum number of relays to select
 * @param selected Output array of selected node IDs
 * @param selected_count Output count of selected nodes
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_select_relays(
    NimcpEnergyGossip* gossip,
    uint32_t max_relays,
    uint32_t* selected,
    uint32_t* selected_count
);

/**
 * @brief Get relay node information
 * @param gossip Gossip protocol instance
 * @param node_id Node identifier
 * @param relay Output relay node info
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_get_relay_info(
    const NimcpEnergyGossip* gossip,
    uint32_t node_id,
    NimcpRelayNode* relay
);

/* ============================================================================
 * Sleep Scheduling Functions
 * ============================================================================ */

/**
 * @brief Add sleep schedule entry
 * @param gossip Gossip protocol instance
 * @param start_time Sleep start time
 * @param duration_seconds Sleep duration
 * @param wake_on_emergency Wake on emergency messages
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_schedule_sleep(
    NimcpEnergyGossip* gossip,
    time_t start_time,
    uint32_t duration_seconds,
    bool wake_on_emergency
);

/**
 * @brief Enter sleep mode
 * @param gossip Gossip protocol instance
 * @param duration_seconds Sleep duration
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_sleep(
    NimcpEnergyGossip* gossip,
    uint32_t duration_seconds
);

/**
 * @brief Wake from sleep mode
 * @param gossip Gossip protocol instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_wake(
    NimcpEnergyGossip* gossip
);

/**
 * @brief Check if currently sleeping
 * @param gossip Gossip protocol instance
 * @return true if sleeping
 */
bool nimcp_energy_gossip_is_sleeping(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Coordinate sleep schedule with other nodes
 * @param gossip Gossip protocol instance
 * @param node_schedules Array of node sleep schedules
 * @param node_count Number of nodes
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_coordinate_sleep(
    NimcpEnergyGossip* gossip,
    const NimcpSleepSchedule* node_schedules,
    uint32_t node_count
);

/* ============================================================================
 * Harvest Awareness Functions
 * ============================================================================ */

/**
 * @brief Register harvest opportunity
 * @param gossip Gossip protocol instance
 * @param position Harvest location position [x, y, z]
 * @param harvest_rate Expected harvest rate
 * @param quality_score Quality of location (0-1)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_register_harvest(
    NimcpEnergyGossip* gossip,
    const float position[3],
    float harvest_rate,
    float quality_score
);

/**
 * @brief Select best harvest opportunity
 * @param gossip Gossip protocol instance
 * @param opportunity Output harvest opportunity
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_select_harvest(
    NimcpEnergyGossip* gossip,
    NimcpHarvestOpportunity* opportunity
);

/**
 * @brief Update harvest congestion level
 * @param gossip Gossip protocol instance
 * @param position Harvest location
 * @param congestion_level Number of drones at location
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_update_harvest_congestion(
    NimcpEnergyGossip* gossip,
    const float position[3],
    uint32_t congestion_level
);

/**
 * @brief Start harvesting at current location
 * @param gossip Gossip protocol instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_start_harvest(
    NimcpEnergyGossip* gossip
);

/**
 * @brief Stop harvesting
 * @param gossip Gossip protocol instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_stop_harvest(
    NimcpEnergyGossip* gossip
);

/* ============================================================================
 * Adaptive Interval Functions
 * ============================================================================ */

/**
 * @brief Get current heartbeat interval based on energy state
 * @param gossip Gossip protocol instance
 * @return Interval in milliseconds
 */
uint32_t nimcp_energy_gossip_get_heartbeat_interval(
    const NimcpEnergyGossip* gossip
);

/**
 * @brief Should process message based on priority and energy state
 * @param gossip Gossip protocol instance
 * @param priority Message priority
 * @return true if should process
 */
bool nimcp_energy_gossip_should_process(
    const NimcpEnergyGossip* gossip,
    NimcpMessagePriority priority
);

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle energy status broadcast message
 * @param gossip Gossip protocol instance
 * @param message Bio-async message
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_handle_energy_broadcast(
    NimcpEnergyGossip* gossip,
    const void* message
);

/**
 * @brief Handle sleep coordination message
 * @param gossip Gossip protocol instance
 * @param message Bio-async message
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_handle_sleep_coordination(
    NimcpEnergyGossip* gossip,
    const void* message
);

/**
 * @brief Handle relay request message
 * @param gossip Gossip protocol instance
 * @param message Bio-async message
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_handle_relay_request(
    NimcpEnergyGossip* gossip,
    const void* message
);

/**
 * @brief Process bio-async inbox
 * @param gossip Gossip protocol instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_energy_gossip_process_inbox(
    NimcpEnergyGossip* gossip
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Output configuration structure
 */
void nimcp_energy_gossip_default_config(
    NimcpEnergyGossipConfig* config
);

/**
 * @brief Convert energy level to state
 * @param energy_level Energy level (0-100%)
 * @param is_charging Whether currently charging
 * @return Energy state
 */
NimcpEnergyState nimcp_energy_level_to_state(
    float energy_level,
    bool is_charging
);

/**
 * @brief Get string representation of energy state
 * @param state Energy state
 * @return String representation
 */
const char* nimcp_energy_state_to_string(
    NimcpEnergyState state
);

/**
 * @brief Print gossip statistics
 * @param gossip Gossip protocol instance
 */
void nimcp_energy_gossip_print_stats(
    const NimcpEnergyGossip* gossip
);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_ENERGY_GOSSIP_H */
