//=============================================================================
// nimcp_neural_link_protocol.h - Neural Link Protocol for Brain-to-Brain Comm
//=============================================================================
/**
 * @file nimcp_neural_link_protocol.h
 * @brief Ruggedized peer-to-peer protocol for master/sub-brain communication
 *
 * WHAT: Encrypted, resilient protocol for distributed brain swarms
 * WHY:  Enable coordination across drones/robots/IoT in chaotic environments
 * HOW:  Three operating modes (Standard/Tactical/Stealth) with auto-adaptation
 *
 * DESIGN PHILOSOPHY:
 * Named after biological neural links, this protocol enables multiple NIMCP
 * brains to coordinate as a distributed cognitive system. Designed for:
 * - War zones (jamming, interception, node destruction)
 * - Natural disasters (infrastructure collapse, power loss)
 * - Search & rescue (time-critical, victim tracking)
 * - Covert operations (minimal RF signature)
 *
 * OPERATING MODES:
 * 1. STANDARD:  Full handshake, persistent sessions, maximum efficiency
 * 2. TACTICAL:  Pre-shared keys, self-contained messages, masterless capable
 * 3. STEALTH:   Burst transmission, traffic shaping, emissions control
 *
 * SECURITY FEATURES:
 * - AES-256-GCM encryption with per-message nonces
 * - Pre-shared key support (zero round-trip in tactical/stealth)
 * - Time-bounded authentication (replay protection)
 * - Perfect forward secrecy (session keys)
 * - Traffic analysis resistance (fixed-size packets in stealth)
 *
 * RESILIENCE FEATURES:
 * - Masterless operation (Byzantine-fault-tolerant consensus)
 * - Mesh relay (any node can forward messages)
 * - Store-and-forward (buffer during blackout)
 * - Auto-mode switching based on environment
 * - Idempotent commands (safe duplicate delivery)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_NEURAL_LINK_PROTOCOL_H
#define NIMCP_NEURAL_LINK_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Protocol Constants
//=============================================================================

#define NLP_VERSION                 1
#define NLP_MAX_PAYLOAD             65535
#define NLP_MAX_PEERS               256
#define NLP_KEY_SLOTS               8
#define NLP_KEY_SIZE                32      // AES-256
#define NLP_NONCE_SIZE              12      // AES-GCM nonce
#define NLP_TAG_SIZE                16      // AES-GCM auth tag
#define NLP_HEADER_SIZE             36
#define NLP_MIN_MESSAGE_SIZE        (NLP_HEADER_SIZE + NLP_TAG_SIZE)
#define NLP_TIMESTAMP_WINDOW        60      // Seconds (replay protection)
#define NLP_STEALTH_PACKET_SIZE     256     // Fixed size for traffic analysis resistance
#define NLP_BURST_INTERVAL_DEFAULT  30      // Seconds between stealth bursts
#define NLP_HEARTBEAT_INTERVAL      5000    // Milliseconds
#define NLP_SESSION_TIMEOUT         30000   // Milliseconds
#define NLP_MAX_RETRIES             3
#define NLP_MAGIC                   0x4E4C50 // 'NLP' (only used internally)

//=============================================================================
// Operating Modes
//=============================================================================

/**
 * @brief Protocol operating modes
 *
 * STANDARD:  Maximum efficiency, requires stable network
 * TACTICAL:  Chaos-resilient, works with intermittent connectivity
 * STEALTH:   Minimal RF emissions, covert operations
 */
typedef enum {
    NLP_MODE_STANDARD = 0,    /**< Full handshake, persistent sessions */
    NLP_MODE_TACTICAL = 1,    /**< Pre-shared keys, self-contained messages */
    NLP_MODE_STEALTH  = 2     /**< Burst TX, traffic shaping, EMCON */
} nlp_mode_t;

/**
 * @brief Emissions Control (EMCON) levels for stealth mode
 */
typedef enum {
    NLP_EMCON_NORMAL    = 0,  /**< Normal operations */
    NLP_EMCON_REDUCED   = 1,  /**< Reduce TX power and frequency */
    NLP_EMCON_RECEIVE   = 2,  /**< Receive only, no transmit */
    NLP_EMCON_SILENT    = 3,  /**< Full radio silence, autonomous only */
    NLP_EMCON_EMERGENCY = 4   /**< Break-glass: critical message override */
} nlp_emcon_level_t;

//=============================================================================
// Message Types
//=============================================================================

/**
 * @brief Neural Link Protocol message types
 */
typedef enum {
    // Session management (0x00xx) - Standard mode
    NLP_MSG_HANDSHAKE_REQ    = 0x0001,  /**< Initiate session */
    NLP_MSG_HANDSHAKE_RESP   = 0x0002,  /**< Session response */
    NLP_MSG_HANDSHAKE_FINAL  = 0x0003,  /**< Complete handshake */
    NLP_MSG_KEY_ROTATE       = 0x0004,  /**< Rotate session key */
    NLP_MSG_DISCONNECT       = 0x0005,  /**< Graceful disconnect */
    NLP_MSG_SESSION_RESUME   = 0x0006,  /**< Resume previous session */

    // Neural synchronization (0x01xx)
    NLP_MSG_SPIKE_BATCH      = 0x0100,  /**< Batch of neural spikes */
    NLP_MSG_WEIGHT_DELTA     = 0x0101,  /**< Synaptic weight changes */
    NLP_MSG_WEIGHT_FULL      = 0x0102,  /**< Full weight matrix sync */
    NLP_MSG_STATE_SYNC       = 0x0103,  /**< Cognitive state snapshot */
    NLP_MSG_GRADIENT_PUSH    = 0x0104,  /**< Learning gradients */
    NLP_MSG_ACTIVATION_SYNC  = 0x0105,  /**< Neuron activations */

    // Swarm coordination (0x02xx)
    NLP_MSG_HEARTBEAT        = 0x0200,  /**< Keepalive */
    NLP_MSG_PEER_ANNOUNCE    = 0x0201,  /**< Announce presence */
    NLP_MSG_PEER_LIST        = 0x0202,  /**< Share known peers */
    NLP_MSG_MASTER_ELECTION  = 0x0203,  /**< Master brain election */
    NLP_MSG_CONSENSUS_VOTE   = 0x0204,  /**< Byzantine consensus vote */
    NLP_MSG_CONSENSUS_COMMIT = 0x0205,  /**< Consensus commit */
    NLP_MSG_ROLE_ASSIGN      = 0x0206,  /**< Assign role to sub-brain */

    // Tactical/Emergency (0x03xx)
    NLP_MSG_PRIORITY_CMD     = 0x0300,  /**< High-priority command */
    NLP_MSG_EMERGENCY        = 0x0301,  /**< Break-glass emergency */
    NLP_MSG_RELAY            = 0x0302,  /**< Mesh relay for another node */
    NLP_MSG_ACK              = 0x0303,  /**< Acknowledgment */
    NLP_MSG_NACK             = 0x0304,  /**< Negative acknowledgment */
    NLP_MSG_RESEND_REQ       = 0x0305,  /**< Request message resend */

    // Stealth mode (0x04xx)
    NLP_MSG_BURST_SYNC       = 0x0400,  /**< Compressed burst update */
    NLP_MSG_CHAFF            = 0x0401,  /**< Fake traffic (decoys) */
    NLP_MSG_LISTEN_WINDOW    = 0x0402,  /**< Schedule receive window */
    NLP_MSG_EMCON_CHANGE     = 0x0403,  /**< Change EMCON level */

    // Disaster/SAR extensions (0x05xx)
    NLP_MSG_LOCATION_UPDATE  = 0x0500,  /**< GPS/position update */
    NLP_MSG_VICTIM_REPORT    = 0x0501,  /**< Search & rescue victim */
    NLP_MSG_SENSOR_DATA      = 0x0502,  /**< Environmental sensors */
    NLP_MSG_HAZARD_ALERT     = 0x0503,  /**< Hazard warning */
    NLP_MSG_RESOURCE_STATUS  = 0x0504,  /**< Battery, fuel, etc. */

    // Diagnostics (0x0Fxx)
    NLP_MSG_PING             = 0x0F00,  /**< Latency test */
    NLP_MSG_PONG             = 0x0F01,  /**< Latency response */
    NLP_MSG_STATS_REQ        = 0x0F02,  /**< Request statistics */
    NLP_MSG_STATS_RESP       = 0x0F03,  /**< Statistics response */
    NLP_MSG_DEBUG            = 0x0FFF   /**< Debug message */
} nlp_msg_type_t;

//=============================================================================
// Priority Levels
//=============================================================================

/**
 * @brief Message priority for QoS
 */
typedef enum {
    NLP_PRIORITY_LOW      = 0,  /**< Background sync, can be delayed */
    NLP_PRIORITY_NORMAL   = 1,  /**< Standard operations */
    NLP_PRIORITY_HIGH     = 2,  /**< Time-sensitive (spike batches) */
    NLP_PRIORITY_CRITICAL = 3   /**< Emergency, preempts all others */
} nlp_priority_t;

//=============================================================================
// Message Header Flags
//=============================================================================

#define NLP_FLAG_ENCRYPTED      0x01  /**< Payload is encrypted */
#define NLP_FLAG_COMPRESSED     0x02  /**< Payload is compressed */
#define NLP_FLAG_FRAGMENTED     0x04  /**< Message is fragmented */
#define NLP_FLAG_LAST_FRAGMENT  0x08  /**< Last fragment */
#define NLP_FLAG_ACK_REQUIRED   0x10  /**< Sender expects ACK */

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @brief Neural Link Protocol message header
 *
 * MEMORY LAYOUT (36 bytes, packed):
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Byte 0:   version(4) | mode(2) | priority(2)               │
 * │ Byte 1:   key_slot(3) | flags(5)                           │
 * │ Byte 2-3: msg_type                                          │
 * │ Byte 4-7: sender_id                                         │
 * │ Byte 8-11: timestamp                                        │
 * │ Byte 12-23: nonce[12]                                       │
 * │ Byte 24-25: sequence                                        │
 * │ Byte 26-27: ack_sequence                                    │
 * │ Byte 28-31: dest_id                                         │
 * │ Byte 32-33: payload_len                                     │
 * │ Byte 34-35: header_crc                                      │
 * └─────────────────────────────────────────────────────────────┘
 */
typedef struct __attribute__((packed)) {
    uint8_t  version_mode_priority;   /**< version(4) | mode(2) | priority(2) */
    uint8_t  key_slot_flags;          /**< key_slot(3) | flags(5) */
    uint16_t msg_type;                /**< Message type (network byte order) */
    uint32_t sender_id;               /**< Brain ID (hash of identity) */
    uint32_t timestamp;               /**< Unix timestamp (seconds) */
    uint8_t  nonce[NLP_NONCE_SIZE];   /**< Unique per message */
    uint16_t sequence;                /**< Message sequence number */
    uint16_t ack_sequence;            /**< Acknowledges up to this sequence */
    uint32_t dest_id;                 /**< Destination brain (0 = broadcast) */
    uint16_t payload_len;             /**< Encrypted payload length */
    uint16_t header_crc;              /**< CRC-16 of bytes 0-33 */
} nlp_header_t;

// Header field accessors
#define NLP_GET_VERSION(h)    (((h)->version_mode_priority >> 4) & 0x0F)
#define NLP_GET_MODE(h)       (((h)->version_mode_priority >> 2) & 0x03)
#define NLP_GET_PRIORITY(h)   ((h)->version_mode_priority & 0x03)
#define NLP_GET_KEY_SLOT(h)   (((h)->key_slot_flags >> 5) & 0x07)
#define NLP_GET_FLAGS(h)      ((h)->key_slot_flags & 0x1F)

#define NLP_SET_VERSION(h, v)   ((h)->version_mode_priority = ((h)->version_mode_priority & 0x0F) | (((v) & 0x0F) << 4))
#define NLP_SET_MODE(h, m)      ((h)->version_mode_priority = ((h)->version_mode_priority & 0xF3) | (((m) & 0x03) << 2))
#define NLP_SET_PRIORITY(h, p)  ((h)->version_mode_priority = ((h)->version_mode_priority & 0xFC) | ((p) & 0x03))
#define NLP_SET_KEY_SLOT(h, k)  ((h)->key_slot_flags = ((h)->key_slot_flags & 0x1F) | (((k) & 0x07) << 5))
#define NLP_SET_FLAGS(h, f)     ((h)->key_slot_flags = ((h)->key_slot_flags & 0xE0) | ((f) & 0x1F))

/**
 * @brief Complete NLP message
 */
typedef struct {
    nlp_header_t header;              /**< Message header */
    uint8_t* payload;                 /**< Encrypted payload (allocated) */
    uint8_t auth_tag[NLP_TAG_SIZE];   /**< AES-GCM authentication tag */
} nlp_message_t;

//=============================================================================
// Session and Peer Management
//=============================================================================

/**
 * @brief Session state
 */
typedef enum {
    NLP_SESSION_DISCONNECTED = 0,
    NLP_SESSION_HANDSHAKE_SENT,
    NLP_SESSION_HANDSHAKE_RECEIVED,
    NLP_SESSION_ESTABLISHED,
    NLP_SESSION_SUSPENDED,
    NLP_SESSION_ERROR
} nlp_session_state_t;

/**
 * @brief Pre-shared key slot
 */
typedef struct {
    uint8_t key[NLP_KEY_SIZE];        /**< 256-bit key */
    uint32_t key_id;                  /**< Key identifier */
    uint64_t valid_from;              /**< Validity start (Unix timestamp) */
    uint64_t valid_until;             /**< Validity end (Unix timestamp) */
    bool active;                      /**< Key is currently active */
} nlp_key_slot_t;

/**
 * @brief Peer information
 */
typedef struct {
    uint32_t peer_id;                 /**< Peer brain ID */
    char address[64];                 /**< IP address or hostname */
    uint16_t port;                    /**< Port number */

    // Session state
    nlp_session_state_t session_state;
    uint8_t session_key[NLP_KEY_SIZE]; /**< Negotiated session key */
    uint16_t tx_sequence;             /**< Next sequence to send */
    uint16_t rx_sequence;             /**< Last sequence received */

    // Timing
    uint64_t last_seen_ms;            /**< Last message timestamp */
    uint64_t last_sent_ms;            /**< Last message sent timestamp */
    uint32_t rtt_ms;                  /**< Round-trip time estimate */

    // Statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t retransmits;

    // Health
    bool healthy;                     /**< Peer is responsive */
    uint32_t missed_heartbeats;       /**< Consecutive missed heartbeats */
    float packet_loss_rate;           /**< Estimated packet loss */
} nlp_peer_t;

//=============================================================================
// Environment Detection
//=============================================================================

/**
 * @brief Environment metrics for auto-mode selection
 */
typedef struct {
    // Network health
    float packet_loss_rate;           /**< 0.0 - 1.0 */
    float avg_latency_ms;             /**< Average round-trip time */
    float jitter_ms;                  /**< Latency variance */
    uint32_t jamming_events;          /**< Suspected jamming count */

    // Connectivity
    uint32_t connected_peers;         /**< Reachable peers */
    bool master_reachable;            /**< Can reach master brain */
    uint32_t master_timeout_ms;       /**< Time since last master contact */

    // Threat indicators
    bool rf_anomaly_detected;         /**< Unusual RF activity */
    bool replay_attempt_detected;     /**< Replay attack detected */
    bool unknown_peer_contact;        /**< Contact from unknown ID */

    // Power status
    float battery_percent;            /**< Remaining battery (0-100) */
    bool low_power_mode;              /**< Power conservation active */

    // Location
    bool gps_available;               /**< GPS fix available */
    double latitude;
    double longitude;
    float altitude_m;
} nlp_environment_t;

//=============================================================================
// Neural Sync Payloads
//=============================================================================

/**
 * @brief Spike batch payload
 */
typedef struct {
    uint32_t batch_id;                /**< Batch identifier */
    uint32_t timestamp_us;            /**< Microseconds offset from msg timestamp */
    uint16_t spike_count;             /**< Number of spikes */
    uint16_t reserved;
    // Followed by: uint32_t neuron_ids[spike_count]
    // Followed by: uint16_t spike_times[spike_count] (us offset)
} nlp_spike_batch_t;

/**
 * @brief Weight delta payload
 */
typedef struct {
    uint32_t base_version;            /**< Base weight version this delta applies to */
    uint32_t new_version;             /**< Resulting weight version */
    uint16_t delta_count;             /**< Number of weight changes */
    uint16_t reserved;
    // Followed by: nlp_weight_delta_entry_t entries[delta_count]
} nlp_weight_delta_header_t;

typedef struct {
    uint32_t synapse_id;              /**< Synapse identifier */
    float old_weight;                 /**< Previous weight */
    float new_weight;                 /**< New weight */
} nlp_weight_delta_entry_t;

/**
 * @brief Cognitive state sync payload
 */
typedef struct {
    uint32_t state_version;           /**< State version number */
    uint32_t neuron_count;            /**< Number of neurons */
    uint32_t synapse_count;           /**< Number of synapses */
    uint32_t flags;                   /**< State flags */
    // Followed by compressed state data
} nlp_state_sync_t;

//=============================================================================
// Disaster/SAR Extensions
//=============================================================================

/**
 * @brief GPS location
 */
typedef struct {
    double latitude;                  /**< Decimal degrees */
    double longitude;                 /**< Decimal degrees */
    float altitude_m;                 /**< Meters above sea level */
    float accuracy_m;                 /**< Horizontal accuracy */
    float heading_deg;                /**< Direction of travel (0-360) */
    float speed_mps;                  /**< Meters per second */
    uint32_t fix_timestamp;           /**< Unix timestamp of fix */
    uint8_t fix_quality;              /**< 0=none, 1=GPS, 2=DGPS, 3=RTK */
} nlp_location_t;

/**
 * @brief Triage levels for SAR
 */
typedef enum {
    NLP_TRIAGE_DECEASED  = 0,         /**< Black - deceased */
    NLP_TRIAGE_IMMEDIATE = 1,         /**< Red - immediate care needed */
    NLP_TRIAGE_DELAYED   = 2,         /**< Yellow - can wait */
    NLP_TRIAGE_MINOR     = 3,         /**< Green - walking wounded */
    NLP_TRIAGE_UNKNOWN   = 4          /**< Unknown status */
} nlp_triage_level_t;

/**
 * @brief Victim report for SAR
 */
typedef struct {
    uint32_t victim_id;               /**< Unique identifier */
    nlp_location_t location;          /**< Where found */
    nlp_triage_level_t triage;        /**< Medical priority */
    uint8_t mobility;                 /**< 0=immobile, 1=assisted, 2=ambulatory */
    uint8_t consciousness;            /**< 0=unresponsive, 1=responsive */
    uint8_t breathing;                /**< 0=not breathing, 1=breathing */
    uint8_t reserved;
    uint16_t notes_len;               /**< Length of notes */
    // Followed by: char notes[notes_len]
} nlp_victim_report_t;

/**
 * @brief Environmental sensor data
 */
typedef struct {
    float temperature_c;              /**< Ambient temperature */
    float humidity_percent;           /**< Relative humidity */
    float pressure_hpa;               /**< Atmospheric pressure */
    float radiation_usv_h;            /**< Microsieverts per hour */
    float co_ppm;                     /**< Carbon monoxide */
    float co2_ppm;                    /**< Carbon dioxide */
    float ch4_ppm;                    /**< Methane */
    float h2s_ppm;                    /**< Hydrogen sulfide */
    float o2_percent;                 /**< Oxygen percentage */
    float visibility_m;               /**< Visibility distance */
    float wind_speed_mps;             /**< Wind speed */
    float wind_direction_deg;         /**< Wind direction */
    uint32_t sensor_bitmap;           /**< Which sensors are valid */
} nlp_sensor_data_t;

// Sensor bitmap flags
#define NLP_SENSOR_TEMPERATURE  0x0001
#define NLP_SENSOR_HUMIDITY     0x0002
#define NLP_SENSOR_PRESSURE     0x0004
#define NLP_SENSOR_RADIATION    0x0008
#define NLP_SENSOR_CO           0x0010
#define NLP_SENSOR_CO2          0x0020
#define NLP_SENSOR_CH4          0x0040
#define NLP_SENSOR_H2S          0x0080
#define NLP_SENSOR_O2           0x0100
#define NLP_SENSOR_VISIBILITY   0x0200
#define NLP_SENSOR_WIND         0x0400

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief NLP node configuration
 */
typedef struct {
    // Identity
    uint32_t brain_id;                /**< This brain's unique ID */
    bool is_master;                   /**< This brain is the master */

    // Operating mode
    nlp_mode_t default_mode;          /**< Default operating mode */
    bool auto_mode_switch;            /**< Enable automatic mode switching */

    // Network
    char bind_address[64];            /**< Address to bind to */
    uint16_t port;                    /**< Port to listen on */
    uint32_t max_peers;               /**< Maximum peer connections */

    // Timing
    uint32_t heartbeat_interval_ms;   /**< Heartbeat interval */
    uint32_t session_timeout_ms;      /**< Session timeout */
    uint32_t handshake_timeout_ms;    /**< Handshake timeout */

    // Stealth mode
    uint32_t burst_interval_s;        /**< Seconds between bursts */
    nlp_emcon_level_t initial_emcon;  /**< Initial EMCON level */

    // Security
    bool require_encryption;          /**< Require all messages encrypted */
    uint32_t key_rotation_interval_s; /**< Session key rotation interval */

    // Pre-shared keys
    nlp_key_slot_t psk[NLP_KEY_SLOTS]; /**< Pre-shared key slots */

    // Callbacks
    void* user_data;                  /**< User context for callbacks */
} nlp_config_t;

/**
 * @brief NLP statistics
 */
typedef struct {
    // Message counts
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t messages_relayed;

    // Byte counts
    uint64_t bytes_sent;
    uint64_t bytes_received;

    // Errors
    uint64_t encryption_errors;
    uint64_t decryption_errors;
    uint64_t replay_attacks_blocked;
    uint64_t invalid_signatures;

    // Network
    uint32_t active_sessions;
    uint32_t connected_peers;
    float avg_latency_ms;
    float packet_loss_rate;

    // Mode stats
    uint64_t mode_switches;
    nlp_mode_t current_mode;
    nlp_emcon_level_t current_emcon;
} nlp_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief NLP node handle (opaque)
 */
typedef struct nlp_node_struct* nlp_node_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Message received callback
 * @param node NLP node
 * @param peer Peer that sent the message
 * @param msg Received message
 * @param user_data User context
 */
typedef void (*nlp_message_callback_t)(
    nlp_node_t node,
    const nlp_peer_t* peer,
    const nlp_message_t* msg,
    void* user_data
);

/**
 * @brief Peer state change callback
 * @param node NLP node
 * @param peer Peer that changed state
 * @param old_state Previous state
 * @param new_state New state
 * @param user_data User context
 */
typedef void (*nlp_peer_callback_t)(
    nlp_node_t node,
    const nlp_peer_t* peer,
    nlp_session_state_t old_state,
    nlp_session_state_t new_state,
    void* user_data
);

/**
 * @brief Mode change callback
 * @param node NLP node
 * @param old_mode Previous mode
 * @param new_mode New mode
 * @param reason Reason for change
 * @param user_data User context
 */
typedef void (*nlp_mode_callback_t)(
    nlp_node_t node,
    nlp_mode_t old_mode,
    nlp_mode_t new_mode,
    const char* reason,
    void* user_data
);

//=============================================================================
// Node Lifecycle API
//=============================================================================

/**
 * @brief Create NLP node
 * @param config Configuration (NULL for defaults)
 * @return Node handle or NULL on failure
 */
nlp_node_t nlp_node_create(const nlp_config_t* config);

/**
 * @brief Destroy NLP node
 * @param node Node handle
 */
void nlp_node_destroy(nlp_node_t node);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
nlp_config_t nlp_config_default(void);

/**
 * @brief Start NLP node
 * @param node Node handle
 * @return 0 on success, negative on error
 */
int nlp_node_start(nlp_node_t node);

/**
 * @brief Stop NLP node
 * @param node Node handle
 * @return 0 on success, negative on error
 */
int nlp_node_stop(nlp_node_t node);

//=============================================================================
// Peer Management API
//=============================================================================

/**
 * @brief Connect to peer
 * @param node Node handle
 * @param address Peer address
 * @param port Peer port
 * @return Peer ID on success, 0 on failure
 */
uint32_t nlp_connect_peer(nlp_node_t node, const char* address, uint16_t port);

/**
 * @brief Disconnect from peer
 * @param node Node handle
 * @param peer_id Peer ID
 * @return 0 on success, negative on error
 */
int nlp_disconnect_peer(nlp_node_t node, uint32_t peer_id);

/**
 * @brief Get peer information
 * @param node Node handle
 * @param peer_id Peer ID
 * @param peer Output peer info
 * @return 0 on success, negative on error
 */
int nlp_get_peer(nlp_node_t node, uint32_t peer_id, nlp_peer_t* peer);

/**
 * @brief Get all connected peers
 * @param node Node handle
 * @param peers Output array
 * @param max_peers Maximum peers to return
 * @return Number of peers returned
 */
uint32_t nlp_get_peers(nlp_node_t node, nlp_peer_t* peers, uint32_t max_peers);

//=============================================================================
// Messaging API
//=============================================================================

/**
 * @brief Send message to peer
 * @param node Node handle
 * @param peer_id Destination peer (0 = broadcast)
 * @param msg_type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @param priority Message priority
 * @return 0 on success, negative on error
 */
int nlp_send(
    nlp_node_t node,
    uint32_t peer_id,
    nlp_msg_type_t msg_type,
    const void* payload,
    size_t payload_len,
    nlp_priority_t priority
);

/**
 * @brief Broadcast message to all peers
 * @param node Node handle
 * @param msg_type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @param priority Message priority
 * @return Number of peers sent to
 */
int nlp_broadcast(
    nlp_node_t node,
    nlp_msg_type_t msg_type,
    const void* payload,
    size_t payload_len,
    nlp_priority_t priority
);

/**
 * @brief Relay message through mesh network
 * @param node Node handle
 * @param dest_id Final destination brain ID
 * @param msg_type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @return 0 on success, negative on error
 */
int nlp_relay(
    nlp_node_t node,
    uint32_t dest_id,
    nlp_msg_type_t msg_type,
    const void* payload,
    size_t payload_len
);

//=============================================================================
// Neural Sync API
//=============================================================================

/**
 * @brief Send spike batch
 * @param node Node handle
 * @param peer_id Destination (0 = all)
 * @param neuron_ids Array of neuron IDs that spiked
 * @param spike_times Array of spike times (microseconds)
 * @param count Number of spikes
 * @return 0 on success, negative on error
 */
int nlp_send_spikes(
    nlp_node_t node,
    uint32_t peer_id,
    const uint32_t* neuron_ids,
    const uint16_t* spike_times,
    uint32_t count
);

/**
 * @brief Send weight deltas
 * @param node Node handle
 * @param peer_id Destination (0 = all)
 * @param synapse_ids Array of synapse IDs
 * @param old_weights Array of old weights
 * @param new_weights Array of new weights
 * @param count Number of changes
 * @return 0 on success, negative on error
 */
int nlp_send_weight_deltas(
    nlp_node_t node,
    uint32_t peer_id,
    const uint32_t* synapse_ids,
    const float* old_weights,
    const float* new_weights,
    uint32_t count
);

/**
 * @brief Send full cognitive state
 * @param node Node handle
 * @param peer_id Destination (0 = all)
 * @param state_data Compressed state data
 * @param state_len State data length
 * @return 0 on success, negative on error
 */
int nlp_send_state(
    nlp_node_t node,
    uint32_t peer_id,
    const void* state_data,
    size_t state_len
);

//=============================================================================
// Mode Control API
//=============================================================================

/**
 * @brief Set operating mode
 * @param node Node handle
 * @param mode New mode
 * @return 0 on success, negative on error
 */
int nlp_set_mode(nlp_node_t node, nlp_mode_t mode);

/**
 * @brief Get current operating mode
 * @param node Node handle
 * @return Current mode
 */
nlp_mode_t nlp_get_mode(nlp_node_t node);

/**
 * @brief Set EMCON level (stealth mode)
 * @param node Node handle
 * @param level EMCON level
 * @return 0 on success, negative on error
 */
int nlp_set_emcon(nlp_node_t node, nlp_emcon_level_t level);

/**
 * @brief Get current EMCON level
 * @param node Node handle
 * @return Current EMCON level
 */
nlp_emcon_level_t nlp_get_emcon(nlp_node_t node);

/**
 * @brief Update environment metrics (for auto mode)
 * @param node Node handle
 * @param env Environment metrics
 */
void nlp_update_environment(nlp_node_t node, const nlp_environment_t* env);

//=============================================================================
// Key Management API
//=============================================================================

/**
 * @brief Set pre-shared key
 * @param node Node handle
 * @param slot Key slot (0-7)
 * @param key Key data (32 bytes)
 * @param key_id Key identifier
 * @param valid_from Validity start timestamp
 * @param valid_until Validity end timestamp
 * @return 0 on success, negative on error
 */
int nlp_set_psk(
    nlp_node_t node,
    uint8_t slot,
    const uint8_t* key,
    uint32_t key_id,
    uint64_t valid_from,
    uint64_t valid_until
);

/**
 * @brief Rotate session key with peer
 * @param node Node handle
 * @param peer_id Peer ID
 * @return 0 on success, negative on error
 */
int nlp_rotate_session_key(nlp_node_t node, uint32_t peer_id);

//=============================================================================
// Callback Registration API
//=============================================================================

/**
 * @brief Set message received callback
 * @param node Node handle
 * @param callback Callback function
 */
void nlp_set_message_callback(nlp_node_t node, nlp_message_callback_t callback);

/**
 * @brief Set peer state change callback
 * @param node Node handle
 * @param callback Callback function
 */
void nlp_set_peer_callback(nlp_node_t node, nlp_peer_callback_t callback);

/**
 * @brief Set mode change callback
 * @param node Node handle
 * @param callback Callback function
 */
void nlp_set_mode_callback(nlp_node_t node, nlp_mode_callback_t callback);

//=============================================================================
// SAR/Disaster API
//=============================================================================

/**
 * @brief Send location update
 * @param node Node handle
 * @param location Location data
 * @return 0 on success, negative on error
 */
int nlp_send_location(nlp_node_t node, const nlp_location_t* location);

/**
 * @brief Send victim report
 * @param node Node handle
 * @param report Victim report
 * @param notes Optional notes
 * @return 0 on success, negative on error
 */
int nlp_send_victim_report(
    nlp_node_t node,
    const nlp_victim_report_t* report,
    const char* notes
);

/**
 * @brief Send sensor data
 * @param node Node handle
 * @param sensors Sensor readings
 * @return 0 on success, negative on error
 */
int nlp_send_sensors(nlp_node_t node, const nlp_sensor_data_t* sensors);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get node statistics
 * @param node Node handle
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int nlp_get_stats(nlp_node_t node, nlp_stats_t* stats);

/**
 * @brief Reset statistics
 * @param node Node handle
 */
void nlp_reset_stats(nlp_node_t node);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get message type name
 * @param type Message type
 * @return String name
 */
const char* nlp_msg_type_name(nlp_msg_type_t type);

/**
 * @brief Get mode name
 * @param mode Operating mode
 * @return String name
 */
const char* nlp_mode_name(nlp_mode_t mode);

/**
 * @brief Get EMCON level name
 * @param level EMCON level
 * @return String name
 */
const char* nlp_emcon_name(nlp_emcon_level_t level);

/**
 * @brief Calculate CRC-16 for header
 * @param header Header to checksum
 * @return CRC-16 value
 */
uint16_t nlp_header_crc(const nlp_header_t* header);

/**
 * @brief Generate unique brain ID
 * @return Unique brain ID
 */
uint32_t nlp_generate_brain_id(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LINK_PROTOCOL_H
