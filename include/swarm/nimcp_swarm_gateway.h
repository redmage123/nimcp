/**
 * @file nimcp_swarm_gateway.h
 * @brief Server-to-Swarm Gateway for NIMCP systems
 *
 * Enables large NIMCP server brains to communicate with drone swarms,
 * providing learning synchronization, mission control, and telemetry
 * aggregation capabilities.
 */

#ifndef NIMCP_SWARM_GATEWAY_H
#define NIMCP_SWARM_GATEWAY_H

#include "core/brain/nimcp_brain.h"
#include "swarm/nimcp_swarm_protocol.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gateway configuration
 */
typedef struct {
    char gateway_name[64];           /**< Gateway identifier */
    uint32_t max_swarms;             /**< Maximum connected swarms */
    uint32_t broadcast_interval_ms;  /**< Update broadcast rate */
    uint32_t timeout_ms;             /**< Swarm timeout threshold */
    bool enable_learning_sync;       /**< Push learning updates */
    bool enable_mission_control;     /**< Accept mission commands */
    bool enable_telemetry;           /**< Receive swarm telemetry */
} swarm_gateway_config_t;

/**
 * @brief Server-to-swarm message types
 */
typedef enum {
    GATEWAY_MSG_LEARNING_UPDATE,     /**< Weight updates from server */
    GATEWAY_MSG_MISSION_PARAMS,      /**< Mission objectives */
    GATEWAY_MSG_THREAT_INTEL,        /**< Threat information */
    GATEWAY_MSG_FORMATION_CMD,       /**< Formation commands */
    GATEWAY_MSG_RECALL,              /**< Return to base command */
    GATEWAY_MSG_NEUROMOD_OVERRIDE,   /**< Override neuromodulators */
    GATEWAY_MSG_HEARTBEAT,           /**< Keep-alive signal */
    GATEWAY_MSG_SYNC_REQUEST,        /**< Request swarm synchronization */
} swarm_gateway_msg_type_t;

/**
 * @brief Swarm connection status
 */
typedef enum {
    SWARM_STATUS_DISCONNECTED,       /**< Not connected */
    SWARM_STATUS_CONNECTING,         /**< Connection in progress */
    SWARM_STATUS_CONNECTED,          /**< Active connection */
    SWARM_STATUS_DEGRADED,           /**< Partial connectivity */
    SWARM_STATUS_TIMEOUT,            /**< Connection timeout */
} swarm_status_t;

/**
 * @brief Learning update compression format
 */
typedef struct {
    uint32_t update_id;              /**< Sequential update ID */
    uint32_t base_timestamp;         /**< Base timestamp */
    uint32_t num_deltas;             /**< Number of weight deltas */
    float compression_ratio;         /**< Achieved compression ratio */
    void* delta_data;                /**< Compressed delta data */
    size_t delta_size;               /**< Size of delta data */
} learning_update_t;

/**
 * @brief Mission parameters
 */
typedef struct {
    char mission_id[32];             /**< Mission identifier */
    uint32_t mission_type;           /**< Mission type code */
    float target_coordinates[3];     /**< Target location (x, y, z) */
    float search_radius;             /**< Search area radius */
    uint32_t duration_ms;            /**< Mission duration */
    uint32_t num_objectives;         /**< Number of objectives */
    void* objective_data;            /**< Mission-specific data */
    size_t objective_size;           /**< Size of objective data */
} mission_params_t;

/**
 * @brief Threat intelligence data
 */
typedef struct {
    uint32_t threat_id;              /**< Threat identifier */
    uint32_t threat_level;           /**< Severity level (0-10) */
    float position[3];               /**< Threat location */
    float velocity[3];               /**< Threat movement vector */
    uint32_t detection_time;         /**< Detection timestamp */
    char threat_type[32];            /**< Threat classification */
} threat_intel_t;

/**
 * @brief Formation command
 */
typedef struct {
    uint32_t formation_type;         /**< Formation pattern ID */
    float center_position[3];        /**< Formation center */
    float spacing;                   /**< Inter-drone spacing */
    float orientation;               /**< Formation orientation */
    uint32_t transition_time_ms;     /**< Time to achieve formation */
} formation_cmd_t;

/**
 * @brief Neuromodulator override
 */
typedef struct {
    uint32_t modulator_type;         /**< Which neuromodulator to override */
    float override_value;            /**< New value (0.0-1.0) */
    uint32_t duration_ms;            /**< Override duration (0 = permanent) */
    bool apply_to_all;               /**< Apply to all drones in swarm */
} neuromod_override_t;

/**
 * @brief Aggregated swarm telemetry
 */
typedef struct {
    char swarm_id[32];               /**< Swarm identifier */
    uint32_t timestamp;              /**< Telemetry timestamp */
    uint32_t num_drones;             /**< Number of active drones */
    uint32_t num_responsive;         /**< Number of responsive drones */
    float avg_battery_level;         /**< Average battery percentage */
    float avg_cpu_usage;             /**< Average CPU utilization */
    float avg_memory_usage;          /**< Average memory usage */
    float formation_coherence;       /**< Formation quality (0.0-1.0) */
    float mission_progress;          /**< Mission completion (0.0-1.0) */
    float center_of_mass[3];         /**< Swarm center position */
    float bounding_box[6];           /**< Swarm bounding box (min/max xyz) */
    uint32_t num_threats_detected;   /**< Total threats detected */
    uint32_t communication_health;   /**< Link quality (0-100) */
} swarm_telemetry_t;

/**
 * @brief Swarm health status
 */
typedef struct {
    char swarm_id[32];               /**< Swarm identifier */
    swarm_status_t status;           /**< Connection status */
    uint32_t last_contact_ms;        /**< Time since last contact */
    uint32_t num_drones_total;       /**< Total drones in swarm */
    uint32_t num_drones_active;      /**< Active drones */
    uint32_t num_drones_failed;      /**< Failed drones */
    float overall_health;            /**< Overall health score (0.0-1.0) */
    uint32_t packets_sent;           /**< Total packets sent */
    uint32_t packets_received;       /**< Total packets received */
    float latency_ms;                /**< Average round-trip latency */
} swarm_health_t;

/**
 * @brief Gateway message
 */
typedef struct {
    swarm_gateway_msg_type_t type;   /**< Message type */
    char target_swarm[32];           /**< Target swarm ID (empty = broadcast) */
    uint32_t timestamp;              /**< Message timestamp */
    uint32_t sequence_num;           /**< Sequence number */
    bool requires_ack;               /**< Requires acknowledgment */
    void* payload;                   /**< Message payload */
    size_t payload_size;             /**< Payload size */
} gateway_message_t;

/**
 * @brief Opaque gateway handle
 */
typedef struct swarm_gateway swarm_gateway_t;

/**
 * @brief Telemetry callback function
 *
 * Called when telemetry is received from a swarm
 *
 * @param swarm_id Swarm identifier
 * @param telemetry Telemetry data
 * @param user_data User-provided context
 */
typedef void (*telemetry_callback_t)(const char* swarm_id,
                                     const swarm_telemetry_t* telemetry,
                                     void* user_data);

/**
 * @brief Swarm event callback function
 *
 * Called when significant swarm events occur
 *
 * @param swarm_id Swarm identifier
 * @param event_type Event type string
 * @param event_data Event-specific data
 * @param user_data User-provided context
 */
typedef void (*swarm_event_callback_t)(const char* swarm_id,
                                       const char* event_type,
                                       const void* event_data,
                                       void* user_data);

/* ========================================================================
 * Core Gateway Functions
 * ======================================================================== */

/**
 * @brief Create swarm gateway
 *
 * Creates a gateway for server-to-swarm communication with the specified
 * configuration.
 *
 * @param server_brain Server NIMCP brain instance
 * @param config Gateway configuration
 * @return Gateway handle or NULL on failure
 */
swarm_gateway_t* swarm_gateway_create(brain_t server_brain,
                                      const swarm_gateway_config_t* config);

/**
 * @brief Destroy swarm gateway
 *
 * Disconnects all swarms and releases resources.
 *
 * @param gateway Gateway handle
 */
void swarm_gateway_destroy(swarm_gateway_t* gateway);

/**
 * @brief Connect to a swarm
 *
 * Establishes connection to a drone swarm using the swarm's endpoint.
 *
 * @param gateway Gateway handle
 * @param swarm_id Swarm identifier
 * @param endpoint Swarm endpoint (IP:port or similar)
 * @return 0 on success, negative on error
 */
int swarm_gateway_connect_swarm(swarm_gateway_t* gateway,
                                const char* swarm_id,
                                const char* endpoint);

/**
 * @brief Disconnect from a swarm
 *
 * Gracefully disconnects from the specified swarm.
 *
 * @param gateway Gateway handle
 * @param swarm_id Swarm identifier
 * @return 0 on success, negative on error
 */
int swarm_gateway_disconnect_swarm(swarm_gateway_t* gateway,
                                   const char* swarm_id);

/* ========================================================================
 * Message Transmission Functions
 * ======================================================================== */

/**
 * @brief Broadcast update to all swarms
 *
 * Sends a message to all connected swarms. The gateway will use P2P
 * propagation where possible (send to one drone per swarm, let it propagate).
 *
 * @param gateway Gateway handle
 * @param message Message to broadcast
 * @return Number of swarms reached, negative on error
 */
int swarm_gateway_broadcast_update(swarm_gateway_t* gateway,
                                   const gateway_message_t* message);

/**
 * @brief Send message to specific swarm
 *
 * Sends a message to a single swarm via P2P propagation.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm identifier
 * @param message Message to send
 * @return 0 on success, negative on error
 */
int swarm_gateway_send_to_swarm(swarm_gateway_t* gateway,
                                const char* swarm_id,
                                const gateway_message_t* message);

/**
 * @brief Send mission parameters
 *
 * Transmits mission objectives and parameters to a swarm.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm identifier
 * @param mission Mission parameters
 * @return 0 on success, negative on error
 */
int swarm_gateway_send_mission(swarm_gateway_t* gateway,
                               const char* swarm_id,
                               const mission_params_t* mission);

/**
 * @brief Send learning update
 *
 * Broadcasts compressed weight updates from server brain to swarms.
 * The update uses delta encoding for efficiency.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm (NULL for all swarms)
 * @param update Learning update data
 * @return Number of swarms updated, negative on error
 */
int swarm_gateway_send_learning_update(swarm_gateway_t* gateway,
                                       const char* swarm_id,
                                       const learning_update_t* update);

/**
 * @brief Send threat intelligence
 *
 * Distributes threat information to swarms for coordinated response.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm (NULL for all swarms)
 * @param threat Threat intelligence data
 * @return Number of swarms notified, negative on error
 */
int swarm_gateway_send_threat_intel(swarm_gateway_t* gateway,
                                    const char* swarm_id,
                                    const threat_intel_t* threat);

/**
 * @brief Send formation command
 *
 * Commands a swarm to adopt a specific formation.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm identifier
 * @param formation Formation parameters
 * @return 0 on success, negative on error
 */
int swarm_gateway_send_formation_cmd(swarm_gateway_t* gateway,
                                     const char* swarm_id,
                                     const formation_cmd_t* formation);

/**
 * @brief Send recall command
 *
 * Commands a swarm to return to base immediately.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm identifier
 * @param emergency If true, triggers emergency return
 * @return 0 on success, negative on error
 */
int swarm_gateway_send_recall(swarm_gateway_t* gateway,
                              const char* swarm_id,
                              bool emergency);

/**
 * @brief Send neuromodulator override
 *
 * Overrides neuromodulator levels in swarm drones for behavioral control.
 *
 * @param gateway Gateway handle
 * @param swarm_id Target swarm identifier
 * @param override Override parameters
 * @return 0 on success, negative on error
 */
int swarm_gateway_send_neuromod_override(swarm_gateway_t* gateway,
                                         const char* swarm_id,
                                         const neuromod_override_t* override);

/* ========================================================================
 * Telemetry and Status Functions
 * ======================================================================== */

/**
 * @brief Receive swarm telemetry
 *
 * Retrieves aggregated telemetry from a swarm. This function performs
 * non-blocking read of the latest available telemetry.
 *
 * @param gateway Gateway handle
 * @param swarm_id Swarm identifier
 * @param telemetry Output telemetry data
 * @return 0 on success, -EAGAIN if no new data, negative on error
 */
int swarm_gateway_receive_telemetry(swarm_gateway_t* gateway,
                                    const char* swarm_id,
                                    swarm_telemetry_t* telemetry);

/**
 * @brief Get swarm health status
 *
 * Retrieves current health and connection status for a swarm.
 *
 * @param gateway Gateway handle
 * @param swarm_id Swarm identifier
 * @param health Output health data
 * @return 0 on success, negative on error
 */
int swarm_gateway_get_swarm_status(swarm_gateway_t* gateway,
                                   const char* swarm_id,
                                   swarm_health_t* health);

/**
 * @brief Get all connected swarms
 *
 * Retrieves list of currently connected swarm IDs.
 *
 * @param gateway Gateway handle
 * @param swarm_ids Output array of swarm ID strings
 * @param max_swarms Maximum number of swarms to return
 * @return Number of swarms returned, negative on error
 */
int swarm_gateway_get_connected_swarms(swarm_gateway_t* gateway,
                                       char swarm_ids[][32],
                                       uint32_t max_swarms);

/**
 * @brief Register telemetry callback
 *
 * Registers a callback to be invoked when telemetry is received.
 *
 * @param gateway Gateway handle
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return 0 on success, negative on error
 */
int swarm_gateway_register_telemetry_callback(swarm_gateway_t* gateway,
                                              telemetry_callback_t callback,
                                              void* user_data);

/**
 * @brief Register swarm event callback
 *
 * Registers a callback for swarm events (connection, disconnection, etc.)
 *
 * @param gateway Gateway handle
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return 0 on success, negative on error
 */
int swarm_gateway_register_event_callback(swarm_gateway_t* gateway,
                                          swarm_event_callback_t callback,
                                          void* user_data);

/* ========================================================================
 * Processing and Maintenance Functions
 * ======================================================================== */

/**
 * @brief Process gateway operations
 *
 * Main processing loop that handles message routing, telemetry aggregation,
 * timeout detection, and scheduled broadcasts.
 *
 * Should be called periodically (e.g., every 10-100ms).
 *
 * @param gateway Gateway handle
 * @param timeout_ms Maximum time to spend processing (0 = non-blocking)
 * @return Number of events processed, negative on error
 */
int swarm_gateway_process(swarm_gateway_t* gateway, uint32_t timeout_ms);

/**
 * @brief Synchronize learning with swarms
 *
 * Triggers manual learning synchronization. Computes weight deltas from
 * the server brain and broadcasts to all swarms.
 *
 * @param gateway Gateway handle
 * @return Number of swarms synchronized, negative on error
 */
int swarm_gateway_sync_learning(swarm_gateway_t* gateway);

/**
 * @brief Aggregate swarm data to server
 *
 * Collects telemetry and learning data from all swarms and feeds it back
 * to the server brain for macro-level decision making.
 *
 * @param gateway Gateway handle
 * @return 0 on success, negative on error
 */
int swarm_gateway_aggregate_to_server(swarm_gateway_t* gateway);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Create learning update from brain
 *
 * Extracts weight deltas from server brain and compresses them for
 * transmission to swarms.
 *
 * @param gateway Gateway handle
 * @param update Output learning update structure
 * @return 0 on success, negative on error
 */
int swarm_gateway_create_learning_update(swarm_gateway_t* gateway,
                                         learning_update_t* update);

/**
 * @brief Free learning update
 *
 * Releases resources associated with a learning update.
 *
 * @param update Learning update to free
 */
void swarm_gateway_free_learning_update(learning_update_t* update);

/**
 * @brief Get gateway statistics
 *
 * Retrieves operational statistics for the gateway.
 *
 * @param gateway Gateway handle
 * @param num_swarms_out Output: number of connected swarms
 * @param total_drones_out Output: total drones across all swarms
 * @param msgs_sent_out Output: total messages sent
 * @param msgs_received_out Output: total messages received
 * @return 0 on success, negative on error
 */
int swarm_gateway_get_stats(swarm_gateway_t* gateway,
                            uint32_t* num_swarms_out,
                            uint32_t* total_drones_out,
                            uint64_t* msgs_sent_out,
                            uint64_t* msgs_received_out);

/**
 * @brief Convert status enum to string
 *
 * @param status Status enum value
 * @return Human-readable status string
 */
const char* swarm_gateway_status_to_string(swarm_status_t status);

/**
 * @brief Convert message type enum to string
 *
 * @param msg_type Message type enum value
 * @return Human-readable message type string
 */
const char* swarm_gateway_msg_type_to_string(swarm_gateway_msg_type_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_GATEWAY_H */
