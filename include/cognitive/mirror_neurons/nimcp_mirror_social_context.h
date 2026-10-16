/**
 * @file nimcp_mirror_social_context.h
 * @brief Social Context Modulation for Mirror Neurons
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Social context modulation of mirror neuron activation
 * WHY:  Mirror neuron response varies with social context - in-group, hierarchy, cultural factors
 * HOW:  Modulation factors scale mirror neuron activation based on social relationships
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 * Mirror neuron activation is not constant - it's modulated by social context:
 *
 * 1. In-Group vs Out-Group (STS, medial PFC)
 *    - Stronger mirror responses for in-group members
 *    - Reduced empathic resonance for out-group
 *    - Neural basis of in-group bias in imitation
 *    - Reference: Gutsell & Inzlicht (2010) "Empathy constrained"
 *
 * 2. Social Hierarchy (vmPFC, striatum)
 *    - Dominance/subordination affects imitation tendency
 *    - Higher-status models elicit stronger mirroring
 *    - Learning preference from competent demonstrators
 *    - Reference: Cheng et al. (2017) "Social hierarchy in mirror system"
 *
 * 3. Cultural Familiarity (temporal-parietal junction)
 *    - Familiar cultural actions have stronger resonance
 *    - Cross-cultural action understanding modulated
 *    - Embodied cultural knowledge affects mirroring
 *    - Reference: Molnar-Szakacs & Overy (2006) "Music and mirror neurons"
 *
 * 4. Emotional Contagion (insula, amygdala)
 *    - Emotional state of observed agent affects resonance
 *    - Fear/threat contexts suppress imitation
 *    - Positive emotional contexts enhance mirroring
 *
 * ARCHITECTURE:
 * ==============================================================================
 *    Agent Detection     Social Memory     Context Signals
 *         |                   |                  |
 *         v                   v                  v
 *    +----------+       +-----------+      +------------+
 *    | Agent ID |------>| Retrieve  |----->| Compute    |
 *    | Features |       | Social    |      | Modulation |
 *    +----------+       | Relations |      | Factors    |
 *                       +-----------+      +------------+
 *                                                |
 *                                                v
 *                       +----------------------------------+
 *                       |    SOCIAL MODULATION OUTPUT      |
 *                       |  - in_group_affinity   (0-1)     |
 *                       |  - social_hierarchy    (-1 to 1) |
 *                       |  - cultural_familiarity(0-1)     |
 *                       |  - emotional_valence   (-1 to 1) |
 *                       +----------------------------------+
 *                                                |
 *                                                v
 *                       +----------------------------------+
 *                       |     MIRROR NEURON SYSTEM         |
 *                       |  activation *= compute_gain()    |
 *                       +----------------------------------+
 *
 * KEY FEATURES:
 * ==============================================================================
 * 1. Agent-Specific Modulation
 *    - Track social relationships per observed agent
 *    - Learn and update social factors over time
 *
 * 2. Context-Dependent Gain
 *    - Combined modulation factors produce gain multiplier
 *    - Configurable weighting of different factors
 *
 * 3. Integration with Theory of Mind
 *    - Social context informs intent inference
 *    - Hierarchy affects goal attribution
 *
 * 4. Learning Social Relationships
 *    - Update factors based on interaction outcomes
 *    - Build social memory from observations
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free
 *
 * @see nimcp_mirror_neurons.h
 * @see Phase 10.11.9 - Social Context Modulation
 */

#ifndef NIMCP_MIRROR_SOCIAL_CONTEXT_H
#define NIMCP_MIRROR_SOCIAL_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Defaults
//=============================================================================

/** @brief Maximum tracked agents for social context */
#define NIMCP_SOCIAL_MAX_AGENTS             64

/** @brief Default in-group affinity for unknown agents */
#define NIMCP_SOCIAL_DEFAULT_AFFINITY       0.5f

/** @brief Default hierarchy position (neutral) */
#define NIMCP_SOCIAL_DEFAULT_HIERARCHY      0.0f

/** @brief Default cultural familiarity */
#define NIMCP_SOCIAL_DEFAULT_CULTURAL       0.5f

/** @brief Default emotional valence (neutral) */
#define NIMCP_SOCIAL_DEFAULT_EMOTIONAL      0.0f

/** @brief In-group weight in gain computation */
#define NIMCP_SOCIAL_INGROUP_WEIGHT         0.4f

/** @brief Hierarchy weight in gain computation */
#define NIMCP_SOCIAL_HIERARCHY_WEIGHT       0.25f

/** @brief Cultural weight in gain computation */
#define NIMCP_SOCIAL_CULTURAL_WEIGHT        0.2f

/** @brief Emotional weight in gain computation */
#define NIMCP_SOCIAL_EMOTIONAL_WEIGHT       0.15f

/** @brief Minimum gain (even hostile agents get some mirroring) */
#define NIMCP_SOCIAL_MIN_GAIN               0.1f

/** @brief Maximum gain (strong in-group, high status) */
#define NIMCP_SOCIAL_MAX_GAIN               2.0f

/** @brief Learning rate for social factor updates */
#define NIMCP_SOCIAL_LEARNING_RATE          0.1f

//=============================================================================
// Social Context Types
//=============================================================================

/**
 * @brief Social group membership type
 *
 * WHAT: Categories of social group relationships
 * WHY:  In-group/out-group affects mirror neuron response
 */
typedef enum {
    SOCIAL_GROUP_UNKNOWN = 0,        /**< Unknown relationship */
    SOCIAL_GROUP_INGROUP,            /**< Same group member */
    SOCIAL_GROUP_OUTGROUP,           /**< Different group member */
    SOCIAL_GROUP_ALLY,               /**< Known ally/friend */
    SOCIAL_GROUP_RIVAL,              /**< Known competitor/rival */
    SOCIAL_GROUP_NEUTRAL,            /**< Neutral party */
    SOCIAL_GROUP_COUNT
} social_group_t;

/**
 * @brief Social hierarchy position type
 *
 * WHAT: Relative social status/dominance
 * WHY:  Hierarchy affects imitation tendency and learning
 */
typedef enum {
    SOCIAL_RANK_UNKNOWN = 0,         /**< Unknown rank */
    SOCIAL_RANK_DOMINANT,            /**< Higher status than self */
    SOCIAL_RANK_PEER,                /**< Similar status to self */
    SOCIAL_RANK_SUBORDINATE,         /**< Lower status than self */
    SOCIAL_RANK_EXPERT,              /**< Recognized expert/teacher */
    SOCIAL_RANK_NOVICE,              /**< Recognized novice/learner */
    SOCIAL_RANK_COUNT
} social_rank_t;

/**
 * @brief Cultural context type
 *
 * WHAT: Cultural familiarity categories
 * WHY:  Cultural context affects action interpretation
 */
typedef enum {
    CULTURAL_UNKNOWN = 0,            /**< Unknown cultural context */
    CULTURAL_NATIVE,                 /**< Same cultural background */
    CULTURAL_FAMILIAR,               /**< Familiar but different culture */
    CULTURAL_FOREIGN,                /**< Unfamiliar cultural context */
    CULTURAL_UNIVERSAL,              /**< Culture-independent action */
    CULTURAL_COUNT
} cultural_context_t;

/**
 * @brief Emotional context type
 *
 * WHAT: Emotional state of observed agent
 * WHY:  Emotional contagion affects mirror activation
 */
typedef enum {
    EMOTIONAL_NEUTRAL = 0,           /**< Neutral emotional state */
    EMOTIONAL_POSITIVE,              /**< Positive valence (happy, excited) */
    EMOTIONAL_NEGATIVE,              /**< Negative valence (sad, angry) */
    EMOTIONAL_FEARFUL,               /**< Fear/threat state */
    EMOTIONAL_FRIENDLY,              /**< Affiliative/welcoming */
    EMOTIONAL_HOSTILE,               /**< Hostile/threatening */
    EMOTIONAL_COUNT
} emotional_context_t;

//=============================================================================
// Social Modulation Structures
//=============================================================================

/**
 * @brief Social modulation factors
 *
 * WHAT: Continuous values modulating mirror neuron activation
 * WHY:  Provide fine-grained social context control
 *
 * Ranges:
 * - in_group_affinity: 0.0 (out-group) to 1.0 (strong in-group)
 * - social_hierarchy: -1.0 (subordinate) to 1.0 (dominant/expert)
 * - cultural_familiarity: 0.0 (foreign) to 1.0 (native)
 * - emotional_valence: -1.0 (hostile/fearful) to 1.0 (friendly/positive)
 */
typedef struct {
    // Core modulation factors
    float in_group_affinity;         /**< In-group membership strength (0-1) */
    float social_hierarchy;          /**< Relative social rank (-1 to 1) */
    float cultural_familiarity;      /**< Cultural context familiarity (0-1) */
    float emotional_valence;         /**< Emotional context (-1 to 1) */

    // Categorical classifications (for reference)
    social_group_t group_type;       /**< Categorical group membership */
    social_rank_t rank_type;         /**< Categorical social rank */
    cultural_context_t cultural_type;/**< Categorical cultural context */
    emotional_context_t emotional_type; /**< Categorical emotional state */

    // Confidence in modulation factors
    float affinity_confidence;       /**< Confidence in affinity estimate (0-1) */
    float hierarchy_confidence;      /**< Confidence in hierarchy estimate (0-1) */
    float cultural_confidence;       /**< Confidence in cultural estimate (0-1) */
    float emotional_confidence;      /**< Confidence in emotional estimate (0-1) */

    // Computed gain
    float computed_gain;             /**< Combined modulation gain */
    bool gain_valid;                 /**< Whether gain has been computed */
} social_modulation_t;

/**
 * @brief Per-agent social context record
 *
 * WHAT: Stored social context for a specific observed agent
 * WHY:  Maintain persistent social relationships
 */
typedef struct {
    uint32_t agent_id;               /**< Agent identifier */
    char agent_name[64];             /**< Optional agent name */
    social_modulation_t modulation;  /**< Current modulation factors */
    uint32_t interaction_count;      /**< Number of interactions */
    uint64_t first_seen;             /**< First observation timestamp */
    uint64_t last_seen;              /**< Last observation timestamp */
    float trust_level;               /**< Learned trust (0-1) */
    float competence_estimate;       /**< Estimated skill level (0-1) */
    bool is_self;                    /**< True if this is self-representation */
} agent_social_context_t;

/**
 * @brief Social context configuration
 *
 * WHAT: Parameters controlling social modulation behavior
 * WHY:  Allow customization of social context effects
 */
typedef struct {
    // Weight configuration
    float ingroup_weight;            /**< Weight for in-group factor */
    float hierarchy_weight;          /**< Weight for hierarchy factor */
    float cultural_weight;           /**< Weight for cultural factor */
    float emotional_weight;          /**< Weight for emotional factor */

    // Gain limits
    float min_gain;                  /**< Minimum modulation gain */
    float max_gain;                  /**< Maximum modulation gain */

    // Learning parameters
    float learning_rate;             /**< Rate of social factor updates */
    float decay_rate;                /**< Decay of unused relationships */

    // Behavior flags
    bool enable_ingroup_bias;        /**< Enable in-group favoritism */
    bool enable_hierarchy_effects;   /**< Enable hierarchy modulation */
    bool enable_cultural_effects;    /**< Enable cultural modulation */
    bool enable_emotional_effects;   /**< Enable emotional modulation */

    // Default values for unknown agents
    float default_affinity;          /**< Default in-group affinity */
    float default_hierarchy;         /**< Default hierarchy position */
    float default_cultural;          /**< Default cultural familiarity */

    /* Bio-async */
    bool bio_async_enabled;              /**< Enable bio-async messaging (default: true) */
} social_context_config_t;

/**
 * @brief Social context system statistics
 *
 * WHAT: Runtime statistics for social context modulation
 * WHY:  Monitor social context processing
 */
typedef struct {
    uint32_t num_tracked_agents;     /**< Number of agents being tracked */
    uint32_t total_lookups;          /**< Total context lookups performed */
    uint32_t cache_hits;             /**< Context cache hits */
    float avg_gain;                  /**< Average computed gain */
    float avg_affinity;              /**< Average in-group affinity */
    float avg_hierarchy;             /**< Average hierarchy position */
    uint64_t last_update;            /**< Last update timestamp */
} social_context_stats_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Opaque handle to social context system
 */
typedef struct social_context_system* social_context_t;

/**
 * @brief Forward declaration for mirror neurons
 */
typedef struct mirror_neurons_system* mirror_neurons_t;

//=============================================================================
// Core API - Lifecycle Management
//=============================================================================

/**
 * @brief Create social context system
 *
 * WHAT: Initialize social context modulation system
 * WHY:  Enable social context-aware mirror neuron activation
 * HOW:  Allocate agent tracking, initialize defaults
 *
 * COMPLEXITY: O(max_agents)
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param config Configuration (NULL = use defaults)
 * @return Social context system handle or NULL on error
 */
social_context_t social_context_create(const social_context_config_t* config);

/**
 * @brief Destroy social context system
 *
 * WHAT: Free all resources for social context system
 * WHY:  Prevent memory leaks
 * HOW:  Release agent records, free memory
 *
 * COMPLEXITY: O(num_agents)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param ctx Social context system to destroy (NULL-safe)
 */
void social_context_destroy(social_context_t ctx);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Provide good starting point
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @return Default social context configuration
 */
social_context_config_t social_context_get_default_config(void);

//=============================================================================
// Core API - Modulation Factor Setting
//=============================================================================

/**
 * @brief Set in-group affinity for agent
 *
 * WHAT: Update in-group membership strength for an agent
 * WHY:  Reflect social group relationships
 * HOW:  Store/update agent record with new affinity
 *
 * COMPLEXITY: O(1) average (hash lookup)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent to update
 * @param affinity In-group affinity (0-1)
 * @param confidence Confidence in estimate (0-1)
 * @return true on success, false on error
 */
bool social_context_set_affinity(social_context_t ctx,
                                  uint32_t agent_id,
                                  float affinity,
                                  float confidence);

/**
 * @brief Set social hierarchy for agent
 *
 * WHAT: Update relative social rank for an agent
 * WHY:  Reflect dominance/subordination relationships
 * HOW:  Store/update agent record with new hierarchy
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent to update
 * @param hierarchy Relative hierarchy (-1 to 1)
 * @param confidence Confidence in estimate (0-1)
 * @return true on success, false on error
 */
bool social_context_set_hierarchy(social_context_t ctx,
                                   uint32_t agent_id,
                                   float hierarchy,
                                   float confidence);

/**
 * @brief Set cultural familiarity for agent
 *
 * WHAT: Update cultural context familiarity for an agent
 * WHY:  Reflect cultural knowledge/background
 * HOW:  Store/update agent record with new cultural factor
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent to update
 * @param familiarity Cultural familiarity (0-1)
 * @param confidence Confidence in estimate (0-1)
 * @return true on success, false on error
 */
bool social_context_set_cultural(social_context_t ctx,
                                  uint32_t agent_id,
                                  float familiarity,
                                  float confidence);

/**
 * @brief Set emotional valence for agent
 *
 * WHAT: Update perceived emotional state of an agent
 * WHY:  Emotional contagion affects mirroring
 * HOW:  Store/update agent record with new emotional valence
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent to update
 * @param valence Emotional valence (-1 to 1)
 * @param confidence Confidence in estimate (0-1)
 * @return true on success, false on error
 */
bool social_context_set_emotional(social_context_t ctx,
                                   uint32_t agent_id,
                                   float valence,
                                   float confidence);

/**
 * @brief Set all modulation factors at once
 *
 * WHAT: Update complete social modulation for an agent
 * WHY:  Batch update all factors efficiently
 * HOW:  Store/update agent record with all factors
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent to update
 * @param modulation Complete modulation factors
 * @return true on success, false on error
 */
bool social_context_set_modulation(social_context_t ctx,
                                    uint32_t agent_id,
                                    const social_modulation_t* modulation);

//=============================================================================
// Core API - Modulation Application
//=============================================================================

/**
 * @brief Get modulation factors for agent
 *
 * WHAT: Retrieve social modulation factors for an agent
 * WHY:  Query current social context for mirror neuron modulation
 * HOW:  Lookup agent record, return modulation factors
 *
 * If agent is unknown, returns default factors.
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: Yes (read-only)
 *
 * @param ctx Social context system
 * @param agent_id Agent to query
 * @param out_modulation Output modulation factors
 * @return true on success, false on error
 */
bool social_context_get_modulation(social_context_t ctx,
                                    uint32_t agent_id,
                                    social_modulation_t* out_modulation);

/**
 * @brief Compute modulation gain for agent
 *
 * WHAT: Calculate combined gain multiplier for mirror neuron activation
 * WHY:  Single value to scale activation based on social context
 * HOW:  Weighted combination of all modulation factors
 *
 * Gain = weighted_sum(affinity, hierarchy, cultural, emotional)
 * Clamped to [min_gain, max_gain]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param ctx Social context system
 * @param agent_id Agent to compute gain for
 * @return Modulation gain (min_gain to max_gain), or 1.0 on error
 */
float social_context_compute_gain(social_context_t ctx, uint32_t agent_id);

/**
 * @brief Apply modulation to activation value
 *
 * WHAT: Scale activation by social modulation gain
 * WHY:  Directly apply social context to mirror neuron response
 * HOW:  activation * gain, clamped to valid range
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param ctx Social context system
 * @param agent_id Agent performing the observed action
 * @param activation Raw activation value
 * @return Modulated activation value
 */
float social_context_apply_modulation(social_context_t ctx,
                                       uint32_t agent_id,
                                       float activation);

//=============================================================================
// Core API - Social Learning
//=============================================================================

/**
 * @brief Update social factors based on interaction outcome
 *
 * WHAT: Learn social relationships from interaction results
 * WHY:  Build social memory from experience
 * HOW:  Adjust factors based on positive/negative outcomes
 *
 * Positive outcomes increase trust, affinity
 * Negative outcomes decrease trust, may affect hierarchy perception
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent interacted with
 * @param outcome Interaction outcome (-1 to 1, negative=bad, positive=good)
 * @param interaction_type Type of interaction (for context-specific learning)
 * @return true on success, false on error
 */
bool social_context_learn_interaction(social_context_t ctx,
                                       uint32_t agent_id,
                                       float outcome,
                                       uint32_t interaction_type);

/**
 * @brief Update competence estimate for agent
 *
 * WHAT: Learn agent's skill level from demonstrations
 * WHY:  Competence affects hierarchy and learning from agent
 * HOW:  Adjust competence estimate based on observation quality
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent observed
 * @param demonstrated_skill Quality of demonstrated skill (0-1)
 * @return true on success, false on error
 */
bool social_context_update_competence(social_context_t ctx,
                                       uint32_t agent_id,
                                       float demonstrated_skill);

/**
 * @brief Decay unused social relationships
 *
 * WHAT: Reduce confidence/strength of unused social records
 * WHY:  Prevent stale relationships from affecting behavior
 * HOW:  Exponential decay based on time since last interaction
 *
 * COMPLEXITY: O(num_agents)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param delta_time_ms Time since last decay (milliseconds)
 * @return true on success, false on error
 */
bool social_context_decay(social_context_t ctx, uint32_t delta_time_ms);

//=============================================================================
// Core API - Query Functions
//=============================================================================

/**
 * @brief Get agent social context record
 *
 * WHAT: Retrieve full social context record for an agent
 * WHY:  Access complete social relationship information
 * HOW:  Lookup and copy agent record
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: Yes (read-only)
 *
 * @param ctx Social context system
 * @param agent_id Agent to query
 * @param out_context Output agent context record
 * @return true if agent found, false otherwise
 */
bool social_context_get_agent(social_context_t ctx,
                               uint32_t agent_id,
                               agent_social_context_t* out_context);

/**
 * @brief Get system statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor social context processing
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only with atomic reads)
 *
 * @param ctx Social context system
 * @param out_stats Output statistics
 * @return true on success, false on error
 */
bool social_context_get_stats(social_context_t ctx,
                               social_context_stats_t* out_stats);

/**
 * @brief Check if agent is tracked
 *
 * WHAT: Determine if agent has social context record
 * WHY:  Check before detailed queries
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: Yes (read-only)
 *
 * @param ctx Social context system
 * @param agent_id Agent to check
 * @return true if agent is tracked, false otherwise
 */
bool social_context_has_agent(social_context_t ctx, uint32_t agent_id);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to mirror neuron system
 *
 * WHAT: Integrate social context with mirror neurons
 * WHY:  Enable automatic social modulation of mirror activation
 * HOW:  Register callback for observation events
 *
 * When connected, social context automatically modulates mirror neuron
 * activation based on the observed agent's social context.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param mirror Mirror neuron system
 * @return true on success, false on error
 */
bool social_context_connect_mirror(social_context_t ctx,
                                    mirror_neurons_t mirror);

/**
 * @brief Register agent observation
 *
 * WHAT: Record that an agent was observed
 * WHY:  Update last_seen, create record if needed
 * HOW:  Lookup/create agent record, update timestamp
 *
 * COMPLEXITY: O(1) average
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param ctx Social context system
 * @param agent_id Agent observed
 * @param timestamp Current timestamp
 * @return true on success, false on error
 */
bool social_context_observe_agent(social_context_t ctx,
                                   uint32_t agent_id,
                                   uint64_t timestamp);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string name for social group type
 *
 * @param type Social group type
 * @return String name (static, do not free)
 */
const char* social_group_name(social_group_t type);

/**
 * @brief Get string name for social rank type
 *
 * @param type Social rank type
 * @return String name (static, do not free)
 */
const char* social_rank_name(social_rank_t type);

/**
 * @brief Get string name for cultural context type
 *
 * @param type Cultural context type
 * @return String name (static, do not free)
 */
const char* cultural_context_name(cultural_context_t type);

/**
 * @brief Get string name for emotional context type
 *
 * @param type Emotional context type
 * @return String name (static, do not free)
 */
const char* emotional_context_name(emotional_context_t type);

/**
 * @brief Initialize default modulation factors
 *
 * WHAT: Create modulation struct with default values
 * WHY:  Safe initialization for new agents
 *
 * @return Default modulation factors
 */
social_modulation_t social_modulation_init(void);

/**
 * @brief Print social modulation for debugging
 *
 * @param modulation Modulation to print
 * @param prefix Line prefix (can be NULL)
 */
void social_modulation_print(const social_modulation_t* modulation,
                             const char* prefix);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_SOCIAL_CONTEXT_H
