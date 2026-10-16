/**
 * @file nimcp_analogical_reasoning.h
 * @brief Analogical reasoning engine for cross-domain transfer
 *
 * WHAT: Engine for analogical reasoning and cross-domain knowledge transfer
 * WHY:  Enable learning by analogy - applying solutions from known to new domains
 * HOW:  Structure mapping, relational abstraction, analogical inference
 *
 * BIOLOGICAL BASIS:
 * Analogical reasoning relies on the parietal cortex's ability to detect
 * relational similarities across different representational domains. This
 * is central to human creativity and problem-solving by transfer.
 *
 * CAPABILITIES:
 * - Structure Mapping: Identify structural similarities between domains
 * - Cross-Domain Transfer: Apply solutions from one domain to another
 * - Analogy Generation: Create new analogies to explain concepts
 * - Relational Abstraction: Extract abstract relationships from examples
 * - Analogy Evaluation: Assess quality and applicability of analogies
 *
 * THEORETICAL BASIS:
 * Based on Structure-Mapping Theory (Gentner) and analogical reasoning
 * research from cognitive science.
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_ANALOGICAL_REASONING_H
#define NIMCP_ANALOGICAL_REASONING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum domain name length */
#define ANALOG_MAX_NAME_LENGTH          128

/** Maximum entities in a domain */
#define ANALOG_MAX_ENTITIES             64

/** Maximum relations in a domain */
#define ANALOG_MAX_RELATIONS            128

/** Maximum mappings in an analogy */
#define ANALOG_MAX_MAPPINGS             64

/** Maximum abstraction levels */
#define ANALOG_MAX_ABSTRACTION_LEVELS   8

/** Bio-async module ID for analogical reasoning */
#define BIO_MODULE_ANALOGICAL_REASONING 0x03A1

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/** Opaque handle for analogical reasoning engine */
typedef struct analogical_engine analogical_engine_t;

/* ============================================================================
 * CORE TYPES
 * ============================================================================ */

/**
 * @brief Entity in a conceptual domain
 */
typedef struct {
    uint32_t id;                        /**< Unique entity identifier */
    char name[ANALOG_MAX_NAME_LENGTH];  /**< Entity name */
    float* features;                    /**< Feature vector */
    uint32_t num_features;              /**< Number of features */
    char type[64];                      /**< Entity type/category */
    float salience;                     /**< How prominent in domain [0,1] */
} analog_entity_t;

/**
 * @brief Relation between entities
 */
typedef struct {
    uint32_t id;                        /**< Unique relation identifier */
    char name[ANALOG_MAX_NAME_LENGTH];  /**< Relation name (e.g., "causes") */
    uint32_t subject_id;                /**< Subject entity ID */
    uint32_t object_id;                 /**< Object entity ID */
    float strength;                     /**< Relation strength [0,1] */
    uint32_t order;                     /**< Relation order (1st, 2nd, higher) */
    bool is_symmetric;                  /**< Is relation symmetric? */
    bool is_transitive;                 /**< Is relation transitive? */
} analog_relation_t;

/**
 * @brief Conceptual domain representation
 */
typedef struct {
    uint32_t id;                        /**< Unique domain identifier */
    char name[ANALOG_MAX_NAME_LENGTH];  /**< Domain name */
    char description[256];              /**< Domain description */

    analog_entity_t* entities;          /**< Array of entities */
    uint32_t num_entities;              /**< Number of entities */

    analog_relation_t* relations;       /**< Array of relations */
    uint32_t num_relations;             /**< Number of relations */

    float* domain_features;             /**< Global domain features */
    uint32_t num_domain_features;       /**< Number of domain features */

    float abstraction_level;            /**< How abstract is domain [0,1] */
    float familiarity;                  /**< System familiarity with domain [0,1] */
} analog_domain_t;

/**
 * @brief Mapping between entities/relations across domains
 */
typedef struct {
    uint32_t source_id;                 /**< ID in source domain */
    uint32_t target_id;                 /**< ID in target domain */
    bool is_entity_mapping;             /**< true=entity, false=relation */
    float confidence;                   /**< Mapping confidence [0,1] */
    float structural_support;           /**< Support from structure [0,1] */
} analog_mapping_t;

/**
 * @brief Complete analogy between two domains
 */
typedef struct {
    uint32_t id;                        /**< Unique analogy identifier */

    analog_domain_t* source;            /**< Known/base domain */
    analog_domain_t* target;            /**< New/target domain */

    analog_mapping_t* mappings;         /**< Entity and relation mappings */
    uint32_t num_mappings;              /**< Number of mappings */

    float mapping_strength;             /**< Overall mapping quality [0,1] */
    float structural_consistency;       /**< One-to-one consistency [0,1] */
    float relational_depth;             /**< Depth of relational match [0,1] */
    float systematicity;                /**< Preference for systems [0,1] */

    char* inference_potential;          /**< What can be inferred */
    bool is_superficial;                /**< Surface vs structural analogy */
} analog_analogy_t;

/**
 * @brief Abstracted principle extracted from analogies
 */
typedef struct {
    uint32_t id;                        /**< Unique principle identifier */
    char name[ANALOG_MAX_NAME_LENGTH];  /**< Principle name */
    char description[512];              /**< Principle description */

    char** abstract_relations;          /**< Abstract relation patterns */
    uint32_t num_abstract_relations;    /**< Number of patterns */

    float* abstract_features;           /**< Abstract feature pattern */
    uint32_t num_abstract_features;     /**< Number of features */

    float generality;                   /**< How general is principle [0,1] */
    uint32_t supporting_analogies;      /**< Count of supporting analogies */
} analog_abstraction_t;

/**
 * @brief Solution that can be transferred
 */
typedef struct {
    uint32_t id;                        /**< Unique solution identifier */
    char description[256];              /**< Solution description */

    float* solution_steps;              /**< Solution as sequence */
    uint32_t num_steps;                 /**< Number of steps */

    uint32_t source_domain_id;          /**< Original domain */
    float applicability;                /**< Applicability to target [0,1] */
    float adaptation_required;          /**< How much adaptation needed [0,1] */

    char** adaptations;                 /**< Required adaptations */
    uint32_t num_adaptations;           /**< Number of adaptations */
} analog_solution_t;

/**
 * @brief Analogy quality assessment
 */
typedef struct {
    float structural_similarity;        /**< Structural match [0,1] */
    float relational_similarity;        /**< Relational match [0,1] */
    float surface_similarity;           /**< Surface feature match [0,1] */
    float systematicity_score;          /**< System preference [0,1] */
    float one_to_one_score;             /**< Mapping consistency [0,1] */
    float parallel_connectivity;        /**< Connected structure match [0,1] */
    float overall_quality;              /**< Overall quality [0,1] */
    bool is_valid;                      /**< Meets minimum criteria */
} analog_quality_t;

/**
 * @brief Engine configuration
 */
typedef struct {
    float min_mapping_strength;         /**< Minimum to accept mapping */
    float systematicity_weight;         /**< Weight for systematicity */
    float surface_weight;               /**< Weight for surface features */
    float structural_weight;            /**< Weight for structural features */

    bool prefer_deep_analogies;         /**< Prefer structural over surface */
    bool allow_partial_mappings;        /**< Allow incomplete mappings */
    bool enable_abstraction;            /**< Enable principle extraction */

    uint32_t max_domains_cache;         /**< Max domains to cache */
    uint32_t max_analogies_cache;       /**< Max analogies to cache */

    float learning_rate;                /**< How fast to update similarities */
    float inflammation_sensitivity;     /**< Response to inflammation */
    float fatigue_sensitivity;          /**< Response to fatigue */
} analog_config_t;

/**
 * @brief Engine statistics
 */
typedef struct {
    uint64_t analogies_found;           /**< Total analogies discovered */
    uint64_t mappings_created;          /**< Total mappings made */
    uint64_t solutions_transferred;     /**< Solutions successfully transferred */
    uint64_t abstractions_extracted;    /**< Principles extracted */
    uint64_t domains_processed;         /**< Domains analyzed */

    float avg_mapping_strength;         /**< Average mapping quality */
    float avg_transfer_success;         /**< Average transfer success rate */
    float avg_processing_time_us;       /**< Average processing time */
} analog_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create analogical reasoning engine with default config
 * @return Engine handle or NULL on failure
 */
analogical_engine_t* analogical_engine_create(void);

/**
 * @brief Create engine with custom configuration
 * @param config Configuration parameters
 * @return Engine handle or NULL on failure
 */
analogical_engine_t* analogical_engine_create_custom(const analog_config_t* config);

/**
 * @brief Destroy engine and free resources
 * @param engine Engine to destroy
 */
void analogical_engine_destroy(analogical_engine_t* engine);

/**
 * @brief Get default configuration
 * @return Default config structure
 */
analog_config_t analogical_engine_default_config(void);

/* ============================================================================
 * DOMAIN MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Create a new domain
 *
 * @param name Domain name
 * @param description Domain description
 * @return Domain structure (caller must free)
 */
analog_domain_t* analogical_create_domain(
    const char* name,
    const char* description
);

/**
 * @brief Add entity to domain
 *
 * @param domain Domain to modify
 * @param name Entity name
 * @param type Entity type
 * @param features Feature vector
 * @param num_features Number of features
 * @return Entity ID or 0 on failure
 */
uint32_t analogical_add_entity(
    analog_domain_t* domain,
    const char* name,
    const char* type,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Add relation to domain
 *
 * @param domain Domain to modify
 * @param name Relation name
 * @param subject_id Subject entity ID
 * @param object_id Object entity ID
 * @param strength Relation strength
 * @return Relation ID or 0 on failure
 */
uint32_t analogical_add_relation(
    analog_domain_t* domain,
    const char* name,
    uint32_t subject_id,
    uint32_t object_id,
    float strength
);

/**
 * @brief Register domain with engine
 *
 * @param engine Engine handle
 * @param domain Domain to register
 * @return 0 on success, -1 on error
 */
int analogical_register_domain(
    analogical_engine_t* engine,
    analog_domain_t* domain
);

/**
 * @brief Free domain resources
 * @param domain Domain to free
 */
void analogical_free_domain(analog_domain_t* domain);

/* ============================================================================
 * ANALOGY FINDING API
 * ============================================================================ */

/**
 * @brief Find analogy between source and target domains
 *
 * Performs structure mapping to find correspondences.
 *
 * @param engine Engine handle
 * @param source Source (known) domain
 * @param target Target (new) domain
 * @return Analogy or NULL if none found
 */
analog_analogy_t* analogical_find_analogy(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target
);

/**
 * @brief Find best analogy for target from known domains
 *
 * Searches registered domains for best match.
 *
 * @param engine Engine handle
 * @param target Target domain to understand
 * @return Best analogy or NULL if none found
 */
analog_analogy_t* analogical_find_best_analogy(
    analogical_engine_t* engine,
    const analog_domain_t* target
);

/**
 * @brief Find multiple analogies for target
 *
 * @param engine Engine handle
 * @param target Target domain
 * @param analogies Output array of analogies
 * @param max_analogies Maximum to find
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int analogical_find_multiple_analogies(
    analogical_engine_t* engine,
    const analog_domain_t* target,
    analog_analogy_t** analogies,
    uint32_t max_analogies,
    uint32_t* num_found
);

/**
 * @brief Evaluate analogy quality
 *
 * @param engine Engine handle
 * @param analogy Analogy to evaluate
 * @param quality Output quality assessment
 * @return 0 on success, -1 on error
 */
int analogical_evaluate_analogy(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    analog_quality_t* quality
);

/**
 * @brief Free analogy resources
 * @param analogy Analogy to free
 */
void analogical_free_analogy(analog_analogy_t* analogy);

/* ============================================================================
 * STRUCTURE MAPPING API
 * ============================================================================ */

/**
 * @brief Compute structural similarity between domains
 *
 * @param engine Engine handle
 * @param domain1 First domain
 * @param domain2 Second domain
 * @return Structural similarity [0,1]
 */
float analogical_structural_similarity(
    analogical_engine_t* engine,
    const analog_domain_t* domain1,
    const analog_domain_t* domain2
);

/**
 * @brief Map entities between domains
 *
 * @param engine Engine handle
 * @param source Source domain
 * @param target Target domain
 * @param mappings Output array of mappings
 * @param max_mappings Maximum mappings
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int analogical_map_entities(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target,
    analog_mapping_t* mappings,
    uint32_t max_mappings,
    uint32_t* num_found
);

/**
 * @brief Map relations between domains
 *
 * @param engine Engine handle
 * @param source Source domain
 * @param target Target domain
 * @param entity_mappings Existing entity mappings
 * @param num_entity_mappings Number of entity mappings
 * @param relation_mappings Output relation mappings
 * @param max_mappings Maximum mappings
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int analogical_map_relations(
    analogical_engine_t* engine,
    const analog_domain_t* source,
    const analog_domain_t* target,
    const analog_mapping_t* entity_mappings,
    uint32_t num_entity_mappings,
    analog_mapping_t* relation_mappings,
    uint32_t max_mappings,
    uint32_t* num_found
);

/* ============================================================================
 * CROSS-DOMAIN TRANSFER API
 * ============================================================================ */

/**
 * @brief Transfer solution from source to target domain
 *
 * Uses analogy to adapt solution.
 *
 * @param engine Engine handle
 * @param analogy Analogy to use
 * @param source_solution Solution in source domain
 * @return Adapted solution or NULL on failure
 */
analog_solution_t* analogical_transfer_solution(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_solution_t* source_solution
);

/**
 * @brief Infer new knowledge in target domain
 *
 * @param engine Engine handle
 * @param analogy Analogy to use
 * @param source_relation Relation from source domain
 * @param inferred_relation Output inferred relation
 * @return Confidence of inference [0,1], or -1 on error
 */
float analogical_infer_relation(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_relation_t* source_relation,
    analog_relation_t* inferred_relation
);

/**
 * @brief Predict entity properties in target domain
 *
 * @param engine Engine handle
 * @param analogy Analogy to use
 * @param source_entity Entity from source domain
 * @param predicted_features Output predicted features
 * @param max_features Maximum features
 * @param num_features Output number of features
 * @return 0 on success, -1 on error
 */
int analogical_predict_properties(
    analogical_engine_t* engine,
    const analog_analogy_t* analogy,
    const analog_entity_t* source_entity,
    float* predicted_features,
    uint32_t max_features,
    uint32_t* num_features
);

/**
 * @brief Free solution resources
 * @param solution Solution to free
 */
void analogical_free_solution(analog_solution_t* solution);

/* ============================================================================
 * ABSTRACTION API
 * ============================================================================ */

/**
 * @brief Extract abstract principle from analogies
 *
 * @param engine Engine handle
 * @param analogies Array of analogies
 * @param num_analogies Number of analogies
 * @return Abstracted principle or NULL
 */
analog_abstraction_t* analogical_extract_principle(
    analogical_engine_t* engine,
    const analog_analogy_t** analogies,
    uint32_t num_analogies
);

/**
 * @brief Abstract relation from multiple examples
 *
 * @param engine Engine handle
 * @param relations Array of concrete relations
 * @param num_relations Number of relations
 * @param abstract_name Output abstract relation name
 * @param max_name_len Maximum name length
 * @return Abstraction level [0,1], or -1 on error
 */
float analogical_abstract_relation(
    analogical_engine_t* engine,
    const analog_relation_t* relations,
    uint32_t num_relations,
    char* abstract_name,
    uint32_t max_name_len
);

/**
 * @brief Apply abstraction to new domain
 *
 * @param engine Engine handle
 * @param abstraction Abstraction to apply
 * @param target Target domain
 * @param instantiated_relations Output relations
 * @param max_relations Maximum relations
 * @param num_found Output number found
 * @return 0 on success, -1 on error
 */
int analogical_apply_abstraction(
    analogical_engine_t* engine,
    const analog_abstraction_t* abstraction,
    const analog_domain_t* target,
    analog_relation_t* instantiated_relations,
    uint32_t max_relations,
    uint32_t* num_found
);

/**
 * @brief Free abstraction resources
 * @param abstraction Abstraction to free
 */
void analogical_free_abstraction(analog_abstraction_t* abstraction);

/* ============================================================================
 * ANALOGY GENERATION API
 * ============================================================================ */

/**
 * @brief Generate analogy to explain concept
 *
 * @param engine Engine handle
 * @param concept_domain Domain containing concept
 * @param audience_familiarity What audience knows
 * @return Generated analogy or NULL
 */
analog_analogy_t* analogical_generate_explanation(
    analogical_engine_t* engine,
    const analog_domain_t* concept_domain,
    const analog_domain_t* audience_familiarity
);

/**
 * @brief Complete partial analogy
 *
 * A:B :: C:? - find the missing element
 *
 * @param engine Engine handle
 * @param a First element of analogy
 * @param b Second element (related to a)
 * @param c Third element (analogous to a)
 * @param d_features Output features of fourth element
 * @param max_features Maximum features
 * @param num_features Output number of features
 * @return Confidence [0,1], or -1 on error
 */
float analogical_complete_analogy(
    analogical_engine_t* engine,
    const analog_entity_t* a,
    const analog_entity_t* b,
    const analog_entity_t* c,
    float* d_features,
    uint32_t max_features,
    uint32_t* num_features
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 * @param engine Engine handle
 * @param level Inflammation level [0,1]
 * @return 0 on success, -1 on error
 */
int analogical_set_inflammation(analogical_engine_t* engine, float level);

/**
 * @brief Set fatigue level
 * @param engine Engine handle
 * @param level Fatigue level [0,1]
 * @return 0 on success, -1 on error
 */
int analogical_set_fatigue(analogical_engine_t* engine, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get engine statistics
 * @param engine Engine handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int analogical_get_stats(const analogical_engine_t* engine, analog_stats_t* stats);

/**
 * @brief Reset statistics
 * @param engine Engine handle
 */
void analogical_reset_stats(analogical_engine_t* engine);

/**
 * @brief Get last error message
 * @return Error message string
 */
const char* analogical_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALOGICAL_REASONING_H */
