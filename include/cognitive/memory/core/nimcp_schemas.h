//=============================================================================
// nimcp_schemas.h - Schema System for Cognitive Knowledge Organization
//=============================================================================
/**
 * @file nimcp_schemas.h
 * @brief Cognitive schemas for organizing knowledge and guiding inference
 *
 * WHAT: Structured knowledge templates (schemas) for organizing memories
 * WHY:  Human cognition uses schemas to interpret events, fill gaps, and
 *       make predictions - enabling efficient knowledge organization
 * HOW:  Slot-based templates with prime signature integration, inheritance,
 *       and statistical learning from instances
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Schema Theory and Memory Organization:
 *   +-----------------------------------------------------------------------+
 *   |  Schemas are mental structures that organize knowledge:               |
 *   |                                                                       |
 *   |  Neural Basis:                                                        |
 *   |  - Medial prefrontal cortex: Schema storage and activation           |
 *   |  - Hippocampus: Schema-memory integration during encoding            |
 *   |  - Ventromedial PFC: Schema-based inference and gap filling          |
 *   |  - Temporal cortex: Semantic schema representation                    |
 *   |                                                                       |
 *   |  Cognitive Functions:                                                 |
 *   |  1. Pattern Recognition: Match situations to known schemas           |
 *   |  2. Gap Filling: Infer missing information from schema defaults      |
 *   |  3. Expectation: Predict what comes next in schema sequence          |
 *   |  4. Organization: Group related memories under shared schema         |
 *   +-----------------------------------------------------------------------+
 *
 *   Schema Types:
 *   +-----------------------------------------------------------------------+
 *   |  Type        | Description               | Example                    |
 *   |--------------|---------------------------|----------------------------|
 *   |  EVENT       | Script for event sequence | Restaurant visit           |
 *   |  OBJECT      | Prototype for category    | Bird, car, furniture       |
 *   |  PERSON      | Stereotype for person type| Doctor, teacher            |
 *   |  SITUATION   | Frame for situation type  | Job interview, party       |
 *   |  PROCEDURE   | Script for procedural task| Cooking recipe, assembly   |
 *   +-----------------------------------------------------------------------+
 *
 *   Slot Structure:
 *   +-----------------------------------------------------------------------+
 *   |  Each schema has named SLOTS that can be filled:                      |
 *   |                                                                       |
 *   |  Slot Properties:                                                     |
 *   |  - name: Identifier for the slot (e.g., "agent", "location")         |
 *   |  - signature: Prime signature for content-addressable matching       |
 *   |  - is_required: Whether slot must be filled for valid instantiation  |
 *   |  - default_value: Used when slot not explicitly filled               |
 *   |  - confidence: How certain we are about the slot value               |
 *   |                                                                       |
 *   |  Example (Restaurant Schema):                                         |
 *   |  +-------------+------------+------------+-------------------------+  |
 *   |  | Slot        | Required   | Default    | Example Filler          |  |
 *   |  |-------------|------------|------------|-------------------------|  |
 *   |  | location    | yes        | -          | "Cafe Bella"           |  |
 *   |  | customer    | yes        | -          | (self)                  |  |
 *   |  | server      | no         | "waiter"   | "Maria"                 |  |
 *   |  | food        | yes        | -          | "pasta"                 |  |
 *   |  | payment     | no         | "credit"   | "cash"                  |  |
 *   |  +-------------+------------+------------+-------------------------+  |
 *   +-----------------------------------------------------------------------+
 *
 *   Schema Learning and Abstraction:
 *   +-----------------------------------------------------------------------+
 *   |  Schemas are learned through experience:                              |
 *   |                                                                       |
 *   |  1. Instance Accumulation: Multiple experiences of same situation    |
 *   |  2. Slot Cooccurrence: Track which slot values appear together       |
 *   |  3. Abstraction: Create more general schemas from specific ones      |
 *   |  4. Specialization: Create more specific schemas for variants        |
 *   |                                                                       |
 *   |  Hierarchy:                                                           |
 *   |        [Event]                                                        |
 *   |           |                                                           |
 *   |    [Dining Event]                                                     |
 *   |       /      \                                                        |
 *   |  [Restaurant]  [Home Dinner]                                          |
 *   |     /    \                                                            |
 *   | [Fast Food] [Fine Dining]                                             |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Schema creation: O(num_slots)
 * - Schema matching: O(num_schemas * num_slots)
 * - Slot filling: O(1) per slot
 * - Schema inference: O(num_slots) for defaults
 * - Learning from instance: O(num_slots^2) for cooccurrence update
 *
 * MEMORY:
 * - schema_t: ~200 bytes + slots + children
 * - schema_slot_t: ~80 bytes per slot
 * - schema_instantiation_t: ~100 bytes + filled slots
 *
 * INTEGRATION:
 * - Prime Signature: Schema slots use signatures for content matching
 * - Entanglement Graph: Schemas connect to related memory nodes
 * - Memory Nodes: Schema instances are stored as memory nodes
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_SCHEMAS_H
#define NIMCP_SCHEMAS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Dependencies
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum length of schema name */
#define SCHEMA_MAX_NAME_LENGTH          128

/** Maximum length of slot name */
#define SCHEMA_MAX_SLOT_NAME_LENGTH     64

/** Maximum number of slots per schema */
#define SCHEMA_MAX_SLOTS                64

/** Maximum number of child schemas */
#define SCHEMA_MAX_CHILDREN             32

/** Default minimum fit score for schema matching */
#define SCHEMA_DEFAULT_MIN_FIT          0.5f

/** Default confidence for inferred slots */
#define SCHEMA_DEFAULT_INFERRED_CONFIDENCE  0.7f

/** Default abstraction threshold for schema generalization */
#define SCHEMA_DEFAULT_ABSTRACTION_THRESHOLD    0.8f

/** Maximum active instantiations */
#define SCHEMA_MAX_ACTIVE_INSTANTIATIONS    256

/** Maximum schemas in library */
#define SCHEMA_MAX_SCHEMAS              1024

/** Invalid schema ID sentinel */
#define SCHEMA_INVALID_ID               UINT64_MAX

/** Epsilon for floating point comparisons */
#define SCHEMA_EPSILON                  1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Schema type enumeration
 *
 * Different types of schemas serve different cognitive functions:
 * - EVENT: Script for typical event sequences (temporal structure)
 * - OBJECT: Prototype for object categories (feature structure)
 * - PERSON: Stereotype for person types (role-based)
 * - SITUATION: Frame for situation types (contextual)
 * - PROCEDURE: Script for procedural knowledge (action sequence)
 */
typedef enum {
    SCHEMA_EVENT = 0,       /**< Script for typical event sequence */
    SCHEMA_OBJECT,          /**< Prototype for object category */
    SCHEMA_PERSON,          /**< Stereotype for person type */
    SCHEMA_SITUATION,       /**< Frame for situation type */
    SCHEMA_PROCEDURE,       /**< Script for procedural knowledge */
    SCHEMA_TYPE_COUNT       /**< Number of schema types (for arrays) */
} schema_type_t;

/**
 * @brief Slot constraint type
 *
 * Defines how slot values are constrained:
 * - NONE: Any value allowed
 * - SIGNATURE: Must match prime signature pattern
 * - RANGE: Numeric value within range
 * - ENUM: One of enumerated values
 * - SCHEMA: Must be instance of another schema
 */
typedef enum {
    SLOT_CONSTRAINT_NONE = 0,       /**< No constraint on value */
    SLOT_CONSTRAINT_SIGNATURE,      /**< Must match signature pattern */
    SLOT_CONSTRAINT_RANGE,          /**< Numeric range constraint */
    SLOT_CONSTRAINT_ENUM,           /**< Enumerated value constraint */
    SLOT_CONSTRAINT_SCHEMA          /**< Must be instance of schema */
} slot_constraint_type_t;

/**
 * @brief Schema slot definition
 *
 * A slot is a named placeholder in a schema that can be filled
 * with specific values when the schema is instantiated.
 *
 * Memory layout: ~80 bytes per slot
 */
typedef struct {
    char* slot_name;                    /**< Name of the slot (allocated) */
    prime_signature_t slot_signature;   /**< Signature pattern for slot type */
    bool is_required;                   /**< Whether slot must be filled */
    bool is_filled;                     /**< Whether slot currently has value */
    prime_signature_t filler;           /**< Current value (as signature) */
    prime_signature_t default_value;    /**< Default if not filled */
    float confidence;                   /**< Confidence in current filler [0,1] */

    // Constraint information
    slot_constraint_type_t constraint_type; /**< Type of constraint */
    union {
        struct {
            float min_value;            /**< Minimum for RANGE constraint */
            float max_value;            /**< Maximum for RANGE constraint */
        } range;
        struct {
            uint64_t schema_id;         /**< Required schema for SCHEMA constraint */
        } schema_ref;
        struct {
            uint32_t enum_count;        /**< Number of enum values */
            // enum values stored separately
        } enumeration;
    } constraint_data;

    // Statistics
    uint64_t fill_count;                /**< Times this slot has been filled */
    float avg_confidence;               /**< Average confidence when filled */
} schema_slot_t;

/**
 * @brief Schema structure
 *
 * Represents a cognitive schema - a structured template for organizing
 * and interpreting knowledge. Supports inheritance hierarchy.
 *
 * Memory layout: ~200 bytes + slots + children
 */
typedef struct schema_struct {
    uint64_t schema_id;                 /**< Unique schema identifier */
    char* schema_name;                  /**< Human-readable name (allocated) */
    schema_type_t type;                 /**< Type of schema */
    prime_signature_t schema_signature; /**< Overall schema signature */

    // Structure
    schema_slot_t* slots;               /**< Array of slots */
    size_t num_slots;                   /**< Number of slots */
    size_t slots_capacity;              /**< Allocated capacity for slots */

    // Hierarchy
    uint64_t parent_schema_id;          /**< Parent schema ID (inheritance) */
    uint64_t* child_schemas;            /**< Array of child schema IDs */
    size_t num_children;                /**< Number of children */
    size_t children_capacity;           /**< Allocated capacity for children */

    // Statistics
    uint64_t activation_count;          /**< Times schema has been activated */
    uint64_t instantiation_count;       /**< Times schema has been instantiated */
    float avg_fit;                      /**< Average fit score of instances */
    float abstraction_level;            /**< How abstract (0=specific, 1=abstract) */

    // Timestamps
    uint64_t created_time_ms;           /**< Creation timestamp */
    uint64_t last_activated_ms;         /**< Last activation timestamp */

    // Memory integration
    pr_memory_node_t* memory_node;      /**< Associated memory node (optional) */
} schema_t;

/**
 * @brief Schema instantiation
 *
 * Represents an active instance of a schema with specific slot fillers.
 * Tracks which slots were filled from input vs. inferred from defaults.
 */
typedef struct {
    schema_t* schema;                   /**< Reference to parent schema */
    float fit_score;                    /**< How well schema fits situation [0,1] */

    // Filled slots
    schema_slot_t* filled_slots;        /**< Slots filled from input */
    size_t num_filled;                  /**< Number of filled slots */

    // Inferred slots
    schema_slot_t* inferred_slots;      /**< Slots filled by default values */
    size_t num_inferred;                /**< Number of inferred slots */

    // Context
    uint64_t instantiation_id;          /**< Unique ID for this instantiation */
    uint64_t created_time_ms;           /**< When instantiation was created */
    float overall_confidence;           /**< Overall confidence in instantiation */

    // Source tracking
    uint64_t* source_memory_ids;        /**< Memory IDs used to fill slots */
    size_t num_sources;                 /**< Number of source memories */
} schema_instantiation_t;

/**
 * @brief Schema match result
 *
 * Result of matching a situation against available schemas.
 */
typedef struct {
    uint64_t schema_id;                 /**< Matched schema ID */
    float fit_score;                    /**< How well schema fits [0,1] */
    size_t slots_matched;               /**< Number of slots matched */
    size_t slots_total;                 /**< Total slots in schema */
    size_t required_matched;            /**< Required slots matched */
    size_t required_total;              /**< Total required slots */
} schema_match_result_t;

/**
 * @brief Schema expectation
 *
 * What to expect next based on schema activation.
 */
typedef struct {
    uint64_t schema_id;                 /**< Active schema */
    char* slot_name;                    /**< Expected slot to be filled next */
    prime_signature_t expected_value;   /**< Expected value (from defaults/stats) */
    float probability;                  /**< Probability of this expectation */
    float confidence;                   /**< Confidence in expectation */
} schema_expectation_t;

/**
 * @brief Schema violation
 *
 * Detected deviation from schema expectations.
 */
typedef struct {
    uint64_t schema_id;                 /**< Violated schema */
    char* slot_name;                    /**< Slot with violation */
    prime_signature_t expected;         /**< Expected value */
    prime_signature_t actual;           /**< Actual value encountered */
    float violation_severity;           /**< How severe the violation [0,1] */
    float surprise;                     /**< Surprise factor (information theoretic) */
} schema_violation_t;

/**
 * @brief Schema system configuration
 */
typedef struct {
    float min_fit_threshold;            /**< Minimum fit for schema matching */
    float inferred_confidence;          /**< Default confidence for inferences */
    float abstraction_threshold;        /**< Threshold for abstraction */
    bool enable_learning;               /**< Enable learning from instances */
    bool enable_cooccurrence;           /**< Track slot cooccurrence */
    bool enable_hierarchy;              /**< Enable schema inheritance */
    size_t max_schemas;                 /**< Maximum schemas in library */
    size_t max_active;                  /**< Maximum active instantiations */
} schema_config_t;

/**
 * @brief Schema system handle (opaque)
 *
 * Manages schema library, active instantiations, and learning.
 */
typedef struct schema_system_struct* schema_system_t;

/**
 * @brief Schema system statistics
 */
typedef struct {
    size_t num_schemas;                 /**< Total schemas in library */
    size_t num_active;                  /**< Active instantiations */
    uint64_t total_matches;             /**< Total match attempts */
    uint64_t successful_matches;        /**< Successful matches */
    uint64_t total_inferences;          /**< Total inferences made */
    uint64_t violations_detected;       /**< Schema violations detected */
    float avg_fit_score;                /**< Average fit score */
    float avg_confidence;               /**< Average confidence */
    size_t schemas_by_type[SCHEMA_TYPE_COUNT]; /**< Count per type */
    size_t memory_bytes;                /**< Approximate memory usage */
} schema_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default schema system configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - min_fit_threshold: 0.5
 *         - inferred_confidence: 0.7
 *         - abstraction_threshold: 0.8
 *         - enable_learning: true
 *         - enable_cooccurrence: true
 *         - enable_hierarchy: true
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT schema_config_t schema_config_default(void);

/**
 * @brief Validate schema system configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool schema_config_validate(const schema_config_t* config);

//=============================================================================
// Schema System Lifecycle
//=============================================================================

/**
 * @brief Create a new schema system
 *
 * WHAT: Allocates and initializes schema management system
 * WHY:  Central hub for schema storage, matching, and learning
 * HOW:  Creates schema library, active list, and cooccurrence matrix
 *
 * @param entanglement Entanglement graph for memory integration (can be NULL)
 * @param node_manager Memory node manager for node integration (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Schema system handle or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~1KB base + capacity-dependent
 *
 * Thread safety: The returned system is NOT thread-safe (external sync required)
 *
 * Example:
 *   schema_system_t sys = schema_system_create(graph, manager, NULL);
 *   if (!sys) {
 *       fprintf(stderr, "Failed: %s\n", schema_get_last_error());
 *   }
 */
NIMCP_EXPORT schema_system_t schema_system_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const schema_config_t* config);

/**
 * @brief Destroy schema system and free resources
 *
 * WHAT: Deallocates schema system and all owned schemas
 * WHY:  Resource cleanup
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(num_schemas)
 *
 * Warning: Destroys all schemas and instantiations
 */
NIMCP_EXPORT void schema_system_destroy(schema_system_t system);

/**
 * @brief Clear all schemas from system
 *
 * @param system System to clear
 * @return true on success, false on error
 *
 * Performance: O(num_schemas)
 */
NIMCP_EXPORT bool schema_system_clear(schema_system_t system);

//=============================================================================
// Schema Creation and Management
//=============================================================================

/**
 * @brief Create a new schema
 *
 * WHAT: Creates empty schema with given name and type
 * WHY:  Entry point for defining new cognitive schemas
 *
 * @param system Schema system
 * @param name Human-readable name
 * @param type Schema type
 * @return New schema pointer or NULL on failure
 *
 * Performance: O(1)
 *
 * Example:
 *   schema_t* restaurant = schema_create(sys, "RestaurantVisit", SCHEMA_EVENT);
 */
NIMCP_EXPORT schema_t* schema_create(
    schema_system_t system,
    const char* name,
    schema_type_t type);

/**
 * @brief Destroy a schema
 *
 * WHAT: Removes schema from system and frees memory
 * WHY:  Remove unused or outdated schemas
 *
 * @param system Schema system
 * @param schema Schema to destroy
 * @return true on success, false if schema not in system
 *
 * Performance: O(num_children) for hierarchy update
 *
 * Warning: Also destroys active instantiations of this schema
 */
NIMCP_EXPORT bool schema_destroy(schema_system_t system, schema_t* schema);

/**
 * @brief Add schema to system library
 *
 * WHAT: Registers schema in the system's library
 * WHY:  Make schema available for matching and instantiation
 *
 * @param system Schema system
 * @param schema Schema to add
 * @return true on success, false on error
 *
 * Performance: O(1) average
 *
 * Note: Schema ownership transfers to system
 */
NIMCP_EXPORT bool schema_add(schema_system_t system, schema_t* schema);

/**
 * @brief Find schema by ID
 *
 * @param system Schema system
 * @param schema_id Schema ID to find
 * @return Schema pointer or NULL if not found
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT schema_t* schema_find_by_id(schema_system_t system, uint64_t schema_id);

/**
 * @brief Find schema by name
 *
 * @param system Schema system
 * @param name Schema name to find
 * @return Schema pointer or NULL if not found
 *
 * Performance: O(num_schemas)
 */
NIMCP_EXPORT schema_t* schema_find_by_name(schema_system_t system, const char* name);

/**
 * @brief Get all schemas of a given type
 *
 * @param system Schema system
 * @param type Schema type to filter by
 * @param schemas Output array (caller-allocated)
 * @param max_schemas Maximum to return
 * @param count Output: actual count returned
 * @return true on success, false on error
 *
 * Performance: O(num_schemas)
 */
NIMCP_EXPORT bool schema_get_by_type(
    schema_system_t system,
    schema_type_t type,
    schema_t** schemas,
    size_t max_schemas,
    size_t* count);

//=============================================================================
// Slot Management
//=============================================================================

/**
 * @brief Define a slot in a schema
 *
 * WHAT: Adds a named slot to schema structure
 * WHY:  Slots are the fillable parts of schemas
 *
 * @param schema Schema to modify
 * @param slot_name Name of the slot
 * @param is_required Whether slot must be filled
 * @param default_sig Default value signature (can be NULL)
 * @return true on success, false on error
 *
 * Performance: O(1) amortized
 *
 * Example:
 *   schema_define_slot(restaurant, "location", true, NULL);
 *   schema_define_slot(restaurant, "payment_method", false, &credit_sig);
 */
NIMCP_EXPORT bool schema_define_slot(
    schema_t* schema,
    const char* slot_name,
    bool is_required,
    const prime_signature_t* default_sig);

/**
 * @brief Define slot with constraint
 *
 * WHAT: Adds constrained slot to schema
 * WHY:  Some slots have restricted valid values
 *
 * @param schema Schema to modify
 * @param slot_name Name of the slot
 * @param is_required Whether slot must be filled
 * @param default_sig Default value signature
 * @param constraint_type Type of constraint
 * @param constraint_data Constraint-specific data
 * @return true on success, false on error
 *
 * Performance: O(1) amortized
 */
NIMCP_EXPORT bool schema_define_slot_constrained(
    schema_t* schema,
    const char* slot_name,
    bool is_required,
    const prime_signature_t* default_sig,
    slot_constraint_type_t constraint_type,
    const void* constraint_data);

/**
 * @brief Remove a slot from schema
 *
 * @param schema Schema to modify
 * @param slot_name Name of slot to remove
 * @return true if removed, false if not found
 *
 * Performance: O(num_slots)
 */
NIMCP_EXPORT bool schema_remove_slot(schema_t* schema, const char* slot_name);

/**
 * @brief Get slot by name
 *
 * @param schema Schema to query
 * @param slot_name Name of slot
 * @return Slot pointer or NULL if not found
 *
 * Performance: O(num_slots)
 */
NIMCP_EXPORT schema_slot_t* schema_get_slot(schema_t* schema, const char* slot_name);

/**
 * @brief Get number of slots
 *
 * @param schema Schema to query
 * @return Number of slots
 */
NIMCP_EXPORT size_t schema_get_slot_count(const schema_t* schema);

/**
 * @brief Get number of required slots
 *
 * @param schema Schema to query
 * @return Number of required slots
 */
NIMCP_EXPORT size_t schema_get_required_slot_count(const schema_t* schema);

//=============================================================================
// Schema Instantiation
//=============================================================================

/**
 * @brief Instantiate schema with given slot fillers
 *
 * WHAT: Creates active instance of schema with specific values
 * WHY:  Apply schema to interpret a specific situation
 * HOW:  Matches fillers to slots, fills defaults for missing
 *
 * @param system Schema system
 * @param schema Schema to instantiate
 * @param slot_names Array of slot names to fill
 * @param fillers Array of prime signatures for slot values
 * @param num_fillers Number of fillers provided
 * @return Instantiation or NULL on failure
 *
 * Performance: O(num_slots)
 *
 * Example:
 *   const char* names[] = {"location", "food"};
 *   prime_signature_t fillers[2];
 *   // ... fill signatures ...
 *   schema_instantiation_t* inst = schema_instantiate(
 *       sys, restaurant, names, fillers, 2);
 */
NIMCP_EXPORT schema_instantiation_t* schema_instantiate(
    schema_system_t system,
    schema_t* schema,
    const char** slot_names,
    const prime_signature_t* fillers,
    size_t num_fillers);

/**
 * @brief Instantiate schema from memory node
 *
 * WHAT: Creates instantiation by extracting slot values from memory
 * WHY:  Direct integration with memory system
 *
 * @param system Schema system
 * @param schema Schema to instantiate
 * @param memory Memory node to extract values from
 * @return Instantiation or NULL on failure
 *
 * Performance: O(num_slots * memory_data_size)
 */
NIMCP_EXPORT schema_instantiation_t* schema_instantiate_from_memory(
    schema_system_t system,
    schema_t* schema,
    pr_memory_node_t* memory);

/**
 * @brief Destroy schema instantiation
 *
 * @param instantiation Instantiation to destroy (NULL safe)
 *
 * Performance: O(num_slots)
 */
NIMCP_EXPORT void schema_instantiation_destroy(schema_instantiation_t* instantiation);

/**
 * @brief Get active instantiation by ID
 *
 * @param system Schema system
 * @param instantiation_id ID to find
 * @return Instantiation or NULL if not found
 *
 * Performance: O(num_active)
 */
NIMCP_EXPORT schema_instantiation_t* schema_get_instantiation(
    schema_system_t system,
    uint64_t instantiation_id);

/**
 * @brief Update slot in active instantiation
 *
 * @param instantiation Instantiation to update
 * @param slot_name Slot to update
 * @param filler New value
 * @param confidence Confidence in new value
 * @return true on success, false on error
 *
 * Performance: O(num_slots)
 */
NIMCP_EXPORT bool schema_instantiation_update_slot(
    schema_instantiation_t* instantiation,
    const char* slot_name,
    const prime_signature_t* filler,
    float confidence);

//=============================================================================
// Schema Matching
//=============================================================================

/**
 * @brief Find best matching schema for given slot values
 *
 * WHAT: Searches library for schema that best fits input
 * WHY:  Interpret unknown situation using known schemas
 * HOW:  Computes fit score for each schema, returns best
 *
 * @param system Schema system
 * @param slot_names Array of slot names
 * @param values Array of slot values (signatures)
 * @param num_slots Number of slots provided
 * @param result Output: best match result
 * @return true if match found above threshold, false otherwise
 *
 * Performance: O(num_schemas * num_slots)
 *
 * Example:
 *   schema_match_result_t result;
 *   if (schema_match(sys, names, values, 3, &result)) {
 *       printf("Best match: schema %lu (fit=%.2f)\n",
 *              result.schema_id, result.fit_score);
 *   }
 */
NIMCP_EXPORT bool schema_match(
    schema_system_t system,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots,
    schema_match_result_t* result);

/**
 * @brief Find top-K matching schemas
 *
 * WHAT: Returns K best matching schemas
 * WHY:  Multiple schemas may apply to a situation
 *
 * @param system Schema system
 * @param slot_names Array of slot names
 * @param values Array of slot values
 * @param num_slots Number of slots
 * @param k Number of matches to return
 * @param results Output array (caller-allocated, size >= k)
 * @param result_count Output: actual count returned
 * @return true on success, false on error
 *
 * Performance: O(num_schemas * num_slots + k log k)
 */
NIMCP_EXPORT bool schema_match_top_k(
    schema_system_t system,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots,
    size_t k,
    schema_match_result_t* results,
    size_t* result_count);

/**
 * @brief Compute fit score between schema and slot values
 *
 * WHAT: Calculates how well values fit schema's structure
 * WHY:  Core metric for schema matching
 *
 * @param schema Schema to match against
 * @param slot_names Array of slot names
 * @param values Array of slot values
 * @param num_slots Number of slots
 * @return Fit score [0,1] or -1.0f on error
 *
 * Performance: O(num_slots)
 *
 * Scoring:
 * - Required slots filled: +weight per slot
 * - Optional slots filled: +weight per slot (lower)
 * - Signature match quality factors in
 */
NIMCP_EXPORT float schema_compute_fit(
    const schema_t* schema,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots);

//=============================================================================
// Schema Inference
//=============================================================================

/**
 * @brief Infer missing slot values from schema defaults
 *
 * WHAT: Fills unfilled slots with default values
 * WHY:  Gap-filling based on schema expectations
 *
 * @param instantiation Instantiation to update
 * @return Number of slots inferred
 *
 * Performance: O(num_slots)
 *
 * Example:
 *   // After partial instantiation
 *   int inferred = schema_infer(inst);
 *   printf("Inferred %d slots from defaults\n", inferred);
 */
NIMCP_EXPORT int schema_infer(schema_instantiation_t* instantiation);

/**
 * @brief Infer slot value based on cooccurrence statistics
 *
 * WHAT: Predicts slot value based on other filled slots
 * WHY:  Statistical inference beyond simple defaults
 *
 * @param system Schema system
 * @param instantiation Instantiation with partial fillers
 * @param slot_name Slot to infer
 * @param inferred_value Output: predicted signature
 * @param confidence Output: confidence in prediction
 * @return true if inference made, false if cannot infer
 *
 * Performance: O(num_slots^2) for cooccurrence lookup
 */
NIMCP_EXPORT bool schema_infer_slot(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    const char* slot_name,
    prime_signature_t* inferred_value,
    float* confidence);

/**
 * @brief Get expectations based on active schema
 *
 * WHAT: Predicts what slot values to expect next
 * WHY:  Schema-based prediction for top-down processing
 *
 * @param system Schema system
 * @param instantiation Active instantiation
 * @param expectations Output array (caller-allocated)
 * @param max_expectations Maximum to return
 * @param count Output: actual count
 * @return true on success, false on error
 *
 * Performance: O(num_slots)
 *
 * Example:
 *   schema_expectation_t expects[10];
 *   size_t count;
 *   schema_get_expectation(sys, inst, expects, 10, &count);
 *   for (size_t i = 0; i < count; i++) {
 *       printf("Expect %s with prob %.2f\n",
 *              expects[i].slot_name, expects[i].probability);
 *   }
 */
NIMCP_EXPORT bool schema_get_expectation(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    schema_expectation_t* expectations,
    size_t max_expectations,
    size_t* count);

//=============================================================================
// Schema Learning
//=============================================================================

/**
 * @brief Learn from schema instance
 *
 * WHAT: Updates schema statistics based on new instance
 * WHY:  Schemas improve through experience
 * HOW:  Updates slot cooccurrence, average fit, etc.
 *
 * @param system Schema system
 * @param instantiation Completed instantiation to learn from
 * @return true on success, false on error
 *
 * Performance: O(num_slots^2) for cooccurrence update
 *
 * Example:
 *   // After completing an instantiation
 *   schema_learn_from_instance(sys, inst);
 */
NIMCP_EXPORT bool schema_learn_from_instance(
    schema_system_t system,
    const schema_instantiation_t* instantiation);

/**
 * @brief Create abstract schema from instances
 *
 * WHAT: Generalizes common structure from multiple instances
 * WHY:  Build more general schemas from specific experience
 *
 * @param system Schema system
 * @param instantiations Array of instantiations to generalize
 * @param num_instances Number of instances
 * @param name Name for new abstract schema
 * @return New abstract schema or NULL on failure
 *
 * Performance: O(num_instances * num_slots)
 *
 * Example:
 *   // After many restaurant experiences
 *   schema_t* dining = schema_abstract(sys, instances, 10, "DiningEvent");
 */
NIMCP_EXPORT schema_t* schema_abstract(
    schema_system_t system,
    const schema_instantiation_t** instantiations,
    size_t num_instances,
    const char* name);

/**
 * @brief Specialize schema for specific variant
 *
 * WHAT: Creates more specific schema from general one
 * WHY:  Capture variants with additional constraints
 *
 * @param system Schema system
 * @param parent_schema Schema to specialize
 * @param name Name for new specialized schema
 * @param extra_slots Additional slots for specialization
 * @param num_extra Number of extra slots
 * @return New specialized schema or NULL on failure
 *
 * Performance: O(num_slots + num_extra)
 *
 * Example:
 *   schema_t* fast_food = schema_specialize(
 *       sys, restaurant, "FastFoodRestaurant", slots, 2);
 */
NIMCP_EXPORT schema_t* schema_specialize(
    schema_system_t system,
    const schema_t* parent_schema,
    const char* name,
    const schema_slot_t* extra_slots,
    size_t num_extra);

/**
 * @brief Merge similar schemas
 *
 * WHAT: Combines two schemas with high overlap
 * WHY:  Reduce redundancy in schema library
 *
 * @param system Schema system
 * @param schema1 First schema
 * @param schema2 Second schema
 * @param name Name for merged schema
 * @return Merged schema or NULL if not similar enough
 *
 * Performance: O(num_slots1 + num_slots2)
 */
NIMCP_EXPORT schema_t* schema_merge(
    schema_system_t system,
    const schema_t* schema1,
    const schema_t* schema2,
    const char* name);

//=============================================================================
// Schema Violation Detection
//=============================================================================

/**
 * @brief Detect schema violation
 *
 * WHAT: Checks if value violates schema expectations
 * WHY:  Violation detection drives learning and attention
 *
 * @param system Schema system
 * @param instantiation Active instantiation
 * @param slot_name Slot being filled
 * @param value Value being used
 * @param violation Output: violation details (if detected)
 * @return true if violation detected, false otherwise
 *
 * Performance: O(1)
 *
 * Example:
 *   schema_violation_t v;
 *   if (schema_detect_violation(sys, inst, "payment", &weird_sig, &v)) {
 *       printf("Violation! Severity=%.2f, Surprise=%.2f\n",
 *              v.violation_severity, v.surprise);
 *   }
 */
NIMCP_EXPORT bool schema_detect_violation(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    const char* slot_name,
    const prime_signature_t* value,
    schema_violation_t* violation);

/**
 * @brief Get all violations in instantiation
 *
 * @param system Schema system
 * @param instantiation Instantiation to check
 * @param violations Output array (caller-allocated)
 * @param max_violations Maximum to return
 * @param count Output: actual count
 * @return true on success, false on error
 *
 * Performance: O(num_slots)
 */
NIMCP_EXPORT bool schema_get_violations(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    schema_violation_t* violations,
    size_t max_violations,
    size_t* count);

//=============================================================================
// Hierarchy Management
//=============================================================================

/**
 * @brief Set parent schema (inheritance)
 *
 * WHAT: Establishes inheritance relationship
 * WHY:  Child inherits parent slots
 *
 * @param system Schema system
 * @param child Child schema
 * @param parent Parent schema
 * @return true on success, false on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool schema_set_parent(
    schema_system_t system,
    schema_t* child,
    const schema_t* parent);

/**
 * @brief Get parent schema
 *
 * @param system Schema system
 * @param schema Schema to query
 * @return Parent schema or NULL if no parent
 *
 * Performance: O(1)
 */
NIMCP_EXPORT schema_t* schema_get_parent(
    schema_system_t system,
    const schema_t* schema);

/**
 * @brief Get child schemas
 *
 * @param system Schema system
 * @param schema Schema to query
 * @param children Output array (caller-allocated)
 * @param max_children Maximum to return
 * @param count Output: actual count
 * @return true on success, false on error
 *
 * Performance: O(num_children)
 */
NIMCP_EXPORT bool schema_get_children(
    schema_system_t system,
    const schema_t* schema,
    schema_t** children,
    size_t max_children,
    size_t* count);

/**
 * @brief Get all ancestor schemas
 *
 * @param system Schema system
 * @param schema Schema to query
 * @param ancestors Output array (caller-allocated)
 * @param max_ancestors Maximum to return
 * @param count Output: actual count
 * @return true on success, false on error
 *
 * Performance: O(hierarchy_depth)
 */
NIMCP_EXPORT bool schema_get_ancestors(
    schema_system_t system,
    const schema_t* schema,
    schema_t** ancestors,
    size_t max_ancestors,
    size_t* count);

//=============================================================================
// Statistics and Utilities
//=============================================================================

/**
 * @brief Get schema system statistics
 *
 * @param system Schema system
 * @param stats Output statistics
 * @return true on success, false on error
 *
 * Performance: O(num_schemas)
 */
NIMCP_EXPORT bool schema_get_stats(schema_system_t system, schema_stats_t* stats);

/**
 * @brief Reset schema system statistics
 *
 * @param system Schema system
 */
NIMCP_EXPORT void schema_reset_stats(schema_system_t system);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* schema_get_last_error(void);

/**
 * @brief Get schema type name as string
 *
 * @param type Schema type
 * @return Static string name (e.g., "EVENT", "OBJECT")
 */
NIMCP_EXPORT const char* schema_type_name(schema_type_t type);

/**
 * @brief Print schema to stdout for debugging
 *
 * @param schema Schema to print
 */
NIMCP_EXPORT void schema_print(const schema_t* schema);

/**
 * @brief Print instantiation to stdout for debugging
 *
 * @param instantiation Instantiation to print
 */
NIMCP_EXPORT void schema_instantiation_print(const schema_instantiation_t* instantiation);

/**
 * @brief Validate schema internal consistency
 *
 * @param schema Schema to validate
 * @return true if consistent, false if corrupted
 *
 * Performance: O(num_slots + num_children)
 */
NIMCP_EXPORT bool schema_validate(const schema_t* schema);

/**
 * @brief Compute schema similarity
 *
 * WHAT: Measures structural similarity between two schemas
 * WHY:  Used for merging and hierarchy decisions
 *
 * @param schema1 First schema
 * @param schema2 Second schema
 * @return Similarity [0,1] or -1.0f on error
 *
 * Performance: O(num_slots1 * num_slots2)
 */
NIMCP_EXPORT float schema_similarity(
    const schema_t* schema1,
    const schema_t* schema2);

/**
 * @brief Get current time in milliseconds
 *
 * Utility for timestamps.
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t schema_current_time_ms(void);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if schema has required unfilled slots
 *
 * @param instantiation Instantiation to check
 * @return true if all required slots are filled
 */
static inline bool schema_is_complete(const schema_instantiation_t* instantiation) {
    if (!instantiation || !instantiation->schema) return false;

    // Check all required slots
    for (size_t i = 0; i < instantiation->schema->num_slots; i++) {
        if (instantiation->schema->slots[i].is_required &&
            !instantiation->schema->slots[i].is_filled) {
            // Check if it's in filled or inferred
            bool found = false;
            for (size_t j = 0; j < instantiation->num_filled && !found; j++) {
                if (instantiation->filled_slots[j].slot_name &&
                    instantiation->schema->slots[i].slot_name &&
                    strcmp(instantiation->filled_slots[j].slot_name,
                           instantiation->schema->slots[i].slot_name) == 0) {
                    found = true;
                }
            }
            for (size_t j = 0; j < instantiation->num_inferred && !found; j++) {
                if (instantiation->inferred_slots[j].slot_name &&
                    instantiation->schema->slots[i].slot_name &&
                    strcmp(instantiation->inferred_slots[j].slot_name,
                           instantiation->schema->slots[i].slot_name) == 0) {
                    found = true;
                }
            }
            if (!found) return false;
        }
    }
    return true;
}

/**
 * @brief Get schema abstraction level category
 *
 * @param schema Schema to categorize
 * @return "specific", "moderate", or "abstract"
 */
static inline const char* schema_abstraction_category(const schema_t* schema) {
    if (!schema) return "unknown";
    if (schema->abstraction_level < 0.33f) return "specific";
    if (schema->abstraction_level < 0.67f) return "moderate";
    return "abstract";
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SCHEMAS_H
