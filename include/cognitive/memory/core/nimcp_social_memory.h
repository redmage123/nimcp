//=============================================================================
// nimcp_social_memory.h - Social Memory System for Prime Resonant Architecture
//=============================================================================
/**
 * @file nimcp_social_memory.h
 * @brief Memory for people, relationships, and social interactions
 *
 * WHAT: Social memory system storing representations of individuals, relationships,
 *       trust levels, and social episodes with Prime Resonant integration
 * WHY:  Social cognition requires specialized memory structures for tracking
 *       individuals, their trustworthiness, relationship types, and interaction history
 * HOW:  Person nodes with multi-modal signatures (face, voice), relationship matrices,
 *       trust dynamics, and social episode memories - all integrated with PR architecture
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Social Memory in the Brain:
 *   +-----------------------------------------------------------------------+
 *   |  Social memory involves multiple specialized circuits:                |
 *   |                                                                       |
 *   |  1. FUSIFORM FACE AREA (FFA)                                          |
 *   |     - Face recognition and identity processing                        |
 *   |     - Maps to: face_signature in person_node_t                        |
 *   |                                                                       |
 *   |  2. SUPERIOR TEMPORAL SULCUS (STS)                                    |
 *   |     - Voice recognition, social perception, gaze tracking             |
 *   |     - Maps to: voice_signature in person_node_t                       |
 *   |                                                                       |
 *   |  3. ANTERIOR TEMPORAL LOBE (ATL)                                      |
 *   |     - Person-specific semantic knowledge                              |
 *   |     - Maps to: facts array in person_node_t                           |
 *   |                                                                       |
 *   |  4. AMYGDALA & INSULA                                                 |
 *   |     - Trust evaluation, social emotion                                |
 *   |     - Maps to: trust_level, liking, person_quaternion                 |
 *   |                                                                       |
 *   |  5. MEDIAL PREFRONTAL CORTEX (mPFC)                                   |
 *   |     - Self-other distinction, mentalizing, social reasoning           |
 *   |     - Maps to: relationship tracking, social network analysis         |
 *   |                                                                       |
 *   |  6. HIPPOCAMPUS                                                       |
 *   |     - Episodic social memories (who-what-where-when)                  |
 *   |     - Maps to: social_episode_t                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Trust Learning Dynamics:
 *   +-----------------------------------------------------------------------+
 *   |  Trust evolves via reinforcement learning:                            |
 *   |                                                                       |
 *   |  trust_new = trust_old + alpha * (outcome - expected_outcome)         |
 *   |                                                                       |
 *   |  where alpha (learning rate) depends on:                              |
 *   |  - Relationship type (higher for close relationships)                 |
 *   |  - Emotional salience of outcome (amygdala modulation)                |
 *   |  - Recency of interaction (more recent = higher alpha)                |
 *   |                                                                       |
 *   |  Trust decay: trust *= decay^elapsed_time (gradual forgetting)        |
 *   +-----------------------------------------------------------------------+
 *
 *   Relationship Network Architecture:
 *   +-----------------------------------------------------------------------+
 *   |  Social network stored as weighted graph:                             |
 *   |                                                                       |
 *   |  - Nodes: Person IDs                                                  |
 *   |  - Edges: Relationship strength (0-1)                                 |
 *   |  - Edge types: REL_* enum (family, friend, colleague, etc.)           |
 *   |                                                                       |
 *   |  Network metrics:                                                     |
 *   |  - Centrality: Who is most connected                                  |
 *   |  - Clustering: Social cliques/groups                                  |
 *   |  - Path length: Degrees of separation                                 |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Person add/lookup: O(1) average (hash table)
 * - Person identification: O(log N) with signature index
 * - Relationship query: O(1) average (matrix/hash)
 * - Social network analysis: O(N^2) for full graph metrics
 * - Episode recording: O(1) + entanglement cost
 *
 * MEMORY:
 * - person_node_t: ~256 bytes + signature data
 * - social_episode_t: ~128 bytes + participant array
 * - social_memory_t: ~2KB base + N*persons + E*episodes
 *
 * THREAD SAFETY:
 * - Read operations: Thread-safe (RW lock)
 * - Write operations: Thread-safe (exclusive lock)
 * - Iteration: Use snapshot or external locking
 *
 * INTEGRATION:
 * - Prime Signatures: Face/voice recognition via content fingerprints
 * - Quaternions: Emotional state toward each person
 * - Entanglement Graph: Person-person associations
 * - PR Memory Nodes: Episode storage in Z-ladder
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_SOCIAL_MEMORY_H
#define NIMCP_SOCIAL_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Prime Resonant dependencies
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_resonance.h"

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

/** Maximum number of persons in social memory (default) */
#define SOCIAL_MEM_DEFAULT_MAX_PERSONS      1024

/** Maximum number of episodes in social memory (default) */
#define SOCIAL_MEM_DEFAULT_MAX_EPISODES     4096

/** Maximum facts per person */
#define SOCIAL_MEM_MAX_FACTS_PER_PERSON     64

/** Maximum participants per episode */
#define SOCIAL_MEM_MAX_EPISODE_PARTICIPANTS 32

/** Default trust decay rate (per day) */
#define SOCIAL_MEM_TRUST_DECAY_RATE         0.001f

/** Default relationship decay rate (per day) */
#define SOCIAL_MEM_REL_DECAY_RATE           0.0005f

/** Trust learning rate (alpha) default */
#define SOCIAL_MEM_TRUST_ALPHA              0.1f

/** Minimum trust level */
#define SOCIAL_MEM_MIN_TRUST                0.0f

/** Maximum trust level */
#define SOCIAL_MEM_MAX_TRUST                1.0f

/** Initial trust for strangers */
#define SOCIAL_MEM_INITIAL_TRUST            0.5f

/** Threshold for "trusted" classification */
#define SOCIAL_MEM_TRUSTED_THRESHOLD        0.7f

/** Threshold for "familiar" classification */
#define SOCIAL_MEM_FAMILIAR_THRESHOLD       0.5f

/** Signature similarity threshold for identification */
#define SOCIAL_MEM_ID_THRESHOLD             0.85f

/** Invalid person ID sentinel */
#define SOCIAL_MEM_INVALID_PERSON_ID        UINT64_MAX

/** Invalid episode ID sentinel */
#define SOCIAL_MEM_INVALID_EPISODE_ID       UINT64_MAX

/** Numerical epsilon for floating-point comparisons */
#define SOCIAL_MEM_EPSILON                  1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Relationship type enumeration
 *
 * WHAT: Categorical classification of interpersonal relationships
 * WHY:  Different relationship types have different trust dynamics and social norms
 * HOW:  Enum stored with each person-to-person relationship
 *
 * Biological basis: Attachment theory + social brain hypothesis
 */
typedef enum {
    REL_FAMILY = 0,      /**< Kin relationship (highest baseline trust) */
    REL_FRIEND,          /**< Close friend (high trust, mutual support) */
    REL_ACQUAINTANCE,    /**< Known person, limited interaction */
    REL_COLLEAGUE,       /**< Work/professional relationship */
    REL_AUTHORITY,       /**< Person with authority over self */
    REL_SUBORDINATE,     /**< Person under self's authority */
    REL_ROMANTIC,        /**< Romantic partner (highest emotional investment) */
    REL_STRANGER,        /**< Unknown person (default) */
    REL_ADVERSARY,       /**< Hostile/competitive relationship */
    REL_TYPE_COUNT       /**< Number of relationship types (for arrays) */
} relationship_type_t;

/**
 * @brief Social role enumeration
 *
 * WHAT: Functional role a person plays in one's life
 * WHY:  Roles affect expected behaviors and interaction patterns
 */
typedef enum {
    ROLE_NONE = 0,       /**< No specific role */
    ROLE_CAREGIVER,      /**< Provider of care/support */
    ROLE_DEPENDENT,      /**< Receiver of care/support */
    ROLE_MENTOR,         /**< Guide/teacher */
    ROLE_MENTEE,         /**< Student/learner */
    ROLE_COLLABORATOR,   /**< Work partner */
    ROLE_COMPETITOR,     /**< Rival/opponent */
    ROLE_CONFIDANT,      /**< Trusted confidant */
    ROLE_COUNT           /**< Number of roles */
} social_role_t;

/**
 * @brief Trust update outcome type
 *
 * WHAT: Classification of interaction outcomes for trust learning
 * WHY:  Trust updates depend on outcome valence and magnitude
 */
typedef enum {
    TRUST_OUTCOME_POSITIVE = 0,   /**< Positive outcome (trust increases) */
    TRUST_OUTCOME_NEGATIVE,       /**< Negative outcome (trust decreases) */
    TRUST_OUTCOME_NEUTRAL,        /**< Neutral outcome (minimal change) */
    TRUST_OUTCOME_BETRAYAL,       /**< Severe negative (large trust decrease) */
    TRUST_OUTCOME_EXCEPTIONAL     /**< Exceptional positive (large trust increase) */
} trust_outcome_t;

/**
 * @brief Social memory error codes
 */
typedef enum {
    SOCIAL_MEM_SUCCESS = 0,                /**< Operation succeeded */
    SOCIAL_MEM_ERROR_NULL_POINTER = -1,    /**< NULL pointer argument */
    SOCIAL_MEM_ERROR_INVALID_ID = -2,      /**< Invalid person/episode ID */
    SOCIAL_MEM_ERROR_CAPACITY = -3,        /**< Capacity limit reached */
    SOCIAL_MEM_ERROR_NOT_FOUND = -4,       /**< Person/episode not found */
    SOCIAL_MEM_ERROR_DUPLICATE = -5,       /**< Duplicate entry */
    SOCIAL_MEM_ERROR_NO_MEMORY = -6,       /**< Memory allocation failed */
    SOCIAL_MEM_ERROR_INVALID_PARAM = -7,   /**< Invalid parameter value */
    SOCIAL_MEM_ERROR_LOCKED = -8,          /**< Resource is locked */
    SOCIAL_MEM_ERROR_SERIALIZE = -9,       /**< Serialization failed */
    SOCIAL_MEM_ERROR_DESERIALIZE = -10     /**< Deserialization failed */
} social_mem_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Person node representing memory of an individual
 *
 * WHAT: Complete representation of a known person in social memory
 * WHY:  Store all information needed for recognition, relationship tracking,
 *       trust assessment, and interaction history
 * HOW:  Multi-modal signatures (face, voice) for recognition, quaternion for
 *       emotional state, semantic facts, relationship metadata
 *
 * Memory layout: ~256 bytes + variable signature/facts data
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t person_id;                   /**< Unique identifier */
    char* name;                           /**< Person's name (heap-allocated) */
    prime_signature_t identity_signature; /**< Combined identity signature */
    nimcp_quaternion_t person_quaternion; /**< Emotional state toward person */

    //-------------------------------------------------------------------------
    // Recognition Signatures (Multi-modal)
    //-------------------------------------------------------------------------
    prime_signature_t face_signature;     /**< Face recognition signature */
    prime_signature_t voice_signature;    /**< Voice recognition signature */
    bool has_face_signature;              /**< Whether face is known */
    bool has_voice_signature;             /**< Whether voice is known */

    //-------------------------------------------------------------------------
    // Semantic Knowledge
    //-------------------------------------------------------------------------
    prime_signature_t* facts;             /**< Array of known facts about person */
    size_t num_facts;                     /**< Number of facts stored */
    size_t max_facts;                     /**< Allocated capacity for facts */

    //-------------------------------------------------------------------------
    // Social Standing
    //-------------------------------------------------------------------------
    float familiarity;                    /**< How well known (0=stranger, 1=intimate) */
    float trust_level;                    /**< Trustworthiness assessment (0-1) */
    float liking;                         /**< Positive/negative affect (-1 to +1) */
    float perceived_competence;           /**< Perceived skill/ability (0-1) */
    float perceived_warmth;               /**< Perceived warmth/friendliness (0-1) */

    //-------------------------------------------------------------------------
    // Relationship
    //-------------------------------------------------------------------------
    relationship_type_t relationship;     /**< Primary relationship type */
    social_role_t role;                   /**< Functional role */
    float relationship_strength;          /**< Strength of relationship (0-1) */
    float reciprocity;                    /**< Balance of give/take (-1 to +1) */

    //-------------------------------------------------------------------------
    // Interaction History
    //-------------------------------------------------------------------------
    float last_interaction_time;          /**< Timestamp of last interaction (sec) */
    float first_interaction_time;         /**< Timestamp of first interaction (sec) */
    size_t interaction_count;             /**< Total number of interactions */
    size_t positive_interactions;         /**< Count of positive interactions */
    size_t negative_interactions;         /**< Count of negative interactions */

    //-------------------------------------------------------------------------
    // Trust History
    //-------------------------------------------------------------------------
    float trust_baseline;                 /**< Long-term trust baseline */
    float trust_volatility;               /**< How much trust fluctuates */
    float last_trust_update;              /**< Time of last trust update */

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;        /**< Backing PR memory node */
    uint32_t entanglement_count;          /**< Number of entangled persons */

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;             /**< Creation timestamp */
    uint64_t modified_time_ms;            /**< Last modification timestamp */
    uint32_t flags;                       /**< State flags */

} person_node_t;

/**
 * @brief Social episode representing a social interaction memory
 *
 * WHAT: Episodic memory of a social interaction/event
 * WHY:  Store contextual details of social encounters for later retrieval
 * HOW:  Participant list, context signature, emotional valence, importance
 *
 * Memory layout: ~128 bytes + participant array
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t episode_id;                  /**< Unique episode identifier */

    //-------------------------------------------------------------------------
    // Participants
    //-------------------------------------------------------------------------
    uint64_t* participant_ids;            /**< Array of person IDs involved */
    size_t num_participants;              /**< Number of participants */
    size_t max_participants;              /**< Allocated capacity */

    //-------------------------------------------------------------------------
    // Context
    //-------------------------------------------------------------------------
    prime_signature_t context_signature;  /**< Contextual content signature */
    char* location;                       /**< Location description (optional) */
    char* description;                    /**< Episode description (optional) */

    //-------------------------------------------------------------------------
    // Temporal
    //-------------------------------------------------------------------------
    float episode_time;                   /**< When episode occurred (sec) */
    float duration;                       /**< Episode duration (sec) */

    //-------------------------------------------------------------------------
    // Emotional/Importance
    //-------------------------------------------------------------------------
    float emotional_valence;              /**< Overall emotion (-1 to +1) */
    float emotional_arousal;              /**< Emotional intensity (0-1) */
    float social_importance;              /**< How important to social graph (0-1) */

    //-------------------------------------------------------------------------
    // Outcome
    //-------------------------------------------------------------------------
    trust_outcome_t outcome_type;         /**< Classification of outcome */
    float outcome_magnitude;              /**< Magnitude of outcome (0-1) */

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;        /**< Backing PR memory node */

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;             /**< Creation timestamp */
    uint32_t flags;                       /**< State flags */

} social_episode_t;

/**
 * @brief Configuration for social memory system
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Capacity
    //-------------------------------------------------------------------------
    size_t max_persons;                   /**< Maximum persons to store */
    size_t max_episodes;                  /**< Maximum episodes to store */
    size_t max_facts_per_person;          /**< Max facts per person */

    //-------------------------------------------------------------------------
    // Trust Parameters
    //-------------------------------------------------------------------------
    float initial_trust;                  /**< Trust level for new persons */
    float trust_decay_rate;               /**< Trust decay per day */
    float trust_learning_rate;            /**< Alpha for trust updates */

    //-------------------------------------------------------------------------
    // Relationship Parameters
    //-------------------------------------------------------------------------
    float relationship_decay_rate;        /**< Relationship decay per day */
    float familiarity_threshold;          /**< Threshold for "familiar" */

    //-------------------------------------------------------------------------
    // Recognition Parameters
    //-------------------------------------------------------------------------
    float id_threshold;                   /**< Threshold for person identification */
    float face_weight;                    /**< Weight for face in identification */
    float voice_weight;                   /**< Weight for voice in identification */

    //-------------------------------------------------------------------------
    // Integration
    //-------------------------------------------------------------------------
    bool enable_entanglement;             /**< Enable person-person entanglement */
    bool enable_episode_linking;          /**< Link episodes to persons */
    resonance_config_t resonance_config;  /**< Resonance computation config */

} social_memory_config_t;

/**
 * @brief Social memory system instance
 *
 * WHAT: Complete social memory system with persons, episodes, relationships
 * WHY:  Central manager for all social memory operations
 * HOW:  Person registry, episode store, relationship matrix, PR integration
 */
typedef struct social_memory_struct* social_memory_t;

/**
 * @brief Statistics for social memory system
 */
typedef struct {
    size_t num_persons;                   /**< Current person count */
    size_t num_episodes;                  /**< Current episode count */
    size_t total_facts;                   /**< Total facts across all persons */
    size_t total_relationships;           /**< Non-stranger relationships */
    float avg_trust;                      /**< Average trust level */
    float avg_familiarity;                /**< Average familiarity */
    size_t trusted_count;                 /**< Persons above trust threshold */
    size_t family_count;                  /**< Family relationships */
    size_t friend_count;                  /**< Friend relationships */
    uint64_t total_interactions;          /**< Total interaction count */
    size_t memory_bytes;                  /**< Approximate memory usage */
} social_memory_stats_t;

/**
 * @brief Person query result
 */
typedef struct {
    uint64_t person_id;                   /**< Person ID */
    float match_score;                    /**< Match/relevance score (0-1) */
    float trust_level;                    /**< Person's trust level */
    relationship_type_t relationship;     /**< Relationship type */
} person_query_result_t;

/**
 * @brief Social network analysis result
 */
typedef struct {
    uint64_t person_id;                   /**< Person ID */
    float centrality;                     /**< Network centrality (0-1) */
    size_t degree;                        /**< Number of connections */
    float avg_trust;                      /**< Average trust from others */
    float clustering;                     /**< Local clustering coefficient */
} social_network_node_t;

/**
 * @brief Behavior prediction result
 */
typedef struct {
    float cooperation_prob;               /**< Probability of cooperation (0-1) */
    float defection_prob;                 /**< Probability of defection (0-1) */
    float helpfulness_prob;               /**< Probability of helping (0-1) */
    float confidence;                     /**< Prediction confidence (0-1) */
} behavior_prediction_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default social memory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - max_persons: 1024
 *         - max_episodes: 4096
 *         - initial_trust: 0.5
 *         - trust_decay_rate: 0.001 per day
 *         - id_threshold: 0.85
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT social_memory_config_t social_memory_config_default(void);

/**
 * @brief Validate social memory configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool social_memory_config_validate(const social_memory_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new social memory system
 *
 * WHAT: Allocates and initializes social memory system
 * WHY:  Entry point for social cognition memory
 * HOW:  Allocates person/episode stores, relationship matrix, initializes PR integration
 *
 * @param entanglement Entanglement graph for person associations (can be NULL)
 * @param node_manager PR node manager for episode storage (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Social memory handle, or NULL on failure
 *
 * Performance: O(max_persons^2) for relationship matrix
 * Memory: ~100KB for default configuration
 *
 * Thread safety: Returned instance is thread-safe
 *
 * Example:
 *   social_memory_config_t config = social_memory_config_default();
 *   config.max_persons = 500;
 *   social_memory_t social_mem = social_memory_create(entanglement, mgr, &config);
 */
NIMCP_EXPORT social_memory_t social_memory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const social_memory_config_t* config);

/**
 * @brief Destroy social memory system and free all resources
 *
 * WHAT: Deallocates social memory and all stored data
 * WHY:  Resource cleanup
 * HOW:  Frees all persons, episodes, relationship matrix
 *
 * @param social_mem Social memory to destroy (NULL safe)
 *
 * Performance: O(N + E) where N=persons, E=episodes
 *
 * Warning: Does not destroy entanglement graph or node manager (external)
 */
NIMCP_EXPORT void social_memory_destroy(social_memory_t social_mem);

/**
 * @brief Clear all data from social memory
 *
 * @param social_mem Social memory to clear
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_clear(social_memory_t social_mem);

//=============================================================================
// Person Management Functions
//=============================================================================

/**
 * @brief Add a new person to social memory
 *
 * WHAT: Creates a new person node in social memory
 * WHY:  Register newly encountered individuals
 * HOW:  Allocates person node, initializes with provided data
 *
 * @param social_mem Social memory system
 * @param name Person's name (copied)
 * @param identity_signature Initial identity signature (can be NULL)
 * @param relationship Initial relationship type
 * @return New person ID, or SOCIAL_MEM_INVALID_PERSON_ID on error
 *
 * Performance: O(1) average
 *
 * Example:
 *   prime_signature_t* sig = prime_sig_from_content(face_data, face_size);
 *   uint64_t person_id = social_memory_add_person(social_mem, "Alice", sig, REL_COLLEAGUE);
 */
NIMCP_EXPORT uint64_t social_memory_add_person(
    social_memory_t social_mem,
    const char* name,
    const prime_signature_t* identity_signature,
    relationship_type_t relationship);

/**
 * @brief Add person with full initialization
 *
 * @param social_mem Social memory system
 * @param person Person data (person_id will be assigned)
 * @return Assigned person ID, or SOCIAL_MEM_INVALID_PERSON_ID on error
 */
NIMCP_EXPORT uint64_t social_memory_add_person_full(
    social_memory_t social_mem,
    const person_node_t* person);

/**
 * @brief Remove a person from social memory
 *
 * @param social_mem Social memory system
 * @param person_id Person to remove
 * @return SOCIAL_MEM_SUCCESS or error code
 *
 * Note: Also removes person from relationship matrix and episodes
 */
NIMCP_EXPORT social_mem_error_t social_memory_remove_person(
    social_memory_t social_mem,
    uint64_t person_id);

/**
 * @brief Get person by ID
 *
 * @param social_mem Social memory system
 * @param person_id Person ID to retrieve
 * @return const pointer to person node, or NULL if not found
 *
 * Performance: O(1) average
 *
 * Note: Returns const pointer - do not modify directly
 */
NIMCP_EXPORT const person_node_t* social_memory_get_person(
    social_memory_t social_mem,
    uint64_t person_id);

/**
 * @brief Get person by name
 *
 * @param social_mem Social memory system
 * @param name Name to search for
 * @return const pointer to person node, or NULL if not found
 *
 * Performance: O(N) linear scan
 */
NIMCP_EXPORT const person_node_t* social_memory_get_person_by_name(
    social_memory_t social_mem,
    const char* name);

/**
 * @brief Check if person exists
 *
 * @param social_mem Social memory system
 * @param person_id Person ID to check
 * @return true if person exists, false otherwise
 */
NIMCP_EXPORT bool social_memory_person_exists(
    social_memory_t social_mem,
    uint64_t person_id);

/**
 * @brief Get all person IDs
 *
 * @param social_mem Social memory system
 * @param ids Output array (caller-allocated)
 * @param max_ids Maximum IDs to return
 * @param count Output: actual count returned
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_all_persons(
    social_memory_t social_mem,
    uint64_t* ids,
    size_t max_ids,
    size_t* count);

//=============================================================================
// Person Identification Functions
//=============================================================================

/**
 * @brief Identify person from feature signature
 *
 * WHAT: Find best-matching person based on recognition features
 * WHY:  Face/voice recognition for person identification
 * HOW:  Compare signature against stored identities, return best match
 *
 * @param social_mem Social memory system
 * @param features Recognition features (face, voice, combined)
 * @param confidence Output: confidence of identification (0-1)
 * @return Person ID of best match, or SOCIAL_MEM_INVALID_PERSON_ID if none
 *
 * Performance: O(N) where N = number of persons
 *
 * Example:
 *   prime_signature_t* face_sig = prime_sig_from_content(face_embedding, 128);
 *   float confidence;
 *   uint64_t person = social_memory_identify_person(social_mem, face_sig, &confidence);
 *   if (person != SOCIAL_MEM_INVALID_PERSON_ID && confidence > 0.9f) {
 *       printf("Identified: %s\n", social_memory_get_person(social_mem, person)->name);
 *   }
 */
NIMCP_EXPORT uint64_t social_memory_identify_person(
    social_memory_t social_mem,
    const prime_signature_t* features,
    float* confidence);

/**
 * @brief Identify person using face signature
 *
 * @param social_mem Social memory system
 * @param face_signature Face recognition signature
 * @param confidence Output: identification confidence
 * @return Person ID or SOCIAL_MEM_INVALID_PERSON_ID
 */
NIMCP_EXPORT uint64_t social_memory_identify_by_face(
    social_memory_t social_mem,
    const prime_signature_t* face_signature,
    float* confidence);

/**
 * @brief Identify person using voice signature
 *
 * @param social_mem Social memory system
 * @param voice_signature Voice recognition signature
 * @param confidence Output: identification confidence
 * @return Person ID or SOCIAL_MEM_INVALID_PERSON_ID
 */
NIMCP_EXPORT uint64_t social_memory_identify_by_voice(
    social_memory_t social_mem,
    const prime_signature_t* voice_signature,
    float* confidence);

/**
 * @brief Identify person using multimodal features
 *
 * @param social_mem Social memory system
 * @param face_signature Face signature (can be NULL)
 * @param voice_signature Voice signature (can be NULL)
 * @param confidence Output: combined identification confidence
 * @return Person ID or SOCIAL_MEM_INVALID_PERSON_ID
 */
NIMCP_EXPORT uint64_t social_memory_identify_multimodal(
    social_memory_t social_mem,
    const prime_signature_t* face_signature,
    const prime_signature_t* voice_signature,
    float* confidence);

/**
 * @brief Get top-K person matches for features
 *
 * @param social_mem Social memory system
 * @param features Recognition features
 * @param k Number of matches to return
 * @param results Output array (caller-allocated, size >= k)
 * @param count Output: actual count returned
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_identify_top_k(
    social_memory_t social_mem,
    const prime_signature_t* features,
    size_t k,
    person_query_result_t* results,
    size_t* count);

//=============================================================================
// Person Update Functions
//=============================================================================

/**
 * @brief Update person's face signature
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param face_signature New face signature
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_update_face(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* face_signature);

/**
 * @brief Update person's voice signature
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param voice_signature New voice signature
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_update_voice(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* voice_signature);

/**
 * @brief Add a fact about a person
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param fact_signature Signature of the fact
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_add_fact(
    social_memory_t social_mem,
    uint64_t person_id,
    const prime_signature_t* fact_signature);

/**
 * @brief Update person's name
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param name New name
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_update_name(
    social_memory_t social_mem,
    uint64_t person_id,
    const char* name);

//=============================================================================
// Trust Management Functions
//=============================================================================

/**
 * @brief Update trust level based on interaction outcome
 *
 * WHAT: Adjust trust level based on observed behavior
 * WHY:  Trust evolves through interaction outcomes
 * HOW:  Reinforcement learning: trust += alpha * (outcome - expected)
 *
 * @param social_mem Social memory system
 * @param person_id Person whose trust to update
 * @param outcome Type of interaction outcome
 * @param magnitude Magnitude of outcome (0-1)
 * @return New trust level, or -1.0f on error
 *
 * Performance: O(1)
 *
 * Example:
 *   // Person kept their promise - positive outcome
 *   float new_trust = social_memory_update_trust(
 *       social_mem, person_id, TRUST_OUTCOME_POSITIVE, 0.8f);
 */
NIMCP_EXPORT float social_memory_update_trust(
    social_memory_t social_mem,
    uint64_t person_id,
    trust_outcome_t outcome,
    float magnitude);

/**
 * @brief Set trust level directly
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param trust_level New trust level (0-1)
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_set_trust(
    social_memory_t social_mem,
    uint64_t person_id,
    float trust_level);

/**
 * @brief Get trust level for person
 *
 * @param social_mem Social memory system
 * @param person_id Person to query
 * @return Trust level (0-1), or -1.0f if not found
 */
NIMCP_EXPORT float social_memory_get_trust(
    social_memory_t social_mem,
    uint64_t person_id);

/**
 * @brief Apply trust decay to all persons
 *
 * WHAT: Decay trust levels based on elapsed time
 * WHY:  Trust fades without reinforcement
 * HOW:  trust *= decay^elapsed_time for each person
 *
 * @param social_mem Social memory system
 * @param elapsed_days Days since last decay application
 * @return Number of persons affected
 */
NIMCP_EXPORT size_t social_memory_decay_trust(
    social_memory_t social_mem,
    float elapsed_days);

/**
 * @brief Get all persons above trust threshold
 *
 * @param social_mem Social memory system
 * @param threshold Minimum trust level
 * @param ids Output array (caller-allocated)
 * @param max_ids Maximum IDs to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_trusted(
    social_memory_t social_mem,
    float threshold,
    uint64_t* ids,
    size_t max_ids,
    size_t* count);

//=============================================================================
// Relationship Management Functions
//=============================================================================

/**
 * @brief Update relationship type and strength
 *
 * WHAT: Change relationship classification between persons
 * WHY:  Relationships evolve over time
 * HOW:  Updates relationship matrix entry
 *
 * @param social_mem Social memory system
 * @param person_id Person to update relationship for
 * @param relationship New relationship type
 * @param strength New relationship strength (0-1)
 * @return SOCIAL_MEM_SUCCESS or error code
 *
 * Example:
 *   // Upgrade acquaintance to friend
 *   social_memory_update_relationship(social_mem, alice_id, REL_FRIEND, 0.8f);
 */
NIMCP_EXPORT social_mem_error_t social_memory_update_relationship(
    social_memory_t social_mem,
    uint64_t person_id,
    relationship_type_t relationship,
    float strength);

/**
 * @brief Set relationship between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @param relationship Relationship type
 * @param strength Relationship strength
 * @param bidirectional Whether to set in both directions
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_set_relationship_between(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    relationship_type_t relationship,
    float strength,
    bool bidirectional);

/**
 * @brief Get relationship strength between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @return Relationship strength (0-1), or -1.0f if invalid
 */
NIMCP_EXPORT float social_memory_get_relationship_strength(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id);

/**
 * @brief Get relationship type between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @return Relationship type, or REL_STRANGER if not set
 */
NIMCP_EXPORT relationship_type_t social_memory_get_relationship_type(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id);

/**
 * @brief Update familiarity level
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param delta Familiarity change (positive or negative)
 * @return New familiarity level, or -1.0f on error
 */
NIMCP_EXPORT float social_memory_update_familiarity(
    social_memory_t social_mem,
    uint64_t person_id,
    float delta);

/**
 * @brief Update liking level
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param delta Liking change (positive or negative)
 * @return New liking level (-1 to +1), or -2.0f on error
 */
NIMCP_EXPORT float social_memory_update_liking(
    social_memory_t social_mem,
    uint64_t person_id,
    float delta);

//=============================================================================
// Social Query Functions
//=============================================================================

/**
 * @brief Query persons by relationship type
 *
 * WHAT: Find all persons with specified relationship
 * WHY:  Filter by relationship category (e.g., "all friends")
 * HOW:  Scan relationship matrix for matching type
 *
 * @param social_mem Social memory system
 * @param relationship Relationship type to find
 * @param results Output array (caller-allocated)
 * @param max_results Maximum results to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 *
 * Example:
 *   person_query_result_t friends[100];
 *   size_t count;
 *   social_memory_query_by_relationship(social_mem, REL_FRIEND, friends, 100, &count);
 */
NIMCP_EXPORT social_mem_error_t social_memory_query_by_relationship(
    social_memory_t social_mem,
    relationship_type_t relationship,
    person_query_result_t* results,
    size_t max_results,
    size_t* count);

/**
 * @brief Query persons by familiarity threshold
 *
 * @param social_mem Social memory system
 * @param min_familiarity Minimum familiarity level (0-1)
 * @param results Output array
 * @param max_results Maximum results
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_query_by_familiarity(
    social_memory_t social_mem,
    float min_familiarity,
    person_query_result_t* results,
    size_t max_results,
    size_t* count);

/**
 * @brief Get mutual friends (persons connected to both)
 *
 * WHAT: Find persons known to both person1 and person2
 * WHY:  Social network analysis, introduction recommendations
 * HOW:  Intersection of relationship sets
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @param mutual_ids Output array of mutual friend IDs
 * @param max_ids Maximum IDs to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_mutual_friends(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    uint64_t* mutual_ids,
    size_t max_ids,
    size_t* count);

//=============================================================================
// Social Episode Functions
//=============================================================================

/**
 * @brief Record a social episode (interaction)
 *
 * WHAT: Store memory of a social interaction
 * WHY:  Episodic memory of social encounters
 * HOW:  Create episode node, link to participants, store in PR memory
 *
 * @param social_mem Social memory system
 * @param participant_ids Array of person IDs involved
 * @param num_participants Number of participants
 * @param context_signature Context/content signature
 * @param episode_time Timestamp of episode
 * @param emotional_valence Emotional tone (-1 to +1)
 * @param social_importance Importance level (0-1)
 * @return Episode ID, or SOCIAL_MEM_INVALID_EPISODE_ID on error
 *
 * Example:
 *   uint64_t participants[] = {alice_id, bob_id};
 *   uint64_t ep_id = social_memory_record_episode(
 *       social_mem, participants, 2, &context_sig, time_now, 0.7f, 0.8f);
 */
NIMCP_EXPORT uint64_t social_memory_record_episode(
    social_memory_t social_mem,
    const uint64_t* participant_ids,
    size_t num_participants,
    const prime_signature_t* context_signature,
    float episode_time,
    float emotional_valence,
    float social_importance);

/**
 * @brief Record episode with full initialization
 *
 * @param social_mem Social memory system
 * @param episode Episode data (episode_id will be assigned)
 * @return Assigned episode ID, or SOCIAL_MEM_INVALID_EPISODE_ID on error
 */
NIMCP_EXPORT uint64_t social_memory_record_episode_full(
    social_memory_t social_mem,
    const social_episode_t* episode);

/**
 * @brief Get episode by ID
 *
 * @param social_mem Social memory system
 * @param episode_id Episode to retrieve
 * @return const pointer to episode, or NULL if not found
 */
NIMCP_EXPORT const social_episode_t* social_memory_get_episode(
    social_memory_t social_mem,
    uint64_t episode_id);

/**
 * @brief Get interaction history for a person
 *
 * WHAT: Retrieve all episodes involving a person
 * WHY:  Review interaction history
 * HOW:  Filter episodes by participant
 *
 * @param social_mem Social memory system
 * @param person_id Person to get history for
 * @param episode_ids Output array of episode IDs
 * @param max_episodes Maximum episodes to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_person_history(
    social_memory_t social_mem,
    uint64_t person_id,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count);

/**
 * @brief Get recent episodes
 *
 * @param social_mem Social memory system
 * @param since_time Minimum episode time
 * @param episode_ids Output array
 * @param max_episodes Maximum to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_recent_episodes(
    social_memory_t social_mem,
    float since_time,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count);

/**
 * @brief Get episodes between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @param episode_ids Output array
 * @param max_episodes Maximum to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_episodes_between(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    uint64_t* episode_ids,
    size_t max_episodes,
    size_t* count);

//=============================================================================
// Social Network Analysis Functions
//=============================================================================

/**
 * @brief Compute social network metrics
 *
 * WHAT: Analyze structure of social network
 * WHY:  Identify central figures, clusters, network health
 * HOW:  Compute centrality, clustering, path metrics
 *
 * @param social_mem Social memory system
 * @param results Output array of network node metrics
 * @param max_results Maximum results
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 *
 * Performance: O(N^2) where N = number of persons
 */
NIMCP_EXPORT social_mem_error_t social_memory_compute_social_network(
    social_memory_t social_mem,
    social_network_node_t* results,
    size_t max_results,
    size_t* count);

/**
 * @brief Get network centrality for a person
 *
 * @param social_mem Social memory system
 * @param person_id Person to analyze
 * @return Centrality score (0-1), or -1.0f on error
 */
NIMCP_EXPORT float social_memory_get_centrality(
    social_memory_t social_mem,
    uint64_t person_id);

/**
 * @brief Compute degrees of separation between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @return Degrees of separation, or -1 if not connected
 */
NIMCP_EXPORT int social_memory_degrees_of_separation(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id);

/**
 * @brief Find social clusters/communities
 *
 * @param social_mem Social memory system
 * @param cluster_ids Output: cluster assignment per person
 * @param person_ids Output: corresponding person IDs
 * @param max_size Maximum array size
 * @param count Output: actual count
 * @param num_clusters Output: number of clusters found
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_find_clusters(
    social_memory_t social_mem,
    uint32_t* cluster_ids,
    uint64_t* person_ids,
    size_t max_size,
    size_t* count,
    size_t* num_clusters);

//=============================================================================
// Behavior Prediction Functions
//=============================================================================

/**
 * @brief Predict person's behavior based on history
 *
 * WHAT: Estimate likelihood of various behaviors
 * WHY:  Social reasoning requires behavior prediction
 * HOW:  Bayesian inference from trust, relationship, interaction history
 *
 * @param social_mem Social memory system
 * @param person_id Person to predict
 * @param prediction Output prediction structure
 * @return SOCIAL_MEM_SUCCESS or error code
 *
 * Example:
 *   behavior_prediction_t pred;
 *   social_memory_predict_behavior(social_mem, person_id, &pred);
 *   if (pred.cooperation_prob > 0.8f && pred.confidence > 0.7f) {
 *       // Likely to cooperate
 *   }
 */
NIMCP_EXPORT social_mem_error_t social_memory_predict_behavior(
    social_memory_t social_mem,
    uint64_t person_id,
    behavior_prediction_t* prediction);

/**
 * @brief Predict outcome of interaction between two persons
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @param prediction Output prediction
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_predict_interaction(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    behavior_prediction_t* prediction);

//=============================================================================
// Entanglement Functions
//=============================================================================

/**
 * @brief Entangle two persons (create association)
 *
 * WHAT: Create association link between persons in entanglement graph
 * WHY:  Track person-person associations for retrieval
 * HOW:  Add edge to entanglement graph with computed resonance
 *
 * @param social_mem Social memory system
 * @param person1_id First person
 * @param person2_id Second person
 * @param edge_type Type of association
 * @param strength Association strength (0-1)
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_entangle_persons(
    social_memory_t social_mem,
    uint64_t person1_id,
    uint64_t person2_id,
    entangle_edge_type_t edge_type,
    float strength);

/**
 * @brief Get entangled persons (associated with given person)
 *
 * @param social_mem Social memory system
 * @param person_id Person to query associations for
 * @param ids Output array of associated person IDs
 * @param max_ids Maximum IDs to return
 * @param count Output: actual count
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_entangled(
    social_memory_t social_mem,
    uint64_t person_id,
    uint64_t* ids,
    size_t max_ids,
    size_t* count);

/**
 * @brief Update emotional state toward person
 *
 * @param social_mem Social memory system
 * @param person_id Person to update
 * @param quaternion New emotional state quaternion
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_update_emotion(
    social_memory_t social_mem,
    uint64_t person_id,
    nimcp_quaternion_t quaternion);

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

/**
 * @brief Get social memory statistics
 *
 * @param social_mem Social memory system
 * @param stats Output statistics structure
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_get_stats(
    social_memory_t social_mem,
    social_memory_stats_t* stats);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* social_memory_get_last_error(void);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* social_memory_error_string(social_mem_error_t error);

/**
 * @brief Get relationship type name
 *
 * @param type Relationship type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* social_memory_relationship_name(relationship_type_t type);

/**
 * @brief Print social memory summary to stdout
 *
 * @param social_mem Social memory system
 */
NIMCP_EXPORT void social_memory_print_summary(social_memory_t social_mem);

/**
 * @brief Validate social memory internal consistency
 *
 * @param social_mem Social memory system
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool social_memory_validate(social_memory_t social_mem);

/**
 * @brief Record interaction (update familiarity and counts)
 *
 * @param social_mem Social memory system
 * @param person_id Person interacted with
 * @param current_time Current timestamp
 * @return SOCIAL_MEM_SUCCESS or error code
 */
NIMCP_EXPORT social_mem_error_t social_memory_record_interaction(
    social_memory_t social_mem,
    uint64_t person_id,
    float current_time);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t social_memory_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SOCIAL_MEMORY_H
