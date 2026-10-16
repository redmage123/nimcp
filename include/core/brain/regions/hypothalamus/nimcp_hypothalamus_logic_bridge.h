/**
 * @file nimcp_hypothalamus_logic_bridge.h
 * @brief Bidirectional Hypothalamus-Logic Integration Bridge
 *
 * WHAT: Full bidirectional integration between drive system and symbolic reasoning
 * WHY:  Implements motivated reasoning (hot cognition) and logic-driven drive updates
 * HOW:  FEP-based integration with bio-async messaging
 *
 * BIDIRECTIONAL DATA FLOWS:
 *
 * ========== HYPOTHALAMUS -> LOGIC (Motivated Reasoning) ==========
 * 1. Drive Salience: Drives boost relevance of related predicates
 *    - Hunger -> food predicates more salient
 *    - Safety -> threat predicates prioritized
 *    - Curiosity -> novel predicates explored
 *
 * 2. Inference Depth: Urgency modulates reasoning thoroughness
 *    - High urgency -> shallow, fast inference (heuristics)
 *    - Low urgency -> deep, thorough proof search
 *    - Survival mode -> skip non-critical chains
 *
 * 3. Proof Thresholds: Drives affect acceptance criteria
 *    - Hungry agent accepts weaker evidence for "food nearby"
 *    - Fearful agent requires stronger evidence for "safe"
 *    - Wishful thinking modeled explicitly
 *
 * 4. Goal Prioritization: Drives set inference priorities
 *    - Most urgent drive's goals proved first
 *    - Resource allocation to drive-relevant reasoning
 *
 * ========== LOGIC -> HYPOTHALAMUS (Conclusions Affect Drives) ==========
 * 1. Anticipation Updates: Proven facts update drive expectations
 *    - prove(food_available(X)) -> anticipate hunger satisfaction
 *    - prove(threat_present(X)) -> boost safety drive urgency
 *    - prove(social_opportunity(X)) -> anticipate social satisfaction
 *
 * 2. Satisfaction/Frustration: Logical outcomes affect drive state
 *    - Goal achieved logically -> partial satisfaction
 *    - Goal proven impossible -> frustration, drive suppression
 *
 * 3. Belief-Desire Interaction: Logic mediates between beliefs and desires
 *    - Desires (drives) motivate proving certain goals
 *    - Beliefs (facts) constrain achievable satisfactions
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-Hypothalamic connections for goal-directed behavior
 * - Orbitofrontal cortex: Value-based decision making
 * - Anterior cingulate: Effort/reward trade-offs in reasoning
 * - Amygdala-PFC: Emotional biasing of logic (hot cognition)
 *
 * FEP INTEGRATION:
 * - Drive states generate predictions about logical conclusions
 * - Prediction errors from unexpected conclusions update drives
 * - Free energy from logical uncertainty modulates arousal
 *
 * @version 1.0
 * @date 2025-01-10
 */

#ifndef NIMCP_HYPOTHALAMUS_LOGIC_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_LOGIC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/nimcp_symbolic_logic.h"

/*=============================================================================
 * CONFIGURATION CONSTANTS
 *===========================================================================*/

#define HYPO_LOGIC_MAX_PREDICATES 64      /**< Max tracked predicate-drive mappings */
#define HYPO_LOGIC_MAX_GOALS 32           /**< Max concurrent drive-motivated goals */
#define HYPO_LOGIC_MAX_CONCLUSIONS 128    /**< Max cached logical conclusions */
#define HYPO_LOGIC_MODULE_ID 0x4C4F4749   /**< "LOGI" module ID for bio-async */

/*=============================================================================
 * PREDICATE-DRIVE MAPPING
 *===========================================================================*/

/**
 * @brief Predicate relevance to a specific drive
 *
 * Maps logical predicates to the drives they relate to.
 * Example: "food(X)" relates to HUNGER drive with positive valence
 */
typedef struct {
    char predicate_name[LOGIC_MAX_NAME_LENGTH];  /**< Predicate name pattern */
    hypo_drive_type_t drive;                      /**< Related drive */
    float relevance;                              /**< How strongly related [0,1] */
    float valence;                                /**< +1=satisfies, -1=frustrates */
    bool is_goal_predicate;                       /**< Can be a drive-motivated goal */
} hypo_logic_predicate_map_t;

/**
 * @brief Predicate categories for automatic mapping
 */
typedef enum {
    HYPO_PRED_CAT_FOOD = 0,        /**< Food-related predicates */
    HYPO_PRED_CAT_WATER,           /**< Water/hydration predicates */
    HYPO_PRED_CAT_THREAT,          /**< Threat/danger predicates */
    HYPO_PRED_CAT_SAFETY,          /**< Safety/security predicates */
    HYPO_PRED_CAT_SOCIAL,          /**< Social interaction predicates */
    HYPO_PRED_CAT_KNOWLEDGE,       /**< Knowledge/information predicates */
    HYPO_PRED_CAT_TEMPERATURE,     /**< Temperature-related predicates */
    HYPO_PRED_CAT_REST,            /**< Rest/sleep predicates */
    HYPO_PRED_CAT_ACHIEVEMENT,     /**< Competence/mastery predicates */
    HYPO_PRED_CAT_AUTONOMY,        /**< Self-determination predicates */
    HYPO_PRED_CAT_NEUTRAL,         /**< No strong drive association */
    HYPO_PRED_CAT_COUNT
} hypo_predicate_category_t;

/*=============================================================================
 * MOTIVATED REASONING STATE (HYPO -> LOGIC)
 *===========================================================================*/

/**
 * @brief Drive-based reasoning modulation
 *
 * Controls how drives affect logical inference
 */
typedef struct {
    /* Inference depth control */
    uint32_t max_inference_depth;        /**< Current max depth (urgency-modulated) */
    uint32_t base_inference_depth;       /**< Baseline depth when calm */
    float depth_urgency_factor;          /**< How much urgency reduces depth */

    /* Proof threshold modulation */
    float proof_threshold;               /**< Current acceptance threshold [0,1] */
    float base_threshold;                /**< Baseline threshold */
    float wishful_thinking_bias;         /**< Threshold reduction for desired conclusions */

    /* Salience modulation */
    float salience_boost[HYPO_DRIVE_COUNT];  /**< Per-drive salience multiplier */
    float global_salience_bias;              /**< Overall salience adjustment */

    /* Goal prioritization */
    hypo_drive_type_t priority_drive;    /**< Currently prioritized drive */
    float priority_weight;               /**< Weight given to priority drive's goals */

    /* Cognitive load */
    float reasoning_capacity;            /**< Available capacity [0,1] (fatigue-reduced) */
    float effort_willingness;            /**< Willingness to engage in effortful reasoning */

    /* Timestamps */
    uint64_t last_modulation_us;         /**< When modulation was last computed */
} hypo_logic_modulation_t;

/**
 * @brief Drive-motivated goal
 *
 * A logical goal motivated by a drive state
 */
typedef struct {
    hypo_drive_type_t motivating_drive;   /**< Which drive motivates this goal */
    logic_clause_t* goal_clause;          /**< The goal to prove */
    float priority;                       /**< Goal priority [0,1] */
    float anticipated_satisfaction;       /**< Expected satisfaction if proven */
    bool active;                          /**< Currently being pursued */
    uint64_t created_us;                  /**< When goal was created */
    uint64_t deadline_us;                 /**< Urgency-based deadline (0=none) */
} hypo_motivated_goal_t;

/*=============================================================================
 * LOGIC-DRIVEN DRIVE UPDATES (LOGIC -> HYPO)
 *===========================================================================*/

/**
 * @brief Logical conclusion types affecting drives
 */
typedef enum {
    HYPO_CONCL_RESOURCE_AVAILABLE = 0,  /**< Resource that satisfies drive is available */
    HYPO_CONCL_RESOURCE_UNAVAILABLE,    /**< Resource is proven unavailable */
    HYPO_CONCL_THREAT_PRESENT,          /**< Threat has been logically confirmed */
    HYPO_CONCL_THREAT_ABSENT,           /**< Safety has been logically confirmed */
    HYPO_CONCL_GOAL_ACHIEVED,           /**< Drive-motivated goal was proven */
    HYPO_CONCL_GOAL_IMPOSSIBLE,         /**< Drive-motivated goal proven impossible */
    HYPO_CONCL_OPPORTUNITY,             /**< Opportunity for satisfaction discovered */
    HYPO_CONCL_PREDICTION_CONFIRMED,    /**< Predicted conclusion was proven */
    HYPO_CONCL_PREDICTION_VIOLATED,     /**< Expected conclusion was refuted */
    HYPO_CONCL_NEUTRAL,                 /**< No drive-relevant content */
    HYPO_CONCL_COUNT
} hypo_conclusion_type_t;

/**
 * @brief Cached logical conclusion with drive implications
 */
typedef struct {
    logic_clause_t* conclusion;          /**< The proven conclusion */
    hypo_conclusion_type_t type;         /**< Type of conclusion */
    hypo_drive_type_t affected_drive;    /**< Which drive is affected */
    float drive_impact;                  /**< Magnitude of impact [-1,1] */
    float confidence;                    /**< Confidence in conclusion [0,1] */
    bool processed;                      /**< Has been processed by hypothalamus */
    uint64_t timestamp_us;               /**< When conclusion was reached */
} hypo_logic_conclusion_t;

/**
 * @brief Anticipation state from logical reasoning
 */
typedef struct {
    float anticipation[HYPO_DRIVE_COUNT];    /**< Per-drive anticipation [0,1] */
    float frustration[HYPO_DRIVE_COUNT];     /**< Per-drive frustration [0,1] */
    float confidence[HYPO_DRIVE_COUNT];      /**< Confidence in anticipation [0,1] */
    uint64_t timestamp_us;                   /**< When anticipation was computed */
} hypo_logic_anticipation_t;

/*=============================================================================
 * FEP INTEGRATION
 *===========================================================================*/

/**
 * @brief Free Energy from logical reasoning
 */
typedef struct {
    float logical_free_energy;           /**< FE from logical uncertainty */
    float prediction_error;              /**< Error from unexpected conclusions */
    float precision;                     /**< Confidence in logical predictions */
    float complexity_cost;               /**< Cost of reasoning effort */
    float expected_info_gain;            /**< Expected reduction in uncertainty */
    uint64_t timestamp_us;               /**< When computed */
} hypo_logic_fep_state_t;

/**
 * @brief Logical prediction (drive-based expectations)
 */
typedef struct {
    hypo_drive_type_t drive;             /**< Drive making prediction */
    char predicate_pattern[LOGIC_MAX_NAME_LENGTH];  /**< What we expect to prove */
    float predicted_probability;          /**< Expected probability of proving */
    float precision;                     /**< Confidence in prediction */
    bool resolved;                       /**< Has been checked against reality */
    float actual_result;                 /**< Actual provability (if resolved) */
} hypo_logic_prediction_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Hypothalamus-Logic bridge configuration
 */
typedef struct {
    /* Motivated reasoning settings */
    float base_inference_depth;          /**< Default inference depth */
    float min_inference_depth;           /**< Minimum depth under high urgency */
    float urgency_depth_sensitivity;     /**< How much urgency reduces depth */

    float base_proof_threshold;          /**< Default proof acceptance threshold */
    float wishful_thinking_max;          /**< Max threshold reduction for desires */
    float threshold_drive_sensitivity;   /**< How much drives affect threshold */

    float salience_drive_weight;         /**< Weight of drives in salience computation */
    float salience_decay_rate;           /**< How fast salience boosts decay */

    /* Logic-driven updates settings */
    float anticipation_gain;             /**< How much proofs affect anticipation */
    float frustration_gain;              /**< How much failures affect frustration */
    float conclusion_decay_rate;         /**< How fast conclusion effects decay */

    /* FEP settings */
    bool enable_fep_integration;         /**< Use FEP for prediction/error */
    float prediction_learning_rate;      /**< Rate of updating predictions */
    float precision_base;                /**< Default precision for predictions */

    /* Fatigue/capacity settings */
    float fatigue_capacity_weight;       /**< How much fatigue reduces capacity */
    float arousal_capacity_weight;       /**< How arousal affects capacity */

    /* Bio-async settings */
    bool enable_bio_async;               /**< Use bio-async messaging */
    uint32_t update_interval_us;         /**< Minimum update interval */

    /* Predicate mappings */
    uint32_t num_predicate_maps;         /**< Number of predicate-drive mappings */
    hypo_logic_predicate_map_t predicate_maps[HYPO_LOGIC_MAX_PREDICATES];
} hypo_logic_config_t;

/*=============================================================================
 * BRIDGE STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge operation statistics
 */
typedef struct {
    /* Hypo -> Logic stats */
    uint64_t salience_modulations;       /**< Times salience was modulated */
    uint64_t depth_modulations;          /**< Times depth was modulated */
    uint64_t threshold_modulations;      /**< Times threshold was modulated */
    uint64_t goals_created;              /**< Drive-motivated goals created */
    uint64_t goals_achieved;             /**< Goals successfully proven */
    uint64_t goals_abandoned;            /**< Goals abandoned (impossible/timeout) */

    /* Logic -> Hypo stats */
    uint64_t conclusions_processed;      /**< Logical conclusions processed */
    uint64_t anticipation_updates;       /**< Times anticipation was updated */
    uint64_t frustration_events;         /**< Times frustration was triggered */
    uint64_t drive_boosts;               /**< Times drives were boosted by logic */
    uint64_t drive_reductions;           /**< Times drives were reduced by logic */

    /* FEP stats */
    uint64_t predictions_made;           /**< Logical predictions generated */
    uint64_t predictions_confirmed;      /**< Predictions that were confirmed */
    uint64_t predictions_violated;       /**< Predictions that were wrong */
    float avg_prediction_error;          /**< Average prediction error */
    float avg_logical_free_energy;       /**< Average logical FE */

    /* Performance stats */
    float avg_modulation_latency_us;     /**< Average modulation computation time */
    float avg_conclusion_latency_us;     /**< Average conclusion processing time */
    uint64_t bio_messages_sent;          /**< Bio-async messages sent */
    uint64_t bio_messages_received;      /**< Bio-async messages received */
} hypo_logic_stats_t;

/*=============================================================================
 * OPAQUE BRIDGE TYPE
 *===========================================================================*/

/** @brief Opaque bridge type */
typedef struct hypo_logic_bridge hypo_logic_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * @return Default configuration with reasonable defaults
 */
hypo_logic_config_t hypo_logic_default_config(void);

/**
 * @brief Create hypothalamus-logic bridge
 *
 * @param drives Drive system handle
 * @param logic Symbolic logic engine
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
hypo_logic_bridge_t* hypo_logic_bridge_create(
    hypo_drive_system_handle_t* drives,
    symbolic_logic_t* logic,
    const hypo_logic_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_logic_bridge_destroy(hypo_logic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_bridge_reset(hypo_logic_bridge_t* bridge);

/**
 * @brief Update bridge (call periodically)
 *
 * Processes bidirectional updates:
 * - Computes drive-based modulation of logic
 * - Processes logical conclusions for drive updates
 * - Updates FEP predictions and errors
 *
 * @param bridge Bridge instance
 * @param delta_us Time since last update
 * @return 0 on success, -1 on failure
 */
int hypo_logic_bridge_update(hypo_logic_bridge_t* bridge, uint64_t delta_us);

/*=============================================================================
 * HYPOTHALAMUS -> LOGIC: MOTIVATED REASONING
 *===========================================================================*/

/**
 * @brief Compute drive-based modulation of reasoning
 *
 * Computes how current drive state should affect logical inference:
 * - Inference depth based on urgency
 * - Proof thresholds based on desires
 * - Predicate salience based on drive relevance
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_compute_modulation(hypo_logic_bridge_t* bridge);

/**
 * @brief Get current reasoning modulation state
 *
 * @param bridge Bridge instance
 * @param modulation Output modulation state
 * @return 0 on success, -1 on failure
 */
int hypo_logic_get_modulation(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_modulation_t* modulation);

/**
 * @brief Apply modulation to logic engine
 *
 * Actually applies the computed modulation to the logic engine:
 * - Sets max inference depth
 * - Adjusts predicate salience in knowledge base
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_apply_modulation(hypo_logic_bridge_t* bridge);

/**
 * @brief Get drive-weighted salience for a predicate
 *
 * Returns how salient a predicate should be given current drives.
 *
 * @param bridge Bridge instance
 * @param predicate_name Predicate name
 * @return Salience multiplier (>1 = more salient, <1 = less salient)
 */
float hypo_logic_get_predicate_salience(
    const hypo_logic_bridge_t* bridge,
    const char* predicate_name);

/**
 * @brief Get drive-adjusted proof threshold for a goal
 *
 * Returns the acceptance threshold for proving a specific goal,
 * adjusted by how much we "want" it to be true.
 *
 * @param bridge Bridge instance
 * @param goal Goal clause
 * @return Acceptance threshold [0,1] (lower = more accepting)
 */
float hypo_logic_get_goal_threshold(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* goal);

/**
 * @brief Create drive-motivated goal
 *
 * Creates a logical goal motivated by a drive state.
 * The goal will be prioritized based on drive urgency.
 *
 * @param bridge Bridge instance
 * @param drive Motivating drive
 * @param goal_clause Goal to prove
 * @param anticipated_satisfaction Expected satisfaction if proven
 * @return 0 on success, -1 on failure
 */
int hypo_logic_create_motivated_goal(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    logic_clause_t* goal_clause,
    float anticipated_satisfaction);

/**
 * @brief Get prioritized goals
 *
 * Returns goals sorted by drive-weighted priority.
 *
 * @param bridge Bridge instance
 * @param goals Output goal array
 * @param max_goals Maximum goals to return
 * @param num_goals Output: actual number returned
 * @return 0 on success, -1 on failure
 */
int hypo_logic_get_prioritized_goals(
    const hypo_logic_bridge_t* bridge,
    hypo_motivated_goal_t* goals,
    uint32_t max_goals,
    uint32_t* num_goals);

/**
 * @brief Get recommended inference depth for current state
 *
 * @param bridge Bridge instance
 * @return Recommended max inference depth
 */
uint32_t hypo_logic_get_recommended_depth(const hypo_logic_bridge_t* bridge);

/*=============================================================================
 * LOGIC -> HYPOTHALAMUS: CONCLUSIONS AFFECT DRIVES
 *===========================================================================*/

/**
 * @brief Process logical conclusion for drive updates
 *
 * Analyzes a logical conclusion and updates drive state accordingly:
 * - Resource availability -> anticipation
 * - Threat confirmation -> safety drive boost
 * - Goal achievement -> satisfaction
 * - Goal impossibility -> frustration
 *
 * @param bridge Bridge instance
 * @param conclusion Logical conclusion clause
 * @param confidence Confidence in conclusion [0,1]
 * @return 0 on success, -1 on failure
 */
int hypo_logic_process_conclusion(
    hypo_logic_bridge_t* bridge,
    logic_clause_t* conclusion,
    float confidence);

/**
 * @brief Notify that a motivated goal was achieved
 *
 * @param bridge Bridge instance
 * @param drive Motivating drive
 * @param goal Achieved goal
 * @return Resulting reward signal
 */
float hypo_logic_goal_achieved(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    const logic_clause_t* goal);

/**
 * @brief Notify that a motivated goal is proven impossible
 *
 * @param bridge Bridge instance
 * @param drive Motivating drive
 * @param goal Failed goal
 * @return Resulting frustration signal
 */
float hypo_logic_goal_impossible(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    const logic_clause_t* goal);

/**
 * @brief Get current anticipation state from logical reasoning
 *
 * @param bridge Bridge instance
 * @param anticipation Output anticipation state
 * @return 0 on success, -1 on failure
 */
int hypo_logic_get_anticipation(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_anticipation_t* anticipation);

/**
 * @brief Classify conclusion type
 *
 * Determines what type of drive-relevant conclusion this is.
 *
 * @param bridge Bridge instance
 * @param conclusion Conclusion to classify
 * @return Conclusion type
 */
hypo_conclusion_type_t hypo_logic_classify_conclusion(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion);

/**
 * @brief Get affected drive for a conclusion
 *
 * @param bridge Bridge instance
 * @param conclusion Conclusion to analyze
 * @return Affected drive type, or HYPO_DRIVE_COUNT if none
 */
hypo_drive_type_t hypo_logic_get_affected_drive(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion);

/*=============================================================================
 * FEP INTEGRATION
 *===========================================================================*/

/**
 * @brief Generate drive-based logical predictions
 *
 * Creates predictions about what we expect to prove based on drives.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_generate_predictions(hypo_logic_bridge_t* bridge);

/**
 * @brief Update predictions based on logical results
 *
 * Computes prediction errors from actual logical conclusions.
 *
 * @param bridge Bridge instance
 * @param conclusion Actual conclusion reached
 * @return Prediction error magnitude
 */
float hypo_logic_update_predictions(
    hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion);

/**
 * @brief Compute logical free energy
 *
 * Computes free energy from logical uncertainty and prediction errors.
 *
 * @param bridge Bridge instance
 * @param fe_state Output FEP state
 * @return 0 on success, -1 on failure
 */
int hypo_logic_compute_free_energy(
    hypo_logic_bridge_t* bridge,
    hypo_logic_fep_state_t* fe_state);

/**
 * @brief Get current FEP state
 *
 * @param bridge Bridge instance
 * @param fe_state Output FEP state
 * @return 0 on success, -1 on failure
 */
int hypo_logic_get_fep_state(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_fep_state_t* fe_state);

/*=============================================================================
 * PREDICATE-DRIVE MAPPING
 *===========================================================================*/

/**
 * @brief Register predicate-drive mapping
 *
 * Maps a predicate pattern to a drive for automatic salience/impact.
 *
 * @param bridge Bridge instance
 * @param map Mapping to register
 * @return 0 on success, -1 on failure
 */
int hypo_logic_register_predicate_map(
    hypo_logic_bridge_t* bridge,
    const hypo_logic_predicate_map_t* map);

/**
 * @brief Get predicate category
 *
 * Returns the category of a predicate for automatic mapping.
 *
 * @param predicate_name Predicate name
 * @return Predicate category
 */
hypo_predicate_category_t hypo_logic_get_predicate_category(
    const char* predicate_name);

/**
 * @brief Map category to primary drive
 *
 * @param category Predicate category
 * @return Primary associated drive
 */
hypo_drive_type_t hypo_logic_category_to_drive(hypo_predicate_category_t category);

/**
 * @brief Auto-register common predicate mappings
 *
 * Registers standard mappings for common predicates:
 * - food, eat, hungry -> HUNGER
 * - water, drink, thirsty -> THIRST
 * - threat, danger, safe -> SAFETY
 * - etc.
 *
 * @param bridge Bridge instance
 * @return Number of mappings registered
 */
int hypo_logic_auto_register_mappings(hypo_logic_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register bridge with bio-async router
 *
 * @param bridge Bridge instance
 * @param use_kg_wiring Use knowledge graph wiring
 * @return true on success, false on failure
 */
bool hypo_logic_bridge_register_bio(
    hypo_logic_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Broadcast modulation state via bio-async
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_broadcast_modulation(hypo_logic_bridge_t* bridge);

/**
 * @brief Broadcast conclusion impact via bio-async
 *
 * @param bridge Bridge instance
 * @param conclusion Conclusion that was processed
 * @param impact Impact magnitude
 * @return 0 on success, -1 on failure
 */
int hypo_logic_broadcast_conclusion_impact(
    hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion,
    float impact);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int hypo_logic_get_stats(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int hypo_logic_reset_stats(hypo_logic_bridge_t* bridge);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge instance
 */
void hypo_logic_print_summary(const hypo_logic_bridge_t* bridge);

/*=============================================================================
 * STRING UTILITIES
 *===========================================================================*/

/**
 * @brief Get conclusion type name
 *
 * @param type Conclusion type
 * @return Human-readable name
 */
const char* hypo_conclusion_type_name(hypo_conclusion_type_t type);

/**
 * @brief Get predicate category name
 *
 * @param category Predicate category
 * @return Human-readable name
 */
const char* hypo_predicate_category_name(hypo_predicate_category_t category);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_LOGIC_BRIDGE_H */
