/**
 * @file nimcp_mesh_bio_bridge.h
 * @brief Bio-Async to Mesh Transaction Bridge
 *
 * WHAT: Bridges bio-async router messages to mesh network transactions
 * WHY:  Enable full integration between bio-router messaging and mesh consensus
 * HOW:  Message category to pattern mapping, channel routing, bidirectional translation
 *
 * ARCHITECTURE:
 * ```
 * ┌────────────────┐                     ┌────────────────┐
 * │   Bio-Router   │  ─── Bio Message ───>│  Bio Bridge    │
 * │   (Async)      │                     │                │
 * └────────────────┘                     │  - Category    │
 *                                        │    to Pattern  │
 * ┌────────────────┐                     │  - Channel     │
 * │  Mesh Network  │  <── Mesh Tx ───────│    Mapping     │
 * │   (Consensus)  │                     │  - Bidirectional│
 * └────────────────┘                     └────────────────┘
 * ```
 *
 * BIO MESSAGE CATEGORY TO PATTERN DIMENSION MAPPING:
 * ```
 * Bio Category    | Pattern Dims | Features
 * ─────────────────────────────────────────────────────
 * NEURAL (0x0100) | dims[0-7]    | Activation, spike, learning
 * PLASTICITY      | dims[8-15]   | STDP, LTP, consolidation
 * NEUROMOD        | dims[16-23]  | DA, 5HT, NE, ACh levels
 * PERCEPTION      | dims[24-31]  | Visual, auditory, sensory
 * COGNITIVE       | dims[32-39]  | Reasoning, attention, memory
 * MOTOR           | dims[40-47]  | Planning, execution, feedback
 * SECURITY        | dims[48-55]  | Threat, immune, BBB
 * SYSTEM          | dims[56-63]  | Lifecycle, metrics, error
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_BIO_BRIDGE_H
#define NIMCP_MESH_BIO_BRIDGE_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_channel.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Forward declaration - bio_router_t is a pointer typedef in nimcp_bio_router.h */
typedef struct bio_router_struct* bio_router_t;
typedef struct bio_message bio_message_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;
typedef struct mesh_bio_bridge mesh_bio_bridge_t;

/* ============================================================================
 * Bio Message Categories (from async/nimcp_bio_router.h)
 * ============================================================================ */

/**
 * @brief Bio message category base values
 *
 * These correspond to the bio-router message type categories
 */
#define MESH_BIO_CAT_NEURAL       0x0100  /**< Neural activation, spikes */
#define MESH_BIO_CAT_PLASTICITY   0x0200  /**< STDP, LTP, LTD */
#define MESH_BIO_CAT_NEUROMOD     0x0300  /**< Neuromodulator signaling */
#define MESH_BIO_CAT_PERCEPTION   0x0400  /**< Sensory perception */
#define MESH_BIO_CAT_COGNITIVE    0x0500  /**< Cognitive processing */
#define MESH_BIO_CAT_MOTOR        0x0600  /**< Motor commands */
#define MESH_BIO_CAT_SECURITY     0x0700  /**< Security/immune */
#define MESH_BIO_CAT_SYSTEM       0x0800  /**< System lifecycle */
#define MESH_BIO_CAT_GLIAL        0x0900  /**< Glial signaling */
#define MESH_BIO_CAT_MEMORY       0x0A00  /**< Memory operations */

/* ============================================================================
 * Pattern Dimension Ranges
 * ============================================================================ */

/**
 * @brief Pattern dimension allocation per category
 */
typedef struct mesh_pattern_dim_range {
    size_t start;                         /**< Start dimension index */
    size_t end;                           /**< End dimension index (exclusive) */
} mesh_pattern_dim_range_t;

/**
 * @brief Standard dimension ranges for bio categories
 */
static const mesh_pattern_dim_range_t MESH_BIO_PATTERN_RANGES[] = {
    { 0,  8},   /* NEURAL - dims[0-7] */
    { 8, 16},   /* PLASTICITY - dims[8-15] */
    {16, 24},   /* NEUROMOD - dims[16-23] */
    {24, 32},   /* PERCEPTION - dims[24-31] */
    {32, 40},   /* COGNITIVE - dims[32-39] */
    {40, 48},   /* MOTOR - dims[40-47] */
    {48, 56},   /* SECURITY - dims[48-55] */
    {56, 64},   /* SYSTEM - dims[56-63] */
};

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Bio bridge configuration
 */
typedef struct mesh_bio_bridge_config {
    /* Routing configuration */
    bool enable_pattern_routing;          /**< Use pattern-based routing */
    bool enable_channel_mapping;          /**< Map bio categories to channels */
    bool bidirectional;                   /**< Enable mesh-to-bio translation */

    /* Transaction settings */
    float default_timeout_ms;             /**< Default transaction timeout */
    size_t max_pending_translations;      /**< Max pending bio->mesh translations */

    /* Pattern extraction */
    float pattern_magnitude_threshold;    /**< Min magnitude for pattern */
    bool normalize_patterns;              /**< Normalize extracted patterns */

    /* Channel mapping overrides */
    mesh_channel_id_t neural_channel;     /**< Channel for neural messages */
    mesh_channel_id_t cognitive_channel;  /**< Channel for cognitive messages */
    mesh_channel_id_t motor_channel;      /**< Channel for motor messages */
    mesh_channel_id_t security_channel;   /**< Channel for security messages */

    /* Logging */
    bool verbose_logging;

} mesh_bio_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct mesh_bio_bridge_stats {
    /* Bio to Mesh */
    uint64_t bio_messages_received;       /**< Total bio messages received */
    uint64_t mesh_transactions_created;   /**< Transactions created from bio */
    uint64_t translation_failures;        /**< Failed translations */
    uint64_t pattern_extractions;         /**< Pattern vectors extracted */

    /* Mesh to Bio */
    uint64_t mesh_events_received;        /**< Mesh events received */
    uint64_t bio_messages_sent;           /**< Bio messages sent from mesh */
    uint64_t reverse_translation_failures;

    /* Per-category counts */
    uint64_t neural_translations;
    uint64_t plasticity_translations;
    uint64_t neuromod_translations;
    uint64_t perception_translations;
    uint64_t cognitive_translations;
    uint64_t motor_translations;
    uint64_t security_translations;
    uint64_t system_translations;

} mesh_bio_bridge_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_default_config(mesh_bio_bridge_config_t* config);

/**
 * @brief Create bio-mesh bridge
 *
 * @param bootstrap Mesh bootstrap handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
mesh_bio_bridge_t* mesh_bio_bridge_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_bio_bridge_config_t* config
);

/**
 * @brief Destroy bio-mesh bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mesh_bio_bridge_destroy(mesh_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-router
 *
 * Installs hooks into the bio-router to intercept and translate messages
 *
 * @param bridge Bio bridge handle
 * @param router Bio router to connect
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_connect_router(
    mesh_bio_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect bridge from bio-router
 *
 * @param bridge Bio bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_disconnect_router(mesh_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio bridge handle
 * @return true if connected to bio-router
 */
bool mesh_bio_bridge_is_connected(const mesh_bio_bridge_t* bridge);

/* ============================================================================
 * Translation API: Bio -> Mesh
 * ============================================================================ */

/**
 * @brief Translate bio message to mesh transaction
 *
 * Converts a bio-router message into a mesh network transaction,
 * extracting pattern vectors and determining appropriate channels.
 *
 * @param bridge Bio bridge handle
 * @param bio_msg Bio message to translate
 * @param tx_out Output mesh transaction (caller must free)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_translate_to_mesh(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size,
    mesh_transaction_t** tx_out
);

/**
 * @brief Route bio message through mesh network
 *
 * Convenience function that translates and submits in one call.
 *
 * @param bridge Bio bridge handle
 * @param bio_msg Bio message to route
 * @param msg_size Message size
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_route_bio_message(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size
);

/**
 * @brief Extract pattern vector from bio message
 *
 * Analyzes bio message content to create a pattern vector for
 * pattern-based routing.
 *
 * @param bridge Bio bridge handle
 * @param bio_msg Bio message
 * @param msg_size Message size
 * @param pattern_out Output pattern vector
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_extract_pattern(
    mesh_bio_bridge_t* bridge,
    const void* bio_msg,
    size_t msg_size,
    mesh_pattern_t* pattern_out
);

/* ============================================================================
 * Translation API: Mesh -> Bio
 * ============================================================================ */

/**
 * @brief Callback for mesh events to route back to bio-router
 */
typedef nimcp_error_t (*mesh_to_bio_callback_t)(
    const mesh_transaction_t* tx,
    void* bio_msg_out,
    size_t* msg_size_out,
    void* ctx
);

/**
 * @brief Register callback for mesh-to-bio translation
 *
 * @param bridge Bio bridge handle
 * @param callback Translation callback
 * @param ctx User context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_register_mesh_callback(
    mesh_bio_bridge_t* bridge,
    mesh_to_bio_callback_t callback,
    void* ctx
);

/**
 * @brief Translate mesh transaction to bio message format
 *
 * @param bridge Bio bridge handle
 * @param tx Mesh transaction
 * @param bio_msg_out Output bio message buffer
 * @param msg_size_out Output message size
 * @param max_size Maximum output buffer size
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_translate_to_bio(
    mesh_bio_bridge_t* bridge,
    const mesh_transaction_t* tx,
    void* bio_msg_out,
    size_t* msg_size_out,
    size_t max_size
);

/* ============================================================================
 * Channel Mapping API
 * ============================================================================ */

/**
 * @brief Get mesh channel for bio message category
 *
 * @param bridge Bio bridge handle
 * @param bio_category Bio message category (e.g., MESH_BIO_CAT_NEURAL)
 * @return Mesh channel ID
 */
mesh_channel_id_t mesh_bio_bridge_get_channel(
    const mesh_bio_bridge_t* bridge,
    uint32_t bio_category
);

/**
 * @brief Set channel mapping for bio category
 *
 * @param bridge Bio bridge handle
 * @param bio_category Bio message category
 * @param channel_id Target mesh channel
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_set_channel_mapping(
    mesh_bio_bridge_t* bridge,
    uint32_t bio_category,
    mesh_channel_id_t channel_id
);

/**
 * @brief Get pattern dimension range for bio category
 *
 * @param bio_category Bio message category
 * @param range_out Output dimension range
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_get_pattern_range(
    uint32_t bio_category,
    mesh_pattern_dim_range_t* range_out
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bio bridge handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_get_stats(
    const mesh_bio_bridge_t* bridge,
    mesh_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio bridge handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_bridge_reset_stats(mesh_bio_bridge_t* bridge);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get bio bridge from bootstrap
 *
 * @param bootstrap Mesh bootstrap handle
 * @return Bio bridge or NULL
 */
mesh_bio_bridge_t* mesh_bootstrap_get_bio_bridge(mesh_bootstrap_t* bootstrap);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_BIO_BRIDGE_H */
