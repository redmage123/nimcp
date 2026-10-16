/**
 * @file nimcp_love_loyalty_friendship.h
 * @brief Love, loyalty, friendship, and positive social bonding system
 *
 * WHAT: Models positive social emotions and relationship bonds
 * WHY:  Essential for social connection, cooperation, and flourishing
 * HOW:  Integrates attachment, trust, reciprocity, and emotional bonding
 *
 * BIOLOGICAL BASIS:
 * - Oxytocin System: Social bonding, trust, attachment
 * - Ventral Striatum/Nucleus Accumbens: Reward from social connection
 * - Anterior Cingulate Cortex (ACC): Social pain/pleasure
 * - Temporoparietal Junction (TPJ): Perspective-taking, empathy
 * - Medial Prefrontal Cortex (mPFC): Mentalizing, understanding others
 *
 * PSYCHOLOGICAL MODELS:
 * - Attachment Theory (Bowlby, 1969): Secure vs. insecure bonds
 * - Sternberg's Triangular Theory of Love: Intimacy, passion, commitment
 * - Equity Theory (Walster et al., 1978): Balance in relationships
 * - Social Exchange Theory: Reciprocity and relationship maintenance
 * - Friendship Development (Hays, 1985): Acquaintance → friendship → close friend
 * - Loyalty as Virtue Ethics (Royce, 1908): Commitment to person/group/cause
 *
 * NEUROSCIENCE REFERENCES:
 * - Bartels & Zeki (2000): "The neural basis of romantic love"
 * - Zeki (2007): "The neurobiology of love"
 * - Feldman (2017): "The neurobiology of human attachments"
 * - Rilling et al. (2002): "A neural basis for social cooperation"
 *
 * KEY CONCEPTS:
 * - Love: Deep affection, care, attachment (romantic, platonic, familial)
 * - Loyalty: Steadfast allegiance, commitment through adversity
 * - Friendship: Mutual affection, trust, shared experiences, reciprocity
 * - Trust: Belief in reliability, honesty, integrity of other
 * - Intimacy: Emotional closeness, vulnerability, mutual understanding
 *
 * @version Phase E4: Love, Loyalty, Friendship (Positive Social Emotions)
 * @date 2025-11-13
 */

#ifndef NIMCP_LOVE_LOYALTY_FRIENDSHIP_H
#define NIMCP_LOVE_LOYALTY_FRIENDSHIP_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"

/* Forward declarations for bridge types */
typedef struct social_snn_bridge social_snn_bridge_t;
typedef struct social_plasticity_bridge social_plasticity_bridge_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum number of relationships to track */
#define SOCIAL_MAX_RELATIONSHIPS 32

/* Maximum number of shared experiences per relationship */
#define SOCIAL_MAX_SHARED_EXPERIENCES 64

/* Time constants (in seconds) */
#define OXYTOCIN_HALF_LIFE (3600.0f)              /* 1 hour - oxytocin decay */
#define RELATIONSHIP_GROWTH_RATE (86400.0f * 30.0f) /* 1 month - bond strengthening */
#define BETRAYAL_RECOVERY_TIME (86400.0f * 365.0f) /* 1 year - trust repair */

/* Intensity thresholds */
#define FRIENDSHIP_THRESHOLD 0.4f       /* Closeness >= 0.4 = friend */
#define CLOSE_FRIEND_THRESHOLD 0.7f     /* Closeness >= 0.7 = close friend */
#define LOVE_THRESHOLD 0.7f             /* Affection >= 0.7 = love */
#define LOYALTY_THRESHOLD 0.6f          /* Commitment >= 0.6 = loyal */

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Types of love
 */
typedef enum {
    LOVE_TYPE_ROMANTIC,         /**< Romantic/passionate love */
    LOVE_TYPE_COMPANIONATE,     /**< Deep friendship love */
    LOVE_TYPE_FAMILIAL,         /**< Family bond */
    LOVE_TYPE_PLATONIC,         /**< Non-romantic deep affection */
    LOVE_TYPE_COMPASSIONATE,    /**< Altruistic caring for others */
    LOVE_TYPE_SELF_LOVE         /**< Healthy self-regard */
} love_type_t;

/**
 * @brief Relationship stage
 */
typedef enum {
    RELATIONSHIP_STRANGER,      /**< Unknown person */
    RELATIONSHIP_ACQUAINTANCE,  /**< Met but not close */
    RELATIONSHIP_FRIEND,        /**< Mutual affection, some sharing */
    RELATIONSHIP_CLOSE_FRIEND,  /**< Deep trust, vulnerability, intimacy */
    RELATIONSHIP_BEST_FRIEND,   /**< Highest closeness, irreplaceable */
    RELATIONSHIP_ROMANTIC,      /**< Romantic love relationship */
    RELATIONSHIP_FAMILY         /**< Family bond */
} relationship_stage_t;

/**
 * @brief Attachment style (Ainsworth et al.)
 */
typedef enum {
    ATTACHMENT_SECURE,          /**< Comfortable with closeness */
    ATTACHMENT_ANXIOUS,         /**< Fear of abandonment */
    ATTACHMENT_AVOIDANT,        /**< Discomfort with intimacy */
    ATTACHMENT_FEARFUL_AVOIDANT /**< Want + fear closeness */
} attachment_style_t;

/**
 * @brief Loyalty type
 */
typedef enum {
    LOYALTY_TO_PERSON,          /**< Allegiance to individual */
    LOYALTY_TO_GROUP,           /**< Allegiance to team/organization */
    LOYALTY_TO_CAUSE,           /**< Commitment to principle/mission */
    LOYALTY_TO_SELF            /**< Self-integrity, authenticity */
} loyalty_type_t;

/**
 * @brief Interaction types
 */
typedef enum {
    INTERACTION_CONVERSATION,   /**< Talking, sharing */
    INTERACTION_SHARED_ACTIVITY,/**< Doing things together */
    INTERACTION_SUPPORT_GIVEN,  /**< Helping, comforting */
    INTERACTION_SUPPORT_RECEIVED,/**< Being helped */
    INTERACTION_VULNERABILITY,  /**< Sharing weakness/fear */
    INTERACTION_CONFLICT,       /**< Disagreement, tension */
    INTERACTION_RECONCILIATION, /**< Making up after conflict */
    INTERACTION_BETRAYAL,       /**< Trust violation */
    INTERACTION_CELEBRATION,    /**< Shared joy, success */
    INTERACTION_COLLABORATION   /**< Working together */
} interaction_type_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Sternberg's love components
 */
typedef struct {
    float intimacy;             /**< Emotional closeness [0-1] */
    float passion;              /**< Intensity, excitement [0-1] */
    float commitment;           /**< Decision to maintain bond [0-1] */
} love_components_t;

/**
 * @brief Shared experience record
 */
typedef struct {
    bool active;                /**< Is this record in use? */
    interaction_type_t type;    /**< What happened */
    uint64_t timestamp;         /**< When it happened */
    float emotional_intensity;  /**< How intense was it [0-1] */
    float valence;              /**< Positive or negative [-1, 1] */
    bool strengthened_bond;     /**< Did this bring us closer? */
} shared_experience_t;

/**
 * @brief Individual relationship representation
 */
typedef struct {
    bool active;                /**< Is this relationship tracked? */
    uint32_t relationship_id;   /**< Unique identifier */

    // Stage and type
    relationship_stage_t stage; /**< Current relationship stage */
    love_type_t love_type;      /**< Type of love (if applicable) */

    // Core dimensions
    float closeness;            /**< Emotional proximity [0-1] */
    float trust;                /**< Confidence in reliability [0-1] */
    float affection;            /**< Warmth, fondness [0-1] */
    float reciprocity;          /**< Balance of giving/receiving [0-1] */

    // Sternberg's love triangle
    love_components_t love;     /**< Intimacy, passion, commitment */

    // Loyalty
    bool is_loyal_to;           /**< Am I loyal to this person/group? */
    loyalty_type_t loyalty_type;/**< What kind of loyalty */
    float loyalty_strength;     /**< How strong is commitment [0-1] */
    uint32_t loyalty_tests_passed; /**< Times loyalty was tested */
    uint32_t loyalty_tests_failed; /**< Times loyalty was broken */

    // Friendship qualities
    float shared_experiences_count; /**< How much time together */
    float vulnerability_shared;  /**< How open have I been [0-1] */
    float support_given;        /**< How much I've helped [0-1] */
    float support_received;     /**< How much they've helped [0-1] */

    // Oxytocin bonding
    float oxytocin_bond_strength; /**< Neurochemical attachment [0-1] */
    uint64_t last_interaction_time; /**< Most recent contact */

    // Relationship history
    shared_experience_t experiences[SOCIAL_MAX_SHARED_EXPERIENCES];
    uint32_t experience_count;
    uint32_t experience_history_index; /**< Ring buffer index */

    // Trust violations
    uint32_t betrayals_experienced; /**< Times trust was broken */
    float trust_repair_progress;   /**< Recovery after betrayal [0-1] */

    // Duration
    uint64_t relationship_start_time; /**< When relationship began */
    float time_known;               /**< Total duration (seconds) */

} relationship_t;

/**
 * @brief Current social-emotional state
 */
typedef struct {
    // Love experience
    bool experiencing_love;     /**< Currently feeling love? */
    float love_intensity;       /**< Current love feeling [0-1] */
    love_type_t love_type_felt; /**< What kind of love */

    // Friendship warmth
    float friendship_warmth;    /**< General positive social feeling [0-1] */
    uint32_t close_friends_count; /**< Number of close friendships */

    // Loyalty state
    bool actively_loyal;        /**< Currently demonstrating loyalty? */
    float loyalty_satisfaction; /**< How fulfilled by loyalties [0-1] */

    // Oxytocin
    float oxytocin_level;       /**< Current oxytocin [0-1] */
    uint64_t last_oxytocin_boost; /**< Most recent bonding moment */

    // Loneliness (absence of connection)
    float loneliness;           /**< Social isolation feeling [0-1] */
    uint64_t last_social_interaction; /**< Time since last contact */

    // Emotional tag integration
    emotional_tag_t social_emotion; /**< Current social emotional tag */

    // Attachment working model
    attachment_style_t attachment_style; /**< How I relate to others */
    float attachment_security;  /**< How secure in relationships [0-1] */

} social_emotional_state_t;

/**
 * @brief Complete love, loyalty, friendship system
 */
typedef struct {
    // Relationships
    relationship_t relationships[SOCIAL_MAX_RELATIONSHIPS];
    uint32_t active_relationship_count;

    // Emotional state
    social_emotional_state_t emotion;

    // Personality traits
    float extraversion;         /**< Social energy, gregariousness [0-1] */
    float agreeableness;        /**< Warmth, compassion [0-1] */
    float openness_to_experience; /**< Willingness to be vulnerable [0-1] */

    // Capacity for love/friendship
    float love_capacity;        /**< Ability to experience love [0-1] */
    float friendship_capacity;  /**< Ability to form friendships [0-1] */
    float loyalty_capacity;     /**< Ability to commit [0-1] */

    // Social skills
    float empathy;              /**< Understanding others' feelings [0-1] */
    float perspective_taking;   /**< Theory of mind for relationships [0-1] */
    float conflict_resolution;  /**< Handling disagreements [0-1] */

    // Integration flags
    bool integrate_with_theory_of_mind;  /**< Use ToM for understanding? */
    bool integrate_with_oxytocin;        /**< Affect oxytocin levels? */
    bool integrate_with_reward;          /**< Social reward via dopamine? */

    // Statistics
    uint64_t total_update_calls;
    uint32_t total_friendships_formed;
    uint32_t total_loves_experienced;
    uint32_t total_loyalty_commitments;
    float average_relationship_closeness;

    // Bio-async integration
    void* bio_ctx;                  /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // SNN and Plasticity bridges
    social_snn_bridge_t* snn_bridge;          /**< SNN integration bridge */
    social_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity integration bridge */
    bool bridges_enabled;                      /**< Bridge integration status */

} social_bond_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Initialize love, loyalty, friendship system
 */
social_bond_system_t* social_bond_system_create(void);

/**
 * @brief Free social bond system resources
 */
void social_bond_system_destroy(social_bond_system_t* system);

/**
 * @brief Reset system to initial state
 */
void social_bond_system_reset(social_bond_system_t* system);

//=============================================================================
// RELATIONSHIP MANAGEMENT FUNCTIONS
//=============================================================================

/**
 * @brief Create a new relationship
 *
 * @param system Social bond system
 * @param initial_stage Starting stage (usually STRANGER or ACQUAINTANCE)
 * @param current_time_us Current time in microseconds
 * @return relationship_id (or 0 if failed)
 */
uint32_t social_create_relationship(social_bond_system_t* system,
                                   relationship_stage_t initial_stage,
                                   uint64_t current_time_us);

/**
 * @brief Process a social interaction
 *
 * WHAT: Record interaction and update relationship dynamics
 * WHY:  Relationships grow through shared experiences
 * HOW:  Update closeness, trust, reciprocity based on interaction
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param type Type of interaction
 * @param emotional_intensity How intense [0-1]
 * @param valence Positive or negative [-1, 1]
 * @param current_time_us Current time in microseconds
 */
void social_process_interaction(social_bond_system_t* system,
                               uint32_t relationship_id,
                               interaction_type_t type,
                               float emotional_intensity,
                               float valence,
                               uint64_t current_time_us);

/**
 * @brief Express vulnerability to someone
 *
 * WHAT: Share weakness, fear, or private information
 * WHY:  Vulnerability deepens intimacy and trust
 * HOW:  Increases closeness if received well
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param vulnerability_level How much shared [0-1]
 * @param received_well Was it met with acceptance?
 */
void social_express_vulnerability(social_bond_system_t* system,
                                 uint32_t relationship_id,
                                 float vulnerability_level,
                                 bool received_well);

/**
 * @brief Provide support to someone
 *
 * WHAT: Help, comfort, or assist in time of need
 * WHY:  Support builds reciprocity and strengthens bonds
 * HOW:  Update support_given, reciprocity, closeness
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param support_quality How helpful was it [0-1]
 */
void social_provide_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality);

/**
 * @brief Receive support from someone
 *
 * WHAT: Be helped by another in time of need
 * WHY:  Receiving support creates gratitude and reciprocity
 * HOW:  Update support_received, trust, closeness
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param support_quality How helpful was it [0-1]
 */
void social_receive_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality);

//=============================================================================
// LOYALTY FUNCTIONS
//=============================================================================

/**
 * @brief Make loyalty commitment
 *
 * WHAT: Pledge allegiance to person/group/cause
 * WHY:  Loyalty provides stability and trust
 * HOW:  Set loyalty flags, increase commitment
 *
 * @param system Social bond system
 * @param relationship_id Which relationship (if to person/group)
 * @param loyalty_type Type of loyalty
 * @param strength Initial commitment strength [0-1]
 */
void social_commit_loyalty(social_bond_system_t* system,
                          uint32_t relationship_id,
                          loyalty_type_t loyalty_type,
                          float strength);

/**
 * @brief Test loyalty (face adversity)
 *
 * WHAT: Loyalty challenged by difficulty or temptation
 * WHY:  Loyalty proven through tests
 * HOW:  Either strengthens or breaks commitment
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param test_difficulty How hard was the test [0-1]
 * @param remained_loyal Did I stay loyal?
 */
void social_test_loyalty(social_bond_system_t* system,
                        uint32_t relationship_id,
                        float test_difficulty,
                        bool remained_loyal);

//=============================================================================
// LOVE FUNCTIONS
//=============================================================================

/**
 * @brief Experience love
 *
 * WHAT: Feel deep affection, care, attachment
 * WHY:  Love is core positive emotion
 * HOW:  Update love components (intimacy, passion, commitment)
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param love_type Type of love being felt
 * @param intensity How intense is the feeling [0-1]
 */
void social_experience_love(social_bond_system_t* system,
                           uint32_t relationship_id,
                           love_type_t love_type,
                           float intensity);

//=============================================================================
// BETRAYAL AND REPAIR
//=============================================================================

/**
 * @brief Experience betrayal
 *
 * WHAT: Trust violation by someone close
 * WHY:  Betrayal damages relationships, needs processing
 * HOW:  Reduce trust, closeness, trigger grief response
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param severity How bad was betrayal [0-1]
 */
void social_experience_betrayal(social_bond_system_t* system,
                               uint32_t relationship_id,
                               float severity);

/**
 * @brief Attempt relationship repair
 *
 * WHAT: Work to rebuild trust after betrayal/conflict
 * WHY:  Relationships can recover with effort
 * HOW:  Gradual trust restoration, forgiveness
 *
 * @param system Social bond system
 * @param relationship_id Which relationship
 * @param repair_effort How much work put in [0-1]
 * @param apology_quality Quality of apology/amends [0-1]
 */
void social_attempt_repair(social_bond_system_t* system,
                          uint32_t relationship_id,
                          float repair_effort,
                          float apology_quality);

//=============================================================================
// UPDATE FUNCTIONS
//=============================================================================

/**
 * @brief Update social-emotional state over time
 *
 * WHAT: Advance relationship dynamics, oxytocin decay, loneliness
 * WHY:  Bonds require maintenance, decay without contact
 * HOW:  Update closeness based on interaction frequency, oxytocin half-life
 *
 * @param system Social bond system
 * @param dt Time step (seconds)
 * @param current_time_us Current time in microseconds
 */
void social_update(social_bond_system_t* system, float dt, uint64_t current_time_us);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if currently experiencing love
 */
bool social_is_experiencing_love(const social_bond_system_t* system);

/**
 * @brief Check if lonely
 */
bool social_is_lonely(const social_bond_system_t* system);

/**
 * @brief Check if loyal to a specific relationship
 */
bool social_is_loyal_to(const social_bond_system_t* system,
                       uint32_t relationship_id);

/**
 * @brief Get relationship closeness
 */
float social_get_relationship_closeness(const social_bond_system_t* system,
                                        uint32_t relationship_id);

/**
 * @brief Get number of close friends
 */
uint32_t social_get_close_friend_count(const social_bond_system_t* system);

/**
 * @brief Get current oxytocin level
 */
float social_get_oxytocin_level(const social_bond_system_t* system);

/**
 * @brief Get neuromodulator effects for integration
 */
void social_get_neuromodulator_effects(const social_bond_system_t* system,
                                      float* dopamine_factor,
                                      float* oxytocin_factor);

//=============================================================================
// EMOTION INTEGRATION
//=============================================================================

/**
 * @brief Get current social emotion tag
 *
 * WHAT: Query love/friendship/loneliness as emotional_tag_t
 * WHY:  Integration with emotional tagging system
 * HOW:  Returns emotional_tag_t with positive valence (love/friendship) or negative (loneliness)
 *
 * @param system Social bond system
 * @return Social emotional tag
 *
 * @note Love: high positive valence [+0.7 to +0.95], high arousal [0.6 to 0.9]
 * @note Friendship: moderate positive valence [+0.4 to +0.7], moderate arousal [0.4 to 0.6]
 * @note Loneliness: negative valence [-0.4 to -0.7], low arousal [0.2 to 0.4]
 */
emotional_tag_t social_get_emotion(const social_bond_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOVE_LOYALTY_FRIENDSHIP_H */
