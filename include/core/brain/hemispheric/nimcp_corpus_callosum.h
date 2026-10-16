//=============================================================================
// nimcp_corpus_callosum.h - Inter-Hemispheric Communication Bridge
//=============================================================================
/**
 * @file nimcp_corpus_callosum.h
 * @brief Corpus callosum connecting left and right brain hemispheres
 *
 * WHAT: Bio-async bridge enabling inter-hemispheric communication
 * WHY:  Coordinates bilateral processing, shares information across hemispheres
 * HOW:  Multiple typed channels with configurable bandwidth and latency
 *
 * BIOLOGICAL BASIS:
 * - ~200 million axons connecting hemispheres
 * - Anterior region: motor/prefrontal areas
 * - Body: somatosensory/auditory
 * - Splenium (posterior): visual areas
 * - Transmission latency: 5-20ms
 * - Bandwidth limited by axon count and myelination
 *
 * ARCHITECTURE:
 * ```
 *   LEFT HEMISPHERE                RIGHT HEMISPHERE
 *        │                               │
 *        │    ┌─────────────────────┐   │
 *        ├───►│  MOTOR CHANNEL      │◄──┤  Fast (ACh)
 *        │    ├─────────────────────┤   │
 *        ├───►│  SENSORY CHANNEL    │◄──┤  Medium (Glutamate)
 *        │    ├─────────────────────┤   │
 *        ├───►│  COGNITIVE CHANNEL  │◄──┤  Slow (Dopamine)
 *        │    ├─────────────────────┤   │
 *        ├───►│  EMOTIONAL CHANNEL  │◄──┤  Variable (Serotonin)
 *        │    ├─────────────────────┤   │
 *        └───►│  INHIBITORY CHANNEL │◄──┘  GABA
 *             └─────────────────────┘
 * ```
 *
 * BANDWIDTH MODES:
 * - UNLIMITED: No limits (fast simulation, not biologically accurate)
 * - REALISTIC: ~200 msg/s, 5-20ms latency (biological)
 * - RESTRICTED: ~50 msg/s, 20-50ms latency (impaired/pathological)
 * - CUSTOM: User-defined limits
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_CORPUS_CALLOSUM_H
#define NIMCP_CORPUS_CALLOSUM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_hemisphere_struct brain_hemisphere_t;
typedef struct corpus_callosum_struct corpus_callosum_t;

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Communication channel types
 *
 * BIOLOGICAL BASIS:
 * - MOTOR: Anterior callosum, fast ACh-mediated coordination
 * - SENSORY: Body region, cross-modal integration
 * - COGNITIVE: Prefrontal connections, working memory sharing
 * - EMOTIONAL: Limbic connections, emotional state synchronization
 * - INHIBITORY: GABAergic, prevents redundant processing
 */
typedef enum {
    CALLOSUM_CHANNEL_MOTOR,          // Fast motor coordination (ACh)
    CALLOSUM_CHANNEL_SENSORY,        // Sensory integration (Glutamate)
    CALLOSUM_CHANNEL_COGNITIVE,      // Working memory, attention (DA)
    CALLOSUM_CHANNEL_EMOTIONAL,      // Emotional state sharing (5-HT)
    CALLOSUM_CHANNEL_INHIBITORY,     // Cross-hemisphere inhibition (GABA)
    CALLOSUM_CHANNEL_COUNT
} callosum_channel_type_t;

/**
 * @brief Bandwidth limiting modes
 */
typedef enum {
    CALLOSUM_BW_UNLIMITED,      // No limits (fast simulation)
    CALLOSUM_BW_REALISTIC,      // ~200 msg/s, 5-20ms latency (biological)
    CALLOSUM_BW_RESTRICTED,     // ~50 msg/s, 20-50ms latency (impaired)
    CALLOSUM_BW_CUSTOM          // User-defined limits
} callosum_bandwidth_mode_t;

/**
 * @brief Message priority levels
 */
typedef enum {
    CALLOSUM_PRIORITY_LOW,       // Background sync
    CALLOSUM_PRIORITY_NORMAL,    // Standard transfer
    CALLOSUM_PRIORITY_HIGH,      // Important coordination
    CALLOSUM_PRIORITY_URGENT     // Emergency (pain, danger)
} callosum_priority_t;

/**
 * @brief Connection health states
 */
typedef enum {
    CALLOSUM_STATE_DISCONNECTED, // Split-brain (no transfer)
    CALLOSUM_STATE_IMPAIRED,     // Partial function (some channels blocked)
    CALLOSUM_STATE_DEGRADED,     // Reduced bandwidth/increased latency
    CALLOSUM_STATE_HEALTHY       // Full function
} callosum_state_t;

//=============================================================================
// Message Structures
//=============================================================================

/**
 * @brief Message transferred across callosum
 */
typedef struct {
    // Header
    hemisphere_id_t source;              // Originating hemisphere
    hemisphere_id_t destination;         // Target hemisphere
    callosum_channel_type_t channel;     // Channel type
    callosum_priority_t priority;        // Message priority
    uint64_t timestamp;                  // Send time (for latency)
    uint32_t sequence_num;               // For ordering

    // Payload
    uint32_t message_type;               // Application-defined type
    void* data;                          // Message payload
    size_t data_size;                    // Payload size

    // Delivery info
    uint64_t scheduled_delivery;         // When to deliver (latency sim)
    bool delivered;                      // Delivery status
} callosum_message_t;

/**
 * @brief Message queue for one direction
 */
typedef struct {
    callosum_message_t* messages;        // Ring buffer
    uint32_t capacity;                   // Buffer size
    uint32_t head;                       // Write index
    uint32_t tail;                       // Read index
    uint32_t count;                      // Current message count
    uint64_t total_enqueued;             // Lifetime counter
    uint64_t total_delivered;            // Lifetime counter
    uint64_t total_dropped;              // Dropped due to overflow
    void* mutex;                         // nimcp_mutex_t*
} callosum_message_queue_t;

//=============================================================================
// Channel Structure
//=============================================================================

/**
 * @brief Single callosum channel
 */
typedef struct {
    callosum_channel_type_t type;        // Channel type
    bool enabled;                        // Channel active

    // Bio-async integration (uses ACETYLCHOLINE channel for fast transfer)
    uint32_t bio_channel_id;             // Underlying bio-async channel ID

    // Bandwidth limiting (per-channel)
    uint32_t max_msgs_per_second;        // 0 = use global limit
    uint32_t current_msg_count;          // This window
    uint64_t window_start;               // Window start time

    // Latency configuration (per-channel)
    float min_latency_ms;                // Minimum transfer time
    float max_latency_ms;                // Maximum transfer time
    float avg_latency_ms;                // Running average

    // Statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_transferred;
} callosum_channel_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Corpus callosum configuration
 */
typedef struct {
    // Bandwidth configuration
    callosum_bandwidth_mode_t bandwidth_mode;
    uint32_t custom_max_msgs_per_second;  // For CUSTOM mode
    float custom_min_latency_ms;          // For CUSTOM mode
    float custom_max_latency_ms;          // For CUSTOM mode

    // Per-channel overrides (0 = use global)
    uint32_t channel_bandwidth[CALLOSUM_CHANNEL_COUNT];
    float channel_min_latency[CALLOSUM_CHANNEL_COUNT];
    float channel_max_latency[CALLOSUM_CHANNEL_COUNT];

    // Queue configuration
    uint32_t queue_capacity;              // Per-direction queue size
    bool drop_on_overflow;                // Drop or block on full queue

    // Connection
    float initial_connection_strength;    // 0.0-1.0

    // Bio-async
    bool enable_bio_async;
} callosum_config_t;

/**
 * @brief Statistics for callosum operations
 */
typedef struct {
    // Message counts
    uint64_t total_messages_left_to_right;
    uint64_t total_messages_right_to_left;
    uint64_t total_bytes_transferred;

    // Per-channel stats
    uint64_t channel_messages[CALLOSUM_CHANNEL_COUNT];
    float channel_avg_latency[CALLOSUM_CHANNEL_COUNT];

    // Bandwidth
    float current_bandwidth_utilization;  // 0.0-1.0
    uint64_t messages_dropped;
    uint64_t messages_delayed;

    // Latency
    float avg_latency_ms;
    float max_latency_ms;
    float min_latency_ms;

    // Connection health
    callosum_state_t current_state;
    uint64_t state_changes;
    uint64_t disconnection_events;
    uint64_t reconnection_events;
} callosum_stats_t;

//=============================================================================
// Main Structure
//=============================================================================

/**
 * @brief Corpus callosum inter-hemispheric bridge
 *
 * WHAT: Connection between left and right brain hemispheres
 * WHY:  Enables bilateral coordination and information sharing
 * HOW:  Bio-async channels with bandwidth limiting and latency simulation
 */
struct corpus_callosum_struct {
    // === Connected Hemispheres ===
    brain_hemisphere_t* left;
    brain_hemisphere_t* right;

    // === Communication Channels ===
    callosum_channel_t channels[CALLOSUM_CHANNEL_COUNT];

    // === Bandwidth Configuration ===
    callosum_bandwidth_mode_t bandwidth_mode;
    uint32_t max_messages_per_second;    // Global limit
    uint32_t current_message_count;      // This window
    uint64_t bandwidth_window_start;     // Window start time

    // === Latency Configuration ===
    bool enable_latency_simulation;
    float min_latency_ms;                // Global minimum
    float max_latency_ms;                // Global maximum

    // === Message Queues ===
    callosum_message_queue_t left_to_right;
    callosum_message_queue_t right_to_left;

    // === Connection State ===
    bool is_connected;                   // false = split-brain
    float connection_strength;           // 0.0-1.0 (degradation)
    callosum_state_t state;

    // === Bio-Async Integration ===
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // === Statistics ===
    callosum_stats_t stats;

    // === Thread Safety ===
    void* mutex;                         // nimcp_mutex_t*
};

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default callosum configuration
 *
 * @return Default configuration (REALISTIC bandwidth mode)
 */
callosum_config_t callosum_default_config(void);

/**
 * @brief Create corpus callosum
 *
 * WHAT: Initialize inter-hemispheric bridge
 * WHY:  Required for bilateral brain coordination
 * HOW:  Create channels, queues, connect to bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return Callosum instance or NULL on error
 */
corpus_callosum_t* callosum_create(const callosum_config_t* config);

/**
 * @brief Destroy corpus callosum
 *
 * @param cc Callosum to destroy
 */
void callosum_destroy(corpus_callosum_t* cc);

/**
 * @brief Connect hemispheres to callosum
 *
 * @param cc Callosum instance
 * @param left Left hemisphere
 * @param right Right hemisphere
 * @return 0 on success
 */
int callosum_connect_hemispheres(
    corpus_callosum_t* cc,
    brain_hemisphere_t* left,
    brain_hemisphere_t* right
);

//=============================================================================
// Messaging
//=============================================================================

/**
 * @brief Send message across callosum
 *
 * WHAT: Transfer data between hemispheres
 * WHY:  Inter-hemispheric coordination
 * HOW:  Queue message with latency delay, respect bandwidth limits
 *
 * @param cc Callosum instance
 * @param from Source hemisphere
 * @param channel Channel type
 * @param priority Message priority
 * @param message_type Application message type
 * @param data Message payload
 * @param size Payload size
 * @return 0 on success, -1 on failure, 1 if queued (bandwidth limited)
 */
int callosum_send(
    corpus_callosum_t* cc,
    hemisphere_id_t from,
    callosum_channel_type_t channel,
    callosum_priority_t priority,
    uint32_t message_type,
    const void* data,
    size_t size
);

/**
 * @brief Receive pending messages for a hemisphere
 *
 * @param cc Callosum instance
 * @param hemisphere Target hemisphere
 * @param messages Output array (caller allocated)
 * @param max_messages Maximum messages to receive
 * @return Number of messages received
 */
int callosum_receive(
    corpus_callosum_t* cc,
    hemisphere_id_t hemisphere,
    callosum_message_t* messages,
    uint32_t max_messages
);

/**
 * @brief Process queued messages (deliver after latency)
 *
 * WHAT: Deliver messages that have completed latency delay
 * WHY:  Simulates biological transmission time
 * HOW:  Check scheduled_delivery vs current time
 *
 * @param cc Callosum instance
 * @return Number of messages delivered
 */
int callosum_process_queues(corpus_callosum_t* cc);

/**
 * @brief Flush all pending messages (immediate delivery)
 *
 * @param cc Callosum instance
 * @return Number of messages flushed
 */
int callosum_flush(corpus_callosum_t* cc);

//=============================================================================
// Bandwidth Control
//=============================================================================

/**
 * @brief Set bandwidth mode
 *
 * @param cc Callosum instance
 * @param mode Bandwidth mode
 * @return 0 on success
 */
int callosum_set_bandwidth_mode(
    corpus_callosum_t* cc,
    callosum_bandwidth_mode_t mode
);

/**
 * @brief Set custom bandwidth limit
 *
 * @param cc Callosum instance
 * @param msgs_per_second Maximum messages per second (0 = unlimited)
 * @return 0 on success
 */
int callosum_set_bandwidth_limit(
    corpus_callosum_t* cc,
    uint32_t msgs_per_second
);

/**
 * @brief Get base bandwidth limit
 *
 * @param cc Callosum instance
 * @return Base bandwidth in messages per second
 */
uint32_t corpus_callosum_get_base_bandwidth(const corpus_callosum_t* cc);

/**
 * @brief Set per-channel bandwidth
 *
 * @param cc Callosum instance
 * @param channel Channel type
 * @param msgs_per_second Maximum messages per second
 * @return 0 on success
 */
int callosum_set_channel_bandwidth(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    uint32_t msgs_per_second
);

/**
 * @brief Get current bandwidth utilization
 *
 * @param cc Callosum instance
 * @return Utilization 0.0-1.0
 */
float callosum_get_bandwidth_utilization(const corpus_callosum_t* cc);

//=============================================================================
// Latency Control
//=============================================================================

/**
 * @brief Set latency range
 *
 * @param cc Callosum instance
 * @param min_ms Minimum latency in milliseconds
 * @param max_ms Maximum latency in milliseconds
 * @return 0 on success
 */
int callosum_set_latency(
    corpus_callosum_t* cc,
    float min_ms,
    float max_ms
);

/**
 * @brief Set per-channel latency
 *
 * @param cc Callosum instance
 * @param channel Channel type
 * @param min_ms Minimum latency
 * @param max_ms Maximum latency
 * @return 0 on success
 */
int callosum_set_channel_latency(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    float min_ms,
    float max_ms
);

/**
 * @brief Enable/disable latency simulation
 *
 * @param cc Callosum instance
 * @param enable true to enable
 * @return 0 on success
 */
int callosum_enable_latency_simulation(
    corpus_callosum_t* cc,
    bool enable
);

/**
 * @brief Get average latency
 *
 * @param cc Callosum instance
 * @return Average latency in milliseconds
 */
float callosum_get_avg_latency(const corpus_callosum_t* cc);

//=============================================================================
// Connection Control
//=============================================================================

/**
 * @brief Disconnect callosum (split-brain)
 *
 * WHAT: Sever inter-hemispheric connection
 * WHY:  Simulate split-brain experiments or pathology
 * HOW:  Block all message transfer, set state to DISCONNECTED
 *
 * @param cc Callosum instance
 * @return 0 on success
 */
int callosum_disconnect(corpus_callosum_t* cc);

/**
 * @brief Reconnect callosum
 *
 * @param cc Callosum instance
 * @return 0 on success
 */
int callosum_reconnect(corpus_callosum_t* cc);

/**
 * @brief Check if callosum is connected
 *
 * @param cc Callosum instance
 * @return true if connected
 */
bool callosum_is_connected(const corpus_callosum_t* cc);

/**
 * @brief Set connection strength (degradation)
 *
 * WHAT: Adjust effective connection strength
 * WHY:  Model partial callosal damage or development
 * HOW:  Scales bandwidth proportionally
 *
 * @param cc Callosum instance
 * @param strength 0.0 (no function) to 1.0 (full function)
 * @return 0 on success
 */
int callosum_set_connection_strength(
    corpus_callosum_t* cc,
    float strength
);

/**
 * @brief Get connection strength
 *
 * @param cc Callosum instance
 * @return Connection strength 0.0-1.0
 */
float callosum_get_connection_strength(const corpus_callosum_t* cc);

/**
 * @brief Get current connection state
 *
 * @param cc Callosum instance
 * @return Connection state
 */
callosum_state_t callosum_get_state(const corpus_callosum_t* cc);

//=============================================================================
// Channel Control
//=============================================================================

/**
 * @brief Enable/disable specific channel
 *
 * @param cc Callosum instance
 * @param channel Channel type
 * @param enable true to enable
 * @return 0 on success
 */
int callosum_set_channel_enabled(
    corpus_callosum_t* cc,
    callosum_channel_type_t channel,
    bool enable
);

/**
 * @brief Check if channel is enabled
 *
 * @param cc Callosum instance
 * @param channel Channel type
 * @return true if enabled
 */
bool callosum_is_channel_enabled(
    const corpus_callosum_t* cc,
    callosum_channel_type_t channel
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get callosum statistics
 *
 * @param cc Callosum instance
 * @param stats Output statistics
 * @return 0 on success
 */
int callosum_get_stats(
    const corpus_callosum_t* cc,
    callosum_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param cc Callosum instance
 * @return 0 on success
 */
int callosum_reset_stats(corpus_callosum_t* cc);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param cc Callosum instance
 * @return 0 on success
 */
int callosum_connect_bio_async(corpus_callosum_t* cc);

/**
 * @brief Disconnect from bio-async router
 *
 * @param cc Callosum instance
 * @return 0 on success
 */
int callosum_disconnect_bio_async(corpus_callosum_t* cc);

/**
 * @brief Check bio-async connection
 *
 * @param cc Callosum instance
 * @return true if connected
 */
bool callosum_is_bio_async_connected(const corpus_callosum_t* cc);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get channel name string
 *
 * @param channel Channel type
 * @return Human-readable name
 */
const char* callosum_channel_name(callosum_channel_type_t channel);

/**
 * @brief Get bandwidth mode name string
 *
 * @param mode Bandwidth mode
 * @return Human-readable name
 */
const char* callosum_bandwidth_mode_name(callosum_bandwidth_mode_t mode);

/**
 * @brief Get state name string
 *
 * @param state Connection state
 * @return Human-readable name
 */
const char* callosum_state_name(callosum_state_t state);

/**
 * @brief Validate callosum configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool callosum_validate_config(const callosum_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CORPUS_CALLOSUM_H
