/**
 * @file nimcp_kg_module_wiring.h
 * @brief Module Wiring Descriptor for Knowledge Graph Self-Assembly
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Module wiring descriptor created by each module during initialization
 * WHY:  Enable brain self-awareness of module topology, connections, and weights
 * HOW:  Each module creates its wiring descriptor, brain assembles into KG
 *
 * BIOLOGICAL BASIS:
 * =================
 * In biological neural networks, each neuron type has:
 * - Defined input sources (dendritic arborization patterns)
 * - Output targets (axonal projection patterns)
 * - Characteristic response properties (receptor types, ion channels)
 * - Synaptic weight distributions (connection strengths)
 *
 * This module wiring system mirrors this organization:
 * - Inputs: What message types this module receives and from where
 * - Outputs: What message types this module produces
 * - Handlers: What message types this module processes
 * - Weights: Network parameters for self-awareness and introspection
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      MODULE WIRING DESCRIPTOR                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                       MODULE IDENTITY                               │  ║
 * ║   │   Name: "prefrontal_cortex"                                        │  ║
 * ║   │   Type: "COGNITIVE"                                                │  ║
 * ║   │   Layer: III (External Pyramidal)                                  │  ║
 * ║   │   Hemisphere: LEFT                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     INPUT CONNECTIONS                               │  ║
 * ║   │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐               │  ║
 * ║   │   │hippocampus  │  │basal_ganglia│  │sensory_ctx  │               │  ║
 * ║   │   │MEMORY_QUERY │  │REWARD_SIGNAL│  │FEATURE_VEC  │               │  ║
 * ║   │   │Required:Yes │  │Required:No  │  │Required:Yes │               │  ║
 * ║   │   └─────────────┘  └─────────────┘  └─────────────┘               │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    OUTPUT CONNECTIONS                               │  ║
 * ║   │   ┌─────────────┐  ┌─────────────┐                                │  ║
 * ║   │   │DECISION_OUT │  │WORKING_MEM  │                                │  ║
 * ║   │   │"Executive   │  │"Working mem │                                │  ║
 * ║   │   │ decisions"  │  │ updates"    │                                │  ║
 * ║   │   └─────────────┘  └─────────────┘                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   MESSAGE HANDLERS                                  │  ║
 * ║   │   MEMORY_QUERY (priority: 100)                                     │  ║
 * ║   │   DECISION_REQUEST (priority: 200)                                 │  ║
 * ║   │   ATTENTION_SHIFT (priority: 150)                                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    WEIGHT STATE                                     │  ║
 * ║   │   Network Type: SNN (Spiking Neural Network)                       │  ║
 * ║   │   Weights: 0x7f8a3b2c (2048 bytes)                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * USAGE:
 * ```c
 * // Module creates its wiring during initialization
 * kg_module_wiring_t* wiring = kg_module_wiring_create("prefrontal_cortex", "COGNITIVE");
 *
 * // Set hierarchical placement
 * wiring->target_layer = KG_LAYER_III;
 * wiring->hemisphere_affinity = KG_HEMISPHERE_LEFT;
 *
 * // Register inputs (sources this module expects)
 * kg_module_wiring_add_input(wiring, "hippocampus", "MEMORY_QUERY", true);
 * kg_module_wiring_add_input(wiring, "basal_ganglia", "REWARD_SIGNAL", false);
 *
 * // Register outputs (messages this module produces)
 * kg_module_wiring_add_output(wiring, "DECISION_OUT", "Executive decisions");
 * kg_module_wiring_add_output(wiring, "WORKING_MEM", "Working memory updates");
 *
 * // Register message handlers
 * kg_module_wiring_add_handler(wiring, "MEMORY_QUERY", 100);
 * kg_module_wiring_add_handler(wiring, "DECISION_REQUEST", 200);
 *
 * // Set initial weights for introspection
 * kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, my_weights, sizeof(my_weights));
 *
 * // Brain assembles all module wirings into KG
 * // ...
 *
 * kg_module_wiring_destroy(wiring);
 * ```
 *
 * THREAD SAFETY: Module wiring descriptors are created per-module during init,
 * typically single-threaded. Assembly into KG is thread-safe.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_MODULE_WIRING_H
#define NIMCP_KG_MODULE_WIRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of module name */
#define KG_WIRING_MAX_NAME_LEN          64

/** Maximum length of module type string */
#define KG_WIRING_MAX_TYPE_LEN          32

/** Maximum length of message type string */
#define KG_WIRING_MAX_MSG_TYPE_LEN      64

/** Maximum length of description string */
#define KG_WIRING_MAX_DESC_LEN          128

/** Maximum input connections per module */
#define KG_WIRING_MAX_INPUTS            32

/** Maximum output connections per module */
#define KG_WIRING_MAX_OUTPUTS           32

/** Maximum message handlers per module */
#define KG_WIRING_MAX_HANDLERS          64

/** Maximum metadata key-value pairs */
#define KG_WIRING_MAX_METADATA          16

/** Maximum length of metadata key */
#define KG_WIRING_MAX_META_KEY_LEN      64

/** Maximum length of metadata value */
#define KG_WIRING_MAX_META_VALUE_LEN    256

/* ============================================================================
 * Weight Type Enumeration
 * ============================================================================ */

/**
 * @brief Network weight/parameter type for self-awareness
 *
 * WHAT: Classification of neural network weight formats
 * WHY:  Enable introspection of module internal representations
 * HOW:  Module declares its network type when registering weights
 *
 * Different network architectures have different weight structures:
 * - SNN: Spike timing dependent weights, membrane potentials
 * - LNN: Liquid state parameters, reservoir weights
 * - CNN: Convolutional kernels, bias vectors
 * - Transformer: Attention weights, layer norms
 * - Hybrid: Combined weight structures
 */
typedef enum {
    KG_WEIGHT_NONE = 0,         /**< No weights (stateless module) */
    KG_WEIGHT_SNN,              /**< Spiking Neural Network weights */
    KG_WEIGHT_LNN,              /**< Liquid Neural Network parameters */
    KG_WEIGHT_CNN,              /**< Convolutional Neural Network weights */
    KG_WEIGHT_RNN,              /**< Recurrent Neural Network weights */
    KG_WEIGHT_TRANSFORMER,      /**< Transformer attention weights */
    KG_WEIGHT_GNN,              /**< Graph Neural Network parameters */
    KG_WEIGHT_HYBRID,           /**< Hybrid network (multiple types) */
    KG_WEIGHT_CUSTOM,           /**< Custom/user-defined weight format */
    KG_WEIGHT_TYPE_COUNT        /**< Number of weight types */
} kg_weight_type_t;

/* ============================================================================
 * Module Metadata Structure
 * ============================================================================ */

/**
 * @brief Key-value metadata pair for module attributes
 *
 * WHAT: Extensible metadata storage for module properties
 * WHY:  Allow modules to expose arbitrary searchable attributes
 * HOW:  String key-value pairs with bounded lengths
 */
typedef struct {
    char key[KG_WIRING_MAX_META_KEY_LEN];     /**< Metadata key */
    char value[KG_WIRING_MAX_META_VALUE_LEN]; /**< Metadata value */
} kg_wiring_metadata_entry_t;

/**
 * @brief Complete metadata block for module searchability
 *
 * WHAT: Searchable metadata associated with a module
 * WHY:  Enable KG queries based on module attributes
 * HOW:  Array of key-value pairs plus common fields
 */
typedef struct {
    /** Common metadata fields */
    char author[KG_WIRING_MAX_NAME_LEN];      /**< Module author/owner */
    char category[KG_WIRING_MAX_TYPE_LEN];    /**< Module category */
    char subcategory[KG_WIRING_MAX_TYPE_LEN]; /**< Module subcategory */
    char description[KG_WIRING_MAX_DESC_LEN]; /**< Human-readable description */

    /** Version information */
    uint32_t version_major;                   /**< Major version */
    uint32_t version_minor;                   /**< Minor version */
    uint32_t version_patch;                   /**< Patch version */

    /** Capability flags */
    uint32_t capabilities;                    /**< Bitmask of capabilities */
    uint32_t requirements;                    /**< Bitmask of requirements */

    /** Extensible key-value metadata */
    kg_wiring_metadata_entry_t entries[KG_WIRING_MAX_METADATA];
    uint32_t entry_count;                     /**< Number of metadata entries */
} kg_module_metadata_t;

/* ============================================================================
 * Input/Output Connection Structures
 * ============================================================================ */

/**
 * @brief Input connection descriptor
 *
 * WHAT: Describes an input this module expects to receive
 * WHY:  Enable topology validation and auto-wiring
 * HOW:  Source module name + message type + required flag
 */
typedef struct {
    char source_module[KG_WIRING_MAX_NAME_LEN]; /**< Source module name */
    char message_type[KG_WIRING_MAX_MSG_TYPE_LEN]; /**< Expected message type */
    bool required;                              /**< Required vs optional input */
} kg_input_connection_t;

/**
 * @brief Output connection descriptor
 *
 * WHAT: Describes an output this module produces
 * WHY:  Enable downstream module discovery
 * HOW:  Message type + human-readable description
 */
typedef struct {
    char message_type[KG_WIRING_MAX_MSG_TYPE_LEN]; /**< Output message type */
    char description[KG_WIRING_MAX_DESC_LEN];      /**< Output description */
} kg_output_connection_t;

/**
 * @brief Message handler registration
 *
 * WHAT: Describes a message type this module handles
 * WHY:  Enable message routing via KG lookup
 * HOW:  Message type + priority for ordering
 */
typedef struct {
    char message_type[KG_WIRING_MAX_MSG_TYPE_LEN]; /**< Handled message type */
    uint32_t priority;                             /**< Handler priority (higher = first) */
} kg_handler_registration_t;

/* ============================================================================
 * Module Wiring Descriptor Structure
 * ============================================================================ */

/**
 * @brief Module wiring descriptor - created by each module during init
 *
 * WHAT: Complete wiring specification for a single module
 * WHY:  Enable brain self-awareness of module topology and connections
 * HOW:  Module creates this during init, brain assembles into KG
 *
 * This structure is the primary interface for modules to declare:
 * 1. Identity: Name, type, layer, hemisphere placement
 * 2. Inputs: What messages/connections this module expects
 * 3. Outputs: What messages this module produces
 * 4. Handlers: What message types this module processes
 * 5. Weights: Internal network parameters for introspection
 */
typedef struct kg_module_wiring {
    /* Module identification */
    char module_name[KG_WIRING_MAX_NAME_LEN];   /**< Module identifier */
    char module_type[KG_WIRING_MAX_TYPE_LEN];   /**< "SNN", "LNN", "CNN", "COGNITIVE", etc. */
    uint8_t target_layer;                       /**< Cortical layer (I-VI, use kg_cortical_layer_t) */
    uint8_t hemisphere_affinity;                /**< LEFT, RIGHT, or BILATERAL (use kg_hemisphere_t) */

    /* Full searchable metadata */
    kg_module_metadata_t metadata;              /**< Complete metadata for this module */

    /* Input connections this module expects */
    kg_input_connection_t inputs[KG_WIRING_MAX_INPUTS];
    uint32_t input_count;                       /**< Number of input connections */

    /* Output connections this module provides */
    kg_output_connection_t outputs[KG_WIRING_MAX_OUTPUTS];
    uint32_t output_count;                      /**< Number of output connections */

    /* Message handlers this module registers */
    kg_handler_registration_t handlers[KG_WIRING_MAX_HANDLERS];
    uint32_t handler_count;                     /**< Number of handlers */

    /* Initial weight/parameter state (for self-awareness) */
    kg_weight_type_t network_type;              /**< Type of neural network */
    void* initial_weights;                      /**< Opaque pointer to weight data */
    size_t weights_size;                        /**< Size of weight data in bytes */

    /* Module versioning */
    uint64_t version;                           /**< Module version identifier */
    uint64_t creation_timestamp;                /**< When wiring was created (ms since epoch) */
} kg_module_wiring_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a new module wiring descriptor
 *
 * WHAT: Allocate and initialize a wiring descriptor
 * WHY:  Module needs to declare its wiring during initialization
 * HOW:  Allocate struct, set name/type, initialize counts to zero
 *
 * @param name Module name (must be unique, max KG_WIRING_MAX_NAME_LEN-1 chars)
 * @param type Module type string (e.g., "SNN", "COGNITIVE", "PERCEPTION")
 * @return New wiring descriptor or NULL on error (invalid params, alloc failure)
 */
kg_module_wiring_t* kg_module_wiring_create(const char* name, const char* type);

/**
 * @brief Destroy a module wiring descriptor
 *
 * WHAT: Free wiring descriptor and associated resources
 * WHY:  Clean up after wiring is registered with brain
 * HOW:  Free weight data copy (if any), free struct
 *
 * @param wiring Wiring descriptor to destroy (NULL safe)
 * @note Does NOT free the original weight data passed to set_weights
 */
void kg_module_wiring_destroy(kg_module_wiring_t* wiring);

/* ============================================================================
 * Input/Output Registration API
 * ============================================================================ */

/**
 * @brief Add an input connection to the wiring
 *
 * WHAT: Register an expected input source for this module
 * WHY:  Enable topology validation and auto-wiring
 * HOW:  Add to inputs array, increment input_count
 *
 * @param w Wiring descriptor
 * @param source Source module name (who sends to us)
 * @param msg_type Message type expected from source
 * @param required True if this input is required for module operation
 * @return 0 on success, -1 on error (NULL params, array full)
 */
int kg_module_wiring_add_input(
    kg_module_wiring_t* w,
    const char* source,
    const char* msg_type,
    bool required
);

/**
 * @brief Add an output connection to the wiring
 *
 * WHAT: Register an output this module produces
 * WHY:  Enable downstream module discovery and wiring validation
 * HOW:  Add to outputs array, increment output_count
 *
 * @param w Wiring descriptor
 * @param msg_type Message type this module produces
 * @param description Human-readable description of output
 * @return 0 on success, -1 on error (NULL params, array full)
 */
int kg_module_wiring_add_output(
    kg_module_wiring_t* w,
    const char* msg_type,
    const char* description
);

/**
 * @brief Add a message handler registration to the wiring
 *
 * WHAT: Register a message type this module handles
 * WHY:  Enable KG-based message routing lookup
 * HOW:  Add to handlers array, increment handler_count
 *
 * @param w Wiring descriptor
 * @param msg_type Message type this module handles
 * @param priority Handler priority (higher values = higher priority)
 * @return 0 on success, -1 on error (NULL params, array full)
 */
int kg_module_wiring_add_handler(
    kg_module_wiring_t* w,
    const char* msg_type,
    uint32_t priority
);

/* ============================================================================
 * Weight State API
 * ============================================================================ */

/**
 * @brief Set initial weights for self-awareness/introspection
 *
 * WHAT: Register module's internal network weights
 * WHY:  Enable brain introspection of module parameters
 * HOW:  Copy weight data into wiring descriptor
 *
 * @param w Wiring descriptor
 * @param type Type of neural network (SNN, LNN, CNN, etc.)
 * @param weights Pointer to weight data (will be copied)
 * @param size Size of weight data in bytes
 * @return 0 on success, -1 on error (NULL params, alloc failure)
 * @note Weight data is copied; caller retains ownership of original
 */
int kg_module_wiring_set_weights(
    kg_module_wiring_t* w,
    kg_weight_type_t type,
    void* weights,
    size_t size
);

/* ============================================================================
 * Metadata API
 * ============================================================================ */

/**
 * @brief Set basic metadata fields
 *
 * WHAT: Set common metadata (author, category, description)
 * WHY:  Enable searchability and documentation
 * HOW:  Copy strings into metadata struct
 *
 * @param w Wiring descriptor
 * @param author Module author/owner (NULL to skip)
 * @param category Module category (NULL to skip)
 * @param description Human-readable description (NULL to skip)
 * @return 0 on success, -1 on error (NULL wiring)
 */
int kg_module_wiring_set_metadata(
    kg_module_wiring_t* w,
    const char* author,
    const char* category,
    const char* description
);

/**
 * @brief Add a custom key-value metadata entry
 *
 * WHAT: Add extensible metadata to wiring
 * WHY:  Enable module-specific searchable attributes
 * HOW:  Add to metadata entries array
 *
 * @param w Wiring descriptor
 * @param key Metadata key (max KG_WIRING_MAX_META_KEY_LEN-1 chars)
 * @param value Metadata value (max KG_WIRING_MAX_META_VALUE_LEN-1 chars)
 * @return 0 on success, -1 on error (NULL params, array full)
 */
int kg_module_wiring_add_metadata_entry(
    kg_module_wiring_t* w,
    const char* key,
    const char* value
);

/**
 * @brief Set module version information
 *
 * WHAT: Set semantic version for the module
 * WHY:  Enable version-aware wiring and compatibility checks
 * HOW:  Store in metadata version fields
 *
 * @param w Wiring descriptor
 * @param major Major version number
 * @param minor Minor version number
 * @param patch Patch version number
 * @return 0 on success, -1 on error (NULL wiring)
 */
int kg_module_wiring_set_version(
    kg_module_wiring_t* w,
    uint32_t major,
    uint32_t minor,
    uint32_t patch
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Check if module has a specific input
 *
 * @param w Wiring descriptor
 * @param source Source module name
 * @param msg_type Message type (NULL to match any from source)
 * @return true if input exists
 */
bool kg_module_wiring_has_input(
    const kg_module_wiring_t* w,
    const char* source,
    const char* msg_type
);

/**
 * @brief Check if module produces a specific output
 *
 * @param w Wiring descriptor
 * @param msg_type Message type
 * @return true if output exists
 */
bool kg_module_wiring_has_output(
    const kg_module_wiring_t* w,
    const char* msg_type
);

/**
 * @brief Check if module handles a specific message type
 *
 * @param w Wiring descriptor
 * @param msg_type Message type
 * @return true if handler exists
 */
bool kg_module_wiring_has_handler(
    const kg_module_wiring_t* w,
    const char* msg_type
);

/**
 * @brief Get handler priority for a message type
 *
 * @param w Wiring descriptor
 * @param msg_type Message type
 * @return Handler priority, or 0 if not found
 */
uint32_t kg_module_wiring_get_handler_priority(
    const kg_module_wiring_t* w,
    const char* msg_type
);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Validate wiring descriptor for completeness
 *
 * WHAT: Check that wiring has required fields and valid data
 * WHY:  Catch configuration errors before assembly
 * HOW:  Verify name, type, check for duplicate handlers
 *
 * @param w Wiring descriptor to validate
 * @param error_buf Buffer for error message (NULL to skip)
 * @param error_buf_size Size of error buffer
 * @return 0 if valid, -1 if invalid (error_buf contains reason)
 */
int kg_module_wiring_validate(
    const kg_module_wiring_t* w,
    char* error_buf,
    size_t error_buf_size
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert weight type to string
 *
 * @param type Weight type enum value
 * @return Human-readable string (e.g., "SNN", "LNN", "CNN")
 */
const char* kg_weight_type_to_string(kg_weight_type_t type);

/**
 * @brief Parse weight type from string
 *
 * @param str String to parse (case-insensitive)
 * @return Weight type enum value, or KG_WEIGHT_NONE if unrecognized
 */
kg_weight_type_t kg_weight_type_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_MODULE_WIRING_H */
