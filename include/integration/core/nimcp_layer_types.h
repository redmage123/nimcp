/**
 * @file nimcp_layer_types.h
 * @brief Common types and definitions for the Layer Integration System
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Defines layer IDs, message types, and common structures for layer integration
 * WHY:  Provide a unified type system for intra-layer and inter-layer communication
 * HOW:  Enumerations, structures, and constants used across all layer components
 *
 * LAYER HIERARCHY:
 * ================
 *   SUPERHUMAN (top)
 *        |
 *   INTEGRATION
 *      /   \
 * EXECUTIVE  MEMORY
 *      \   /
 *    SENSORY --- NEUROMODULATORY
 *        \         /
 *         BIOLOGY
 *            |
 *        CHEMISTRY
 *            |
 *         PHYSICS (bottom)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LAYER_TYPES_H
#define NIMCP_LAYER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of layers in the system */
#ifndef NIMCP_MAX_LAYERS
#define NIMCP_MAX_LAYERS                16
#endif

/** Maximum modules per layer */
#define NIMCP_MAX_MODULES_PER_LAYER     32

/** Maximum inter-layer connections */
#define NIMCP_MAX_INTER_LAYER_CONNECTIONS 64

/** Maximum message queue depth per channel */
#define NIMCP_LAYER_MSG_QUEUE_DEPTH     256

/** Default sync interval in milliseconds */
#define NIMCP_LAYER_SYNC_INTERVAL_MS    10

/** Maximum layer name length */
#define NIMCP_LAYER_NAME_MAX            64

/** Maximum module name length */
#define NIMCP_MODULE_NAME_MAX           64

//=============================================================================
// Layer Identifiers
//=============================================================================

/**
 * @brief Layer identifiers in hierarchical order (bottom to top)
 */
typedef enum {
    NIMCP_LAYER_NONE = 0,           /**< Invalid/unassigned layer */

    /* Physical substrate layers */
    NIMCP_LAYER_PHYSICS = 1,        /**< Ephaptic, Info Geometry, HH, Thermo */
    NIMCP_LAYER_CHEMISTRY = 2,      /**< pH, NO Signaling, Neurovascular */
    NIMCP_LAYER_BIOLOGY = 3,        /**< Epigenetics, Neurogenesis, Gene Expr */

    /* Modulatory layer */
    NIMCP_LAYER_NEUROMODULATORY = 4, /**< LC, VTA, Raphe, Habenula */

    /* Processing layers */
    NIMCP_LAYER_SENSORY = 5,        /**< Somatosensory, Olfactory, Gustatory */
    NIMCP_LAYER_MEMORY = 6,         /**< Entorhinal, Perirhinal, Parahippocampal */
    NIMCP_LAYER_EXECUTIVE = 7,      /**< OFC, Retrosplenial, PFC */

    /* Binding layer */
    NIMCP_LAYER_INTEGRATION = 8,    /**< Claustrum, PAG, Red Nucleus, Reticular */

    /* Enhanced perception layer */
    NIMCP_LAYER_SUPERHUMAN = 9,     /**< Eagle Vision, Echolocation, Time Dilation */

    NIMCP_LAYER_COUNT               /**< Number of defined layers */
} nimcp_layer_id_t;

/**
 * @brief Layer category for grouping
 */
typedef enum {
    NIMCP_LAYER_CAT_SUBSTRATE = 0,  /**< Physics, Chemistry, Biology */
    NIMCP_LAYER_CAT_MODULATORY,     /**< Neuromodulatory */
    NIMCP_LAYER_CAT_PROCESSING,     /**< Sensory, Memory, Executive */
    NIMCP_LAYER_CAT_BINDING,        /**< Integration */
    NIMCP_LAYER_CAT_ENHANCED        /**< Superhuman */
} nimcp_layer_category_t;

//=============================================================================
// Message Types
//=============================================================================

/**
 * @brief Inter-layer message direction
 */
typedef enum {
    NIMCP_MSG_DIR_BOTTOM_UP = 0,    /**< Lower layer → Higher layer */
    NIMCP_MSG_DIR_TOP_DOWN,         /**< Higher layer → Lower layer */
    NIMCP_MSG_DIR_LATERAL,          /**< Same level (intra-layer) */
    NIMCP_MSG_DIR_BROADCAST         /**< To all connected layers */
} nimcp_msg_direction_t;

/**
 * @brief Layer message priority levels
 */
typedef enum {
    NIMCP_MSG_PRIORITY_LOW = 0,     /**< Background updates */
    NIMCP_MSG_PRIORITY_NORMAL,      /**< Standard messages */
    NIMCP_MSG_PRIORITY_HIGH,        /**< Time-sensitive messages */
    NIMCP_MSG_PRIORITY_CRITICAL     /**< Emergency/interrupt level */
} nimcp_msg_priority_t;

/**
 * @brief Generic layer message types
 */
typedef enum {
    /* Lifecycle messages (0x0000 - 0x00FF) */
    NIMCP_LAYER_MSG_INIT = 0x0001,          /**< Module initialization */
    NIMCP_LAYER_MSG_SHUTDOWN = 0x0002,      /**< Module shutdown */
    NIMCP_LAYER_MSG_RESET = 0x0003,         /**< Reset to initial state */
    NIMCP_LAYER_MSG_PAUSE = 0x0004,         /**< Pause processing */
    NIMCP_LAYER_MSG_RESUME = 0x0005,        /**< Resume processing */

    /* State messages (0x0100 - 0x01FF) */
    NIMCP_LAYER_MSG_STATE_UPDATE = 0x0100,  /**< State change notification */
    NIMCP_LAYER_MSG_STATE_QUERY = 0x0101,   /**< Query current state */
    NIMCP_LAYER_MSG_STATE_RESPONSE = 0x0102,/**< Response to state query */

    /* Data messages (0x0200 - 0x02FF) */
    NIMCP_LAYER_MSG_DATA_PUSH = 0x0200,     /**< Push data to module */
    NIMCP_LAYER_MSG_DATA_PULL = 0x0201,     /**< Request data from module */
    NIMCP_LAYER_MSG_DATA_RESPONSE = 0x0202, /**< Response to data pull */

    /* Sync messages (0x0300 - 0x03FF) */
    NIMCP_LAYER_MSG_SYNC_REQUEST = 0x0300,  /**< Request synchronization */
    NIMCP_LAYER_MSG_SYNC_ACK = 0x0301,      /**< Acknowledge sync */
    NIMCP_LAYER_MSG_SYNC_COMPLETE = 0x0302, /**< Sync completed */
    NIMCP_LAYER_MSG_PHASE_LOCK = 0x0303,    /**< Phase lock request */

    /* Control messages (0x0400 - 0x04FF) */
    NIMCP_LAYER_MSG_MODULATE = 0x0400,      /**< Modulation signal */
    NIMCP_LAYER_MSG_GATE = 0x0401,          /**< Gating signal */
    NIMCP_LAYER_MSG_INHIBIT = 0x0402,       /**< Inhibition signal */
    NIMCP_LAYER_MSG_EXCITE = 0x0403,        /**< Excitation signal */

    /* Error messages (0x0F00 - 0x0FFF) */
    NIMCP_LAYER_MSG_ERROR = 0x0F00,         /**< Error notification */
    NIMCP_LAYER_MSG_WARNING = 0x0F01,       /**< Warning notification */

    /* Module-specific messages start at 0x1000 */
    NIMCP_LAYER_MSG_MODULE_BASE = 0x1000,

    /* Bridge-specific messages start at 0x2000 */
    NIMCP_LAYER_MSG_BRIDGE_BASE = 0x2000
} nimcp_layer_msg_type_t;

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Layer system error codes
 */
typedef enum {
    NIMCP_LAYER_OK = 0,                     /**< Success */
    NIMCP_LAYER_ERR_NULL_PTR = -1,          /**< NULL pointer argument */
    NIMCP_LAYER_ERR_INVALID_LAYER = -2,     /**< Invalid layer ID */
    NIMCP_LAYER_ERR_INVALID_MODULE = -3,    /**< Invalid module ID */
    NIMCP_LAYER_ERR_NOT_REGISTERED = -4,    /**< Layer/module not registered */
    NIMCP_LAYER_ERR_ALREADY_REGISTERED = -5,/**< Already registered */
    NIMCP_LAYER_ERR_CAPACITY = -6,          /**< Capacity exceeded */
    NIMCP_LAYER_ERR_NO_CONNECTION = -7,     /**< No connection exists */
    NIMCP_LAYER_ERR_QUEUE_FULL = -8,        /**< Message queue full */
    NIMCP_LAYER_ERR_QUEUE_EMPTY = -9,       /**< Message queue empty */
    NIMCP_LAYER_ERR_TIMEOUT = -10,          /**< Operation timed out */
    NIMCP_LAYER_ERR_NOT_INITIALIZED = -11,  /**< Not initialized */
    NIMCP_LAYER_ERR_INVALID_MSG = -12,      /**< Invalid message */
    NIMCP_LAYER_ERR_ROUTE_FAILED = -13,     /**< Message routing failed */
    NIMCP_LAYER_ERR_SYNC_FAILED = -14,      /**< Synchronization failed */
    NIMCP_LAYER_ERR_NO_MEMORY = -15,        /**< Memory allocation failed */
    NIMCP_LAYER_ERR_INTERNAL = -16          /**< Internal error */
} nimcp_layer_error_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Layer message header
 */
typedef struct {
    uint32_t msg_type;              /**< Message type (nimcp_layer_msg_type_t) */
    nimcp_layer_id_t source_layer;  /**< Source layer ID */
    nimcp_layer_id_t target_layer;  /**< Target layer ID */
    uint32_t source_module;         /**< Source module ID within layer */
    uint32_t target_module;         /**< Target module ID (0 = broadcast) */
    nimcp_msg_direction_t direction;/**< Message direction */
    nimcp_msg_priority_t priority;  /**< Message priority */
    uint64_t timestamp_ns;          /**< Timestamp in nanoseconds */
    uint32_t sequence_num;          /**< Sequence number for ordering */
    uint32_t payload_size;          /**< Size of payload in bytes */
} nimcp_layer_msg_header_t;

/**
 * @brief Layer message structure
 */
typedef struct {
    nimcp_layer_msg_header_t header; /**< Message header */
    void* payload;                   /**< Message payload (type depends on msg_type) */
    bool payload_owned;              /**< If true, payload will be freed on destroy */
} nimcp_layer_msg_t;

/**
 * @brief Module interface - callbacks that each module must implement
 */
typedef struct {
    /** Initialize the module */
    nimcp_layer_error_t (*init)(void* module, void* config);

    /** Shutdown the module */
    nimcp_layer_error_t (*shutdown)(void* module);

    /** Process a tick/update */
    nimcp_layer_error_t (*update)(void* module, float dt);

    /** Handle incoming message */
    nimcp_layer_error_t (*handle_message)(void* module, const nimcp_layer_msg_t* msg);

    /** Get module state */
    nimcp_layer_error_t (*get_state)(void* module, void* state_out, size_t* size);

    /** Set module state */
    nimcp_layer_error_t (*set_state)(void* module, const void* state, size_t size);

    /** Get module name */
    const char* (*get_name)(void* module);
} nimcp_module_interface_t;

/**
 * @brief Module registration info
 */
typedef struct {
    uint32_t module_id;             /**< Unique module ID within layer */
    char name[NIMCP_MODULE_NAME_MAX]; /**< Human-readable name */
    void* module_ptr;               /**< Pointer to module instance */
    nimcp_module_interface_t* interface; /**< Module callbacks */
    bool is_active;                 /**< Whether module is active */
    uint64_t last_update_ns;        /**< Last update timestamp */
} nimcp_module_info_t;

/**
 * @brief Layer configuration
 */
typedef struct {
    nimcp_layer_id_t layer_id;      /**< Layer identifier */
    char name[NIMCP_LAYER_NAME_MAX]; /**< Layer name */
    nimcp_layer_category_t category; /**< Layer category */
    uint32_t max_modules;           /**< Maximum modules in this layer */
    uint32_t sync_interval_ms;      /**< Sync interval */
    bool enable_logging;            /**< Enable layer logging */
    bool enable_metrics;            /**< Enable metrics collection */
} nimcp_layer_config_t;

/**
 * @brief Layer statistics
 */
typedef struct {
    uint64_t messages_sent;         /**< Total messages sent */
    uint64_t messages_received;     /**< Total messages received */
    uint64_t messages_dropped;      /**< Messages dropped (queue full) */
    uint64_t sync_events;           /**< Synchronization events */
    uint64_t errors;                /**< Error count */
    float avg_latency_us;           /**< Average message latency */
    float coherence;                /**< Layer coherence (0-1) */
    uint64_t last_update_ns;        /**< Last update timestamp */
} nimcp_layer_stats_t;

/**
 * @brief Inter-layer connection info
 */
typedef struct {
    nimcp_layer_id_t layer_a;       /**< First layer */
    nimcp_layer_id_t layer_b;       /**< Second layer */
    bool bidirectional;             /**< If true, both directions enabled */
    bool bottom_up_enabled;         /**< A→B if A < B */
    bool top_down_enabled;          /**< B→A if A < B */
    float coupling_strength;        /**< Connection strength (0-1) */
    uint32_t queue_depth;           /**< Message queue depth */
} nimcp_layer_connection_t;

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get layer name string
 */
NIMCP_EXPORT const char* nimcp_layer_id_to_string(nimcp_layer_id_t layer_id);

/**
 * @brief Get error string
 */
NIMCP_EXPORT const char* nimcp_layer_error_to_string(nimcp_layer_error_t error);

/**
 * @brief Get message type string
 */
NIMCP_EXPORT const char* nimcp_layer_msg_type_to_string(uint32_t msg_type);

/**
 * @brief Get layer category
 */
NIMCP_EXPORT nimcp_layer_category_t nimcp_layer_get_category(nimcp_layer_id_t layer_id);

/**
 * @brief Check if two layers are adjacent in hierarchy
 */
NIMCP_EXPORT bool nimcp_layers_are_adjacent(nimcp_layer_id_t a, nimcp_layer_id_t b);

/**
 * @brief Get default layer configuration
 */
NIMCP_EXPORT nimcp_layer_config_t nimcp_layer_default_config(nimcp_layer_id_t layer_id);

/**
 * @brief Create a layer message
 */
NIMCP_EXPORT nimcp_layer_msg_t* nimcp_layer_msg_create(
    uint32_t msg_type,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    void* payload,
    uint32_t payload_size
);

/**
 * @brief Destroy a layer message
 */
NIMCP_EXPORT void nimcp_layer_msg_destroy(nimcp_layer_msg_t* msg);

/**
 * @brief Clone a layer message
 */
NIMCP_EXPORT nimcp_layer_msg_t* nimcp_layer_msg_clone(const nimcp_layer_msg_t* msg);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LAYER_TYPES_H */
