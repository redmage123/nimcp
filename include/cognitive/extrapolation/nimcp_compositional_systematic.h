/**
 * @file nimcp_compositional_systematic.h
 * @brief Compositional Systematic Generalization Module
 *
 * WHAT: Combines known primitives into novel compositions systematically
 * WHY:  Enable human-like compositional generalization in AI systems
 * HOW:  Grammar-based composition rules, binding mechanisms, tree structures
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex for rule application and working memory
 * - Temporal cortex for semantic primitive storage
 * - Parietal cortex for relational binding
 * - Basal ganglia for sequence composition
 *
 * INTEGRATION POINTS:
 * - JEPA for latent primitive representations
 * - Working memory for composition workspace
 * - Executive function for rule selection
 */

#ifndef NIMCP_COMPOSITIONAL_SYSTEMATIC_H
#define NIMCP_COMPOSITIONAL_SYSTEMATIC_H

#include "nimcp.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define CS_MAX_PRIMITIVES       1024
#define CS_MAX_COMPOSITIONS     256
#define CS_MAX_RULES            128
#define CS_MAX_BINDINGS         64
#define CS_MAX_DEPTH            16
#define CS_EMBEDDING_DIM        128
#define CS_MAX_FEATURES         32
#define CS_MAX_CHILDREN         8
#define CS_MAX_NAME_LEN         64

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

typedef enum {
    CS_OK = 0,
    CS_ERR_NULL_PTR,
    CS_ERR_NOT_INITIALIZED,
    CS_ERR_MEMORY_ALLOC,
    CS_ERR_CAPACITY_EXCEEDED,
    CS_ERR_PRIMITIVE_NOT_FOUND,
    CS_ERR_INVALID_PRIMITIVE,
    CS_ERR_INVALID_COMPOSITION,
    CS_ERR_INVALID_RULE,
    CS_ERR_RULE_MISMATCH,
    CS_ERR_BINDING_FAILED,
    CS_ERR_DEPTH_EXCEEDED,
    CS_ERR_VALIDATION_FAILED,
    CS_ERR_DECOMPOSITION_FAILED
} cs_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Composition structure types
 *
 * BIOLOGICAL BASIS: Different brain circuits handle different composition types
 * - Sequential: Basal ganglia, supplementary motor area
 * - Parallel: Prefrontal cortex, working memory
 * - Hierarchical: Temporal-parietal junction
 * - Recursive: Broca's area, recursive language processing
 */
typedef enum {
    CS_COMPOSE_SEQUENTIAL = 0,  /**< A then B then C */
    CS_COMPOSE_PARALLEL,        /**< A and B simultaneously */
    CS_COMPOSE_HIERARCHICAL,    /**< A contains (B, C) */
    CS_COMPOSE_RECURSIVE,       /**< A contains A-like structure */
    CS_COMPOSE_TYPE_COUNT
} cs_composition_type_t;

/**
 * @brief Primitive semantic types
 *
 * Based on linguistic/cognitive categories:
 * - Actions: Verbs, operations, transformations
 * - Objects: Nouns, entities, arguments
 * - Relations: Prepositions, connectors, spatial/temporal
 * - Modifiers: Adjectives, adverbs, attributes
 */
typedef enum {
    CS_PRIM_ACTION = 0,         /**< Verb-like: run, push, combine */
    CS_PRIM_OBJECT,             /**< Noun-like: ball, table, agent */
    CS_PRIM_RELATION,           /**< Connector: on, above, before */
    CS_PRIM_MODIFIER,           /**< Attribute: red, quickly, twice */
    CS_PRIM_TYPE_COUNT
} cs_primitive_type_t;

/**
 * @brief Binding slot types
 */
typedef enum {
    CS_BIND_AGENT = 0,          /**< Who performs action */
    CS_BIND_PATIENT,            /**< What is affected */
    CS_BIND_INSTRUMENT,         /**< Tool used */
    CS_BIND_LOCATION,           /**< Where */
    CS_BIND_TIME,               /**< When */
    CS_BIND_MANNER,             /**< How */
    CS_BIND_TYPE_COUNT
} cs_binding_type_t;

/**
 * @brief Rule application direction
 */
typedef enum {
    CS_RULE_FORWARD = 0,        /**< Apply rule to produce composition */
    CS_RULE_BACKWARD,           /**< Decompose using rule inverse */
    CS_RULE_BIDIRECTIONAL       /**< Can apply either direction */
} cs_rule_direction_t;

/**
 * @brief Module status
 */
typedef enum {
    CS_STATUS_IDLE = 0,
    CS_STATUS_COMPOSING,
    CS_STATUS_DECOMPOSING,
    CS_STATUS_VALIDATING,
    CS_STATUS_LEARNING,
    CS_STATUS_ERROR
} cs_status_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Semantic primitive representation
 *
 * WHAT: Atomic unit of meaning that can be composed
 * WHY:  Building blocks for compositional generalization
 * HOW:  Embedding vector + discrete features + type information
 */
typedef struct {
    uint32_t id;                            /**< Unique primitive ID */
    cs_primitive_type_t type;               /**< Semantic type */
    char name[CS_MAX_NAME_LEN];             /**< Human-readable name */
    float embedding[CS_EMBEDDING_DIM];      /**< Dense embedding vector */
    float features[CS_MAX_FEATURES];        /**< Discrete feature values */
    uint32_t num_features;                  /**< Number of active features */
    uint32_t arity;                         /**< Number of arguments required */
    uint32_t valid_bindings;                /**< Bitmask of valid binding slots */
    float activation;                       /**< Current activation level */
    uint64_t last_used;                     /**< Timestamp of last use */
    uint32_t use_count;                     /**< Total usage count */
} cs_primitive_t;

/**
 * @brief Binding between primitives
 *
 * WHAT: Role-filler pair connecting primitives
 * WHY:  Enable structured composition with semantic roles
 * HOW:  Slot type + primitive reference + binding strength
 */
typedef struct {
    cs_binding_type_t slot;                 /**< Role/slot type */
    uint32_t primitive_id;                  /**< Bound primitive ID */
    float strength;                         /**< Binding strength [0,1] */
    bool is_variable;                       /**< True if unbound variable */
    char variable_name[CS_MAX_NAME_LEN];    /**< Variable name if unbound */
} cs_binding_t;

/**
 * @brief Composition tree node
 *
 * WHAT: Single node in composition structure
 * WHY:  Support hierarchical and recursive compositions
 * HOW:  Head primitive + bindings + child nodes
 */
typedef struct cs_comp_node {
    uint32_t primitive_id;                  /**< Head primitive */
    cs_binding_t bindings[CS_MAX_BINDINGS]; /**< Role-filler bindings */
    uint32_t num_bindings;                  /**< Number of active bindings */
    struct cs_comp_node* children[CS_MAX_CHILDREN]; /**< Child compositions */
    uint32_t num_children;                  /**< Number of children */
    uint32_t depth;                         /**< Depth in tree */
    float confidence;                       /**< Composition confidence */
} cs_comp_node_t;

/**
 * @brief Complete composition structure
 *
 * WHAT: Full compositional representation
 * WHY:  Encapsulate entire composed meaning
 * HOW:  Root node + metadata + combined embedding
 */
typedef struct {
    uint32_t id;                            /**< Composition ID */
    cs_composition_type_t type;             /**< Structure type */
    cs_comp_node_t* root;                   /**< Root of composition tree */
    float combined_embedding[CS_EMBEDDING_DIM]; /**< Composed embedding */
    float novelty_score;                    /**< How novel is this composition */
    float grammaticality;                   /**< How well-formed */
    uint32_t total_primitives;              /**< Count of all primitives used */
    uint32_t max_depth;                     /**< Maximum tree depth */
    uint64_t created_time;                  /**< Creation timestamp */
    bool is_valid;                          /**< Validation status */
} cs_composition_t;

/**
 * @brief Grammar rule for composition
 *
 * WHAT: Production rule for creating valid compositions
 * WHY:  Constrain compositions to valid structures
 * HOW:  Pattern matching + transformation + constraints
 */
typedef struct {
    uint32_t id;                            /**< Rule ID */
    char name[CS_MAX_NAME_LEN];             /**< Rule name */
    cs_composition_type_t comp_type;        /**< Applicable composition type */
    cs_primitive_type_t head_type;          /**< Required head type */
    cs_primitive_type_t arg_types[CS_MAX_BINDINGS]; /**< Required argument types */
    uint32_t num_args;                      /**< Number of arguments */
    cs_binding_type_t slot_order[CS_MAX_BINDINGS]; /**< Order of binding slots */
    cs_rule_direction_t direction;          /**< Application direction */
    float confidence;                       /**< Rule confidence/strength */
    uint32_t application_count;             /**< Times applied */
    float success_rate;                     /**< Successful applications */
} cs_rule_t;

/**
 * @brief Module configuration
 */
typedef struct {
    uint32_t max_primitives;                /**< Maximum primitives to store */
    uint32_t max_compositions;              /**< Maximum compositions */
    uint32_t max_rules;                     /**< Maximum grammar rules */
    uint32_t max_depth;                     /**< Maximum composition depth */
    uint32_t embedding_dim;                 /**< Embedding dimension */
    float novelty_threshold;                /**< Threshold for novel detection */
    float grammaticality_threshold;         /**< Minimum grammaticality */
    float binding_decay;                    /**< Binding strength decay rate */
    float learning_rate;                    /**< Rule learning rate */
    bool enable_recursive;                  /**< Allow recursive compositions */
    bool enable_learning;                   /**< Learn new rules automatically */
    bool enable_logging;                    /**< Debug logging */
} cs_config_t;

/**
 * @brief Module statistics
 */
typedef struct {
    uint64_t primitives_registered;         /**< Total primitives added */
    uint64_t compositions_created;          /**< Total compositions made */
    uint64_t compositions_validated;        /**< Compositions that passed validation */
    uint64_t decompositions;                /**< Successful decompositions */
    uint64_t rules_applied;                 /**< Total rule applications */
    uint64_t rules_learned;                 /**< Auto-learned rules */
    uint32_t active_primitives;             /**< Currently stored primitives */
    uint32_t active_compositions;           /**< Currently stored compositions */
    uint32_t active_rules;                  /**< Currently stored rules */
    float mean_composition_depth;           /**< Average depth */
    float mean_novelty;                     /**< Average novelty score */
    float mean_grammaticality;              /**< Average grammaticality */
} cs_stats_t;

/**
 * @brief Main compositional systematic module
 */
typedef struct nimcp_compositional {
    cs_config_t config;                     /**< Configuration */
    bool initialized;                       /**< Initialization status */
    cs_status_t status;                     /**< Current status */
    cs_error_t last_error;                  /**< Last error code */

    /* Primitive storage */
    cs_primitive_t* primitives;             /**< Primitive array */
    uint32_t num_primitives;                /**< Current primitive count */
    uint32_t primitive_capacity;            /**< Allocated capacity */
    uint32_t next_primitive_id;             /**< Next ID to assign */

    /* Composition storage */
    cs_composition_t* compositions;         /**< Composition array */
    uint32_t num_compositions;              /**< Current composition count */
    uint32_t composition_capacity;          /**< Allocated capacity */
    uint32_t next_composition_id;           /**< Next ID to assign */

    /* Grammar rules */
    cs_rule_t* rules;                       /**< Rule array */
    uint32_t num_rules;                     /**< Current rule count */
    uint32_t rule_capacity;                 /**< Allocated capacity */
    uint32_t next_rule_id;                  /**< Next ID to assign */

    /* Working memory for composition */
    cs_comp_node_t* workspace;              /**< Current workspace */
    uint32_t workspace_depth;               /**< Current depth */

    /* Statistics */
    cs_stats_t stats;                       /**< Performance statistics */

    /* Integration contexts */
    void* jepa_ctx;                         /**< JEPA integration */
    void* wm_ctx;                           /**< Working memory integration */
    void* exec_ctx;                         /**< Executive function integration */
} nimcp_compositional_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns configuration with sensible defaults
 * WHY:  Easy module initialization
 * HOW:  Pre-filled config struct
 *
 * @return Default configuration
 */
NIMCP_EXPORT cs_config_t cs_default_config(void);

/**
 * @brief Create compositional module instance
 *
 * WHAT: Allocates and initializes module
 * WHY:  Entry point for using compositional generalization
 * HOW:  Allocates memory, sets defaults, initializes structures
 *
 * @param config Configuration (NULL for defaults)
 * @return Module instance or NULL on failure
 */
NIMCP_EXPORT nimcp_compositional_t* cs_create(const cs_config_t* config);

/**
 * @brief Initialize module
 *
 * WHAT: Completes initialization after creation
 * WHY:  Two-phase init allows config modification
 * HOW:  Initializes internal state, loads base rules
 *
 * @param cs Module instance
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_init(nimcp_compositional_t* cs);

/**
 * @brief Reset module state
 *
 * WHAT: Clears all compositions, keeps primitives and rules
 * WHY:  Prepare for new composition session
 * HOW:  Clears workspace and composition storage
 *
 * @param cs Module instance
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_reset(nimcp_compositional_t* cs);

/**
 * @brief Destroy module and free resources
 *
 * WHAT: Deallocates all module memory
 * WHY:  Clean resource management
 * HOW:  Frees all allocated structures
 *
 * @param cs Module instance (NULL safe)
 */
NIMCP_EXPORT void cs_destroy(nimcp_compositional_t* cs);

/*=============================================================================
 * PRIMITIVE API
 *===========================================================================*/

/**
 * @brief Register a new primitive
 *
 * WHAT: Adds a semantic primitive to the module
 * WHY:  Build vocabulary for composition
 * HOW:  Copies primitive data, assigns ID
 *
 * @param cs Module instance
 * @param primitive Primitive to register
 * @param id_out Output: assigned primitive ID
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_register_primitive(
    nimcp_compositional_t* cs,
    const cs_primitive_t* primitive,
    uint32_t* id_out);

/**
 * @brief Get primitive by ID
 *
 * WHAT: Retrieves stored primitive
 * WHY:  Access primitive data for composition
 * HOW:  Direct lookup by ID
 *
 * @param cs Module instance
 * @param id Primitive ID
 * @param primitive_out Output: primitive data
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_get_primitive(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_primitive_t* primitive_out);

/**
 * @brief Get primitive by name
 *
 * WHAT: Finds primitive with matching name
 * WHY:  Human-friendly primitive access
 * HOW:  Linear search through primitives
 *
 * @param cs Module instance
 * @param name Primitive name
 * @param primitive_out Output: primitive data
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_get_primitive_by_name(
    nimcp_compositional_t* cs,
    const char* name,
    cs_primitive_t* primitive_out);

/**
 * @brief Remove primitive from storage
 *
 * WHAT: Removes a primitive by ID
 * WHY:  Memory management, vocabulary updates
 * HOW:  Marks slot as unused
 *
 * @param cs Module instance
 * @param id Primitive ID to remove
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_remove_primitive(
    nimcp_compositional_t* cs,
    uint32_t id);

/**
 * @brief Find primitives by type
 *
 * WHAT: Gets all primitives of specified type
 * WHY:  Filter primitives for composition
 * HOW:  Scans and filters by type
 *
 * @param cs Module instance
 * @param type Primitive type to find
 * @param primitives_out Output: primitive array
 * @param count_out Output: number found
 * @param max_count Maximum to return
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_find_primitives_by_type(
    nimcp_compositional_t* cs,
    cs_primitive_type_t type,
    cs_primitive_t* primitives_out,
    uint32_t* count_out,
    uint32_t max_count);

/*=============================================================================
 * COMPOSITION API
 *===========================================================================*/

/**
 * @brief Compose primitives into novel combination
 *
 * WHAT: Creates new composition from primitives
 * WHY:  Core compositional generalization operation
 * HOW:  Applies grammar rules, creates tree structure
 *
 * @param cs Module instance
 * @param comp_type Type of composition to create
 * @param head_id Head primitive ID
 * @param bindings Array of bindings
 * @param num_bindings Number of bindings
 * @param composition_out Output: created composition
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_compose(
    nimcp_compositional_t* cs,
    cs_composition_type_t comp_type,
    uint32_t head_id,
    const cs_binding_t* bindings,
    uint32_t num_bindings,
    cs_composition_t* composition_out);

/**
 * @brief Compose using child compositions
 *
 * WHAT: Creates hierarchical composition
 * WHY:  Build complex nested structures
 * HOW:  Attaches child compositions to parent node
 *
 * @param cs Module instance
 * @param comp_type Type of composition
 * @param head_id Head primitive ID
 * @param children Child composition IDs
 * @param num_children Number of children
 * @param composition_out Output: created composition
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_compose_hierarchical(
    nimcp_compositional_t* cs,
    cs_composition_type_t comp_type,
    uint32_t head_id,
    const uint32_t* children,
    uint32_t num_children,
    cs_composition_t* composition_out);

/**
 * @brief Decompose composition into primitives
 *
 * WHAT: Breaks down composition into components
 * WHY:  Analyze structure, extract primitives
 * HOW:  Traverses tree, extracts primitives and bindings
 *
 * @param cs Module instance
 * @param composition Composition to decompose
 * @param primitives_out Output: extracted primitives
 * @param bindings_out Output: extracted bindings
 * @param num_primitives_out Output: primitive count
 * @param num_bindings_out Output: binding count
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_decompose(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    cs_primitive_t* primitives_out,
    cs_binding_t* bindings_out,
    uint32_t* num_primitives_out,
    uint32_t* num_bindings_out);

/**
 * @brief Validate composition structure
 *
 * WHAT: Checks if composition is well-formed
 * WHY:  Ensure grammatical correctness
 * HOW:  Applies all applicable rules, checks constraints
 *
 * @param cs Module instance
 * @param composition Composition to validate
 * @param grammaticality_out Output: grammaticality score [0,1]
 * @return CS_OK if valid, CS_ERR_VALIDATION_FAILED otherwise
 */
NIMCP_EXPORT cs_error_t cs_validate_composition(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    float* grammaticality_out);

/**
 * @brief Get stored composition by ID
 *
 * WHAT: Retrieves previously created composition
 * WHY:  Access cached compositions
 * HOW:  Direct lookup by ID
 *
 * @param cs Module instance
 * @param id Composition ID
 * @param composition_out Output: composition data
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_get_composition(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_composition_t* composition_out);

/**
 * @brief Compute novelty of composition
 *
 * WHAT: Measures how novel a composition is
 * WHY:  Identify truly new combinations
 * HOW:  Compare to existing compositions, check primitive co-occurrence
 *
 * @param cs Module instance
 * @param composition Composition to evaluate
 * @param novelty_out Output: novelty score [0,1]
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_compute_novelty(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    float* novelty_out);

/*=============================================================================
 * RULE API
 *===========================================================================*/

/**
 * @brief Apply grammar rule to create composition
 *
 * WHAT: Uses specific rule to compose primitives
 * WHY:  Controlled composition via explicit rules
 * HOW:  Matches primitives to rule pattern, produces composition
 *
 * @param cs Module instance
 * @param rule_id Rule to apply
 * @param primitives Input primitives
 * @param num_primitives Number of primitives
 * @param composition_out Output: resulting composition
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_apply_rule(
    nimcp_compositional_t* cs,
    uint32_t rule_id,
    const cs_primitive_t* primitives,
    uint32_t num_primitives,
    cs_composition_t* composition_out);

/**
 * @brief Infer rule from example composition
 *
 * WHAT: Learns new rule from composition example
 * WHY:  Automatic grammar acquisition
 * HOW:  Extracts pattern from composition, creates rule
 *
 * @param cs Module instance
 * @param composition Example composition
 * @param rule_out Output: inferred rule
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_infer_rule(
    nimcp_compositional_t* cs,
    const cs_composition_t* composition,
    cs_rule_t* rule_out);

/**
 * @brief Add grammar rule
 *
 * WHAT: Registers a new grammar rule
 * WHY:  Expand compositional capabilities
 * HOW:  Stores rule in rule set
 *
 * @param cs Module instance
 * @param rule Rule to add
 * @param id_out Output: assigned rule ID
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_add_rule(
    nimcp_compositional_t* cs,
    const cs_rule_t* rule,
    uint32_t* id_out);

/**
 * @brief Get rule by ID
 *
 * WHAT: Retrieves stored rule
 * WHY:  Access rule data
 * HOW:  Direct lookup by ID
 *
 * @param cs Module instance
 * @param id Rule ID
 * @param rule_out Output: rule data
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_get_rule(
    nimcp_compositional_t* cs,
    uint32_t id,
    cs_rule_t* rule_out);

/**
 * @brief Find applicable rules for primitives
 *
 * WHAT: Gets rules that can compose given primitives
 * WHY:  Guide composition process
 * HOW:  Matches primitive types to rule patterns
 *
 * @param cs Module instance
 * @param primitives Input primitives
 * @param num_primitives Number of primitives
 * @param rules_out Output: applicable rules
 * @param count_out Output: number of rules
 * @param max_count Maximum to return
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_find_applicable_rules(
    nimcp_compositional_t* cs,
    const cs_primitive_t* primitives,
    uint32_t num_primitives,
    cs_rule_t* rules_out,
    uint32_t* count_out,
    uint32_t max_count);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Update module state
 *
 * WHAT: Performs periodic maintenance
 * WHY:  Decay old bindings, update statistics
 * HOW:  Called regularly to maintain state
 *
 * @param cs Module instance
 * @param dt_ms Time delta in milliseconds
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_update(
    nimcp_compositional_t* cs,
    float dt_ms);

/**
 * @brief Get module statistics
 *
 * WHAT: Retrieves performance statistics
 * WHY:  Monitor module behavior
 * HOW:  Copies current stats
 *
 * @param cs Module instance
 * @param stats_out Output: statistics
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_get_stats(
    nimcp_compositional_t* cs,
    cs_stats_t* stats_out);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears all statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all stat fields
 *
 * @param cs Module instance
 * @return CS_OK or error code
 */
NIMCP_EXPORT cs_error_t cs_reset_stats(nimcp_compositional_t* cs);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get current module status
 *
 * @param cs Module instance
 * @return Current status
 */
NIMCP_EXPORT cs_status_t cs_get_status(nimcp_compositional_t* cs);

/**
 * @brief Get last error code
 *
 * @param cs Module instance
 * @return Last error
 */
NIMCP_EXPORT cs_error_t cs_get_last_error(nimcp_compositional_t* cs);

/**
 * @brief Convert error code to string
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* cs_error_string(cs_error_t error);

/**
 * @brief Convert status to string
 *
 * @param status Status code
 * @return Human-readable status string
 */
NIMCP_EXPORT const char* cs_status_string(cs_status_t status);

/**
 * @brief Convert composition type to string
 *
 * @param type Composition type
 * @return Human-readable type string
 */
NIMCP_EXPORT const char* cs_composition_type_string(cs_composition_type_t type);

/**
 * @brief Convert primitive type to string
 *
 * @param type Primitive type
 * @return Human-readable type string
 */
NIMCP_EXPORT const char* cs_primitive_type_string(cs_primitive_type_t type);

/**
 * @brief Convert binding type to string
 *
 * @param type Binding type
 * @return Human-readable type string
 */
NIMCP_EXPORT const char* cs_binding_type_string(cs_binding_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMPOSITIONAL_SYSTEMATIC_H */
