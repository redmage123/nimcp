/**
 * @file nimcp_cochlea_bio_async_bridge.h
 * @brief Cochlea Bio-Async complete integration with bidirectional verification
 *
 * WHAT: Full bio-async integration with all message types and verification
 * WHY:  Ensure reliable cross-module communication
 * HOW:  Register handlers, send/receive messages, verify round-trips
 *
 * MESSAGE CATEGORIES:
 * - COCHLEA_TO_*: Outbound messages to other modules
 * - *_TO_COCHLEA: Inbound messages from other modules
 *
 * VERIFICATION:
 * - Ping-pong tests for each connected module
 * - Latency measurement
 * - Message delivery confirmation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_BIO_ASYNC_BRIDGE_H
#define NIMCP_COCHLEA_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* bio_router_t, bio_module_context_t, bio_module_id_t are already defined
 * in nimcp_bio_router.h (included via nimcp_cochlea.h) */

//=============================================================================
// Constants
//=============================================================================

/* Cochlea-specific bio-async module IDs */
#define BIO_MODULE_COCHLEA              0x1100
#define BIO_MODULE_COCHLEA_DOG          0x1101
#define BIO_MODULE_COCHLEA_BAT          0x1102

#define COCHLEA_BIO_MAX_HANDLERS        32
#define COCHLEA_BIO_MAX_CONNECTIONS     32

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Outbound message types (Cochlea -> Others)
 */
typedef enum {
    COCHLEA_MSG_AUDIO_ONSET = 0x1100,
    COCHLEA_MSG_AUDIO_OFFSET,
    COCHLEA_MSG_FREQUENCY_PEAK,
    COCHLEA_MSG_SPEECH_DETECTED,
    COCHLEA_MSG_ALARM_DETECTED,
    COCHLEA_MSG_ECHOLOCATION_TARGET,
    COCHLEA_MSG_SOUND_LOCALIZED,
    COCHLEA_MSG_ULTRASONIC_DETECTED,
    COCHLEA_MSG_STATE_UPDATE,
    COCHLEA_MSG_DAMAGE_ALERT,
    COCHLEA_MSG_PING                    /* For verification */
} cochlea_outbound_msg_t;

/**
 * @brief Inbound message types (Others -> Cochlea)
 */
typedef enum {
    COCHLEA_MSG_ATTENTION_COMMAND = 0x1180,
    COCHLEA_MSG_GAIN_MODULATION,
    COCHLEA_MSG_FREQUENCY_FOCUS,
    COCHLEA_MSG_PROTECTION_TRIGGER,
    COCHLEA_MSG_MODE_SWITCH,
    COCHLEA_MSG_PREDICTION_UPDATE,
    COCHLEA_MSG_VISUAL_CUE,
    COCHLEA_MSG_GOAL_UPDATE,
    COCHLEA_MSG_PONG                    /* For verification */
} cochlea_inbound_msg_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cochlea-specific message handler (simplified signature)
 *
 * Note: bio_message_handler_t is defined in nimcp_bio_router.h with a different
 * signature for full async messaging. This simpler handler is for cochlea-specific
 * message processing.
 */
typedef int (*cochlea_message_handler_t)(
    void* user_data,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Connection info for verification
 */
typedef struct {
    bio_module_id_t module_id;        /**< Connected module ID */
    char module_name[64];             /**< Module name */
    uint64_t last_outbound;           /**< Last outbound timestamp */
    uint64_t last_inbound;            /**< Last inbound timestamp */
    float latency_ms;                 /**< Round-trip latency */
    bool verified;                    /**< Bidirectional verified */
} cochlea_bio_connection_t;

/**
 * @brief Bio-async statistics
 */
typedef struct {
    uint64_t messages_sent;           /**< Total messages sent */
    uint64_t messages_received;       /**< Total messages received */
    uint64_t verification_passes;     /**< Verification successes */
    uint64_t verification_fails;      /**< Verification failures */
    float avg_latency_ms;             /**< Average latency */
} cochlea_bio_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Registration */
    bool auto_register;               /**< Auto-register with router */
    bio_module_id_t module_id;        /**< Module ID to use */
    const char* module_name;          /**< Module name */

    /* Verification */
    bool enable_verification;         /**< Enable ping-pong verification */
    float verification_interval_ms;   /**< How often to verify */
    float verification_timeout_ms;    /**< Timeout for pong response */

    /* Message handling */
    uint32_t inbox_capacity;          /**< Inbox message capacity */
} cochlea_bio_async_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_bio_async_bridge cochlea_bio_async_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_bio_async_config_t cochlea_bio_async_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_bio_async_bridge_t* cochlea_bio_async_bridge_create(
    cochlea_t* cochlea,
    bio_router_t* router,
    const cochlea_bio_async_config_t* config
);

void cochlea_bio_async_bridge_destroy(cochlea_bio_async_bridge_t* bridge);

nimcp_error_t cochlea_bio_async_bridge_update(
    cochlea_bio_async_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_bio_async_bridge_reset(cochlea_bio_async_bridge_t* bridge);

//=============================================================================
// Registration
//=============================================================================

/**
 * @brief Register cochlea module with router
 */
nimcp_error_t cochlea_bio_async_register(cochlea_bio_async_bridge_t* bridge);

/**
 * @brief Unregister cochlea module
 */
nimcp_error_t cochlea_bio_async_unregister(cochlea_bio_async_bridge_t* bridge);

/**
 * @brief Check if registered
 */
bool cochlea_bio_async_is_registered(const cochlea_bio_async_bridge_t* bridge);

//=============================================================================
// Message Handlers
//=============================================================================

/**
 * @brief Register message handler for inbound type
 */
nimcp_error_t cochlea_bio_async_add_handler(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_inbound_msg_t msg_type,
    cochlea_message_handler_t handler,
    void* user_data
);

/**
 * @brief Remove message handler
 */
nimcp_error_t cochlea_bio_async_remove_handler(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_inbound_msg_t msg_type
);

//=============================================================================
// Sending (Outbound)
//=============================================================================

/**
 * @brief Send message to destination module
 */
nimcp_error_t cochlea_bio_async_send(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t dest,
    cochlea_outbound_msg_t msg_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Broadcast message to all connected modules
 */
nimcp_error_t cochlea_bio_async_broadcast(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_outbound_msg_t msg_type,
    const void* payload,
    size_t payload_size
);

//=============================================================================
// Receiving (Inbound)
//=============================================================================

/**
 * @brief Process inbox (called in cochlea update loop)
 */
nimcp_error_t cochlea_bio_async_process_inbox(cochlea_bio_async_bridge_t* bridge);

/**
 * @brief Get pending message count
 */
uint32_t cochlea_bio_async_get_pending(const cochlea_bio_async_bridge_t* bridge);

//=============================================================================
// Connections
//=============================================================================

/**
 * @brief Add connection to track
 */
nimcp_error_t cochlea_bio_async_add_connection(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id,
    const char* module_name
);

/**
 * @brief Get connection info
 */
nimcp_error_t cochlea_bio_async_get_connection(
    const cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id,
    cochlea_bio_connection_t* connection
);

/**
 * @brief Get all connections
 */
nimcp_error_t cochlea_bio_async_get_connections(
    const cochlea_bio_async_bridge_t* bridge,
    cochlea_bio_connection_t* connections,
    uint32_t* num_connections
);

//=============================================================================
// Verification
//=============================================================================

/**
 * @brief Send ping to module (for verification)
 */
nimcp_error_t cochlea_bio_async_ping(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t dest
);

/**
 * @brief Verify all connections
 */
nimcp_error_t cochlea_bio_async_verify_all(cochlea_bio_async_bridge_t* bridge);

/**
 * @brief Check if module is verified
 */
bool cochlea_bio_async_is_verified(
    const cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bio-async statistics
 */
nimcp_error_t cochlea_bio_async_get_stats(
    const cochlea_bio_async_bridge_t* bridge,
    cochlea_bio_stats_t* stats
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_bio_async_verify_bidirectional(const cochlea_bio_async_bridge_t* bridge);
uint64_t cochlea_bio_async_get_last_outbound(const cochlea_bio_async_bridge_t* bridge);
uint64_t cochlea_bio_async_get_last_inbound(const cochlea_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_BIO_ASYNC_BRIDGE_H */
