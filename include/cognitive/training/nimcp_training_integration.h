/**
 * @file nimcp_training_integration.h
 * @brief Training Integration Layer - Unified API for Training Pipeline
 *
 * WHAT: Simplified wrapper around basal ganglia, medulla, symbolic logic,
 *       reasoning chain, and information forager for the training pipeline
 * WHY:  The training pipeline needs clean, simple APIs to interact with
 *       brain subsystems without managing their individual complexities
 * HOW:  Each function guards against NULL brain and missing subsystems,
 *       returning sensible defaults when subsystems are unavailable
 *
 * SUBSYSTEMS WRAPPED:
 * 1. Basal Ganglia (BG) - Reward processing, habit learning, conflict detection
 * 2. Medulla - Arousal modulation, circadian rhythm, homeostatic regulation
 * 3. Symbolic Logic - Fact/rule management, forward/backward chaining
 * 4. Reasoning Chain - Multi-step reasoning with traceable steps
 *
 * DESIGN PRINCIPLES:
 * - All functions use brain_ti_ prefix
 * - NULL brain returns sensible defaults (never crashes)
 * - Disabled subsystems return defaults (graceful degradation)
 * - No heap allocations visible to caller (all managed internally)
 * - Thread-safe: delegates to thread-safe subsystem APIs
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#ifndef NIMCP_TRAINING_INTEGRATION_H
#define NIMCP_TRAINING_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-declare brain_t to avoid header dependency cycles */
#ifndef NIMCP_BRAIN_T_FWD
#define NIMCP_BRAIN_T_FWD
struct brain_struct;
typedef struct brain_struct* brain_t;
#endif

/*=============================================================================
 * BASAL GANGLIA TRAINING INTEGRATION
 *
 * Wraps reward processing, conflict detection, and habit learning
 * for the training pipeline. Uses enhanced BG when available.
 *===========================================================================*/

/**
 * @brief Update dopamine with reward prediction error from training batch
 *
 * WHAT: Feed actual vs expected reward into basal ganglia dopamine system
 * WHY:  Drives reward-based learning rate modulation in the training pipeline
 * HOW:  Calls bg_enhanced_process_reward() or basal_ganglia_update_dopamine()
 *
 * @param brain Brain instance (NULL safe)
 * @param actual_reward Actual reward from training batch (e.g., accuracy)
 * @param expected_reward Expected reward (e.g., running average accuracy)
 * @return 0 on success, -1 on error or NULL brain
 */
int brain_ti_update_reward(brain_t brain, float actual_reward, float expected_reward);

/**
 * @brief Get current conflict level between competing actions
 *
 * WHAT: Query basal ganglia conflict level
 * WHY:  High conflict may indicate the model is uncertain between strategies
 * HOW:  Reads conflict_level from BG core system
 *
 * @param brain Brain instance (NULL safe)
 * @return Conflict level [0,1] where 0=no conflict, 1=max conflict.
 *         Returns 0.0 if brain is NULL or BG is disabled.
 */
float brain_ti_get_conflict(brain_t brain);

/**
 * @brief Get basal ganglia operating mode
 *
 * WHAT: Query current BG operating mode
 * WHY:  Training pipeline can adapt strategy based on BG mode
 * HOW:  Reads mode from BG core system
 *
 * @param brain Brain instance (NULL safe)
 * @return Operating mode: 0=goal_directed, 1=habitual, 2=exploratory, 3=suppressed.
 *         Returns 0 (goal_directed) if brain is NULL or BG is disabled.
 */
int brain_ti_get_mode(brain_t brain);

/**
 * @brief Get current dopamine level
 *
 * WHAT: Query basal ganglia dopamine level
 * WHY:  Dopamine level modulates learning rate and exploration
 * HOW:  Reads dopamine_level from BG core system
 *
 * @param brain Brain instance (NULL safe)
 * @return Dopamine level [0,1]. Returns 0.5 (baseline) if unavailable.
 */
float brain_ti_get_dopamine(brain_t brain);

/**
 * @brief Get reward prediction error from last update
 *
 * WHAT: Query the RPE computed by the last brain_ti_update_reward call
 * WHY:  RPE is used to modulate adaptive learning rate
 * HOW:  Reads reward_prediction_error from BG core system
 *
 * @param brain Brain instance (NULL safe)
 * @return RPE value. Returns 0.0 if unavailable.
 */
float brain_ti_get_rpe(brain_t brain);

/**
 * @brief Register a habit for a domain/action pair
 *
 * WHAT: Create a new habit association between a training domain and action
 * WHY:  Allow the training pipeline to form habitual responses per domain
 * HOW:  Hashes domain string to context ID, registers with BG habit system
 *
 * @param brain Brain instance (NULL safe)
 * @param domain Training domain string (e.g., "math", "language")
 * @param action_id Action identifier to associate with domain
 * @return Habit ID on success, or -1 on error/unavailable
 */
int brain_ti_register_habit(brain_t brain, const char* domain, uint32_t action_id);

/**
 * @brief Check if a habit exists for a training domain
 *
 * WHAT: Query BG for an existing habit triggered by domain context
 * WHY:  Detect when training has formed habitual responses
 * HOW:  Hashes domain string, checks BG habit system
 *
 * @param brain Brain instance (NULL safe)
 * @param domain Training domain string
 * @return Associated action_id if habit exists, or -1 if no habit found
 */
int brain_ti_check_habit(brain_t brain, const char* domain);

/**
 * @brief Strengthen a habit after successful execution
 *
 * WHAT: Reinforce or weaken a habit based on outcome
 * WHY:  Habits should strengthen with repeated success
 * HOW:  Calls basal_ganglia_strengthen_habit()
 *
 * @param brain Brain instance (NULL safe)
 * @param habit_id Habit to strengthen (from brain_ti_register_habit)
 * @param success true if the habitual action was successful
 * @return 0 on success, -1 on error
 */
int brain_ti_strengthen_habit(brain_t brain, int habit_id, bool success);

/**
 * @brief Get habit strength
 *
 * WHAT: Query the strength of a learned habit
 * WHY:  Determine if training has reached habitual proficiency
 * HOW:  Calls basal_ganglia_get_habit_strength()
 *
 * @param brain Brain instance (NULL safe)
 * @param habit_id Habit to query
 * @return Habit strength [0,1], or -1.0 on error
 */
float brain_ti_get_habit_strength(brain_t brain, int habit_id);

/*=============================================================================
 * MEDULLA TRAINING INTEGRATION
 *
 * Wraps arousal modulation and circadian rhythm for learning rate
 * and consolidation scheduling.
 *===========================================================================*/

/**
 * @brief Get arousal level for learning rate modulation
 *
 * WHAT: Query medulla arousal level
 * WHY:  Higher arousal = better learning receptivity
 * HOW:  Calls medulla_get_arousal_level()
 *
 * @param brain Brain instance (NULL safe)
 * @return Arousal level [0,1]. Returns 0.5 (baseline) if unavailable.
 */
float brain_ti_get_arousal(brain_t brain);

/**
 * @brief Get current circadian phase
 *
 * WHAT: Query medulla circadian phase
 * WHY:  Different phases have different learning/consolidation efficiency
 * HOW:  Calls medulla_get_circadian_phase()
 *
 * @param brain Brain instance (NULL safe)
 * @return Circadian phase (0-7, maps to 8 phases). Returns 0 if unavailable.
 */
int brain_ti_get_circadian_phase(brain_t brain);

/**
 * @brief Boost arousal level
 *
 * WHAT: Increase medulla arousal (e.g., during important training phases)
 * WHY:  Boost learning receptivity during critical training moments
 * HOW:  Calls medulla_boost_arousal()
 *
 * @param brain Brain instance (NULL safe)
 * @param delta Arousal increase amount (typically 0.05-0.2)
 * @return 0 on success, -1 on error or unavailable
 */
int brain_ti_boost_arousal(brain_t brain, float delta);

/**
 * @brief Reduce medulla arousal level
 *
 * WHAT: Decrease arousal when brain is over-stimulated
 * WHY:  High arousal during easy examples wastes resources
 * HOW:  Delegates to medulla_reduce_arousal with positive delta
 *
 * @param brain Brain instance (NULL safe)
 * @param delta Arousal decrease amount (positive, typically 0.05-0.2)
 * @return 0 on success, -1 on error or unavailable
 */
int brain_ti_reduce_arousal(brain_t brain, float delta);

/**
 * @brief Get circadian efficiency multiplier for consolidation
 *
 * WHAT: Compute efficiency factor from current circadian phase
 * WHY:  Scale consolidation strength based on time-of-day effects
 * HOW:  Maps circadian phase to efficiency multiplier in range [0.8, 1.5]
 *
 * Phase efficiency mapping:
 *   MORNING (peak):        1.5  - Best for learning
 *   EARLY_MORNING/EVENING: 1.2  - Good
 *   AFTERNOON:             1.0  - Neutral (post-lunch dip recovery)
 *   LATE_EVENING:          0.9  - Declining
 *   NIGHT/DEEP_NIGHT:      0.85 - Low (but consolidation is high)
 *   PRE_DAWN:              0.8  - Minimum
 *
 * @param brain Brain instance (NULL safe)
 * @return Efficiency multiplier [0.8, 1.5]. Returns 1.0 if unavailable.
 */
float brain_ti_get_circadian_efficiency(brain_t brain);

/*=============================================================================
 * SYMBOLIC LOGIC TRAINING INTEGRATION
 *
 * Wraps knowledge base operations and inference for training data
 * that contains logical facts and rules.
 *===========================================================================*/

/**
 * @brief Add a logical fact from training data
 *
 * WHAT: Parse and store a fact in the brain's knowledge base
 * WHY:  Build declarative knowledge from training data
 * HOW:  Calls brain_add_logical_fact()
 *
 * @param brain Brain instance (NULL safe)
 * @param fact_str Fact string in logic syntax (e.g., "Bird(tweety)")
 * @param salience Importance score [0,1]
 * @return true on success, false on error or unavailable
 */
bool brain_ti_add_fact(brain_t brain, const char* fact_str, float salience);

/**
 * @brief Add a logical rule from training data
 *
 * WHAT: Parse and store an inference rule
 * WHY:  Enable automated reasoning over training knowledge
 * HOW:  Calls brain_add_logical_rule()
 *
 * @param brain Brain instance (NULL safe)
 * @param rule_str Rule string in logic syntax (e.g., "Bird(x) -> Fly(x)")
 * @param priority Rule application priority [0,1]
 * @return true on success, false on error or unavailable
 */
bool brain_ti_add_rule(brain_t brain, const char* rule_str, float priority);

/**
 * @brief Run forward chaining to derive new facts
 *
 * WHAT: Apply rules to existing facts to derive new knowledge
 * WHY:  Discover implicit knowledge from training data
 * HOW:  Calls brain_forward_chain() with iteration limit
 *
 * @param brain Brain instance (NULL safe)
 * @param max_iterations Maximum inference iterations (0 = default 100)
 * @return Number of new facts derived, or 0 if unavailable
 */
int brain_ti_forward_chain(brain_t brain, uint32_t max_iterations);

/**
 * @brief Run backward chaining to prove a goal
 *
 * WHAT: Attempt to prove a goal from existing knowledge
 * WHY:  Verify hypotheses derived from training data
 * HOW:  Calls brain_backward_chain() and returns confidence
 *
 * @param brain Brain instance (NULL safe)
 * @param goal_str Goal to prove in logic syntax
 * @return Confidence [0,1] if proven, 0.0 if not proven, -1.0 on error
 */
float brain_ti_backward_chain(brain_t brain, const char* goal_str);

/**
 * @brief Query knowledge base for matching facts
 *
 * WHAT: Search for facts matching a query pattern
 * WHY:  Check what knowledge has been accumulated from training
 * HOW:  Calls brain_query_knowledge() and returns match count
 *
 * @param brain Brain instance (NULL safe)
 * @param query_str Query pattern in logic syntax
 * @return Number of matching facts, or 0 if unavailable
 */
int brain_ti_query_knowledge(brain_t brain, const char* query_str);

/**
 * @brief Logic engine statistics
 */
typedef struct {
    uint32_t total_facts;        /**< Total facts in knowledge base */
    uint32_t total_rules;        /**< Total inference rules */
    uint32_t facts_derived;      /**< Facts derived via forward chaining */
    uint32_t proofs_completed;   /**< Successful backward chain proofs */
    uint32_t proofs_failed;      /**< Failed backward chain proofs */
} brain_ti_logic_stats_t;

/**
 * @brief Get logic engine statistics
 *
 * @param brain Brain instance (NULL safe)
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error or unavailable
 */
int brain_ti_get_logic_stats(brain_t brain, brain_ti_logic_stats_t* stats);

/*=============================================================================
 * REASONING CHAIN TRAINING INTEGRATION
 *
 * Wraps multi-step reasoning engine for training queries that
 * require deliberative, transparent reasoning.
 *===========================================================================*/

/**
 * @brief Initialize reasoning engine for training (attached to brain, cached)
 *
 * WHAT: Create and connect a reasoning engine to the brain
 * WHY:  Required before brain_ti_reason() calls
 * HOW:  Creates reasoning engine, connects to brain, caches internally
 *
 * NOTE: Uses a simple single-brain cache. Only one brain can have
 * an active reasoning engine at a time via this API.
 *
 * @param brain Brain instance (NULL safe)
 * @return 0 on success, -1 on error
 */
int brain_ti_init_reasoning(brain_t brain);

/**
 * @brief Run multi-step reasoning on a query
 *
 * WHAT: Execute full reasoning pipeline for a query
 * WHY:  Enable training pipeline to perform deliberative reasoning
 * HOW:  Delegates to reasoning_engine_reason() with cached engine
 *
 * @param brain Brain instance (must have called brain_ti_init_reasoning first)
 * @param query Natural language query string
 * @return Overall confidence [0,1] on success, -1.0 on error
 */
float brain_ti_reason(brain_t brain, const char* query);

/**
 * @brief Get number of steps from last reasoning chain
 *
 * @param brain Brain instance (NULL safe)
 * @return Number of reasoning steps from last brain_ti_reason call, or 0
 */
uint32_t brain_ti_get_reasoning_steps(brain_t brain);

/**
 * @brief Destroy cached reasoning engine
 *
 * WHAT: Free the internally cached reasoning engine
 * WHY:  Clean up when training is complete
 * HOW:  Destroys engine, clears cache
 *
 * @param brain Brain instance (NULL safe, also clears if brain matches cache)
 */
void brain_ti_destroy_reasoning(brain_t brain);

/*=============================================================================
 * COMBINED TRAINING HELPERS
 *
 * Higher-level functions that combine multiple subsystems for
 * common training pipeline operations.
 *===========================================================================*/

/**
 * @brief Compute scaled learning rate based on BG reward and medulla arousal
 *
 * WHAT: Adaptive learning rate that responds to brain state
 * WHY:  Learning rate should increase when brain is aroused and rewarded
 * HOW:  base_lr * arousal_factor * circadian_factor * (1 + rpe_bonus)
 *
 * FORMULA:
 *   arousal_factor   = 0.5 + 0.5 * arousal       (0.5x at zero, 1.0x at full)
 *   circadian_factor = brain_ti_get_circadian_efficiency()
 *   rpe_bonus        = clamp(rpe * 0.2, -0.2, 0.3)
 *   result           = base_lr * arousal_factor * circadian_factor * (1.0 + rpe_bonus)
 *
 * @param brain Brain instance (NULL safe - returns base_lr unmodified)
 * @param base_lr Base learning rate
 * @return Scaled learning rate
 */
float brain_ti_compute_adaptive_lr(brain_t brain, float base_lr);

/**
 * @brief Run full post-batch integration update
 *
 * WHAT: Combined post-batch processing across all training subsystems
 * WHY:  Consolidate common per-batch operations into one call
 * HOW:  Updates BG reward, checks habits, strengthens on success, logs stats
 *
 * OPERATIONS:
 * 1. Call brain_ti_update_reward(brain, accuracy, expected_accuracy)
 * 2. Call brain_ti_check_habit(brain, domain) - if habit strength > 0.7, log "habitual mode"
 * 3. If accuracy > expected_accuracy, strengthen any active habit
 *
 * @param brain Brain instance (NULL safe)
 * @param accuracy Actual batch accuracy [0,1]
 * @param expected_accuracy Expected/target accuracy [0,1]
 * @param domain Training domain string (e.g., "math", "language")
 * @return 0 on success, -1 on error
 */
int brain_ti_post_batch_update(brain_t brain, float accuracy, float expected_accuracy,
                               const char* domain);

/*=============================================================================
 * PORTIA-REASONING RESOURCE ADAPTATION
 *
 * Wraps the Portia-reasoning bridge for the training pipeline.
 * Allows training to query resource budget before running reasoning.
 *===========================================================================*/

/**
 * @brief Check if Portia recommends skipping reasoning
 *
 * WHAT: Query whether system resources are too exhausted for reasoning
 * WHY:  Training pipeline should skip brain_ti_reason() when resources are critical
 * HOW:  Delegates to reasoning_portia_should_skip()
 *
 * @return true if reasoning should be skipped (EMERGENCY + CRITICAL thermal)
 */
bool brain_ti_should_skip_reasoning(void);

/**
 * @brief Get current Portia degradation level affecting reasoning
 *
 * WHAT: Query the degradation level that shapes the reasoning budget
 * WHY:  Training pipeline can log/adapt based on resource pressure
 * HOW:  Computes Portia budget and returns the source degradation level
 *
 * @return Degradation level 0-4 (NONE to EMERGENCY), or 0 if Portia unavailable
 */
int brain_ti_get_reasoning_degradation(void);

/**
 * @brief Get number of reasoning phases currently disabled by Portia
 *
 * WHAT: Count how many phases would be shed under current resource pressure
 * WHY:  Training can decide to skip reasoning if too many phases are disabled
 * HOW:  Computes budget, applies to default config, counts disabled phases
 *
 * @return Number of phases disabled (0 = full pipeline), or -1 on error
 */
int brain_ti_get_reasoning_phases_disabled(void);

/*=============================================================================
 * HYPOTHALAMUS-REASONING MOTIVATIONAL MODULATION
 *
 * Wraps the hypothalamus-reasoning bridge for the training pipeline.
 * Allows training to query cognitive state before running reasoning.
 *===========================================================================*/

/**
 * @brief Get current cognitive capacity from hypothalamus
 *
 * WHAT: Query brain's hypothalamus for alertness, fatigue, stress
 * WHY:  Training can skip reasoning when cognitive capacity is too low
 * HOW:  Delegates to reasoning_hypo_compute_modulation()
 *
 * @param brain Brain instance (NULL returns 1.0 = full capacity)
 * @return Cognitive capacity [0,1], or 1.0 if hypothalamus unavailable
 */
float brain_ti_get_cognitive_capacity(brain_t brain);

/**
 * @brief Get current urgency mode from hypothalamus
 *
 * WHAT: Query autonomic state for fight-or-flight / alert / normal / relaxed
 * WHY:  Training can adjust reasoning strategy based on urgency
 * HOW:  Delegates to reasoning_hypo_compute_modulation()
 *
 * @param brain Brain instance (NULL returns 1 = NORMAL)
 * @return Urgency mode 0-3 (RELAXED to FIGHT_OR_FLIGHT), or 1 if unavailable
 */
int brain_ti_get_urgency_mode(brain_t brain);

/**
 * @brief Get current stress level from hypothalamus
 *
 * WHAT: Query HPA axis cortisol level
 * WHY:  High stress reduces reasoning quality; training may want to defer
 * HOW:  Delegates to reasoning_hypo_compute_modulation()
 *
 * @param brain Brain instance (NULL returns 0.0)
 * @return Stress level [0,1], or 0.0 if hypothalamus unavailable
 */
float brain_ti_get_stress_level(brain_t brain);

/*=============================================================================
 * CONVERGENT REASONING INTEGRATION
 *
 * Wraps the convergent reasoning architecture for the training pipeline.
 *===========================================================================*/

/**
 * @brief Check if convergent reasoning mode is active
 *
 * WHAT: Query whether the brain's reasoning engine uses convergent mode
 * WHY:  Training pipeline may log different metrics for convergent vs wave
 *
 * @param brain Brain instance (NULL safe)
 * @return true if convergent reasoning is enabled in the brain's config
 */
bool brain_ti_is_convergent_reasoning(brain_t brain);

/**
 * @brief Get convergent contributor count from last reasoning query
 *
 * WHAT: Number of active contributors in the most recent convergent query
 * WHY:  Training pipeline can track how many modules participated
 *
 * @param brain Brain instance (NULL safe)
 * @return Number of active contributors, or 0 if unavailable
 */
uint32_t brain_ti_get_convergent_contributor_count(brain_t brain);

/* ---- Mesh bridge wrappers (reasoning-mesh distributed consensus) ---- */

/**
 * @brief Check if mesh network is available for reasoning
 * @return true if mesh is initialized and reasoning channel exists
 */
bool brain_ti_mesh_is_available(void);

/**
 * @brief Get mesh channel participant count for reasoning
 * @return Number of participants in reasoning mesh channel, or 0 if unavailable
 */
uint32_t brain_ti_mesh_get_participant_count(void);

/**
 * @brief Get mesh channel coherence for reasoning
 * @return Channel coherence [0,1], or 0.0 if unavailable
 */
float brain_ti_mesh_get_coherence(void);

/*=============================================================================
 * UNIFIED CONTINUOUS MODULATION
 *
 * Replaces brain_ti_compute_adaptive_lr with biologically-accurate
 * continuous modulation from all brain subsystems.
 *===========================================================================*/

/**
 * @brief Complete training modulation state from all brain subsystems
 */
typedef struct {
    /* Individual module outputs */
    float arousal_level;                /**< Raw arousal [0,1] */
    float arousal_cognitive_gain;       /**< Inverted-U cognitive gain [0,1] */
    float arousal_memory_consolidation; /**< Memory consolidation factor [0,1] */
    float circadian_efficiency;         /**< Phase-based efficiency [0.8-1.5] */
    float rpe_bonus;                    /**< Reward prediction error bonus [-0.2,0.3] */
    float inflammation_learning_factor; /**< Inflammation LR modulation [0,1] */
    float inflammation_precision;       /**< Precision modulation [0,1] */
    float instability_lr_scale;         /**< exp(-3*score) from instability [0,1] */
    float instability_batch_scale;      /**< Batch modulation from instability [0,1] */
    float instability_clip_factor;      /**< Gradient clip from instability */
    float portia_learning_gate;         /**< Portia feature gate for learning [0,1] */
    float portia_compute_budget;        /**< Compute budget scale [0,1] */
    float stress_level;                 /**< HPA axis cortisol [0,1] */
    float cognitive_capacity;           /**< Overall cognitive capacity [0,1] */
    float conflict_level;               /**< BG conflict [0,1] */

    /* Composed final modulation factors */
    float final_lr_factor;              /**< Composed LR multiplier */
    float final_batch_factor;           /**< Composed batch multiplier */
    float final_clip_factor;            /**< Composed gradient clip multiplier */
    bool should_pause;                  /**< Any module signals pause */
} brain_ti_modulation_state_t;

/**
 * @brief Compute unified continuous modulation from all brain subsystems
 *
 * FORMULA (biologically accurate):
 *   lr = base_lr
 *     x arousal_cognitive_gain          (inverted-U Yerkes-Dodson, not linear)
 *     x circadian_efficiency            (phase-based)
 *     x (1 + clamp(rpe * 0.2, -0.2, 0.3))
 *     x instability_lr_scale            (exp(-3*instability_score))
 *     x inflammation_learning_factor    (cubic/quadratic blend)
 *     x portia_learning_gate            (sigmoid resource gate)
 *     x (1 - 0.3*stress_level)          (stress penalty)
 *     x (0.7 + 0.3*cognitive_capacity)  (cognitive capacity floor)
 *
 * @param brain Brain instance (NULL safe — returns all 1.0 factors)
 * @param state Output modulation state (full breakdown)
 * @return 0 on success, -1 on error
 */
int brain_ti_compute_modulation_state(brain_t brain, brain_ti_modulation_state_t* state);

/**
 * @brief Compute unified adaptive learning rate using all continuous modules
 *
 * This replaces the old brain_ti_compute_adaptive_lr with the full
 * continuous modulation pipeline.
 *
 * @param brain Brain instance (NULL safe)
 * @param base_lr Base learning rate
 * @param state Optional: if non-NULL, filled with full modulation breakdown
 * @return Modulated learning rate
 */
float brain_ti_compute_unified_lr(brain_t brain, float base_lr,
                                   brain_ti_modulation_state_t* state);

/**
 * @brief Compute unified batch size modulation
 *
 * @param brain Brain instance (NULL safe)
 * @param base_batch Base batch size
 * @return Modulated batch size (float, caller casts to int)
 */
float brain_ti_compute_unified_batch(brain_t brain, float base_batch);

/**
 * @brief Compute unified gradient clip threshold
 *
 * @param brain Brain instance (NULL safe)
 * @param base_clip Base gradient clip threshold
 * @return Modulated clip threshold
 */
float brain_ti_compute_unified_clip(brain_t brain, float base_clip);

/*=============================================================================
 * DECISION CYCLE ORCHESTRATOR
 *
 * Combines Layer 1 (convergent decisions), Layer 2 (causal DAG), and
 * Layer 3 (abductive diagnosis) into a single observe → diagnose →
 * simulate → decide pipeline.
 *===========================================================================*/

#include "middleware/training/nimcp_training_convergent_decision.h"
#include "middleware/training/nimcp_training_diagnosis.h"

/**
 * @brief Training metrics snapshot for the decision cycle
 */
typedef struct {
    float loss_current;
    float loss_previous;
    float grad_norm;
    float grad_norm_previous;
    float loss_volatility;
    float gradient_variance;
    float current_lr;
    float current_batch;
} brain_ti_training_metrics_t;

/**
 * @brief Full decision cycle result combining diagnosis + causal reasoning + convergent consensus
 */
typedef struct {
    /* Convergent decision (Layer 1) */
    training_evidence_type_t consensus_action;
    float lr_factor;
    float batch_factor;
    float grad_clip_factor;
    float urgency;
    bool converged;
    uint32_t num_contributors;

    /* Diagnosis (Layer 3) */
    char primary_diagnosis[256];
    float diagnosis_plausibility;
    bool recommend_pause;
    bool recommend_rollback;

    /* Causal reasoning (Layer 2) */
    char causal_explanation[256];
    float causal_confidence;
    bool lr_change_beneficial;
} brain_ti_decision_cycle_result_t;

/**
 * @brief Run full training decision cycle: observe → diagnose → simulate → decide
 *
 * Combines Layer 1 (convergent decisions), Layer 2 (causal DAG), and Layer 3
 * (abductive diagnosis) into a single decision pipeline.
 *
 * @param brain Brain handle (for reading arousal, inflammation, etc.)
 * @param metrics Current training metrics snapshot
 * @param result Output decision cycle result
 * @return 0 on success, -1 on error
 */
int brain_ti_compute_decision_cycle(
    brain_t brain,
    const brain_ti_training_metrics_t* metrics,
    brain_ti_decision_cycle_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_INTEGRATION_H */
